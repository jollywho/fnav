#include <unistd.h>

#include "nav/plugins/dt/dt.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/event/event.h"
#include "nav/event/hook.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/cmdline.h"
#include "nav/event/fs.h"

static void dt_signal_model(void **data)
{
  DT *dt = data[0];
  Handle *h = dt->base->hndl;
  model_flush(h, true);
  model_recv(h->model);
  buf_move(h->buf, 0, 0);
}

static void dt_readfile(DT *dt)
{
  Handle *h = dt->base->hndl;
  dt->f = fopen(dt->path, "rw");

  if (!dt->f)
    return;

  char *line = NULL;
  size_t len = 0;
  ssize_t size;

  //TODO: split line on delim
  while ((size = getline(&line, &len, dt->f)) != -1) {
    trans_rec *r = mk_trans_rec(tbl_fld_count(dt->tbl));
    edit_trans(r, h->fname, (char*)line, NULL);
    CREATE_EVENT(eventq(), commit, 2, dt->tbl, r);
  }

  free(line);
  CREATE_EVENT(eventq(), dt_signal_model, 1, dt);
}

static bool validate_opts(DT *dt)
{
  //TODO: if no file and tbl doesnt exist, error
  return (dt->path && dt->tbl);
}

static bool dt_getopts(DT *dt, char *line)
{
  Handle *h = dt->base->hndl;
  List *flds = NULL;
  dt->tbl = "filename"; //TODO: use name scheme
  const char *fname = "name";
  dt->delm = " ";
  char *tmp;

  Cmdline cmd;
  cmdline_build(&cmd, line);

  List *lst = cmdline_lst(&cmd);
  for (int i = 0; i < utarray_len(lst->items); i++) {
    Token *tok = tok_arg(lst, i);
    switch (tok->var.v_type) {
      case VAR_LIST:
        flds = tok->var.vval.v_list;
        fname = list_arg(flds, 0, VAR_STRING);
        break;
      case VAR_PAIR:
        dt->delm = token_val(&tok->var.vval.v_pair->value, VAR_STRING);
        break;
      case VAR_STRING:
        tmp = valid_full_path(window_cur_dir(), token_val(tok, VAR_STRING));
        if (tmp)
          dt->path = tmp;
        else
          dt->tbl = token_val(tok, VAR_STRING);
    }
  }
  log_err("DT", "%s %s %s", dt->path, dt->tbl, dt->delm);

  bool succ = validate_opts(dt);
  if (!succ)
    goto cleanup;

  if (tbl_mk(dt->tbl)) {
    if (flds) {
      for (int i = 0; i < utarray_len(flds->items); i++)
        tbl_mk_fld(dt->tbl, list_arg(flds, i, VAR_STRING), TYP_STR);
    }
    else
      tbl_mk_fld(dt->tbl, fname, TYP_STR);
  }

  dt->tbl = strdup(dt->tbl);
  h->fname = tbl_fld_str(dt->tbl, fname);
  h->kname = h->fname;
  h->tn = dt->tbl;
  h->key_fld = h->fname;
  h->key = "";
cleanup:
  cmdline_cleanup(&cmd);
  return succ;
}

void dt_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("DT", "init");
  if (!arg)
    return;

  DT *dt = calloc(1, sizeof(DT));
  dt->base = plugin;
  plugin->top = dt;
  plugin->name = "dt";
  plugin->fmt_name = "DT";
  Handle *hndl = calloc(1, sizeof(Handle));
  hndl->buf = buf;
  plugin->hndl = hndl;

  if (!dt_getopts(dt, arg))
    return buf_set_plugin(buf, plugin, SCR_NULL);

  buf_set_plugin(buf, plugin, SCR_SIMPLE);
  buf_set_status(buf, 0, dt->tbl, 0);

  model_init(hndl);
  model_open(hndl);
  dt_readfile(dt);

  //TODO: fileopen user command
  //TODO: VFM navscript plugin
}

void dt_delete(Plugin *plugin)
{
  log_msg("DT", "delete");
  DT *dt = plugin->top;
  Handle *h = plugin->hndl;
  if (h->model) {
    model_close(h);
    model_cleanup(h);
  }
  free(dt->path);
  free(dt->tbl);
  free(h);
  free(dt);
}
