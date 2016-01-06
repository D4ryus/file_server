#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>

#include "errno.h"
#include "types.h"

/* prints error message + FILE, LINE, func and errno string */
#define die(fmt, ...) \
    fprintf(stderr, "%s:%d:%s: error: (%s) " fmt " \n", \
	__FILE__, __LINE__, __func__, \
	errno ? strerror(errno) : "-", \
	##__VA_ARGS__); \
    exit(1);

/* check(condition, error_fmt_string, ...):
 * checks the given condition, if TRUE error message will be printed and program
 * terminated */
#define check(cond, fmt, ...) \
    if ((cond)) { \
	die(fmt, ##__VA_ARGS__); \
    }

#define check_mem(ptr) \
    if (!(ptr)) { \
	die("Out of memory.") \
    }

/* prints the given message as warning + FILE LINE func */
#define warn(fmt, ...) \
    fprintf(stderr, "%s:%d:%s: warning: " fmt "\n", \
	__FILE__, __LINE__, __func__, ##__VA_ARGS__);

/* checks the given condition, if TRUE warning is printed */
#define check_warn(cond, fmt, ...) \
    if ((cond)) { \
	warn(fmt, ##__VA_ARGS__); \
    }

int send_data(int, const char *, uint64_t);
int send_file(int, const char *, uint64_t *, uint64_t, uint64_t);
uint64_t err_string_to_val(const char *);
char *concat(char *, int, ...);
int is_directory(const char *);
char *format_size(uint64_t, char[7]);
void usage_quit(const char *, const char *);
const char *get_err_msg(enum err_status);
char *normalize_ip(char *, const char *);
int ip_matches(const char *, const char *);
int s_printf(int, const char *, ...);

#endif
