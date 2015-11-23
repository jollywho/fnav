#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "fnav/ascii.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/tui/buffer.h"
#include "fnav/tui/fm_cntlr.h"
#include "fnav/log.h"

// TODO: Key Cmdarg redone as reuseable module.
// needed for language extension later.

typedef struct {
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
  int arg;
} Cmdarg;

typedef void (*key_func)(Cntlr *cntlr, Cmdarg *arg);

static void fm_left();
static void fm_right();

static const struct fm_cmd {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* FN_ flags */
  short cmd_arg;                /* value for ca.arg */
} fm_cmds[] =
{
  {'h',     fm_left,        0,             BACKWARD},
  {'l',     fm_right,       0,             FORWARD},
};

void cntlr_cancel(Cntlr *cntlr)
{
  log_msg("FM", "<|_CANCEL_|>");
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->op_count = 1;
  self->mo_count = 1;
  self->fs->cancel = true;
}

void fm_read_scan()
{
  log_msg("FM", "read");
}

void fm_after_scan()
{
  log_msg("FM", "async done");
}

void cntlr_focus(Cntlr *cntlr)
{
  log_msg("FM", "update dir");
  buf_refresh(cntlr->hndl->buf);
}

int fm_opendir(Cntlr *cntlr, String path, short arg)
{
  log_msg("FS", "fm_opendir %s", path);
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  fn_handle *h = cntlr->hndl;
  String cur_dir = self->cur_dir;

  if (!self->fs->running) {
    free(cur_dir);
    model_close(h);
    cur_dir = strdup(path);
    if (arg == BACKWARD)
      cur_dir = fs_parent_dir(cur_dir);
    h->key = cur_dir;
    model_open(h);
    buf_set_cntlr(h->buf, cntlr);
    fs_close(self->fs);
    fs_open(self->fs, cur_dir);
    self->cur_dir = cur_dir;
    return 1;
  }
  return 0;
}

static void fm_left(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd left");
  fn_handle *h = cntlr->hndl;
  String path = model_curs_value(h->model, "dir");
  fm_opendir(cntlr, path, arg->arg);
}

static void fm_right(Cntlr *cntlr, Cmdarg *arg)
{
  log_msg("FM", "cmd right");
  fn_handle *h = cntlr->hndl;

  String path = model_curs_value(h->model, "fullpath");
  if (isdir(path))
    fm_opendir(cntlr, path, arg->arg);
  else
    send_hook_msg("fileopen", cntlr, NULL);
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
void init_cmds(void)
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

int cntlr_input(Cntlr *cntlr, int key)
{
  Cmdarg ca;
  int idx = find_command(key);
  ca.arg = fm_cmds[idx].cmd_arg;
  if (idx >= 0) {
    fm_cmds[idx].cmd_func(cntlr, &ca);
  }
  // TODO: send to pipe_cntlrs if not consumed
  // if consumed return 1
  return 0;
}

static void init_fm_hndl(FM_cntlr *fm, Buffer *b, Cntlr *c, String val)
{
  fn_handle *hndl = malloc(sizeof(fn_handle));
  hndl->tn = "fm_files";
  hndl->buf = b;
  hndl->key_fld = "dir";
  hndl->key = val;
  hndl->fname = "name";
  c->hndl = hndl;
  c->_cancel = cntlr_cancel;
  c->_input = cntlr_input;
  c->top = fm;
}

Cntlr* fm_init(Buffer *buf)
{
  log_msg("FM_CNTLR", "init");
  init_cmds(); //TODO: cleanup loose parts
  FM_cntlr *fm = malloc(sizeof(FM_cntlr));
  fm->base.name = "fm";
  fm->base.fmt_name = "   FM    ";
  fm->op_count = 1;
  fm->mo_count = 1;
  char *init_dir = "/home/chi/casper/YFS";
  fm->cur_dir = malloc(strlen(init_dir)+1);
  strcpy(fm->cur_dir, init_dir);

  if (tbl_mk("fm_files")) {
    tbl_mk_fld("fm_files", "name", typSTRING);
    tbl_mk_fld("fm_files", "dir", typSTRING);
    tbl_mk_fld("fm_files", "fullpath", typSTRING);
  }

  if (tbl_mk("fm_stat")) {
    tbl_mk_fld("fm_stat", "fullpath", typSTRING);
    tbl_mk_fld("fm_stat", "update", typVOID);
    tbl_mk_fld("fm_stat", "stat", typVOID);
  }
  init_fm_hndl(fm, buf, &fm->base, fm->cur_dir);
  model_init(fm->base.hndl);
  model_open(fm->base.hndl);
  buf_set_cntlr(buf, &fm->base);
  hook_init(&fm->base);

  fm->fs = fs_init(fm->base.hndl);
  fs_open(fm->fs, fm->cur_dir);
  fm_test = &fm->base;
  return &fm->base;
}

void fm_cleanup(Cntlr *cntlr)
{
  log_msg("FM_CNTLR", "cleanup");
  FM_cntlr *fm = cntlr->top;
  fn_handle *h = cntlr->hndl;
  model_close(h);
  model_cleanup(h);
  //hook remove
  //hook cleanup
  fs_cleanup(fm->fs);
  free(h);
  free(fm->cur_dir);
  free(fm);
}
