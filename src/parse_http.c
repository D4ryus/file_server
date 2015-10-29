#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <inttypes.h>

#include "parse_http.h"
#include "helper.h"
#include "defines.h"

struct http_keyword parse_table[] = {
	{ "GET ",             _parse_GET },
	{ "POST ",            _parse_POST },
	{ "Host: ",           _parse_host },
	{ "Range: ",          _parse_range },
	{ "Content-Type: ",   _parse_content_type },
	{ "Content-Length: ", _parse_content_length }
};

void
init_http_header(struct http_header *data)
{
	data->method = MISSING;
	data->type = HTTP;
	data->url = NULL;
	data->host = NULL;
	data->range_from = 0;
	data->range_to = 0;
	data->content_length = 0;
	data->boundary = NULL;
	data->range = 0;
}

int
parse_header(struct http_header *data, int sock)
{
	char *line;
	int err;
	int i;
	size_t length;
	int table_size;

	init_http_header(data);
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
				err = parse_table[i].fn(data, line + length);
				if (err) {
					delete_http_header(data);
					return err;
				}
			}
		}
		free(line);
		err = get_line(sock, &line);
		if (err) {
			delete_http_header(data);
			return err;
		}
	}
	free(line);

	return STAT_OK;
}

/*
 * gets a line from socket, and writes it to buffer
 * buffer will be err_malloc'ed by get_line.
 * returns STAT_OK CLOSED_CON or HTTP_HEAD_LINE_EXT
 * buff will be free'd and set to NULL on error
 * buffer will always be terminated with \0
 */
int
get_line(int sock, char **buff)
{
	size_t buf_size;
	size_t bytes_read;
	ssize_t err;
	char cur_char;

	buf_size = 256;
	(*buff) = (char *)err_malloc(buf_size);
	memset((*buff), '\0', buf_size);

	for (bytes_read = 0 ;; bytes_read++) {
		err = recv(sock, &cur_char, (size_t)1, 0);
		if (err < 0 || err == 0) {
			free((*buff));
			(*buff) = NULL;
			return CLOSED_CON;
		}

		if (bytes_read >= buf_size) {
			buf_size += 128;
			if (buf_size > HTTP_HEADER_LINE_LIMIT) {
				free((*buff));
				(*buff) = NULL;
				return HTTP_HEAD_LINE_EXT;
			}
			(*buff) = err_realloc((*buff), buf_size);
			memset((*buff) + buf_size - 128, '\0', (size_t)128);
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

int
_parse_GET(struct http_header *data, char *line)
{
	char *cur;
	size_t length;
	size_t url_pos;
	size_t i;
	char *str_tok_ident;

	data->method = GET;

	cur = strtok_r(line, " ", &str_tok_ident);
	length = strlen(line);
	cur = strtok_r(NULL, " ", &str_tok_ident);
	if (!cur) {
		/* if NULL -> no space is found -> missing HTTP/1.1 -> plain */
		data->type = PLAIN;
	} else {
		/* space found -> check for "HTTP/1.1" */
		data->type = HTTP;
		if ((memcmp(cur, "HTTP/1.1", (size_t)8) != 0)
		    && (memcmp(cur, "HTTP/1.0", (size_t)8) != 0)) {
			return INV_GET;
		}
	}

	/* remove trailing / */
	if (length > 1 && line[length - 1] == '/') {
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

int
_parse_POST(struct http_header *data, char *line)
{
	data->method = POST;

	if (memcmp(line, "/ HTTP/1.1", (size_t)8) != 0 &&
	    memcmp(line, "/ HTTP/1.0", (size_t)8) != 0) {
		return INV_POST;
	}

	return STAT_OK;
}

int
_parse_host(struct http_header *data, char *line)
{
	size_t length;

	length = strlen(line);
	if (length) {
		data->host = err_malloc(length + 1);
		memcpy(data->host, line, length + 1);
	}
	return STAT_OK;
}

int
_parse_range(struct http_header *data, char *line)
{
	char *tmp;
	char *str_tok_ident;

	tmp = line;
	if (memcmp(tmp, "bytes=", (size_t)6) != 0) {
		return INV_RANGE;
	}
	tmp += 6;
	str_tok_ident = NULL;
	tmp = strtok_r(tmp, "-", &str_tok_ident);
	if (!tmp) {
		return INV_RANGE;
	}
	if (tmp[0] == '0' && tmp[1] == '\0') {
		data->range_from = 0;
	} else {
		data->range_from = err_string_to_val(tmp);
		if (data->range_from == 0) {
			return INV_RANGE;
		}
	}

	tmp = strtok_r(NULL, "-", &str_tok_ident);
	if (tmp) {
		data->range_to = err_string_to_val(tmp);
		if (data->range_to == 0) {
			return INV_RANGE;
		}
	}

	data->range = 1;

	return STAT_OK;
}

int
_parse_content_length(struct http_header *data, char *line)
{
	data->content_length = err_string_to_val(line);

	if (data->content_length == 0) {
		return FILESIZE_ZERO;
	}

	return STAT_OK;
}

int
_parse_content_type(struct http_header *data, char *line)
{
	char *tmp;
	size_t length;

	tmp = line;
	if (memcmp(tmp, "multipart/form-data; ", (size_t)21) != 0) {
		return INV_CONTENT_TYPE;
	}
	tmp += 21;
	if (memcmp(tmp, "boundary=", (size_t)9) != 0) {
		return BOUNDARY_MISSING;
	}
	tmp += 9;
	length = strlen(tmp);
	if (length < 1) {
		return BOUNDARY_MISSING;
	}
	data->boundary = err_malloc(length + 1);
	memcpy(data->boundary, tmp, length + 1);

	return STAT_OK;
}

void
_debug_print_header(FILE *fd, struct http_header *data)
{
	static char *http_method_str[] = {
		"missing",
		"GET",
		"POST"
	};

	char *http_type_str[] = {
		"http",
		"plain"
	};

	fprintf(fd, "method: %s\n", http_method_str[data->method]);
	fprintf(fd, "type: %s\n", http_type_str[data->type]);
	fprintf(fd, "url: %s\n", data->url ? data->url : "null");
	fprintf(fd, "host: %s\n", data->host ? data->host : "null");
	fprintf(fd, "range_from: %" PRId64 "\n", data->range_from);
	fprintf(fd, "range_to: %" PRId64 "\n", data->range_to);
	fprintf(fd, "content_length: %" PRId64 "\n", data->content_length);
	fprintf(fd, "boundary: %s\n", data->boundary ? data->boundary : "null");
}

void
delete_http_header(struct http_header *data)
{
	data->method = MISSING;
	data->type = HTTP;
	if (data->url) {
		free(data->url);
		data->url = NULL;
	}
	if (data->host) {
		free(data->host);
		data->host = NULL;
	}
	data->range_from = 0;
	data->range_to = 0;
	data->content_length = 0;
	if (data->boundary) {
		free(data->boundary);
		data->boundary = NULL;
	}
}
