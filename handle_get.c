#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "handle_request.h"
#include "handle_get.h"
#include "defines.h"
#include "helper.h"
#include "msg.h"
#include "http_response.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;

int
handle_get(struct client_info *data, char *request)
{
	char message_buffer[MSG_BUFFER_SIZE];
	enum err_status error;
	char fmt_size[7];
	enum response_type res_type;
	enum request_type req_type;
	char *requ_path_tmp;

	error = STAT_OK;

	requ_path_tmp = NULL;
	error = parse_get(request, &(req_type), &(requ_path_tmp));
	if (error) {
		return error;
	}

	res_type = get_response_type(&(requ_path_tmp));

	if (res_type == FILE_200  || res_type == DIR_200) {
		data->requested_path = requ_path_tmp;
		requ_path_tmp = NULL;
	}

	switch (res_type) {
		case FILE_200:
			error = send_200_file_head(data->socket,
				    req_type,
				    &(data->size),
				    data->requested_path);
			if (error) {
				return error;
			}

			error = send_file(data->socket, data->requested_path,
				    &(data->written));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "%s sent file: %s",
				    format_size(data->size, fmt_size),
				    data->requested_path);
			}
			break;
		case DIR_200:
			error = send_200_directory(data->socket,
				    req_type,
				    &(data->size),
				    data->requested_path);
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "%s sent dir: %s",
				    format_size(data->size, fmt_size),
				    data->requested_path);
				data->written = data->size;
			}
			break;
		case TXT_403:
			error = send_403(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "sent error 403");
				data->written = data->size;
			}
			break;
		case TXT_404:
			error = send_404(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "sent error 404");
				data->written = data->size;
			}
			break;
		default:
			/* not readched */
			break;
	}

	if (error) {
		return error;
	}
	msg_print_log(data, FINISHED, message_buffer);

	return STAT_OK;
}

/*
 * parses given request line
 * allocs given *requested_path with size of requested filename
 * if negative number is returned, error occured
 * STAT_OK: everything went fine.
 * INV_GET: parse error
 */
int
parse_get(char *request, enum request_type *req_type, char **requested_path)
{
	char *tmp;
	size_t length;
	size_t i;
	size_t url_pos;

	tmp = strtok(request, "\n"); /* get first line */

	/* check if GET is valid */
	if (tmp == NULL || !starts_with(tmp, "GET /", 5)) {
		return INV_GET;
	}
	tmp = strtok(tmp, " ");		/* split first line */
	tmp = strtok(NULL, " ");	/* get requested url */

	length = strlen(tmp);
	/* remove trailing / from request */
	while (length > 1 && tmp[length - 1] == '/') {
		tmp[length - 1] = '\0';
		length = strlen(tmp);
	}

	(*requested_path) = err_malloc(length + 1);
	memset((*requested_path), '\0', length + 1);

	for (i = 0, url_pos = 0; i < length; i++, url_pos++) {
		if (*(tmp + i) != '%') {
			(*requested_path)[url_pos] = tmp[i];
			continue;
		}
		/* check for requested_path encoding */
		if (starts_with(tmp + i, "%20", 3)) {
			(*requested_path)[url_pos] = ' ';
			i += 2;
		} else if (starts_with(tmp + i, "%21", 3)) {
			(*requested_path)[url_pos] = '!';
			i += 2;
		} else if (starts_with(tmp + i, "%23", 3)) {
			(*requested_path)[url_pos] = '#';
			i += 2;
		} else if (starts_with(tmp + i, "%24", 3)) {
			(*requested_path)[url_pos] = '$';
			i += 2;
		} else if (starts_with(tmp + i, "%25", 3)) {
			(*requested_path)[url_pos] = '%';
			i += 2;
		} else if (starts_with(tmp + i, "%26", 3)) {
			(*requested_path)[url_pos] = '&';
			i += 2;
		} else if (starts_with(tmp + i, "%27", 3)) {
			(*requested_path)[url_pos] = '\'';
			i += 2;
		} else if (starts_with(tmp + i, "%28", 3)) {
			(*requested_path)[url_pos] = '(';
			i += 2;
		} else if (starts_with(tmp + i, "%29", 3)) {
			(*requested_path)[url_pos] = ')';
			i += 2;
		} else if (starts_with(tmp + i, "%2B", 3)) {
			(*requested_path)[url_pos] = '+';
			i += 2;
		} else if (starts_with(tmp + i, "%2C", 3)) {
			(*requested_path)[url_pos] = ',';
			i += 2;
		} else if (starts_with(tmp + i, "%2D", 3)) {
			(*requested_path)[url_pos] = '-';
			i += 2;
		} else if (starts_with(tmp + i, "%2E", 3)) {
			(*requested_path)[url_pos] = '.';
			i += 2;
		} else if (starts_with(tmp + i, "%3B", 3)) {
			(*requested_path)[url_pos] = ';';
			i += 2;
		} else if (starts_with(tmp + i, "%3D", 3)) {
			(*requested_path)[url_pos] = '=';
			i += 2;
		} else if (starts_with(tmp + i, "%40", 3)) {
			(*requested_path)[url_pos] = '@';
			i += 2;
		} else if (starts_with(tmp + i, "%5B", 3)) {
			(*requested_path)[url_pos] = '[';
			i += 2;
		} else if (starts_with(tmp + i, "%5D", 3)) {
			(*requested_path)[url_pos] = ']';
			i += 2;
		} else if (starts_with(tmp + i, "%5E", 3)) {
			(*requested_path)[url_pos] = '^';
			i += 2;
		} else if (starts_with(tmp + i, "%5F", 3)) {
			(*requested_path)[url_pos] = '_';
			i += 2;
		} else if (starts_with(tmp + i, "%60", 3)) {
			(*requested_path)[url_pos] = '`';
			i += 2;
		} else if (starts_with(tmp + i, "%7B", 3)) {
			(*requested_path)[url_pos] = '{';
			i += 2;
		} else if (starts_with(tmp + i, "%7D", 3)) {
			(*requested_path)[url_pos] = '}';
			i += 2;
		} else if (starts_with(tmp + i, "%7E", 3)) {
			(*requested_path)[url_pos] = '~';
			i += 2;
		} else {
			(*requested_path)[url_pos] = tmp[i];
		}
	}

	/* get requested type */
	tmp = strtok(NULL, " ");
	/* if none given go with PLAIN and return */
	if (tmp == NULL) {
		(*req_type) = PLAIN;
		return STAT_OK;
	}
	if ((starts_with(tmp, "HTTP/1.1", 8))
	    || (starts_with(tmp, "HTTP/1.0", 8))) {
		(*req_type) = HTTP;
	} else {
		(*req_type) = PLAIN;
	}

	return STAT_OK;
}

/*
 * returns the type of response for GET requests
 * in case its a FILE_200 or DIR_200 the given request
 * is set to evaluated requested path
 */
enum response_type
get_response_type(char **request)
{
	char *full_requested_path;
	char *requested_path;
	enum response_type ret;

	if ((*request) == NULL) {
		return TXT_404;
	}
	if (strcmp((*request), "/") == 0) {
		return DIR_200;
	}

	full_requested_path = NULL;
	full_requested_path = concat(concat(full_requested_path, ROOT_DIR),
				  (*request));

	requested_path = realpath(full_requested_path, NULL);
	if (requested_path == NULL) {
		ret = TXT_404;
	} else if (!starts_with(requested_path, ROOT_DIR, strlen(ROOT_DIR))
	    || strlen(ROOT_DIR) > strlen(requested_path)) {
		ret = TXT_403;
	} else if (is_directory(requested_path)) {
		ret = DIR_200;
	} else {
		ret = FILE_200;
	}

	if (ret == DIR_200 || ret == FILE_200) {
		free((*request));
		(*request) = NULL;
		(*request) = concat((*request), requested_path
			         + strlen(ROOT_DIR));
	}
	if (requested_path != NULL) {
		free(requested_path);
	}
	if (full_requested_path != NULL) {
		free(full_requested_path);
	}

	return ret;
}

