#ifndef FN_EVENT_FTW_H
#define FN_EVENT_FTW_H

#include "nav/event/file.h"

void ftw_init();
void ftw_cleanup();
void ftw_add(varg_T, char *, Buffer *, int);
void ftw_cancel();

#endif