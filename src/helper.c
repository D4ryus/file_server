#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <stdarg.h>

#include "config.h"
#include "helper.h"
#include "msg.h"

/*
 * returns:
 * STAT_OK      : everything went fine.
 * WRITE_CLOSED : could not write, client closed connection
 * ZERO_WRITTEN : could not write, 0 bytes written
 */
int
send_data(int sock, const char *data, uint64_t length)
{
	int sending;
	size_t max;
	ssize_t write_res;
	size_t buff_size;
	size_t sent_bytes;
	uint64_t cur_pos;

	max = (size_t)-1;
	sending    = 1;
	write_res  = 0;
	buff_size  = 0;
	sent_bytes = 0;
	cur_pos    = 0;

	while (sending) {
		/* check if length is longer than size_t */
		if (length - cur_pos > (uint64_t)max) {
			buff_size = max;
		} else {
			buff_size = (size_t)(length - cur_pos);
			sending = 0;
		}
		sent_bytes = 0;
		while (sent_bytes < buff_size) {
			write_res = send(sock, data + sent_bytes + cur_pos,
			    buff_size - sent_bytes, MSG_NOSIGNAL);
			if (write_res == -1) {
				return WRITE_CLOSED;
			} else if (write_res == 0) {
				return ZERO_WRITTEN;
			}
			sent_bytes += (size_t)write_res;
		}
		cur_pos += (uint64_t)buff_size;
	}

	return STAT_OK;
}

/*
 * STAT_OK      : everything went fine.
 * WRITE_CLOSED : could not write, client closed connection
 * ZERO_WRITTEN : could not write, 0 bytes written
 */
int
send_file(int sock, const char *filename, uint64_t *written, uint64_t from,
    uint64_t to)
{
	int sending;
	char *buffer;
	size_t read_bytes;
	FILE *fd;
	enum err_status error;
	uint64_t max;

	buffer = err_malloc((size_t)BUFFSIZE_READ);
	error = STAT_OK;

	fd = fopen(filename, "rb");
	check(!fd, "fopen(\"%s\") returned NULL", filename)

	sending = 1;
	read_bytes = 0;

	if (fseek(fd, (long)from, SEEK_SET) != 0) {
		error = FILE_ERROR;
		sending = 0;
	}
	max = to - from + 1;

	while (sending) {
		read_bytes = fread(buffer, (size_t)1, (size_t)BUFFSIZE_READ,
				 fd);
		if (read_bytes < BUFFSIZE_READ) {
			if (ferror(fd) != 0) {
				error = FILE_ERROR;
				break;
			}
			sending = 0;
		}
		if (*written + read_bytes > max) {
			read_bytes = (size_t)(max - *written);
			sending = 0;
		}
		error = send_data(sock, buffer, (uint64_t)read_bytes);
		if (error) {
			break;
		}
		*written += read_bytes;
	}
	fclose(fd);
	free(buffer);
	buffer = NULL;

	return error;
}

/*
 * evaluates string to uint64_t, on error 0 is returned
 */
uint64_t
err_string_to_val(const char *str)
{
	char *endptr;
	uint64_t val;

	errno = 0;
	val = strtoull(str, &endptr, 10);

	if ((errno == ERANGE && (val == ULLONG_MAX))
	    || (errno != 0 && val == 0)) {
		return 0;
	}

	return val;
}

char *
concat(char *dst, int count, ...)
{
	va_list ap1, ap2;
	size_t dst_len;
	int i;
	size_t size;

	if (dst) {
		dst_len = strlen(dst);
	} else {
		dst_len = 0;
	}

	va_start(ap1, count);
	va_copy(ap2, ap1);

	/* get length */
	size = dst_len;
	for (i = 0; i < count; i++) {
		size += strlen(va_arg(ap1, char *));
	}
	va_end(ap1);

	dst = realloc(dst, size + 1);
	memset(dst + dst_len, 0, size + 1 - dst_len);

	/* concat strings */
	for (i = 0; i < count; i++) {
		strcat(dst, va_arg(ap2, char *));
	}
	va_end(ap2);

	return dst;
}

/*
 * checks if given string is a directory, if its a file 0 is returned
 */
int
is_directory(const char *path)
{
	struct stat s;

	if (stat(path, &s) == 0) {
		if (s.st_mode & S_IFDIR) {
			return 1;
		} else if (s.st_mode & S_IFREG) {
			return 0;
		} else {
			die("stat(\"%s\") is not a file nor a directory",
			    path);
		}
	} else {
		die("stat(\"%s\") returned NULL", path);
	}

	return 0;
}

/*
 * formats size into readable format
 */
char *
format_size(uint64_t size, char fmt_size[7])
{
	char *type;
	uint64_t new_size;
	uint64_t xb; /* 8xb */
	uint64_t tb; /* 8tb */
	uint64_t gb; /* 8gb */
	uint64_t mb; /* 8mb */
	uint64_t kb; /* 8kb */

	new_size = 0;

	xb = (uint64_t)1 << 53;
	tb = (uint64_t)1 << 43;
	gb = (uint64_t)1 << 33;
	mb = (uint64_t)1 << 23;
	kb = (uint64_t)1 << 13;

	if (size > xb) {
		new_size = size >> 50;
		type = "xb";
	} else if (size > tb) {
		new_size = size >> 40;
		type = "tb";
	} else if (size > gb) {
		new_size = size >> 30;
		type = "gb";
	} else if (size > mb) {
		new_size = size >> 20;
		type = "mb";
	} else if (size > kb) {
		new_size = size >> 10;
		type = "kb";
	} else {
		new_size = size;
		type = "b ";
	}

	snprintf(fmt_size, (size_t)7, "%4llu%s",
	    (long long unsigned int)new_size, type);

	return fmt_size;
}

/*
 * mallocs given size but also checks if succeded, if not exits
 */
void *
err_malloc(size_t size)
{
	void *tmp;

	tmp = malloc(size);
	check(!tmp, "could not malloc(%lu)", size)

	return tmp;
}

/*
 * callocs given size but also checks if succeded, if not exits
 */
void *
err_calloc(size_t nmemb, size_t size)
{
	void *tmp;

	tmp = calloc(nmemb, size);
	check(!tmp, "could not calloc(%lu, %lu)", nmemb, size);

	return tmp;
}

/*
 * reallocs given size but also checks if succeded, if not exits
 */
void *
err_realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	check(!ptr, "could not realloc(%p, %lu)", ptr, size);

	return ptr;
}

/*
 * prints usage and quits
 */
void
usage_quit(const char *name, const char *msg)
{
	if (msg) {
		fprintf(stderr, "error: %s\n", msg);
	}
	/*
	 * Multiple fputs because C90 only supports strings with length up
	 * to 509 characters
	 */
	printf("usage: %s [-c | --color] [-d | --dir path] [-h | --help] "
	    "[-l | --log_file file] [-u | --upload ip] ", name);
#ifdef NCURSES
	fputs(
	    "[-n | --ncurses] "
	    , stdout);
#endif
	fputs(
	    "[-p | --port port] [-v | --verbosity level]\n"
	    "	-c, --color\n"
	    "		prints to terminal in fancy color.\n"
	    "	-d, --dir path\n"
	    "		use given path as root/shared directory.\n"
	    "	-h, --help\n"
	    "		print this help/usage.\n"
	    "	-l, --log_file file\n"
	    "		write to given file instead of stdout.\n"
	    , stdout);
#ifdef NCURSES
	fputs(
	    "	-n, --ncurses\n"
	    "		use ncurses display. if used with verbosity a log file is\n"
	    "		advised since it would clutter the ncurses screen.\n"
	    , stdout);
#endif
	fputs(
	    "	-p, --port port\n"
	    "		specify the port where server listens.\n"
	    "	-v, --verbosity level\n"
	    "		change verbosity level, level should be between 0 (no messages)\n"
	    "		and 3 (prints connection and transfer status).\n"
	    "	-i, --ip ip\n"
	    "		specify the start of allowed ip's, all others will be blocked.\n"
	    "		to Limit connections to 1.2.3.x -> -i 1.2.3.x\n"
	    "	-u, --upload ip\n"
	    "		enable upload via http POST for users with given ip.\n"
	    , stdout);
	exit(1);
}

const char *
get_err_msg(enum err_status error)
{
	static const char *err_msg[] = {
	/* STAT_OK            */ "no error detected",
	/* WRITE_CLOSED       */ "error - could not write, client closed "
					 "connection",
	/* ZERO_WRITTEN       */ "error - could not write, 0 bytes written",
	/* CLOSED_CON         */ "error - client closed connection",
	/* EMPTY_MESSAGE      */ "error - empty message",
	/* INV_REQ_TYPE       */ "error - invalid or missing Request Type",
	/* INV_GET            */ "error - invalid GET",
	/* INV_POST           */ "error - invalid POST",
	/* CON_LENGTH_MISSING */ "error - Content_Length missing",
	/* BOUNDARY_MISSING   */ "error - boundary missing",
	/* FILESIZE_ZERO      */ "error - filesize is 0 or error ocurred",
	/* WRONG_BOUNDRY      */ "error - wrong boundry specified",
	/* HTTP_HEAD_LINE_EXT */ "error - http header extended line limit",
	/* FILE_HEAD_LINE_EXT */ "error - file header extended line limit",
	/* POST_NO_FILENAME   */ "error - missing filename in post message",
	/* NO_FREE_SPOT       */ "error - the posted filename already exists",
	/* FILE_ERROR         */ "error - could not write the post content to "
					 "file",
	/* NO_CONTENT_DISP    */ "error - Content-Disposition missing",
	/* FILENAME_ERR       */ "error - could not parse filename",
	/* CONTENT_LENGTH_EXT */ "error - content length extended",
	/* POST_DISABLED      */ "error - post is disabled",
	/* HEADER_LINES_EXT   */ "error - too many headerlines",
	/* INV_CONTENT_TYPE   */ "error - invalid Content-Type",
	/* INV_RANGE          */ "error - invalid Range",
	/* INV_POST_PATH      */ "error - invalid POST path specified",
	/* INV_CONNECTION     */ "error - invalid connection type",
	/* INV_HOST           */ "error - invalid host",
	/* IP_BLOCKED         */ "error - ip blocked"
	};

	return err_msg[error];
}

/*
 * normalizes given ip, here are examples:
 *   21.32.111.5 -> 021.032.111.005
 * x21.32.111.x5 -> x21.032.111.0x5
 *	 1.1.1.1 -> 001.001.001.001
 * functions is used to make ip checking easier.
 * returned pointer will point to dst
 * dst must be allocated memory (16 bytes).
 */
char *
normalize_ip(char *dst, const char *src)
{
	int src_pos;
	int dst_pos;

	check(!src, "src is NULL");
	check(strlen(src) > 15, "src length (%lu) > 15.", strlen(src));

	if (strlen(src) == 15) {
		memcpy(dst, src, 15);
		dst[15] = '\0';
		return dst;
	}

	if (strlen(src) == 1 && src[0] == 'x') {
		memcpy(dst, "xxx.xxx.xxx.xxx", 15);
		dst[15] = '\0';
		return dst;
	}

	dst[15] = '\0';
	src_pos = (int)strlen(src) - 1;
	for (dst_pos = 14; dst_pos > -1; dst_pos--) {
		if ((src_pos < 0 || src[src_pos] == '.')
		    && (dst_pos + 1) % 4 != 0) {
			dst[dst_pos] = '0';
		} else {
			dst[dst_pos] = src[src_pos];
			src_pos--;
		}
	}

	return dst;
}

/* checks ip_check against ip_allowed, returns 1 on match, 0 on failure */
int
ip_matches(const char *ip_allowed, const char *ip_check)
{
	char ip_allowed_norm[16];
	char ip_check_norm[16];
	int i;

	check(!ip_allowed, "ip_allowed is NULL");
	check(!ip_check, "ip_check is NULL");

	if (strlen(ip_allowed) == 1)  {
		if (ip_allowed[0] == 'x') {
			return 1;
		} else if (ip_allowed[0] == '-') {
			return 0;
		}
	}

	if (strlen(ip_allowed) != 15) {
		char ip_tmp[16];
		normalize_ip(ip_tmp, ip_allowed);
		memcpy(ip_allowed_norm, ip_tmp, 16);
	} else {
		memcpy(ip_allowed_norm, ip_allowed, 16);
	}

	if (strlen(ip_check) != 15) {
		char ip_tmp[16];
		normalize_ip(ip_tmp, ip_check);
		memcpy(ip_check_norm, ip_tmp, 16);
	} else {
		memcpy(ip_check_norm, ip_check, 16);
	}

	for (i = 0; i < 15; i++) {
		/* if i dont see a x -> check if they match, if not return
		 *     -> failure.
		 * if i dont see a x -> check if they match, if they do
		 *     -> continue.
		 * if i see a x -> continue;
		 */
		if (ip_allowed_norm[i] != 'x'
		    && ip_allowed_norm[i] != ip_check_norm[i]) {
			return 0;
		}
	}

	/* if we came this far, return a success! */
	return 1;
}
