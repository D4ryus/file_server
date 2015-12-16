#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>

#include "globals.h"
#include "helper.h"
#include "msg.h"
#include "defines.h"

static char *mime_types[][2] = {
	{ "txt",     "text/plain" },
	{ "htm",     "text/html" },
	{ "html",    "text/html" },
	{ "shtml",   "text/html" },
	{ "jpeg",    "image/jpeg" },
	{ "jpg",     "image/jpeg" },
	{ "jpe",     "image/jpeg" },
	{ "png",     "image/png" },
	{ "webm",    "video/webm" },
	{ "mp4",     "video/mp4" },
	{ "m4v",     "video/mp4" },
	{ "m4a",     "audio/mp4" },
	{ "ogv",     "video/ogg" },
	{ "ogg",     "audio/ogg" },
	{ "oga",     "audio/ogg" },
	{ "gif",     "image/gif" },
	{ "tar",     "application/x-tar" },
	{ "gz",      "application/gzip" },
	{ "zip",     "application/zip" },
	{ "pdf",     "application/pdf" },
	{ "mpeg",    "video/mpeg" },
	{ "mpg",     "video/mpeg" },
	{ "mpe",     "video/mpeg" },
	{ "dwg",     "application/acad" },
	{ "asd",     "application/astound" },
	{ "asn",     "application/astound" },
	{ "tsp",     "application/dsptype" },
	{ "dxf",     "application/dxf" },
	{ "spl",     "application/futuresplash" },
	{ "ptlk",    "application/listenup" },
	{ "hqx",     "application/mac-binhex40" },
	{ "mbd",     "application/mbedlet" },
	{ "mif",     "application/mif" },
	{ "xls",     "application/msexcel" },
	{ "xla",     "application/msexcel" },
	{ "hlp",     "application/mshelp" },
	{ "chm",     "application/mshelp" },
	{ "ppt",     "application/mspowerpoint" },
	{ "ppz",     "application/mspowerpoint" },
	{ "pps",     "application/mspowerpoint" },
	{ "doc",     "application/msword" },
	{ "dot",     "application/msword" },
	{ "bin",     "application/octet-stream" },
	{ "exe",     "application/octet-stream" },
	{ "com",     "application/octet-stream" },
	{ "dll",     "application/octet-stream" },
	{ "oda",     "application/oda" },
	{ "ai",      "application/postscript" },
	{ "eps",     "application/postscript" },
	{ "rtc",     "application/rtc" },
	{ "rtf",     "application/rtf" },
	{ "smp",     "application/studiom" },
	{ "tbk",     "application/toolbook" },
	{ "vmd",     "application/vocaltec-media-desc" },
	{ "vmf",     "application/vocaltec-media-file" },
	{ "htm",     "application/xhtml+xml" },
	{ "html",    "application/xhtml+xml" },
	{ "shtml",   "application/xhtml+xml" },
	{ "xhtml",   "application/xhtml+xml" },
	{ "xml",     "application/xml" },
	{ "bcpio",   "application/x-bcpio" },
	{ "z",       "application/x-compress" },
	{ "cpio",    "application/x-cpio" },
	{ "csh",     "application/x-csh" },
	{ "dcr",     "application/x-director" },
	{ "dir",     "application/x-director" },
	{ "dxr",     "application/x-director" },
	{ "dvi",     "application/x-dvi" },
	{ "evy",     "application/x-envoy" },
	{ "gtar",    "application/x-gtar" },
	{ "hdf",     "application/x-hdf" },
	{ "php",     "application/x-httpd-php" },
	{ "phtml",   "application/x-httpd-php" },
	{ "js",      "application/x-javascript" },
	{ "latex",   "application/x-latex" },
	{ "bin",     "application/x-macbinary" },
	{ "mif",     "application/x-mif" },
	{ "nc",      "application/x-netcdf" },
	{ "cdf",     "application/x-netcdf" },
	{ "nsc",     "application/x-nschat" },
	{ "sh",      "application/x-sh" },
	{ "shar",    "application/x-shar" },
	{ "swf",     "application/x-shockwave-flash" },
	{ "cab",     "application/x-shockwave-flash" },
	{ "spr",     "application/x-sprite" },
	{ "sprite",  "application/x-sprite" },
	{ "sit",     "application/x-stuffit" },
	{ "sca",     "application/x-supercard" },
	{ "sv4cpio", "application/x-sv4cpio" },
	{ "sv4crc",  "application/x-sv4crc" },
	{ "tcl",     "application/x-tcl" },
	{ "tex",     "application/x-tex" },
	{ "texinfo", "application/x-texinfo" },
	{ "texi",    "application/x-texinfo" },
	{ "t",       "application/x-troff" },
	{ "tr",      "application/x-troff" },
	{ "man",     "application/x-troff-man" },
	{ "troff",   "application/x-troff-man" },
	{ "me",      "application/x-troff-me" },
	{ "troff",   "application/x-troff-me" },
	{ "ms",      "application/x-troff-ms" },
	{ "troff",   "application/x-troff-ms" },
	{ "ustar",   "application/x-ustar" },
	{ "src",     "application/x-wais-source" },
	{ "au",      "audio/basic" },
	{ "snd",     "audio/basic" },
	{ "es",      "audio/echospeech" },
	{ "tsi",     "audio/tsplayer" },
	{ "vox",     "audio/voxware" },
	{ "aif",     "audio/x-aiff" },
	{ "aiff",    "audio/x-aiff" },
	{ "aifc",    "audio/x-aiff" },
	{ "dus",     "audio/x-dspeeh" },
	{ "cht",     "audio/x-dspeeh" },
	{ "mid",     "audio/x-midi" },
	{ "midi",    "audio/x-midi" },
	{ "mp2",     "audio/x-mpeg" },
	{ "ram",     "audio/x-pn-realaudio" },
	{ "ra",      "audio/x-pn-realaudio" },
	{ "rpm",     "audio/x-pn-realaudio-plugin" },
	{ "stream",  "audio/x-qt-stream" },
	{ "wav",     "audio/x-wav" },
	{ "dwf",     "drawing/x-dwf" },
	{ "cod",     "image/cis-cod" },
	{ "ras",     "image/cmu-raster" },
	{ "fif",     "image/fif" },
	{ "ief",     "image/ief" },
	{ "tiff",    "image/tiff" },
	{ "tif",     "image/tiff" },
	{ "mcf",     "image/vasa" },
	{ "wbmp",    "image/vnd.wap.wbmp" },
	{ "fh4",     "image/x-freehand" },
	{ "fh5",     "image/x-freehand" },
	{ "fhc",     "image/x-freehand" },
	{ "ico",     "image/x-icon" },
	{ "pnm",     "image/x-portable-anymap" },
	{ "pbm",     "image/x-portable-bitmap" },
	{ "pgm",     "image/x-portable-graymap" },
	{ "ppm",     "image/x-portable-pixmap" },
	{ "rgb",     "image/x-rgb" },
	{ "xwd",     "image/x-windowdump" },
	{ "xbm",     "image/x-xbitmap" },
	{ "xpm",     "image/x-xpixmap" },
	{ "wrl",     "model/vrml" },
	{ "csv",     "text/comma-separated-values" },
	{ "css",     "text/css" },
	{ "js",      "text/javascript" },
	{ "rtx",     "text/richtext" },
	{ "rtf",     "text/rtf" },
	{ "tsv",     "text/tab-separated-values" },
	{ "wml",     "text/vnd.wap.wml" },
	{ "wmlc",    "application/vnd.wap.wmlc" },
	{ "wmls",    "text/vnd.wap.wmlscript" },
	{ "wmlsc",   "application/vnd.wap.wmlscriptc" },
	{ "xml",     "text/xml" },
	{ "etx",     "text/x-setext" },
	{ "sgm",     "text/x-sgml" },
	{ "sgml",    "text/x-sgml" },
	{ "talk",    "text/x-speech" },
	{ "spc",     "text/x-speech" },
	{ "qt",      "video/quicktime" },
	{ "mov",     "video/quicktime" },
	{ "viv",     "video/vnd.vivo" },
	{ "vivo",    "video/vnd.vivo" },
	{ "avi",     "video/x-msvideo" },
	{ "movie",   "video/x-sgi-movie" },
	{ "vts",     "workbook/formulaone" },
	{ "vtts",    "workbook/formulaone" },
	{ "3dmf",    "x-world/x-3dmf" },
	{ "3dm",     "x-world/x-3dmf" },
	{ "qd3d",    "x-world/x-3dmf" },
	{ "qd3",     "x-world/x-3dmf" },
	{ "wrl",     "x-world/x-vrml" },
};

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
 * if negative number is return, error occured
 * STAT_OK      : everything went fine.
 * WRITE_CLOSED : could not write, client closed connection
 * ZERO_WRITTEN : could not write, 0 bytes written
 */
int
send_file(int sock, char *filename, uint64_t *written, uint64_t from,
    uint64_t to)
{
	int sending;
	char *buffer;
	size_t read_bytes;
	FILE *fd;
	char *full_path;
	enum err_status error;
	uint64_t max;

	buffer = err_malloc((size_t)BUFFSIZE_READ);
	error = STAT_OK;
	full_path = NULL;
	full_path = concat(concat(full_path, ROOT_DIR), filename);

	fd = fopen(full_path, "rb");
	free(full_path);
	if (!fd) {
		die(ERR_INFO, "fopen()");
	}

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

	return error;
}

/*
 * evaluates string to uint64_t, on error 0 is returned
 */
uint64_t
err_string_to_val(char *str)
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

/*
 * will reallocated dst and strcat src onto it
 */
char *
concat(char *dst, const char *src)
{
	if (!dst) {
		dst = err_malloc(strlen(src) + 1);
		strncpy(dst, src, strlen(src) + 1);
	} else {
		dst = err_realloc(dst, strlen(dst) + strlen(src) + 1);
		strncat(dst, src, strlen(src));
	}

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
			die(ERR_INFO, "stat() has no file nor a directory");
		}
	} else {
		die(ERR_INFO, "stat()");
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
	if (!tmp) {
		die(ERR_INFO, "malloc()");
	}

	return tmp;
}

/*
 * reallocs given size but also checks if succeded, if not exits
 */
void *
err_realloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (!ptr) {
		die(ERR_INFO, "realloc()");
	}

	return ptr;
}

/*
 * prints out given information to stderr and exits
 */
void
die(const char *file, const int line, const char *function, const char *msg)
{
	if (LOG_FILE_D) {
		fprintf(LOG_FILE_D, "%s:%d:%s: error: %s: %s\n",
		    file, line, function, msg, strerror(errno));
		fflush(LOG_FILE_D);
	} else {
		fprintf(stderr, "%s:%d:%s: error: %s: %s\n",
		    file, line, function, msg, strerror(errno));
	}
	exit(1);
}

/*
 * prints out given information to stderr
 */
void
warning(const char *file, const int line, const char *function,
    const char *msg)
{
	if (LOG_FILE_D) {
		fprintf(LOG_FILE_D, "%s:%d:%s: warning: %s: %s\n",
		    file, line, function, msg, strerror(errno));
		fflush(LOG_FILE_D);
	} else {
		fprintf(stderr, "%s:%d:%s: warning: %s: %s\n",
		    file, line, function, msg, strerror(errno));
	}
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
	    "[-l | --log_file file] [-u | --upload ip]", name);
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
	    "		to Limit connections to 1.2.3.* -> -i 1.2.3.*\n"
	    "	-u, --upload ip\n"
	    "		enable upload via http POST for users with given ip.\n"
	    , stdout);
	exit(1);
}

/*
 * returns content type of given file type
 * given "html" returns  "text/html"
 * given "gz"	returns  "application/gzip"
 * if type is not recognized, NULL will be returned
 */
char *
get_content_encoding(const char *file_name)
{
	char *type;
	size_t i;
	size_t elements;

	type = strrchr(file_name, '.');
	if (!type|| strlen(type) == 1) {
		return "application/octet-stream";
	}
	type = type + 1; // move over dot

	if (!type|| !strcmp(type, "")) {
		return "application/octet-stream";
	}

	elements = sizeof(mime_types) / sizeof(mime_types[0]);
	for (i = 0; i < elements; i++) {
		if (!strcmp(type, mime_types[i][0])) {
			return mime_types[i][1];
		}
	}

	return "text/plain";
}

const char *
get_err_msg(enum err_status error)
{
	static const char *err_msg[] = {
	/* STAT_OK            */ "no error detected",
	/* WRITE_CLOSED       */ "error - could not write, client closed connection",
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
	/* FILE_ERROR         */ "error - could not write the post content to file",
	/* NO_CONTENT_DISP    */ "error - Content-Disposition missing",
	/* FILENAME_ERR       */ "error - could not parse filename",
	/* CONTENT_LENGTH_EXT */ "error - content length extended",
	/* POST_DISABLED      */ "error - post is disabled",
	/* HEADER_LINES_EXT   */ "error - too many headerlines",
	/* INV_CONTENT_TYPE   */ "error - invalid Content-Type",
	/* INV_RANGE          */ "error - invalid Range",
	/* INV_POST_PATH      */ "error - invalid POST path specified"
	};

	return err_msg[error];
}

/*
 * normalizes given ip, here are examples:
 *   21.32.111.5 -> 021.032.111.005
 * *21.32.111.*5 -> *21.032.111.0*5
 *       1.1.1.1 -> 001.001.001.001
 * functions is used to make ip checking easier.
 * returned pointer will point to dst
 * dst must be allocated memory (16 bytes).
 */
char *
normalize_ip(char *dst, const char *src)
{
	int src_pos;
	int dst_pos;

	if (!src || strlen(src) > 15) {
		die(ERR_INFO, "src NULL or length > 15.");
	}

	if (strlen(src) == 15) {
		memcpy(dst, src, 15);
		dst[15] = '\0';
		return dst;
	}

	if (strlen(src) == 1 && src[0] == '*') {
		memcpy(dst, "***.***.***.***", 15);
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

	if (!ip_allowed) {
		die(ERR_INFO, "ip_allowed NULL");
	}
	if (!ip_check) {
		die(ERR_INFO, "ip_check NULL");
	}

	if (strlen(ip_allowed) == 1 && ip_allowed[0] == '*') {
		return 1;
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
		/* if i dont see a star -> check if they match, if not return
		 *     -> failure.
		 * if i dont see a star -> check if they match, if they do
		 *     -> continue.
		 * if i see a star -> continue;
		 */
		if (ip_allowed_norm[i] != '*' && ip_allowed_norm[i] != ip_check_norm[i]) {
			return 0;
		}
	}

	/* if we came this far, return a success! */
	return 1;
}
