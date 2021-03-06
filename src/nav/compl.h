#ifndef NV_COMPL_H
#define NV_COMPL_H

#include "nav/cmd.h"
#include "nav/lib/uthash.h"

typedef void (*compl_genfn)(void);
typedef struct {
  char flag;
  char *label;
  compl_genfn gen;
} compl_param;

typedef struct {
  char *key;
  int argc;
  compl_param **params;
  UT_hash_handle hh;
} compl_context;

typedef struct {
  int argc;           //arg position
  char *key;
  int colcount;       //count of columns; (supports 0 or 1)
  char *columns;      //column string
} compl_item;

typedef struct {
  int matchcount;
  UT_array *rows;     //compl_item
  UT_array *matches;  //compl_item
  int invalid_pos;
} compl_list;

void compl_init();
void compl_cleanup();

void compl_begin(int);
void compl_end();
bool compl_dead();

compl_list* compl_complist();

void compl_backward();
void compl_update(const char *, int, char);
void compl_build();
void compl_filter(const char *);

void compl_list_add(const char *fmt, ...);
void compl_set_col(int idx, char *fmt, ...);
void compl_set_escapes(char ch[3]);
void compl_set_exec(int);

compl_item* compl_idx_match(int idx);
void compl_walk_params(int (*param_cb)(char *,char,int,bool));

void compl_invalidate(int pos);
bool compl_validate(int pos);
bool compl_isexec();
bool compl_isroot();
int compl_next_pos();
int compl_last_pos();
int compl_cur_pos();
int compl_arg_pos();
int compl_root_pos();

#endif
