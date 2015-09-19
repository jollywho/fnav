#ifndef FN_CORE_TABLE_H
#define FN_CORE_TABLE_H

#include "fnav/lib/kbtree.h"
#include "fnav/lib/klist.h"
#include "fnav/rpc.h"

typedef enum {
  typVOID,
  typINT,
  typSTRING,
  typUINT64_T,
  //typTABLE /* record format for lists of tables */
} tFldType;

#define elem_cmp(a, b) (strcmp((a).key, (b).key))

typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct fn_rec fn_rec;
typedef struct tentry tentry;
typedef struct listener listener;

#define _NOP(x)
KLIST_INIT(kl_tentry, tentry*, _NOP)
KLIST_INIT(kl_val,    fn_val*, _NOP)
KLIST_INIT(kl_listen, listener*, _NOP)

struct fn_val {
  String key;
  void *dat;
  klist_t(kl_tentry) *recs;
  fn_fld *fld;
  int count;
};

struct fn_rec {
  klist_t(kl_val) *vals;
  tentry *ent;
  int fld_count;
};

struct tentry {
  tentry *prev;
  tentry *next;
  fn_rec *rec;
};

struct listener {
  fn_buf *buf;
  buf_cb cb;
  tentry *entry;
};

typedef struct {
  fn_rec *rec;
  void(*fn)();
} JobArg;

fn_tbl* tbl_mk();
fn_rec* mk_rec(fn_tbl *t);
void rec_edit(fn_rec *rec, String fname, void *val);
void tbl_mk_fld(fn_tbl *t, String name, tFldType type);
void tbl_del_val(fn_tbl *t, String fname, String val);
void tbl_listen(fn_tbl *t, fn_buf *buf, buf_cb cb);
klist_t(kl_tentry)* fnd_val(fn_tbl *t, String fname, String val);
void commit(Job *job, JobArg *arg);

int tbl_count(fn_tbl *t);
void* rec_fld(fn_rec *rec, String fname);

#define FN_KL_ITERBLK(typ, lst)      \
  kliter_t(typ) *(it);               \
  for ( (it) =  kl_begin( (lst) );   \
        (it) != kl_end  ( (lst) );   \
        (it) =  kl_next ( (it)  ))   \

tentry* head(fn_tbl *t);

#define FN_MV(t,e,d)                         \
  (e) = (e->d != head(t)->prev) ? (e->d) : (e);    \

#endif
