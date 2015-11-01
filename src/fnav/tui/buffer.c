#include <limits.h>

#include "fnav/tui/buffer.h"
#include "fnav/tui/window.h"
#include "fnav/ascii.h"
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"

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

static const struct buf_cmd {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fm_cmds[] =
{
  {Ctrl_J,  buf_mv_page,     0,           FORWARD},
  {Ctrl_U,  buf_mv_page,     0,           BACKWARD},
  {'j',     buf_mv,          0,           FORWARD},
  {'k',     buf_mv,          0,           BACKWARD},
  {'g',     buf_g,           0,           BACKWARD},
  {'G',     buf_g,           0,           FORWARD},
};

#define SET_POS(pos,y,x)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

#define DRAW_LINE(buf,ln,str)   \
  mvwprintw(buf->nc_win, ln, 0, str);

void buf_listen(fn_handle *hndl);
void buf_draw(void **argv);

Buffer* buf_init()
{
  log_msg("BUFFER", "init");
  Buffer *buf = malloc(sizeof(Buffer));
  SET_POS(buf->b_size, 0, 0);
  SET_POS(buf->b_ofs, 0, 0);
  SET_POS(buf->cur, 0, 0);
  buf->nc_win = newwin(1,1,0,0);
  scrollok(buf->nc_win, true);
  buf->dirty = false;
  buf->queued = false;
  buf->attached = false;
  buf->closed = false;
  buf->input_cb = NULL;
  init_cmds();
  return buf;
}

void buf_set_size(Buffer *buf, int r, int c)
{
  log_msg("BUFFER", "SET SIZE %d %d", r, c);
  SET_POS(buf->b_size, r, c);
  wresize(buf->nc_win, buf->b_size.lnum, buf->b_size.col);
}

void buf_set_ofs(Buffer *buf, int y, int x)
{
  log_msg("BUFFER", "SET OFS %d %d", y, x);
  SET_POS(buf->b_ofs, y, x);
  mvwin(buf->nc_win, y, x);
  buf_refresh(buf);
}

void buf_set_pass(Buffer *buf)
{
  buf->closed = true;
  delwin(buf->nc_win);
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
  buf->attached = true;
}

void buf_set(fn_handle *hndl)
{
  log_msg("BUFFER", "set");
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
  if (buf->closed)
    return;
  wclear(buf->nc_win);
  if (buf->attached) {
    Model *m = buf->hndl->model;
    for (int i = 0; i < buf->b_size.lnum; ++i) {
      String it = model_str_line(m, buf->top + i);
      if (!it) break;
      DRAW_LINE(buf, i, it);
    }
    wmove(buf->nc_win, buf->lnum, 0);
    wchgat(buf->nc_win, -1, A_REVERSE, 1, NULL);
  }
  wnoutrefresh(buf->nc_win);
  buf->dirty = false;
  buf->queued = false;
}

void buf_full_invalidate(Buffer *buf, int index, int lnum)
{
  // buffer reset and reentrance
  log_msg("BUFFER", "buf_full_invalidate");
  Model *m = buf->hndl->model;
  wclear(buf->nc_win);
  buf->top = index;
  buf->lnum = lnum;
  model_set_curs(m, buf->top + buf->lnum);
  buf_refresh(buf);
}

// input entry point.
// proper commands are chosen based on buffer state.
int buf_input(BufferNode *bn, int key)
{
  log_msg("BUFFER", "input");
  Buffer *buf = bn->buf;
  if (buf->input_cb) {
    buf->input_cb(bn, key);
    return 1;
  }
  if (!buf->attached)
    return 0;
  if (buf->cntlr) {
    buf->cntlr->_input(buf->cntlr, key);
  }
  // TODO: check consumed
  Cmdarg ca;
  int idx = find_command(key);
  ca.arg = fm_cmds[idx].cmd_arg;
  if (idx >= 0) {
    fm_cmds[idx].cmd_func(buf, &ca);
  }
  return 0;
}

void buf_scroll(Buffer *buf, int y, int m_max)
{
  log_msg("BUFFER", "scroll %d %d", buf->cur.lnum, y);
  int prev = buf->top;
  buf->top += y;
  if (buf->top < 0) {
    buf->top = 0;
  }
  else if (buf->top + buf->b_size.lnum > m_max) {
    buf->top = m_max - buf->b_size.lnum;
  }
  int diff = prev - buf->top;
  buf->lnum += diff;
}

static void buf_mv(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_mv %d", arg->arg);
  // move cursor N times in a dimension.
  // scroll if located on an edge.
  Model *m = buf->hndl->model;
  int y = arg->arg;
  int m_max = model_count(buf->hndl->model);
  buf->lnum += y;

  if (y < 0 && buf->lnum < buf->b_size.lnum * 0.2) {
    buf_scroll(buf, arg->arg, m_max);
  }
  else if (y > 0 && buf->lnum > buf->b_size.lnum * 0.8) {
    buf_scroll(buf, arg->arg, m_max);
  }

  if (buf->lnum < 0) {
    buf->lnum = 0;
  }
  else if (buf->lnum > buf->b_size.lnum) {
    buf->lnum = buf->b_size.lnum;
  }
  if (buf->lnum + buf->top > m_max - 1) {
    int count = m_max - buf->top;
    buf->lnum = count - 1;
  }
  model_set_curs(m, buf->top + buf->lnum);
  buf_refresh(buf);
}

int buf_line(Buffer *buf)
{return buf->lnum;}
int buf_top(Buffer *buf)
{return buf->top;}
pos_T buf_size(Buffer *buf)
{return buf->b_size;}
pos_T buf_ofs(Buffer *buf)
{return buf->b_ofs;}

static void buf_mv_page(Buffer *buf, Cmdarg *arg)
{
  log_msg("BUFFER", "buf_mv_page");
}

static void buf_g(Buffer *buf, Cmdarg *arg)
{
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
