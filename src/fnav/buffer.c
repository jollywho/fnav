#define _GNU_SOURCE
#include <ncurses.h>
#include <limits.h>

#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/buffer.h"
#include "fnav/loop.h"

typedef struct {
  tentry *ent;
  pos_T pos;
} b_focus;

struct fn_buf {
  WINDOW *nc_win;
  int b_offset;
  pos_T b_size;
  pos_T nc_size;
  tentry *b_entf;
  bool inval;
  Job *job;
  JobArg *arg;
  b_focus focus;
  fn_tbl *t;
};

#define SET_POS(pos,x,y)       \
  (pos.col) = (x);             \
  (pos.lnum) = (y);            \

#define INC_POS(pos,x,y)       \
  (pos.col) += (x);            \
  (pos.lnum) += (y);           \

#define DRAW_AT(win,pos,str)   \
  mvwprintw(win, pos.lnum, pos.col, str);

fn_buf* buf_init(fn_tbl *t)
{
  printf("buf init\n");
  fn_buf *buf = malloc(sizeof(fn_buf));
  buf->inval = false;
  SET_POS(buf->focus.pos,0,0);
  SET_POS(buf->b_size,0,0);
  getmaxyx(stdscr, buf->nc_size.lnum, buf->nc_size.col);
  buf->nc_win = newwin(buf->nc_size.lnum, buf->nc_size.col, 0,0);
  buf->t = t;
  buf->job = malloc(sizeof(Job));
  buf->arg = malloc(sizeof(JobArg));
  buf->job->caller = buf;
  buf->arg->fn = buf_draw;
  return buf;
}

void buf_listen(listener *listener)
{
  log_msg("BUFFER", "listen cb");
  fn_buf *buf = listener->buf;
  buf->focus.ent = listener->entry;
  if (buf->inval == false) {
    buf->inval = true;
    QUEUE_PUT(draw, buf->job, buf->arg);
  }
}

void buf_set(fn_buf *buf, fn_handle *th)
{
  log_msg("BUFFER", "set");
  tbl_listen(th->tbl, buf, buf_listen);
}

void buf_draw(Job *job, JobArg *arg)
{
  log_msg("BUFFER", "$_draw_$");
  fn_buf *buf = job->caller;
#ifndef NCURSES_ENABLED
  String n = (String)rec_fld(buf->focus.ent->rec, "name");
  log_msg("BUFFER", " %s", n);
  buf->inval = false;
  return;
#endif

  // from curs_pos loop up while curs_pos - offset > 0
  // from curs_pos loop down while curs_pos - offset < b_size
  wclear(buf->nc_win);

  pos_T p = {.lnum = 0, .col = 0};
  tentry *it = buf->focus.ent;
  for(int i = 0; i < buf->nc_size.lnum; i++) {
    it = it->next;
    if (!it) break;
    if (!it->rec) break;
    String n = (String)rec_fld(it->rec, "name");
    DRAW_AT(buf->nc_win, p, n);
    INC_POS(p,0,1);
  }
  buf->inval = false;
  wrefresh(buf->nc_win);
  refresh();
}

void buf_mv(fn_buf *buf, int x, int y)
{
  if (buf->inval == true) return;

  buf->inval = true;
  for (int i = 0; i < y; i++) {
    FN_MV(buf->t, buf->focus.ent, next);
  }
  for (int i = 0; i > y; i--) {
    FN_MV(buf->t, buf->focus.ent, prev);
  }
  QUEUE_PUT(draw, buf->job, buf->arg);
}