#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <inttypes.h>

#include "parse_http.h"
#include "helper.h"
#include "defines.h"

static int get_line(int, char**);
static int parse_GET(struct http_header *, char *);
static int parse_POST(struct http_header *, char *);
static int parse_url(struct http_header *, char *);
static int parse_host(struct http_header *, char *);
static int parse_connection(struct http_header *, char *);
static int parse_range(struct http_header *, char *);
static int parse_content_length(struct http_header *, char *);
static int parse_content_type(struct http_header *, char *);

/* sorted and match mime enum in helper.h */
static const char *mime_string[] = {
	"application/gzip",
	"application/msexcel",
	"application/mspowerpoint",
	"application/msword",
	"application/octet-stream",
	"application/pdf",
	"application/postscript",
	"application/x-compress",
	"application/x-csh",
	"application/x-dvi",
	"application/xhtml+xml",
	"application/x-httpd-php",
	"application/x-latex",
	"application/x-sh",
	"application/x-shockwave-flash",
	"application/x-tar",
	"application/x-tcl",
	"application/x-tex",
	"application/x-troff-ms",
	"application/zip",
	"audio/basic",
	"audio/mp4",
	"audio/mpeg",
	"audio/ogg",
	"audio/x-qt-stream",
	"audio/x-wav",
	"drawing/x-dwf",
	"image/gif",
	"image/jpeg",
	"image/png",
	"image/tiff",
	"image/x-icon",
	"image/x-portable-anymap",
	"image/x-portable-bitmap",
	"image/x-portable-graymap",
	"image/x-portable-pixmap",
	"image/x-rgb",
	"multipart/form-data",
	"text/comma-separated-values",
	"text/css",
	"text/html",
	"text/javascript",
	"text/plain",
	"text/richtext",
	"text/rtf",
	"text/tab-separated-values",
	"text/xml",
	"video/mp4",
	"video/mpeg",
	"video/ogg",
	"video/quicktime",
	"video/webm",
	"video/x-msvideo",
	"video/x-sgi-movie",
};

struct mime_map {
	char *ending;
	enum mime type;
};

static struct mime_map mime_type_mapper[] = {
	{ "txt",     TEXT_PLAIN },
	{ "htm",     TEXT_HTML },
	{ "html",    TEXT_HTML },
	{ "shtml",   TEXT_HTML },
	{ "xml",     TEXT_XML },
	{ "css",     TEXT_CSS },
	{ "js",      TEXT_JAVASCRIPT },
	{ "jpeg",    IMAGE_JPEG },
	{ "jpg",     IMAGE_JPEG },
	{ "jpe",     IMAGE_JPEG },
	{ "png",     IMAGE_PNG },
	{ "mp3",     AUDIO_MPEG },
	{ "webm",    VIDEO_WEBM },
	{ "mp4",     VIDEO_MP4 },
	{ "m4v",     VIDEO_MP4 },
	{ "m4a",     AUDIO_MP4 },
	{ "ogv",     VIDEO_OGG },
	{ "ogg",     AUDIO_OGG },
	{ "oga",     AUDIO_OGG },
	{ "gif",     IMAGE_GIF },
	{ "tar",     APPLICATION_X_TAR },
	{ "gz",      APPLICATION_GZIP },
	{ "zip",     APPLICATION_ZIP },
	{ "pdf",     APPLICATION_PDF },
	{ "mpeg",    VIDEO_MPEG },
	{ "mpg",     VIDEO_MPEG },
	{ "mpe",     VIDEO_MPEG },
	{ "xls",     APPLICATION_MSEXCEL },
	{ "xla",     APPLICATION_MSEXCEL },
	{ "ppt",     APPLICATION_MSPOWERPOINT },
	{ "ppz",     APPLICATION_MSPOWERPOINT },
	{ "pps",     APPLICATION_MSPOWERPOINT },
	{ "doc",     APPLICATION_MSWORD },
	{ "dot",     APPLICATION_MSWORD },
	{ "bin",     APPLICATION_OCTET_STREAM },
	{ "exe",     APPLICATION_OCTET_STREAM },
	{ "com",     APPLICATION_OCTET_STREAM },
	{ "dll",     APPLICATION_OCTET_STREAM },
	{ "ai",      APPLICATION_POSTSCRIPT },
	{ "eps",     APPLICATION_POSTSCRIPT },
	{ "shtml",   APPLICATION_XHTML_XML },
	{ "xhtml",   APPLICATION_XHTML_XML },
	{ "z",       APPLICATION_X_COMPRESS },
	{ "csh",     APPLICATION_X_CSH },
	{ "dvi",     APPLICATION_X_DVI },
	{ "php",     APPLICATION_X_HTTPD_PHP },
	{ "phtml",   APPLICATION_X_HTTPD_PHP },
	{ "latex",   APPLICATION_X_LATEX },
	{ "sh",      APPLICATION_X_SH },
	{ "swf",     APPLICATION_X_SHOCKWAVE_FLASH },
	{ "cab",     APPLICATION_X_SHOCKWAVE_FLASH },
	{ "tcl",     APPLICATION_X_TCL },
	{ "tex",     APPLICATION_X_TEX },
	{ "troff",   APPLICATION_X_TROFF_MS },
	{ "au",      AUDIO_BASIC },
	{ "snd",     AUDIO_BASIC },
	{ "stream",  AUDIO_X_QT_STREAM },
	{ "wav",     AUDIO_X_WAV },
	{ "dwf",     DRAWING_X_DWF },
	{ "tiff",    IMAGE_TIFF },
	{ "tif",     IMAGE_TIFF },
	{ "ico",     IMAGE_X_ICON },
	{ "pnm",     IMAGE_X_PORTABLE_ANYMAP },
	{ "pbm",     IMAGE_X_PORTABLE_BITMAP },
	{ "pgm",     IMAGE_X_PORTABLE_GRAYMAP },
	{ "ppm",     IMAGE_X_PORTABLE_PIXMAP },
	{ "rgb",     IMAGE_X_RGB },
	{ "csv",     TEXT_COMMA_SEPARATED_VALUES },
	{ "rtx",     TEXT_RICHTEXT },
	{ "rtf",     TEXT_RTF },
	{ "tsv",     TEXT_TAB_SEPARATED_VALUES },
	{ "qt",      VIDEO_QUICKTIME },
	{ "mov",     VIDEO_QUICKTIME },
	{ "avi",     VIDEO_X_MSVIDEO },
	{ "movie",   VIDEO_X_SGI_MOVIE }
};

void
init_http_header(struct http_header *data)
{
	data->method = RESPONSE;
	data->status = _200_OK;
	data->url = NULL;
	data->host = NULL;
	data->content_type = TEXT_PLAIN;
	data->boundary = NULL;
	data->content_length = 0;
	data->range.from = 0;
	data->range.to = 0;
	data->range.size = 0;
	data->flags.keep_alive = 0;
	data->flags.range = 0;
	data->flags.http = 0;
}

int
parse_header(struct http_header *data, int sock)
{
	char *line;
	int err;
	int i;
	size_t length;
	int table_size;

	struct http_keyword {
		char *key;
		int (*parse) (struct http_header *data, char *);
	};

	static struct http_keyword parse_table[] = {
		{ "GET ",             parse_GET },
		{ "POST ",            parse_POST },
		{ "Host: ",           parse_host },
		{ "Connection: ",     parse_connection },
		{ "Range: ",          parse_range },
		{ "Content-Type: ",   parse_content_type },
		{ "Content-Length: ", parse_content_length }
	};

	table_size = (int) sizeof(parse_table) / sizeof(struct http_keyword);

	line = NULL;
	err = get_line(sock, &line);
	if (err) {
		return err;
	}

	while (strlen(line) != 0) {
		for (i = 0; i < table_size; i++) {
			length = strlen(parse_table[i].key);
			if (strncmp(parse_table[i].key, line, length) == 0) {
				err = parse_table[i].parse(data,
					  line + length);
				if (err) {
					delete_http_header(data);
					return err;
				}
			}
		}
		free(line);
		line = NULL;
		err = get_line(sock, &line);
		if (err) {
			delete_http_header(data);
			return err;
		}
	}
	free(line);
	line = NULL;

	return STAT_OK;
}

void
delete_http_header(struct http_header *data)
{
	if (data->url) {
		free(data->url);
		data->url = NULL;
	}
	if (data->host) {
		free(data->host);
		data->host = NULL;
	}
	if (data->boundary) {
		free(data->boundary);
		data->boundary = NULL;
	}
}

enum mime
get_mime_type(const char *file_name)
{
	char *type;
	size_t i;
	size_t elements;

	type = strrchr(file_name, '.');
	if (!type|| strlen(type) == 1) {
		return APPLICATION_OCTET_STREAM;
	}
	type = type + 1; // move over dot

	if (!type|| !strcmp(type, "")) {
		return APPLICATION_OCTET_STREAM;
	}

	elements = sizeof(mime_type_mapper) / sizeof(struct mime_map);
	for (i = 0; i < elements; i++) {
		if (!strcmp(type, mime_type_mapper[i].ending)) {
			return mime_type_mapper[i].type;
		}
	}

	return TEXT_PLAIN;
}

/*
 * gets a line from socket, and writes it to buffer
 * buffer will be err_malloc'ed by get_line.
 * returns STAT_OK CLOSED_CON or HTTP_HEAD_LINE_EXT
 * buff will be free'd and set to NULL on error
 * buffer will always be terminated with \0
 */
static int
get_line(int sock, char **buff)
{
	size_t buf_size;
	size_t bytes_read;
	ssize_t err;
	char cur_char;

	buf_size = 256;
	*buff = (char *)err_malloc(buf_size);
	memset(*buff, '\0', buf_size);

	for (bytes_read = 0 ;; bytes_read++) {
		err = recv(sock, &cur_char, (size_t)1, 0);
		if (err < 0 || err == 0) {
			free(*buff);
			*buff = NULL;
			return CLOSED_CON;
		}

		if (bytes_read >= buf_size) {
			buf_size += 128;
			if (buf_size > HTTP_HEADER_LINE_LIMIT) {
				free(*buff);
				*buff = NULL;
				return HTTP_HEAD_LINE_EXT;
			}
			*buff = err_realloc((*buff), buf_size);
			memset(*buff + buf_size - 128, '\0', (size_t)128);
		}

		if (cur_char == '\n') {
			break;
		}

		if (cur_char == '\r') {
			continue;
		}

		(*buff)[bytes_read] = cur_char;
	}

	return STAT_OK;
}

static int
parse_GET(struct http_header *data, char *line)
{
	data->method = GET;
	return parse_url(data, line);
}

static int
parse_POST(struct http_header *data, char *line)
{
	data->method = POST;
	return parse_url(data, line);
}

static int
parse_url(struct http_header *data, char *line)
{
	char *cur;
	size_t length;
	size_t url_pos;
	size_t i;
	char *str_tok_ident;

	cur = strtok_r(line, " ", &str_tok_ident);
	length = strlen(line);
	cur = strtok_r(NULL, " ", &str_tok_ident);
	if (cur) {
		/* space found -> check for "HTTP/1.1" */
		data->flags.http = 1;
		if ((memcmp(cur, "HTTP/1.1", (size_t)8) != 0)
		    && (memcmp(cur, "HTTP/1.0", (size_t)8) != 0)) {
			return INV_GET;
		}
	}

	/* in case / */
	if (length == 1 && line[0] == '/') {
		data->url = err_malloc(2);
		memcpy(data->url, "/\0", 2);

		return STAT_OK;
	}

	/* remove trailing / */
	if (line[length - 1] == '/') {
		line[length - 1] = '\0';
		length = length - 1;
	}

	data->url = err_malloc(length + 1);

	/* go over request and replace http codes */
	for (i = 0, url_pos = 0; i < length; i++, url_pos++) {
		if (*(line + i) != '%') {
			data->url[url_pos] = line[i];
			continue;
		}
		/* check for encoding */
		if (memcmp(line + i, "%20", (size_t)3) == 0) {
			data->url[url_pos] = ' ';
			i += 2;
		} else if (memcmp(line + i, "%21", (size_t)3) == 0) {
			data->url[url_pos] = '!';
			i += 2;
		} else if (memcmp(line + i, "%23", (size_t)3) == 0) {
			data->url[url_pos] = '#';
			i += 2;
		} else if (memcmp(line + i, "%24", (size_t)3) == 0) {
			data->url[url_pos] = '$';
			i += 2;
		} else if (memcmp(line + i, "%25", (size_t)3) == 0) {
			data->url[url_pos] = '%';
			i += 2;
		} else if (memcmp(line + i, "%26", (size_t)3) == 0) {
			data->url[url_pos] = '&';
			i += 2;
		} else if (memcmp(line + i, "%27", (size_t)3) == 0) {
			data->url[url_pos] = '\'';
			i += 2;
		} else if (memcmp(line + i, "%28", (size_t)3) == 0) {
			data->url[url_pos] = '(';
			i += 2;
		} else if (memcmp(line + i, "%29", (size_t)3) == 0) {
			data->url[url_pos] = ')';
			i += 2;
		} else if (memcmp(line + i, "%2B", (size_t)3) == 0) {
			data->url[url_pos] = '+';
			i += 2;
		} else if (memcmp(line + i, "%2C", (size_t)3) == 0) {
			data->url[url_pos] = ',';
			i += 2;
		} else if (memcmp(line + i, "%2D", (size_t)3) == 0) {
			data->url[url_pos] = '-';
			i += 2;
		} else if (memcmp(line + i, "%2E", (size_t)3) == 0) {
			data->url[url_pos] = '.';
			i += 2;
		} else if (memcmp(line + i, "%3B", (size_t)3) == 0) {
			data->url[url_pos] = ';';
			i += 2;
		} else if (memcmp(line + i, "%3D", (size_t)3) == 0) {
			data->url[url_pos] = '=';
			i += 2;
		} else if (memcmp(line + i, "%40", (size_t)3) == 0) {
			data->url[url_pos] = '@';
			i += 2;
		} else if (memcmp(line + i, "%5B", (size_t)3) == 0) {
			data->url[url_pos] = '[';
			i += 2;
		} else if (memcmp(line + i, "%5D", (size_t)3) == 0) {
			data->url[url_pos] = ']';
			i += 2;
		} else if (memcmp(line + i, "%5E", (size_t)3) == 0) {
			data->url[url_pos] = '^';
			i += 2;
		} else if (memcmp(line + i, "%5F", (size_t)3) == 0) {
			data->url[url_pos] = '_';
			i += 2;
		} else if (memcmp(line + i, "%60", (size_t)3) == 0) {
			data->url[url_pos] = '`';
			i += 2;
		} else if (memcmp(line + i, "%7B", (size_t)3) == 0) {
			data->url[url_pos] = '{';
			i += 2;
		} else if (memcmp(line + i, "%7D", (size_t)3) == 0) {
			data->url[url_pos] = '}';
			i += 2;
		} else if (memcmp(line + i, "%7E", (size_t)3) == 0) {
			data->url[url_pos] = '~';
			i += 2;
		} else {
			data->url[url_pos] = line[i];
		}
	}

	data->url[url_pos] = '\0';

	return STAT_OK;
}

static int
parse_connection(struct http_header *data, char *line)
{
	size_t length;

	length = strlen(line);
	if (!length) {
		return INV_CONNECTION;
	}

	if (length == 5 && !memcmp(line, "close", 5)) {
		data->flags.keep_alive = 0;
	} else if (length == 10 && !memcmp(line, "keep-alive", 10)) {
		data->flags.keep_alive = 1;
	} else {
		return INV_CONNECTION;
	}

	return STAT_OK;
}

static int
parse_host(struct http_header *data, char *line)
{
	size_t length;

	length = strlen(line);
	if (!length) {
		return INV_HOST;
	}
	data->host = err_malloc(length + 1);
	memcpy(data->host, line, length + 1);

	return STAT_OK;
}

static int
parse_range(struct http_header *data, char *line)
{
	char *tmp;
	char *str_tok_ident;

	data->flags.range = 1;

	tmp = line;
	if (memcmp(tmp, "bytes=", (size_t)6) != 0) {
		return INV_RANGE;
	}
	tmp += 6;
	str_tok_ident = NULL;

	/* from */
	tmp = strtok_r(tmp, "-", &str_tok_ident);
	if (!tmp) {
		return INV_RANGE;
	}
	if (tmp[0] == '0' && tmp[1] == '\0') {
		data->range.from = 0;
	} else {
		data->range.from = err_string_to_val(tmp);
		if (data->range.from == 0) {
			return INV_RANGE;
		}
	}

	/* to */
	tmp = strtok_r(NULL, "/", &str_tok_ident);
	if (tmp) {
		data->range.to = err_string_to_val(tmp);
		if (data->range.to == 0) {
			return INV_RANGE;
		}
	} else {
		return INV_RANGE;
	}

	/* size */
	tmp = strtok_r(NULL, "\r\n", &str_tok_ident);
	if (tmp) {
		if (strlen(tmp) == 1 && tmp[0] == '*') {
			/* TODO: if * is found transfer is streaming */
			data->range.size = 0;
		} else {
			data->range.size = err_string_to_val(tmp);
			if (data->range.size == 0) {
				return INV_RANGE;
			}
		}
	} else {
		return INV_RANGE;
	}

	return STAT_OK;
}

static int
parse_content_length(struct http_header *data, char *line)
{
	data->content_length = err_string_to_val(line);

	if (data->content_length == 0) {
		return FILESIZE_ZERO;
	}

	return STAT_OK;
}

static int
parse_content_type(struct http_header *data, char *line)
{
	char *tmp;
	size_t length;

	/* TODO: this will for now just support multipar/form-data */
	tmp = line;
	if (memcmp(tmp, "multipar/form-data", (size_t)19) != 0) {
		return INV_CONTENT_TYPE;
	}
	data->content_type = MULTIPART_FORM_DATA;
	tmp += 19;
	if (memcmp(tmp, "; boundary=", (size_t)11) != 0) {
		return BOUNDARY_MISSING;
	}
	tmp += 11;
	length = strlen(tmp);
	if (length < 1) {
		return BOUNDARY_MISSING;
	}
	data->boundary = err_malloc(length + 1);
	memcpy(data->boundary, tmp, length + 1);

	return STAT_OK;
}

/*
 * Creates a Http Header out of given header struct
 * returns allocated string.
 */
char *
print_header(struct http_header *data)
{
	char *header;
	char *buf;

	static char *http_method_str[] = {
		"response",
		"GET",
		"POST"
	};

	struct status_map {
		int id;
		char *msg;
	};

	/* maps message and id to enum http_status */
	static struct status_map status_table[] = {
		{ 200, "OK",                 }, /* _200_OK */
		{ 201, "Created",            }, /* _201_Created */
		{ 206, "Partial Content",    }, /* _206_Partial_Content */
		{ 403, "Forbidden",          }, /* _403_Forbidden */
		{ 404, "Not Found",          }, /* _404_Not_Found */
		{ 405, "Method Not Allowed", }  /* _405_Method_Not_Allowed */
	};

	header = NULL;
	buf = err_malloc(HTTP_HEADER_LINE_LIMIT);

	if (data->method == RESPONSE) {
		/* http reponse */
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "HTTP/1.1 %d %s\r\n",
		    status_table[data->status].id,
		    status_table[data->status].msg);
		header = concat(header, buf);
	} else {
		/* http request (POST or GET) */
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "%s %s%s\r\n",
		    http_method_str[data->method], data->url,
		    data->flags.http ? " HTTP/1.1" : "");
		header = concat(header, buf);
	}

	if (data->host) {
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "Host: %s\r\n",
		    data->host);
		header = concat(header, buf);
	}

	header = concat(header, "Accept-Ranges: bytes\r\n");

	if (data->flags.keep_alive) {
		header = concat(header, "Connection: keep-alive\r\n");
	} else {
		header = concat(header, "Connection: close\r\n");
	}

	if (data->content_type) {
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "Content_Type: %s",
		    mime_string[data->content_type]);
		header = concat(header, buf);

		if (data->boundary) {
			snprintf(buf, HTTP_HEADER_LINE_LIMIT,
			    "; boundary=%s\r\n",
			    data->boundary);
			header = concat(header, buf);
		} else {
			header = concat(header, "\r\n");
		}
	}

	if (data->content_length) {
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "Content-Length: %" PRId64 "\r\n",
		    data->content_length);
		header = concat(header, buf);
	}

	if (data->flags.range) {
		snprintf(buf, HTTP_HEADER_LINE_LIMIT,
		    "Content-Range: "
			"bytes=%" PRId64 "-%" PRId64 "/%" PRId64 "\r\n",
		    data->range.from,
		    data->range.to,
		    data->range.size);
		header = concat(header, buf);
	}
	header = concat(header, "\r\n");
	free(buf);

	return header;
}
