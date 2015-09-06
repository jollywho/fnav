#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdbool.h>

#include "fnav/fm_cntlr.h"
#include "fnav/log.h"

#define FORWARD  1
#define BACKWARD (-1)

#define NV_NCH      0x01          /* may need to get a second char */
#define NV_NCH_NOP  (0x02|NV_NCH) /* get second char when no operator pending */
#define NV_NCH_ALW  (0x04|NV_NCH) /* always get a second char */

enum Type { OPERATOR, MOTION };

typedef struct {
  String name;
  enum Type type;
  void (*f)();
} Key;

typedef struct {
  long lnum;        /* line number */
  int col;          /* column number */
} pos_T;

typedef struct {
  pos_T start;                  /* start of the operator */
  pos_T end;                    /* end of the operator */
  long opcount;                 /* count before an operator */
} Cmdarg;

typedef void (*key_func)(Cntlr *cntlr, Cmdarg *arg);

Cmd default_lst[] = {
  { "open",  0,  NULL },
  { "cd",    0,  NULL },
  { "list",  0,  NULL },
};

static void fm_down();
static void fm_up();

static const struct fm_cmd {
  int cmd_char;                 /* (first) command character */
  key_func cmd_func;            /* function for this command */
  int cmd_flags;                /* NV_ flags */
  short cmd_arg;                /* value for ca.arg */
} fm_cmds[] =
{
  {'j',     fm_down,        0,                        0},
  {'k',     fm_up,          0,                 BACKWARD},
};

void cancel(Cntlr *cntlr)
{
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  self->op_count = 1;
  self->mo_count = 1;
}

void fm_read_scan(String name)
{
  log_msg("FM", "%s", name);
}

void fm_after_scan()
{
  log_msg("FM", "async done");
}

static void fm_up(Cntlr *cntlr)
{
  FM_cntlr *self = (FM_cntlr*)cntlr->top;
  log_msg("FM", "****************cmd up scan test***************");
  fs_open(self->fs, "/home/chi/casper/YFS/ALL/Chihayafuru");
  log_msg("FM", "waiting on job...");
}

static void fm_down()
{
  log_msg("FM", "cmd down");
}

/* Number of commands in nv_cmds[]. */
#define NV_CMDS_SIZE ARRAY_SIZE(fm_cmds)

/* Sorted index of commands in nv_cmds[]. */
static short nv_cmd_idx[NV_CMDS_SIZE];

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
  for (short int i = 0; i < (short int)NV_CMDS_SIZE; ++i) {
    nv_cmd_idx[i] = i;
  }

  /* Sort the commands by the command character.  */
  qsort(&nv_cmd_idx, NV_CMDS_SIZE, sizeof(short), nv_compare);

  /* Find the first entry that can't be indexed by the command character. */
  short int i;
  for (i = 0; i < (short int)NV_CMDS_SIZE; ++i) {
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
  top = NV_CMDS_SIZE - 1;
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

int input(Cntlr *cntlr, String key)
{
  int idx = find_command(key[0]);
  if (idx >= 0) {
    fm_cmds[idx].cmd_func(cntlr, NULL);
  }
  return 0;
}

FM_cntlr* fm_cntlr_init()
{
  init_cmds(); //TODO: cleanup loose parts
  //TODO: init fs_handle

  FM_cntlr *cntlr = malloc(sizeof(FM_cntlr));
  cntlr->base.top = cntlr;
  cntlr->base.cmd_lst = default_lst;
  cntlr->base._cancel = cancel;
  cntlr->base._input = input;
  cntlr->op_count = 1;
  cntlr->mo_count = 1;
  cntlr->fs = fs_init(&cntlr->base, "", fm_read_scan, fm_after_scan);

  return cntlr;
}
