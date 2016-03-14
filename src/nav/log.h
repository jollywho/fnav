#ifndef FN_LOG_H
#define FN_LOG_H

void log_init();
void log_set(const char *obj);
void log_msg(const char *obj, const char *fmt, ...) \
  __attribute__((format (printf, 2, 3)));
void log_err(const char *obj, const char *fmt, ...) \
  __attribute__((format (printf, 2, 3)));

#endif