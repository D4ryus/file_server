#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>

#include "types.h"

#define ERR_INFO __FILE__, __LINE__, __func__

int send_text(int, const char *, uint64_t);
int send_file(int, char *, uint64_t *);
uint64_t err_string_to_val(char *);
char *concat(char *, const char *);
int is_directory(const char *);
int starts_with(const char *, const char *);
char *format_size(uint64_t, char[7]);
void *err_malloc(size_t);
void *err_realloc(void *, size_t);
void err_quit(const char *, const int, const char *, const char *);
void warning(const char *, const int, const char *, const char *);
void usage_quit(const char *);
char *get_content_encoding(const char *);

#endif
