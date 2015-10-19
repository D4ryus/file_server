#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

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

int
parse_header(struct http_header *data, int sock)
{
	char *line;
	int err;
	int i;
	size_t length;
	int table_size;

	line = NULL;
	err = get_line(sock, &line);
	if (err) {
		return err;
	}

	table_size = (int) sizeof(parse_table) / sizeof(struct http_keyword);
	for (i = 0; i < table_size; i++) {
		length = strlen(parse_table[i].key);
		if (strncmp(parse_table[i].key, line, length) == 0) {
			err = parse_table[i].fn(data, line + length);
			if (err) {
				return err;
			}
		}
	}

	if (data->flags.S.missing) {
		return INV_REQ_TYPE;
	}

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
	return STAT_OK;
}

int
_parse_POST(struct http_header *data, char *line)
{
	return STAT_OK;
}

int
_parse_host(struct http_header *data, char *line)
{
	return STAT_OK;
}

int
_parse_range(struct http_header *data, char *line)
{
	return STAT_OK;
}

int
_parse_boundary(struct http_header *data, char *line)
{
	return STAT_OK;
}

int
_parse_content_length(struct http_header *data, char *line)
{
	return STAT_OK;
}

int
_parse_content_type(struct http_header *data, char *line)
{
	return STAT_OK;
}

