#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>

#include "types.h"

#define ERR_INFO __FILE__, __LINE__, __func__


int send_data(int, const char *, uint64_t);
int send_file(int, const char *, uint64_t *, uint64_t, uint64_t);
uint64_t err_string_to_val(const char *);
char *concat(char *, int, ...);
int is_directory(const char *);
char *format_size(uint64_t, char[7]);
void *err_malloc(size_t);
void *err_calloc(size_t, size_t);
void *err_realloc(void *, size_t);
void die(const char *, const int, const char *, const char *);
void warning(const char *, const int, const char *, const char *);
void usage_quit(const char *, const char *);
const char *get_err_msg(enum err_status);
char *normalize_ip(char *, const char *);
int ip_matches(const char *, const char *);
int s_printf(int, const char *, ...);

#endif
