#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <inttypes.h>

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
handle_get(struct client_info *data, struct http_header *http_head)
{
	char message_buffer[MSG_BUFFER_SIZE];
	enum err_status error;
	char fmt_size[7];
	enum response_type res_type;

	error = STAT_OK;

	res_type = get_response_type(&http_head->url);

	if (http_head->range && res_type == FILE_200) {
		res_type = FILE_206;
	}

	switch (res_type) {
	case FILE_200:
		error = send_200_file_head(data->sock, http_head->type, &(data->size),
			    data->requested_path);
		if (error) {
			return error;
		}

		error = send_file(data->sock, data->requested_path,
			    &(data->written), 0, data->size - 1);
		if (!error) {
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
			    "%s sent file: %s",
			    format_size(data->size, fmt_size),
			    data->requested_path);
		}
		break;
	case FILE_206:
		error = send_206_file_head(data->sock, http_head->type, &(data->size),
			    data->requested_path, http_head->range_from, http_head->range_to);
		if (error) {
			return error;
		}

		if (http_head->range_to == 0) {
			http_head->range_to = data->size - 1;
		}
		error = send_file(data->sock, data->requested_path,
			    &(data->written), http_head->range_from, http_head->range_to);
		if (!error) {
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
			    "%s sent partial file: %s",
			    format_size(data->size, fmt_size),
			    data->requested_path);
		}
		break;
	case DIR_200:
		error = send_200_directory(data->sock, http_head->type, &(data->size),
			    data->requested_path);
		if (!error) {
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
			    "%s sent dir: %s",
			    format_size(data->size, fmt_size),
			    data->requested_path);
			data->written = data->size;
		}
		break;
	case TXT_403:
		error = send_403(data->sock, http_head->type, &(data->size));
		if (!error) {
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
			    "sent error 403");
			data->written = data->size;
		}
		break;
	case TXT_404:
		error = send_404(data->sock, http_head->type, &(data->size));
		if (!error) {
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
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
	} else if ((memcmp(requested_path, ROOT_DIR, strlen(ROOT_DIR)) != 0)
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

