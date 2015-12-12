#ifndef FN_CONFIG_H
#define FN_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

void config_init();
bool config_load(const char* file);
bool config_read(FILE *file);

#endif
