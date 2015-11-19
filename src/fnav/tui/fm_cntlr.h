#ifndef FN_TUI_FM_CNTLR_H
#define FN_TUI_FM_CNTLR_H

#include "fnav/tui/cntlr.h"
#include "fnav/event/fs.h"

typedef struct FM_cntlr FM_cntlr;

struct FM_cntlr {
  Cntlr base;
  int op_count;
  int mo_count;
  String cur_dir;
  FS_handle *fs;
};

Cntlr *fm_test;

Cntlr* fm_init(Buffer *buf);
void fm_cleanup(Cntlr *cntlr);

#endif
