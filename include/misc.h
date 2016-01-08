#ifndef MISC_H
#define MISC_H

#include <stdint.h>
#include <errno.h>

/* prints error message + FILE, LINE, func and errno string */
#define die(fmt, ...) \
    fprintf(stderr, "%s:%d:%s: error: (%s) " fmt " \n", \
	__FILE__, __LINE__, __func__, \
	errno ? strerror(errno) : "-", \
	##__VA_ARGS__); \
    exit(1);

/* check(condition, error_fmt_string, ...):
 * checks the given condition, if TRUE error message will be printed and
 * program terminated */
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

/*
 * intern error enum, used as return value from functions
 * see misc.c get_err_msg() for mapping to string values.
 */
enum err_status {
	STAT_OK = 0,
	WRITE_CLOSED,
	ZERO_WRITTEN,
	CLOSED_CON,
	EMPTY_MESSAGE,
	INV_REQ_TYPE,
	INV_GET,
	INV_POST,
	CON_LENGTH_MISSING,
	BOUNDARY_MISSING,
	FILESIZE_ZERO,
	WRONG_BOUNDRY,
	HTTP_HEAD_LINE_EXT,
	FILE_HEAD_LINE_EXT,
	POST_NO_FILENAME,
	NO_FREE_SPOT,
	FILE_ERROR,
	NO_CONTENT_DISP,
	FILENAME_ERR,
	CONTENT_LENGTH_EXT,
	HEADER_LINES_EXT,
	INV_CONTENT_TYPE,
	INV_RANGE,
	INV_POST_PATH,
	INV_CONNECTION,
	INV_HOST,
	IP_BLOCKED
};

int send_data(int, const char *, uint64_t, uint64_t *);
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
