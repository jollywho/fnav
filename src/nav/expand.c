#include <unistd.h>
#include "nav/tui/window.h"
#include "nav/plugins/op/op.h"
#include "nav/expand.h"
#include "nav/log.h"
#include "nav/util.h"
#include "nav/option.h"
#include "nav/table.h"
#include "nav/model.h"
#include "nav/event/fs.h"
#include "nav/tui/select.h"

enum fm_fmt {
  FM_NORM,
  FM_EXT,
  FM_NAME,
  FM_GROUP,
  FM_KIND,
};

static varg_T bu_type(char ch)
{
  varg_T arg = {};
  if (ch == 'b') {
    arg.argc = 1;
    arg.argv = malloc(sizeof(char*));
    asprintf(&arg.argv[0], "%d", buf_id(window_get_focus()));
  }
  else if (ch == 'B') {
    Plugin *plug = window_get_plugin();
    arg.argc = 1;
    arg.argv = malloc(sizeof(char*));
    arg.argv[0] = strdup(plug->name);
  }
  return arg;
}

static char* fm_ext(void *arg)
{
  return strdup(file_ext(arg));
}

static char* fm_name(void *arg)
{
  return (char*)file_base(strdup(arg));
}

static char* fm_group(void *arg)
{
  nv_syn *syn = get_syn(file_ext(arg));
  if (syn)
    return strdup(syn->group->key);
  else
    return strdup("");
}

static char* fm_kind(void *arg)
{
  struct stat *sb = arg;
  return strdup(stat_kind(sb));
}

static varg_T fm_type(const char *name, enum fm_fmt fmt)
{
  Buffer *buf = window_get_focus();
  select_cb cb = NULL;

  switch (fmt) {
    case FM_EXT:   cb = fm_ext;   break;
    case FM_NAME:  cb = fm_name;  break;
    case FM_GROUP: cb = fm_group; break;
    case FM_KIND:  cb = fm_kind;  break;
    default:
      break;
  }
  return buf_select(buf, name, cb);
}

static varg_T tbl_type(const char *name)
{
  Buffer *buf = window_get_focus();
  return buf_select(buf, name, NULL);
}

static varg_T op_type(const char *name)
{
  varg_T arg = {};
  nv_group *grp = op_active_group();
  if (!name || !grp)
    return arg;

  int isfld = !strcmp(name, "prev") || !strcmp(name, "next");
  Ventry *vent = fnd_val("op_procs", "group", grp->key);

  if (isfld && vent) {
    Ventry *head = ent_head(vent);
    char *ret = rec_fld(head->rec, "pid");
    arg.argc = 1;
    arg.argv = malloc(sizeof(char*));
    arg.argv[0] = strdup(ret);
  }

  return arg;
}

static char* human_time(time_t const *t)
{
  static char str[80];
  struct tm *tm = localtime(t);
  if (!tm)
    return strdup("*** invalid date/time ***");

  strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", tm);
  return strdup(str);
}

//TODO: modify buf_select for extra data field
static varg_T stat_type(const char *name)
{
  varg_T arg = {};
  Buffer *buf = window_get_focus();
  if (!buf_attached(buf))
    return arg;

  struct stat *sb = model_curs_value(buf->hndl->model, "stat");
  if (!sb)
    return arg;

  arg.argc = 1;
  arg.argv = malloc(sizeof(char*));

  if (!strcmp("mtime", name))
    arg.argv[0] = human_time(&sb->st_mtim.tv_sec);
  else if (!strcmp("ctime", name))
    arg.argv[0] = human_time(&sb->st_ctim.tv_sec);
  else if (!strcmp("atime", name))
    arg.argv[0] = human_time(&sb->st_atim.tv_sec);
  else if (!strcmp("mode", name))
    asprintf(&arg.argv[0], "%o", sb->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
  else if (!strcmp("inode", name))
    asprintf(&arg.argv[0], "%ld", sb->st_ino);
  else if (!strcmp("device", name))
    asprintf(&arg.argv[0], "%ld", sb->st_dev);
  else if (!strcmp("nlink", name))
    asprintf(&arg.argv[0], "%ld", sb->st_nlink);
  else if (!strcmp("size", name))
    asprintf(&arg.argv[0], "%ld", sb->st_size);
  else
    arg.argv[0] = strdup("");

  return arg;
}

static varg_T proc_type(const char *name)
{
  varg_T arg = {};
  if (name[1])
    return arg;

  arg.argc = 1;
  arg.argv = malloc(sizeof(char*));
  switch (*name) {
    case '%':
      asprintf(&arg.argv[0], "%d", getpid());
      break;
    case '!':
      arg.argv[0] = op_pid_last();
      break;
    case '?':
      arg.argv[0] = op_status_last();
      break;
  }
  return arg;
}

static varg_T get_type(const char *key, const char *alt)
{
  switch (*key) {
    case 'b':
    case 'B':
      return bu_type(*key);
    case 'f':
      return fm_type("fullpath", FM_NORM);
    case 'n':
      return fm_type("name",     FM_NORM);
    case 'N':
      return fm_type("name",     FM_NAME);
    case 'e':
      return fm_type("name",     FM_EXT);
    case 'd':
      return fm_type("dir",      FM_NORM);
    case 't':
      return fm_type("name",     FM_GROUP);
    case 'k':
      return fm_type("stat",     FM_KIND);
    case '_':
      return tbl_type(alt);
    case 'o':
      return op_type(alt);
    case 's':
      return stat_type(alt);
    case '!':
    case '?':
    case '%':
      return proc_type(key);
    default:
      return (varg_T){};
  }
}

char* yank_symbol(const char *key, const char *alt)
{
  varg_T ret = get_type(key, alt);
  if (!ret.argc)
    return strdup("''");
  char *str = lines2yank(ret.argc, ret.argv);
  del_param_list(ret.argv, ret.argc);
  return str;
}

char* expand_symbol(const char *key, const char *alt)
{
  log_msg("EXPAND", "expand symb {%s:%s}", key, alt);

  varg_T ret = get_type(key, alt);
  if (!ret.argc)
    return strdup("''");
  char *str = lines2argv(ret.argc, ret.argv);
  del_param_list(ret.argv, ret.argc);
  return str;
}
