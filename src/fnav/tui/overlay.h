#ifndef FN_TUI_OVERLAY_H
#define FN_TUI_OVERLAY_H

#include <ncurses.h>
#include "fnav/tui/cntlr.h"

typedef struct Overlay Overlay;
struct Overlay {
  WINDOW *nc_win_sep;
  WINDOW *nc_win_st;
  pos_T ov_size;
  pos_T ov_ofs;
  int separator;
  int lines;
  String cntlr_name;
  String cmd_args;
};

Overlay* overlay_new();
void overlay_set(Overlay *ov, Buffer *buf);
void overlay_edit(Overlay *ov, String name, String label);
void overlay_draw(void **argv);
void overlay_clear(Overlay *ov);

#endif