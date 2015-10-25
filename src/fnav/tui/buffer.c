#include <ncurses.h>
#include <limits.h>

#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"

struct queued_line {
  int lnum;
  String val;
};

struct Buffer {
  WINDOW *nc_win;
  BufferNode *bn;
  Cntlr *cntlr;

  pos_T b_size;
  pos_T b_ofs;
  pos_T cur;

  int lnum;
  int index;
  int start;
  int count;;
  int top;
  int bot;

  fn_handle *hndl;
  bool dirty;
  bool queued;
};

enum Type { OPERATOR, MOTION };

typedef struct {
  String name;
  enum Type type;
  void (*f)();
} Key;

typedef struct {
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int arg;
} Cmdarg;

typedef void (*key_func)(Buffer *buf, Cmdarg *arg);

static void init_cmds(void);
static int find_command(int cmdchar);
static void buf_mv_page();
static void buf_mv();
static void buf_g();
static void buf_ex_cmd();

static const struct buf_cmd {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fm_cmds[] =
{
  {Ctrl_J,  buf_mv_page,     0,           FORWARD},
  {Ctrl_K,  buf_mv_page,     0,           BACKWARD},
  {'o',     buf_ex_cmd,      0,           0},
  {'j',     buf_mv,          0,           FORWARD},
  {'k',     buf_mv,          0,           BACKWARD},
  {'g',     buf_g,           0,           BACKWARD},
  {'G',     buf_g,           0,           FORWARD},
};

#define SET_POS(pos,x,y)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

#define DRAW_LINE(buf,lnum,str)   \
  mvwprintw(buf->nc_win, lnum, 0, str);

void buf_listen(fn_handle *hndl);
void buf_draw(void **argv);

Buffer* buf_init()
{
  log_msg("BUFFER", "init");
  Buffer *buf = malloc(sizeof(Buffer));
  SET_POS(buf->b_size, 0, 0);
  SET_POS(buf->cur, 0, 0);
  buf->nc_win = newwin(1,1,0,0);
  scrollok(buf->nc_win, true);
  buf->dirty = false;
  buf->queued = false;
  init_cmds();
  return buf;
}

void buf_set_size(Buffer *buf, int w, int h)
{
  log_msg("BUFFER", "SET SIZE %d %d", w, h);
  if (h)
    buf->b_size.lnum = h;
  if (w)
    buf->b_size.col = w;
  wresize(buf->nc_win, h, w);
  buf_refresh(buf);
}

void buf_set_ofs(Buffer *buf, int x, int y)
{
  SET_POS(buf->b_size, y, x);
  mvwin(buf->nc_win, y, x);
}

void buf_destroy(Buffer *buf)
{
  delwin(buf->nc_win);
  free(buf);
}

void buf_set_cntlr(Buffer *buf, Cntlr *cntlr)
{
  buf->cntlr = cntlr;
  buf->hndl = cntlr->hndl;
}

void buf_set(fn_handle *hndl)
{
  log_msg("BUFFER", "set");
}

void buf_resize(Buffer *buf, int w, int h)
{
  SET_POS(buf->b_size, w, h);
}

void buf_refresh(Buffer *buf)
{
  log_msg("BUFFER", "refresh");
  if (buf->queued)
    return;
  buf->queued = true;
  window_req_draw(buf, buf_draw);
}

void buf_draw(void **argv)
{
  log_msg("BUFFER", "draw");
  Buffer *buf = (Buffer*)argv[0];
  if (!buf->cntlr) {
    buf->queued = false;
    return;
  }
  int st = buf->start;
  for (int i = 0; i < buf->count; ++i, ++st) {
    String it = model_str_line(buf->hndl->model, buf->top + st);
    DRAW_LINE(buf, st, it);
  }
  wmove(buf->nc_win, buf->cur.lnum, 0);
  wchgat(buf->nc_win, -1, A_REVERSE, 1, NULL);
  wnoutrefresh(buf->nc_win);
  buf->dirty = false;
  buf->queued = false;
}

void buf_full_invalidate(Buffer *buf, int index)
{
  log_msg("BUFFER", "buf_full_invalidate");
  // reset buffer.
  wclear(buf->nc_win);
  int count = model_count(buf->hndl->model);
  count = count > buf->b_size.lnum ? buf->b_size.lnum : count;
  buf->index = 0;
  buf->cur.lnum = 0;
  buf->start = 0;
  buf->bot = count;
  buf->count = count;
  buf_refresh(buf);
}

// input entry point.
// proper commands are chosen based on buffer state.
int buf_input(BufferNode *bn, int key)
{
  log_msg("BUFFER", "input");
  Buffer *buf = bn->buf;
  if (buf->cntlr) {
    buf->cntlr->_input(buf->cntlr, key);
  }
  Cmdarg ca;
  int idx = find_command(key);
  ca.arg = fm_cmds[idx].cmd_arg;
  if (idx >= 0) {
    fm_cmds[idx].cmd_func(buf, &ca);
  }
  // if not consumed send to buffer
  return 0;
}

void buf_scroll(Buffer *buf, int y)
{
  log_msg("BUFFER", "scroll %d %d", buf->cur.lnum, y);
  int top1 = buf->top;
  int bot1 = buf->bot;
  buf->top += y;
  if (buf->top < 0) {
    buf->top = 0;
  }
  else if (buf->top > model_count(buf->hndl->model)) {
    buf->top = model_count(buf->hndl->model);
  }
  int diff = buf->top - top1;
  buf->bot = diff;
  if (y > 0) {
    if (buf->top > top1) {
      buf->start = 0;
      buf->count = diff;
    }
    else {
      buf->start = bot1 - buf->top;
      buf->count = buf->top - top1;
    }
  }
  else {
    buf->start = 0;
    buf->count = top1 - buf->top;
  }
  wscrl(buf->nc_win, diff);
  buf_refresh(buf);
}

static void buf_mv(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_mv %d", arg->arg);
  // move cursor N times in a dimension.
  // scroll if located on an edge.
  int y = arg->arg;
  buf->lnum += y;
  buf->index += y;

  // TODO: make these boundary checkassigns into macro
  if (buf->lnum < 0) {
    buf->lnum = 0;
  }
  if (buf->index < 0) {
    buf->index = 0;
  }
  if (buf->lnum > model_count(buf->hndl->model)) {
    buf->lnum = model_count(buf->hndl->model);
  }

  if (y < 0 && buf->cur.lnum < buf->b_size.lnum * 0.2) {
    buf_scroll(buf, arg->arg);
  }
  if (y > 0 && buf->cur.lnum > buf->b_size.lnum * 0.8) {
    buf_scroll(buf, arg->arg);
  }
  else {
    wmove(buf->nc_win, buf->lnum, 0);
    wchgat(buf->nc_win, -1, A_REVERSE, 1, NULL);
    wnoutrefresh(buf->nc_win);
    doupdate();
  }
  model_set_curs(buf->hndl->model, buf->index);
}

static void buf_mv_page(Buffer *buf, Cmdarg *arg)
{
}

static void buf_g(Buffer *buf, Cmdarg *arg)
{
}

static void buf_ex_cmd(Buffer *buf, Cmdarg *arg)
{
  window_ex_cmd_start();;
}

/* Number of commands in nv_cmds[]. */
#define FM_CMDS_SIZE ARRAY_SIZE(fm_cmds)

/* Sorted index of commands in nv_cmds[]. */
static short nv_cmd_idx[FM_CMDS_SIZE];

/* The highest index for which
 * nv_cmds[idx].cmd_char == nv_cmd_idx[nv_cmds[idx].cmd_char] */
static int nv_max_linear;

/*
 * Compare functions for qsort() below, that checks the command character
 * through the index in nv_cmd_idx[].
 */
static int nv_compare(const void *s1, const void *s2)
{
  int c1, c2;

  /* The commands are sorted on absolute value. */
  c1 = fm_cmds[*(const short *)s1].cmd_char;
  c2 = fm_cmds[*(const short *)s2].cmd_char;
  if (c1 < 0)
    c1 = -c1;
  if (c2 < 0)
    c2 = -c2;
  return c1 - c2;
}

/*
 * Initialize the nv_cmd_idx[] table.
 */
static void init_cmds(void)
{
  /* Fill the index table with a one to one relation. */
  for (short int i = 0; i < (short int)FM_CMDS_SIZE; ++i) {
    nv_cmd_idx[i] = i;
  }

  /* Sort the commands by the command character.  */
  qsort(&nv_cmd_idx, FM_CMDS_SIZE, sizeof(short), nv_compare);

  /* Find the first entry that can't be indexed by the command character. */
  short int i;
  for (i = 0; i < (short int)FM_CMDS_SIZE; ++i) {
    if (i != fm_cmds[nv_cmd_idx[i]].cmd_char) {
      break;
    }
  }
  nv_max_linear = i - 1;
}

/*
 * Search for a command in the commands table.
 * Returns -1 for invalid command.
 */
static int find_command(int cmdchar)
{
  int i;
  int idx;
  int top, bot;
  int c;

  /* A multi-byte character is never a command. */
  if (cmdchar >= 0x100)
    return -1;

  /* We use the absolute value of the character.  Special keys have a
   * negative value, but are sorted on their absolute value. */
  if (cmdchar < 0)
    cmdchar = -cmdchar;

  /* If the character is in the first part: The character is the index into
   * nv_cmd_idx[]. */
  if (cmdchar <= nv_max_linear)
    return nv_cmd_idx[cmdchar];

  /* Perform a binary search. */
  bot = nv_max_linear + 1;
  top = FM_CMDS_SIZE - 1;
  idx = -1;
  while (bot <= top) {
    i = (top + bot) / 2;
    c = fm_cmds[nv_cmd_idx[i]].cmd_char;
    if (c < 0)
      c = -c;
    if (cmdchar == c) {
      idx = nv_cmd_idx[i];
      break;
    }
    if (cmdchar > c)
      bot = i + 1;
    else
      top = i - 1;
  }
  return idx;
}