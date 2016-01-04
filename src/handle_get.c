#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <inttypes.h>

#include "globals.h"
#include "handle_request.h"
#include "handle_get.h"
#include "defines.h"
#include "helper.h"
#include "msg.h"
#include "file_list.h"

static void generate_response_header(struct http_header *,
    struct http_header *);
static enum http_status get_response_status(char **);

int
handle_get(int msg_id, int sock, struct http_header *request, int upload)
{
	char message_buffer[MSG_BUFFER_SIZE];
	enum err_status error;
	char fmt_size[7];
	struct http_header response;
	char *filename;
	struct stat sb;
	uint64_t *written;
	char *tmp;

	filename = NULL;
	error = STAT_OK;

	init_http_header(&response);
	generate_response_header(request, &response);

	switch (response.status) {
	case _200_OK:
	case _206_Partial_Content:
		filename = concat(concat(filename, ROOT_DIR), request->url);

		/* in case its a directory */
		if (response.status == _200_OK && is_directory(filename)) {
			char *body;

			free(filename);
			response.content_type = TEXT_HTML;
			body = dir_to_table(response.flags.http, request->url,
				   upload);
			response.content_length = strlen(body);
			if (request->flags.http) {
				char *header;

				header = print_header(&response);
				error = send_data(sock, header,
					    strlen(header));
				free(header);
				if (error) {
					free(body);
					return error;
				}
			}
			error = send_data(sock, body, response.content_length);
			free(body);
			if (error) {
				return error;
			}
			format_size(response.content_length, fmt_size);
			snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
			    "%s tx: %s",
			    fmt_size, request->url);

			break;
		}

		/* in case its a file */

		if (stat(filename, &sb) == -1) {
			die(ERR_INFO, "stat()");
		}

		switch (response.status) {
		case _200_OK:
			response.content_length = (uint64_t)sb.st_size;
			tmp = "tx";
			break;
		case _206_Partial_Content:
			if (response.range.to == 0) {
				response.range.to = (uint64_t)(sb.st_size - 1);
			}
			if (response.range.size == 0) {
				response.range.size = (uint64_t)sb.st_size;
			}
			response.content_length = response.range.to
						- response.range.from + 1;
			response.range.size = (uint64_t)sb.st_size;
			tmp = "px";
			break;
		default:
			tmp = "er";
			/* not reached */
			break;
		}

		if (request->flags.http) {
			char *header;

			header = print_header(&response);
			error = send_data(sock, header, strlen(header));
			free(header);
			if (error) {
				free(filename);
				return error;
			}
		}
		written = msg_hook_new_transfer(msg_id, request->url,
			      response.content_length, tmp);

		switch (response.status) {
		case _200_OK:
			error = send_file(sock, filename, written, 0,
				    response.content_length - 1);
			break;
		case _206_Partial_Content:
			error = send_file(sock, filename, written,
				    response.range.from, response.range.to);
			break;
		default:
			/* not reached */
			break;
		}
		free(filename);
		if (error) {
			return error;
		}

		format_size(response.content_length, fmt_size);
		snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE,
		    "%s tx: %s",
		    fmt_size, request->url);
		break;
	case _403_Forbidden:
	case _404_Not_Found:
	case _405_Method_Not_Allowed:
		switch (response.status) {
		case _403_Forbidden:
			response.content_length = strlen(RESPONSE_403);
			break;
		case _404_Not_Found:
			response.content_length = strlen(RESPONSE_404);
			break;
		case _405_Method_Not_Allowed:
			response.content_length = strlen(RESPONSE_405);
			break;
		default:
			/* not reached */
			break;
		}

		if (request->flags.http) {
			char *header;

			header = print_header(&response);
			error = send_data(sock, header, strlen(header));
			free(header);
			if (error) {
				return error;
			}
		}

		switch (response.status) {
		case _403_Forbidden:
			error = send_data(sock, RESPONSE_403,
				    response.content_length);
			tmp = "403";
			break;
		case _404_Not_Found:
			error = send_data(sock, RESPONSE_404,
				    response.content_length);
			tmp = "404";
			break;
		case _405_Method_Not_Allowed:
			error = send_data(sock, RESPONSE_404,
				    response.content_length);
			tmp = "405";
			break;
		default:
			/* not reached */
			tmp = "-";
			break;
		}
		if (error) {
			return error;
		}

		snprintf(message_buffer, (size_t)MSG_BUFFER_SIZE, "err %s",
		    tmp);
		break;
	default:
		/* not reached */
		break;
	}

	msg_print_log(msg_id, FINISHED, message_buffer);

	return STAT_OK;
}

static void
generate_response_header(struct http_header *request,
    struct http_header *response)
{
	response->method = RESPONSE;
	response->status = get_response_status(&request->url);
	response->content_type = get_mime_type(request->url);

	if (request->flags.range) {
		if (response->status == _200_OK) {
			response->status = _206_Partial_Content;
		}
		response->flags.range = 1;
		response->range.from = request->range.from;
		response->range.to = request->range.to;
		response->range.size = request->range.size;
	}

	if (request->flags.keep_alive) {
		response->flags.keep_alive = 1;
	}

	if (request->flags.http) {
		response->flags.http = 1;
	}
}

static enum http_status
get_response_status(char **request)
{
	char *full_requested_path;
	char *requested_path;
	char *free_me;
	struct stat s;

	if (!*request) {
		return _404_Not_Found;
	}
	if (memcmp(*request, "/\0", (size_t)2) == 0) {
		return _200_OK;
	}

	full_requested_path = NULL;
	full_requested_path = concat(concat(full_requested_path, ROOT_DIR),
				  *request);

	requested_path = realpath(full_requested_path, NULL);
	free(full_requested_path);
	full_requested_path = NULL;

	/* on gnux/linux this will return NULL on file not found */
	if (!requested_path) {
		return _404_Not_Found;
	}

	/* but not on bsd, so check with stat */
	if (stat(requested_path, &s) != 0) {
		free(requested_path);
		requested_path = NULL;
		return _404_Not_Found;
	}

	/* now check if file starts with ROOT_DIR path */
	if ((memcmp(requested_path, ROOT_DIR, strlen(ROOT_DIR)) != 0)
	    || strlen(ROOT_DIR) > strlen(requested_path)) {
		free(requested_path);
		requested_path = NULL;
		return _403_Forbidden;
	}

	/* ok file exists, check if its a directory */
	if (!(s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG)) {
		die(ERR_INFO, "stat() has no file nor a directory.");
	}

	/* and update request string to a full path */
	free_me = *request;
	*request = NULL;
	*request = concat(*request, requested_path
		         + strlen(ROOT_DIR));
	free(free_me);
	free_me = NULL;

	free(requested_path);
	requested_path = NULL;

	return _200_OK;
}
