#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>

#include "helper.h"
#include "msg.h"
#include "defines.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern FILE *_LOG_FILE;

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
send_file(int sock, char *filename, uint64_t *written, uint64_t from, uint64_t to)
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
	if (fd == NULL) {
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
		if ((*written) + read_bytes > max) {
			read_bytes = (size_t)(max - (*written));
			sending = 0;
		}
		error = send_data(sock, buffer, (uint64_t)read_bytes);
		if (error) {
			break;
		}
		(*written) += read_bytes;
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
	if (dst == NULL) {
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
	if (tmp == NULL) {
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
	if (ptr == NULL) {
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
	if (_LOG_FILE != NULL) {
		fprintf(_LOG_FILE, "%s:%d:%s: error: %s: %s\n",
		    file, line, function, msg, strerror(errno));
		fflush(_LOG_FILE);
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
warning(const char *file, const int line, const char *function, const char *msg)
{
	if (_LOG_FILE != NULL) {
		fprintf(_LOG_FILE, "%s:%d:%s: warning: %s: %s\n",
		    file, line, function, msg, strerror(errno));
		fflush(_LOG_FILE);
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
	if (msg != NULL) {
		fprintf(stderr, "error: %s\n", msg);
	}
	/*
	 * Multiple fputs because C90 only supports strings with length up
	 * to 509 characters
	 */
	printf("usage: %s [-c | --color] [-d | --dir path] [-h | --help] "
	    "[-l | --log_file file] [-u | --upload path]", name);
	fputs(
#ifdef NCURSES
	    "[-n | --ncurses] "
#endif
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
	fputs(
#ifdef NCURSES
	    "	-n, --ncurses\n"
	    "		use ncurses display. if used with verbosity a log file is\n"
	    "		advised since it would clutter the ncurses screen.\n"
#endif
	    "	-p, --port port\n"
	    "		specify the port where server listens.\n"
	    "	-v, --verbosity level\n"
	    "		change verbosity level, level should be between 0 (no messages)\n"
	    "		and 3 (prints connection and transfer status).\n"
	    "	-u, --upload path\n"
	    "		enable upload via http POST and save files at given directory.\n"
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

	type = strrchr(file_name, '.');
	if (type == NULL || strlen(type) == 1) {
		return "application/octet-stream";
	}
	type = type + 1; // move over dot

	if (type == NULL || !strcmp(type, "")) {
		return "application/octet-stream";
	} else if (!strcmp(type, "txt")) {
		return "text/plain";
	} else if (!strcmp(type, "htm") || !strcmp(type, "html") || !strcmp(type, "shtml")) {
		return "text/html";
	} else if (!strcmp(type, "jpeg") || !strcmp(type, "jpg") || !strcmp(type, "jpe")) {
		return "image/jpeg";
	} else if (!strcmp(type, "png")) {
		return "image/png";
	} else if (!strcmp(type, "webm")) {
		return "video/webm";
	} else if (!strcmp(type, "gif")) {
		return "image/gif";
	} else if (!strcmp(type, "tar")) {
		return "application/x-tar";
	} else if (!strcmp(type, "gz")) {
		return "application/gzip";
	} else if (!strcmp(type, "zip")) {
		return "application/zip";
	} else if (!strcmp(type, "pdf")) {
		return "application/pdf";
	} else if (!strcmp(type, "mpeg") || !strcmp(type, "mpg") || !strcmp(type, "mpe")) {
		return "video/mpeg";
	} else if (!strcmp(type, "dwg")) {
		return "application/acad";
	} else if (!strcmp(type, "asd") || !strcmp(type, "asn")) {
		return "application/astound";
	} else if (!strcmp(type, "tsp")) {
		return "application/dsptype";
	} else if (!strcmp(type, "dxf")) {
		return "application/dxf";
	} else if (!strcmp(type, "spl")) {
		return "application/futuresplash";
	} else if (!strcmp(type, "ptlk")) {
		return "application/listenup";
	} else if (!strcmp(type, "hqx")) {
		return "application/mac-binhex40";
	} else if (!strcmp(type, "mbd")) {
		return "application/mbedlet";
	} else if (!strcmp(type, "mif")) {
		return "application/mif";
	} else if (!strcmp(type, "xls") || !strcmp(type, "xla")) {
		return "application/msexcel";
	} else if (!strcmp(type, "hlp") || !strcmp(type, "chm")) {
		return "application/mshelp";
	} else if (!strcmp(type, "ppt") || !strcmp(type, "ppz") || !strcmp(type, "pps") || !strcmp(type, "pot")) {
		return "application/mspowerpoint";
	} else if (!strcmp(type, "doc") || !strcmp(type, "dot")) {
		return "application/msword";
	} else if (!strcmp(type, "bin") || !strcmp(type, "exe") || !strcmp(type, "com") || !strcmp(type, "dll") || !strcmp(type, "class")) {
		return "application/octet-stream";
	} else if (!strcmp(type, "oda")) {
		return "application/oda";
	} else if (!strcmp(type, "ai") || !strcmp(type, "eps") || !strcmp(type, "ps")) {
		return "application/postscript";
	} else if (!strcmp(type, "rtc")) {
		return "application/rtc";
	} else if (!strcmp(type, "rtf")) {
		return "application/rtf";
	} else if (!strcmp(type, "smp")) {
		return "application/studiom";
	} else if (!strcmp(type, "tbk")) {
		return "application/toolbook";
	} else if (!strcmp(type, "vmd")) {
		return "application/vocaltec-media-desc";
	} else if (!strcmp(type, "vmf")) {
		return "application/vocaltec-media-file";
	} else if (!strcmp(type, "htm") || !strcmp(type, "html") || !strcmp(type, "shtml") || !strcmp(type, "xhtml")) {
		return "application/xhtml+xml";
	} else if (!strcmp(type, "xml")) {
		return "application/xml";
	} else if (!strcmp(type, "bcpio")) {
		return "application/x-bcpio";
	} else if (!strcmp(type, "z")) {
		return "application/x-compress";
	} else if (!strcmp(type, "cpio")) {
		return "application/x-cpio";
	} else if (!strcmp(type, "csh")) {
		return "application/x-csh";
	} else if (!strcmp(type, "dcr") || !strcmp(type, "dir") || !strcmp(type, "dxr")) {
		return "application/x-director";
	} else if (!strcmp(type, "dvi")) {
		return "application/x-dvi";
	} else if (!strcmp(type, "evy")) {
		return "application/x-envoy";
	} else if (!strcmp(type, "gtar")) {
		return "application/x-gtar";
	} else if (!strcmp(type, "hdf")) {
		return "application/x-hdf";
	} else if (!strcmp(type, "php") || !strcmp(type, "phtml")) {
		return "application/x-httpd-php";
	} else if (!strcmp(type, "js")) {
		return "application/x-javascript";
	} else if (!strcmp(type, "latex")) {
		return "application/x-latex";
	} else if (!strcmp(type, "bin")) {
		return "application/x-macbinary";
	} else if (!strcmp(type, "mif")) {
		return "application/x-mif";
	} else if (!strcmp(type, "nc") || !strcmp(type, "cdf")) {
		return "application/x-netcdf";
	} else if (!strcmp(type, "nsc")) {
		return "application/x-nschat";
	} else if (!strcmp(type, "sh")) {
		return "application/x-sh";
	} else if (!strcmp(type, "shar")) {
		return "application/x-shar";
	} else if (!strcmp(type, "swf") || !strcmp(type, "cab")) {
		return "application/x-shockwave-flash";
	} else if (!strcmp(type, "spr") || !strcmp(type, "sprite")) {
		return "application/x-sprite";
	} else if (!strcmp(type, "sit")) {
		return "application/x-stuffit";
	} else if (!strcmp(type, "sca")) {
		return "application/x-supercard";
	} else if (!strcmp(type, "sv4cpio")) {
		return "application/x-sv4cpio";
	} else if (!strcmp(type, "sv4crc")) {
		return "application/x-sv4crc";
	} else if (!strcmp(type, "tcl")) {
		return "application/x-tcl";
	} else if (!strcmp(type, "tex")) {
		return "application/x-tex";
	} else if (!strcmp(type, "texinfo") || !strcmp(type, "texi")) {
		return "application/x-texinfo";
	} else if (!strcmp(type, "t") || !strcmp(type, "tr") || !strcmp(type, "roff")) {
		return "application/x-troff";
	} else if (!strcmp(type, "man") || !strcmp(type, "troff")) {
		return "application/x-troff-man";
	} else if (!strcmp(type, "me") || !strcmp(type, "troff")) {
		return "application/x-troff-me";
	} else if (!strcmp(type, "ms") || !strcmp(type, "troff")) {
		return "application/x-troff-ms";
	} else if (!strcmp(type, "ustar")) {
		return "application/x-ustar";
	} else if (!strcmp(type, "src")) {
		return "application/x-wais-source";
	} else if (!strcmp(type, "au") || !strcmp(type, "snd")) {
		return "audio/basic";
	} else if (!strcmp(type, "es")) {
		return "audio/echospeech";
	} else if (!strcmp(type, "tsi")) {
		return "audio/tsplayer";
	} else if (!strcmp(type, "vox")) {
		return "audio/voxware";
	} else if (!strcmp(type, "aif") || !strcmp(type, "aiff") || !strcmp(type, "aifc")) {
		return "audio/x-aiff";
	} else if (!strcmp(type, "dus") || !strcmp(type, "cht")) {
		return "audio/x-dspeeh";
	} else if (!strcmp(type, "mid") || !strcmp(type, "midi")) {
		return "audio/x-midi";
	} else if (!strcmp(type, "mp2")) {
		return "audio/x-mpeg";
	} else if (!strcmp(type, "ram") || !strcmp(type, "ra")) {
		return "audio/x-pn-realaudio";
	} else if (!strcmp(type, "rpm")) {
		return "audio/x-pn-realaudio-plugin";
	} else if (!strcmp(type, "stream")) {
		return "audio/x-qt-stream";
	} else if (!strcmp(type, "wav")) {
		return "audio/x-wav";
	} else if (!strcmp(type, "dwf")) {
		return "drawing/x-dwf";
	} else if (!strcmp(type, "cod")) {
		return "image/cis-cod";
	} else if (!strcmp(type, "ras")) {
		return "image/cmu-raster";
	} else if (!strcmp(type, "fif")) {
		return "image/fif";
	} else if (!strcmp(type, "ief")) {
		return "image/ief";
	} else if (!strcmp(type, "tiff") || !strcmp(type, "tif")) {
		return "image/tiff";
	} else if (!strcmp(type, "mcf")) {
		return "image/vasa";
	} else if (!strcmp(type, "wbmp")) {
		return "image/vnd.wap.wbmp";
	} else if (!strcmp(type, "fh4") || !strcmp(type, "fh5") || !strcmp(type, "fhc")) {
		return "image/x-freehand";
	} else if (!strcmp(type, "ico")) {
		return "image/x-icon";
	} else if (!strcmp(type, "pnm")) {
		return "image/x-portable-anymap";
	} else if (!strcmp(type, "pbm")) {
		return "image/x-portable-bitmap";
	} else if (!strcmp(type, "pgm")) {
		return "image/x-portable-graymap";
	} else if (!strcmp(type, "ppm")) {
		return "image/x-portable-pixmap";
	} else if (!strcmp(type, "rgb")) {
		return "image/x-rgb";
	} else if (!strcmp(type, "xwd")) {
		return "image/x-windowdump";
	} else if (!strcmp(type, "xbm")) {
		return "image/x-xbitmap";
	} else if (!strcmp(type, "xpm")) {
		return "image/x-xpixmap";
	} else if (!strcmp(type, "wrl")) {
		return "model/vrml";
	} else if (!strcmp(type, "csv")) {
		return "text/comma-separated-values";
	} else if (!strcmp(type, "css")) {
		return "text/css";
	} else if (!strcmp(type, "js")) {
		return "text/javascript";
	} else if (!strcmp(type, "rtx")) {
		return "text/richtext";
	} else if (!strcmp(type, "rtf")) {
		return "text/rtf";
	} else if (!strcmp(type, "tsv")) {
		return "text/tab-separated-values";
	} else if (!strcmp(type, "wml")) {
		return "text/vnd.wap.wml";
	} else if (!strcmp(type, "wmlc")) {
		return "application/vnd.wap.wmlc";
	} else if (!strcmp(type, "wmls")) {
		return "text/vnd.wap.wmlscript";
	} else if (!strcmp(type, "wmlsc")) {
		return "application/vnd.wap.wmlscriptc";
	} else if (!strcmp(type, "xml")) {
		return "text/xml";
	} else if (!strcmp(type, "etx")) {
		return "text/x-setext";
	} else if (!strcmp(type, "sgm") || !strcmp(type, "sgml")) {
		return "text/x-sgml";
	} else if (!strcmp(type, "talk") || !strcmp(type, "spc")) {
		return "text/x-speech";
	} else if (!strcmp(type, "qt") || !strcmp(type, "mov")) {
		return "video/quicktime";
	} else if (!strcmp(type, "viv") || !strcmp(type, "vivo")) {
		return "video/vnd.vivo";
	} else if (!strcmp(type, "avi")) {
		return "video/x-msvideo";
	} else if (!strcmp(type, "movie")) {
		return "video/x-sgi-movie";
	} else if (!strcmp(type, "vts") || !strcmp(type, "vtts")) {
		return "workbook/formulaone";
	} else if (!strcmp(type, "3dmf") || !strcmp(type, "3dm") || !strcmp(type, "qd3d") || !strcmp(type, "qd3")) {
		return "x-world/x-3dmf";
	} else if (!strcmp(type, "wrl")) {
		return "x-world/x-vrml";
	} else {
		return "text/plain";
	}
}
