#include <errno.h>
#include <malloc.h>
#include "fnav/log.h"
#include "fnav/table.h"
#include "fnav/model.h"
#include "fnav/event/hook.h"
#include "fnav/tui/op_cntlr.h"

void exit_cb(uv_process_t *req, int64_t exit_status, int term_signal) {
  log_msg("OP", "exit_cb");
  uv_close((uv_handle_t*) req, NULL);
  Op_cntlr *op = (Op_cntlr*)req->data;
  op->ready = true;
}

static void create_proc(Op_cntlr *op, String path)
{
  log_msg("OP", "create_proc");
  op->opts.file = "mpv";
  op->opts.flags = UV_PROCESS_DETACHED;
  op->opts.exit_cb = exit_cb;
  char *rv[] = {"mpv", "--fs", path, NULL};
  op->opts.args = rv;
  op->proc.data = op;

  //trans_rec *r = mk_trans_rec(tbl_fld_count("op_procs"));
  //edit_trans(r, "ext",     (void*)"mkv",  NULL);
  //edit_trans(r, "file",    (void*)"mpv",  NULL);
  //edit_trans(r, "single",  NULL,          (void*)"0");
  //edit_trans(r, "ensure",  NULL,          (void*)"0");
  //edit_trans(r, "args",    "--fs",        NULL);
  //edit_trans(r, "uv_proc", NULL,          (void*)proc);
  //edit_trans(r, "uv_opts", NULL,          (void*)opts);
  //CREATE_EVENT(&op->loop.events, commit, 2, "op_procs", r);
  if (!op->ready) {
    log_msg("OP", "kill");
    uv_process_kill(&op->proc, SIGINT);
    uv_close((uv_handle_t*)&op->proc, NULL);
    uv_run(&op->loop.uv, UV_RUN_NOWAIT);
  }
  log_msg("OP", "spawn");
  int ret = uv_spawn(&op->loop.uv, &op->proc, &op->opts);
  op->ready = false;
  log_msg("?", "%s", uv_strerror(ret));
  uv_unref((uv_handle_t*) &op->proc);
}

const char *file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

static void fileopen_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("OP", "fileopen_cb");
  Op_cntlr *op = (Op_cntlr*)caller->top;

  String path = model_curs_value(host->hndl->model, "fullpath");
  //const char* ext = file_ext(path);
  //ventry *head = fnd_val("op_procs", "ext", (char*)ext);

  create_proc(op, path);

  //if (head) {
  //  String file = rec_fld(head->rec, "file");
  //  String args = rec_fld(head->rec, "args");
  //  uv_process_t *proc = rec_fld(head->rec, "uv_proc");
  //  uv_process_options_t *opts = rec_fld(head->rec, "uv_opts");
  //}
}

void op_loop(Loop *loop, int ms)
{
  process_loop(loop, ms);
}

static void pipe_attach_cb(Cntlr *host, Cntlr *caller)
{
  log_msg("OP", "pipe_attach_cb");
  hook_add(caller, host, fileopen_cb, "fileopen");
}

Cntlr* op_init()
{
  log_msg("OP", "INIT");
  Op_cntlr *op = malloc(sizeof(Op_cntlr));
  op->base.top = op;
  op->ready = true;
  loop_add(&op->loop, op_loop);
  uv_timer_init(&op->loop.uv, &op->loop.delay);
  if (tbl_mk("op_procs")) {
    tbl_mk_fld("op_procs", "ext", typSTRING);
    tbl_mk_fld("op_procs", "file", typSTRING);
    tbl_mk_fld("op_procs", "single", typSTRING);
    tbl_mk_fld("op_procs", "ensure", typSTRING);
    tbl_mk_fld("op_procs", "args", typSTRING);
    tbl_mk_fld("op_procs", "uv_proc", typVOID);
    tbl_mk_fld("op_procs", "uv_opts", typVOID);
  }

  hook_init(&op->base);
  hook_add(&op->base, &op->base, pipe_attach_cb, "pipe_attach");
  return &op->base;
}

void op_cleanup(Op_cntlr *cntlr);
