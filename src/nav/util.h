#ifndef FN_UTIL_H
#define FN_UTIL_H

#include <ncurses.h>

bool exparg_isset();
wchar_t* str2wide(char *src);
char* wide2str(wchar_t *src);
int cell_len(char *str);
void draw_wide(WINDOW *win, int row, int col, char *src, int max);
void readable_fs(double size/*in bytes*/, char buf[]);
char* escape_shell(char *src);
void trans_char(char *src, char from, char to);
char* lines2argv(int, char **);
char* lines2yank(int argc, char **argv);
void del_param_list(char **params, int argc);
char* strip_quotes(const char *);
char* add_quotes(char *);
bool fuzzy_match(char *, const char *);

#endif
