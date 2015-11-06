#ifndef FN_CMD_H
#define FN_CMD_H

#include "fnav/lib/uthash.h"
#include "fnav/cmdline.h"

#define XFILE     0x01
#define BUFFER    0x02
#define FIELD     0x04
#define FUNCTION  0x08
#define RANGE     0x10
#define REGEX     0x20
#define CNTLR     0x40

typedef struct Cmd_T Cmd_T;
typedef struct Cmd_Arg_T Cmd_Arg_T;
typedef struct Cmd_Param_T Cmd_Param_T;
typedef void (*Cmd_Func_T)(Token *args, int cmd_flags);

struct Cmd_T {
  char *name;
  Cmd_Func_T cmd_func;
  int cmd_flags;
  UT_hash_handle hh;
};

void cmd_add(Cmd_T *cmd);
void cmd_remove(String name);
void cmd_run(Cmdstr *cmdstr);
Cmd_T* cmd_find(String name);

#endif
