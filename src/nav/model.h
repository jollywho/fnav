#ifndef FN_MODEL_H
#define FN_MODEL_H

#include "nav/table.h"

typedef struct fn_line fn_line;

void model_init(fn_handle *hndl);
void model_cleanup(fn_handle *hndl);
void model_open(fn_handle *hndl);
void model_close(fn_handle *hndl);
bool model_blocking(fn_handle *hndl);

void model_sort(Model *m, const char *fld, int flags);
void model_recv(Model *m);
void refind_line(Model *m);

void model_read_entry(Model *m, fn_lis *lis, ventry *head);
void model_null_entry(Model *m, fn_lis *lis);

char* model_str_line(Model *m, int index);
void* model_fld_line(Model *m, const char *fld, int index);
void* model_curs_value(Model *m, const char *fld);
void model_set_curs(Model *m, int index);
int model_count(Model *m);

char* model_str_expansion(char* val);

#endif