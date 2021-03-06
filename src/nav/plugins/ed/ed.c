#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#include "nav/plugins/ed/ed.h"
#include "nav/plugins/term/term.h"
#include "nav/event/event.h"
#include "nav/tui/message.h"
#include "nav/tui/buffer.h"
#include "nav/tui/window.h"
#include "nav/event/hook.h"
#include "nav/event/file.h"
#include "nav/event/fs.h"
#include "nav/cmdline.h"
#include "nav/log.h"
#include "nav/table.h"
#include "nav/util.h"
#include "nav/option.h"

#define ED_PASSVE 1
#define ED_RENAME 2
#define ED_CONFRM 3
#define ED_CLOSED 4

static const char* ED_MSG =
  "# This file will be executed when you close the editor.\n"
  "# Please double-check everything, clear the file to abort.";

typedef struct {
  varg_T src;
  char *dst;
} ed_data;

static void ed_chown_plugin(Ed *ed)
{
  ed->base->top = ed;
  ed->base->name = "ed";
  buf_set_plugin(ed->buf, ed->base, SCR_NULL);
}

static void ed_cleanup(Ed *ed)
{
  log_msg("ED", "ed_cleanup");
  log_msg("ED", "%d", ed->state);

  if (BITMASK_CHECK(ED_RENAME, ed->state)) {
    del_param_list(ed->src.argv,  ed->src.argc);
    unlink(ed->tmp_name);
    close(ed->fd);
  }
  if (BITMASK_CHECK(ED_CONFRM, ed->state))
    del_param_list(ed->dest.argv, ed->dest.argc);

  if (!BITMASK_CHECK(ED_CLOSED, ed->state)) {
    ed_chown_plugin(ed);
    window_close_focus();
  }
  else
    free(ed);
}

static void ed_dump_contents(Ed *ed, varg_T *args)
{
  log_msg("ED", "ed_dump_contents");

  lseek(ed->fd, 0, SEEK_SET);

  if (ed->state == ED_CONFRM) {
    write(ed->fd, ED_MSG, strlen(ED_MSG));
    write(ed->fd, "\n", 1);
  }

  for (int i = 0; i < args->argc; i++) {
    log_err("ED", "write: [%s]", args->argv[i]);
    if (!args->argv[i])
      continue;
    write(ed->fd, args->argv[i], strlen(args->argv[i]));
    write(ed->fd, "\n", 1);
  }
}

static void resize_src(varg_T *a, varg_T *b)
{
  log_msg("ED", "resize_src");
  for (int i = a->argc - 1; i > b->argc - 1; i--)
    free(a->argv[i]);
  a->argc = b->argc;
  a->argv = realloc(a->argv, a->argc*sizeof(char*));
}

static bool validate_lines(Ed *ed)
{
  int src_count = ed->src.argc;
  int dst_count = ed->dest.argc;
  if (dst_count > src_count || dst_count < 1) {
    goto err;
  }

  int count = 0;
  for (int i = 0; i < ed->dest.argc; i++) {
    char **src  = &ed->src.argv[i];
    char **dest = &ed->dest.argv[i];
    if (!strcmp(*src, *dest)) {
      count++;
    }
  }

  if (count == ed->dest.argc) {
    nv_msg("no renaming to be done");
    goto err;
  }

  if (ed->state == ED_RENAME && dst_count < src_count)
    resize_src(&ed->src, &ed->dest);

  return true;
err:
  del_param_list(ed->dest.argv, ed->dest.argc);
  return false;
}

static bool ed_read_temp(Ed *ed)
{
  log_msg("ED", "ed_read_temp");

  lseek(ed->fd, 0, SEEK_SET);

  char buf[1024];
  ssize_t len, off, max;
  char *dest = malloc(sizeof(buf));
  off = 0;
  max = 1024;

  while ((len = read(ed->fd, buf, sizeof(buf)))) {
    if (off + len > max)
      dest = realloc(dest, max *= 2);
    strncpy(&dest[off], buf, len);
    off += len;
  }
  dest[off] = '\0';

  if (ed->state == ED_CONFRM)
    del_param_list(ed->dest.argv, ed->dest.argc);

  ed->dest.argc = count_lines(dest);
  ed->dest.argv = malloc((1+ed->dest.argc)*sizeof(char*));
  log_err("ED", "%d count", ed->dest.argc);

  char *next = NULL;
  char *prev = dest;
  int i = 0;
  while ((next = strstr(prev, "\n"))) {
    log_err("ED", "newline");
    *next = '\0';
    log_err("ED", "[%s]", prev);
    if (*prev != '#') {
      ed->dest.argv[i] = strdup(prev);
      i++;
    }
    prev = next+1;
    if (!prev)
      break;
  }
  free(dest);

  if (!validate_lines(ed))
    return false;

  return true;
}

static void ed_start_term(Ed *ed, varg_T *arg)
{
  log_msg("ED", "ed_start_term");
  ed->state++;
  if (ed->state > ED_CONFRM) {
    //error
    return ed_cleanup(ed);
  }

  ed_dump_contents(ed, arg);

  char *editor = getenv("EDITOR");
  if (!editor)
    editor = "vi";

  char *line;
  asprintf(&line, "%s %s", editor, ed->tmp_name);
  term_new(ed->base, ed->buf, line);
  free(line);

  term_set_editor(ed->base, ed);
  buf_set_status(ed->buf, "ED", NULL, NULL);
}

static void ed_stage_confirm(Ed *ed)
{
  log_msg("ED", "ed_stage_confirm");
  int max = 0;
  for (int i = 0; i < ed->src.argc; i++)
    max = MAX(max, strlen(ed->src.argv[i]));

  char buf[PATH_MAX];
  for (int i = 0; i < ed->dest.argc; i++) {
    char **src  = &ed->src.argv[i];
    char **dest = &ed->dest.argv[i];
    if (!strcmp(*src, *dest)) {
      free(*dest);
      *dest = NULL;
      continue;
    }
    sprintf(buf, "%-*s  %s", max, *src, *dest);
    SWAP_ALLOC_PTR(*dest, strdup(buf));
  }

  ed_start_term(ed, &ed->dest);
}

static char* crop_dest(char *src, char *dst)
{
  int i;
  /* NOTE: assume src portion untouched.
   * will fail later anyway. */
  for (i = 0; dst[i]; i++) {
    if (src[i] != dst[i])
      break;
  }
  /* src error or no space delimit */
  if (dst[i] == '\0' || dst[i] != ' ')
    return NULL;

  /* strip spaces */
  while (dst[i] == ' ')
    i++;

  return &dst[i];
}

static void ed_do_rename(Ed *ed)
{
  log_msg("ED", "ed_do_rename");
  char from[PATH_MAX];
  char to[PATH_MAX];
  char curdir[PATH_MAX];

  sprintf(curdir, "%s", fs_pwd());

  for (int i = 0; i < ed->dest.argc; i++) {
    char *src = ed->src.argv[i];
    char *dst = ed->dest.argv[i];
    log_err("ED", "[%s] [%s]", src, dst);

    char *str = crop_dest(src, dst);
    if (!str) {
      //error
      continue;
    }
    conspath_buf(from, curdir, src);
    conspath_buf(to,   curdir, str);
    file_move_str(from, to, NULL);
  }
  ed->state &= ~ED_CLOSED;
  ed_cleanup(ed);
}

void ed_close_cb(Plugin *plugin, Ed *ed, bool closed)
{
  log_msg("ED", "ed_close_cb");

  if (closed) {
    ed->state |= ED_CLOSED;
    ed_cleanup(ed);
    return;
  }

  term_delete(ed->base);
  window_refresh();

  if (!ed_read_temp(ed))
    return ed_cleanup(ed);

  if (ed->state == ED_RENAME)
    return ed_stage_confirm(ed);
  if (ed->state == ED_CONFRM)
    return ed_do_rename(ed);
}

static bool ed_pull_source(varg_T *arg, Plugin *caller)
{
  if (!caller || !caller->hndl)
    return false;

  Buffer *buf = caller->hndl->buf;
  *arg = buf_select(buf, "name", NULL);
  buf_end_sel(buf);
  return arg->argc;
}

static void ed_prepare(Ed *ed)
{
  sprintf(ed->tmp_name, "/tmp/navedit-XXXXXX");
  ed->fd = mkstemp(ed->tmp_name);
  log_err("ED", "%d:::::::::::%s", ed->fd, ed->tmp_name);
}

static void pipe_cb(Plugin *host, Plugin *caller, HookArg *hka)
{
  log_msg("ED", "pipe_cb");
  Ed *ed = host->top;

  if (!ed_pull_source(&ed->src, caller))
    return;

  ed_prepare(ed);
  ed_start_term(ed, &ed->src);
}

static void ed_direct_prompt(void **args)
{
  ed_data *data = args[0];

  if (get_opt_int("askrename")) {
    bool ans = 0;

    if (data->src.argc > 1)
      ans = confirm("Rename %d items to %s ?", data->src.argc, data->dst);
    else
      ans = confirm("Rename %s to %s ?", data->src.argv[0], data->dst);

    if (!ans)
      goto cleanup;
  }

  char from[PATH_MAX];
  char to[PATH_MAX];
  char *curdir = fs_pwd();

  //NOTE: multiargs rename all to same dest
  for (int i = 0; i < data->src.argc; i++) {
    log_err("ED", "[%s] [%s]", data->src.argv[i], data->dst);
    conspath_buf(from, curdir, data->src.argv[i]);
    conspath_buf(to,   curdir, data->dst);
    file_move_str(from, to, NULL);
  }
cleanup:
  del_param_list(data->src.argv,  data->src.argc);
  free(data->dst);
  free(data);
}

void ed_direct_rename(Plugin *caller, char *source, char *dest)
{
  log_msg("ED", "ed_direct_rename");
  log_msg("ED", "%s,  %s", source, dest);

  /* use source as dest. get source from buffer */
  if (!dest) {
    dest = source;
    source = NULL;
  }

  varg_T src;
  if (source) {
    src.argc = 1;
    src.argv = malloc(sizeof(char*));
    src.argv[0] = strdup(source);
  }
  else if (!ed_pull_source(&src, caller))
    return;

  log_msg("ED", "%s,  %s", source, dest);
  ed_data *data = malloc(sizeof(ed_data));
  data->src = src;
  data->dst = strdup(dest);

  CREATE_EVENT(eventq(), ed_direct_prompt, 1, data);
}

//new ed  : await pipe data to start
//edit    : new ed dest
void ed_new(Plugin *plugin, Buffer *buf, char *arg)
{
  log_msg("ED", "init");

  Ed *ed = malloc(sizeof(Ed));
  ed->base = plugin;
  ed->buf = buf;
  ed->state = ED_PASSVE;
  ed->fd = -1;
  plugin->top = ed;
  buf_set_plugin(ed->buf, ed->base, SCR_NULL);
  buf_set_status(ed->buf, "ED", NULL, NULL);
  hook_add_intl(buf->id, plugin, NULL, pipe_cb, EVENT_PIPE);
}

void ed_delete(Plugin *plugin)
{
  log_msg("ED", "delete");
  Ed *ed = plugin->top;
  free(ed);
}
