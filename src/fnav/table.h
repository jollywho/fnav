#ifndef FN_TABLE_H
#define FN_TABLE_H

#include "fnav/lib/uthash.h"
#include "fnav/rpc.h"

#define elem_cmp(a, b) (strcmp((a).key, (b).key))

typedef struct fn_rec fn_rec;
typedef struct fn_val fn_val;
typedef struct fn_fld fn_fld;
typedef struct fn_lis fn_lis;
typedef struct tentry tentry;
typedef struct ventry ventry;

typedef enum {
  typVOID,
  typINT,
  typSTRING,
  typUINT64_T,
  //typTABLE /* record format for lists of tables */
} tFldType;

struct ventry {
  ventry *prev;
  ventry *next;
  fn_rec *rec;
  fn_val *val;
  unsigned int head;
};

void tables_init();
bool tbl_mk(String name);
void tbl_del(String name);
void tbl_mk_fld(String tn, String name, tFldType typ);

void tbl_add_lis(String tn, String key_fld, String key);
void commit(void **data);

ventry* fnd_val(String tn, String fname, String val);
fn_lis* fnd_lis(String tn, String key_fld, String key);
void lis_data(fn_lis *lis, ventry *ent, int *index, int *lnum);
ventry* lis_set_val(fn_lis *lis, String fname);
void* rec_fld(fn_rec *rec, String fname);

void tbl_del_val(String tn, String fname, String val);

int tbl_fld_count(String tn);
int tbl_ent_count(ventry *e);
int FN_MV(ventry *e, int n);

#endif
