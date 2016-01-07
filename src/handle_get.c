#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <inttypes.h>

#include "config.h"
#include "handle_request.h"
#include "handle_get.h"
#include "misc.h"
#include "msg.h"
#include "file_list.h"

static void generate_response_header(struct http_header *,
    struct http_header *);
static enum http_status get_response_status(char **);

/*
 * plaintext message which will be sent on 403 - Forbidden
 */
static const char *RESPONSE_403 =
	"403 - U better not go down this road!\r\n";

/*
 * plaintext message which will be sent on 404 - File not found
 */
static const char *RESPONSE_404 =
	"404 - Watcha pulling here buddy?\r\n";

/*
 * plaintext message which will be sent on 405 - Method Not Allowed
 */
static const char *RESPONSE_405 =
	"405 - ur Method is not allowed sir, if tried to "
	"POST, ask for permission.\r\n";

int
handle_get(int msg_id, int sock, struct http_header *request, int upload)
{
	enum err_status error;
	char fmt_size[7];
	struct http_header response;
	char *filename;
	struct stat sb;
	uint64_t *written;
	char *type;
	char *name;

	filename = NULL;
	error = STAT_OK;

	init_http_header(&response);
	generate_response_header(request, &response);

	filename = concat(filename, 2, CONF.root_dir, request->url);

	/* in case its a directory */
	if (response.status == _200_OK && is_directory(filename)) {
		char *body;

		free(filename);
		filename = NULL;
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
		type = "tx";
		written = msg_hook_new_transfer(msg_id, request->url,
			      response.content_length, type);
		error = send_data(sock, body, response.content_length);
		free(body);
		if (error) {
			return error;
		} else {
			*written = response.content_length;
		}

		format_size(response.content_length, fmt_size);
		msg_print_log(msg_id, 1, "%s %s: %s", fmt_size, type,
		    request->url);

		return STAT_OK;
	}

	/* set resposne.content_length, name and type */
	switch (response.status) {
	case _200_OK:
		check(stat(filename, &sb) == -1, "stat(\"%s\") returned -1",
		    filename);
		response.content_length = (uint64_t)sb.st_size;
		name = request->url;
		type = "tx";
		break;
	case _206_Partial_Content:
		check(stat(filename, &sb) == -1, "stat(\"%s\") returned -1",
		    filename);
		if (response.range.to == 0) {
			response.range.to = (uint64_t)(sb.st_size - 1);
		}
		if (response.range.size == 0) {
			response.range.size = (uint64_t)sb.st_size;
		}
		response.content_length = response.range.to
					- response.range.from + 1;
		response.range.size = (uint64_t)sb.st_size;
		name = request->url;
		type = "px";
		break;
	case _403_Forbidden:
		response.content_length = strlen(RESPONSE_403);
		name = "403";
		type = "tx";
		break;
	case _404_Not_Found:
		response.content_length = strlen(RESPONSE_404);
		name = "404";
		type = "tx";
		break;
	case _405_Method_Not_Allowed:
		response.content_length = strlen(RESPONSE_405);
		name = "405";
		type = "tx";
		break;
	default:
		/* not reached */
		die("this should not happen, resposne.status: %d",
		    response.status);
		break;
	}

	if (request->flags.http) {
		char *header;

		header = print_header(&response);
		error = send_data(sock, header, strlen(header));
		free(header);
		if (error) {
			free(filename);
			filename = NULL;
			return error;
		}
	}

	written = msg_hook_new_transfer(msg_id, name,
		      response.content_length, type);

	/* send data */
	switch (response.status) {
	case _200_OK:
		error = send_file(sock, filename, written, (uint64_t)0,
			    response.content_length - 1);
		break;
	case _206_Partial_Content:
		error = send_file(sock, filename, written,
			    response.range.from, response.range.to);
		break;
	case _403_Forbidden:
		error = send_data(sock, RESPONSE_403,
			    response.content_length);
		break;
	case _404_Not_Found:
		error = send_data(sock, RESPONSE_404,
			    response.content_length);
		break;
	case _405_Method_Not_Allowed:
		error = send_data(sock, RESPONSE_404,
			    response.content_length);
		break;
	default:
		die("this should not happen, resposne.status: %d",
		    response.status);
		break;
	}

	free(filename);
	filename = NULL;

	if (error) {
		return error;
	} else {
		/* if there was no error set *written on 403, 404 and 405,
		 * on 200 and 206 it will be set by send_file() */
		switch (response.status) {
		case _403_Forbidden:
		case _404_Not_Found:
		case _405_Method_Not_Allowed:
			*written = response.content_length;
		default:
			break;
		}
	}

	format_size(response.content_length, fmt_size);
	msg_print_log(msg_id, 1, "%s %s: %s",
	    fmt_size, type, name);

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

	full_requested_path = concat(NULL, 2, CONF.root_dir, *request);

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

	/* now check if file starts with CONF.root_dir path */
	if ((memcmp(requested_path, CONF.root_dir, strlen(CONF.root_dir)) != 0)
	    || strlen(CONF.root_dir) > strlen(requested_path)) {
		free(requested_path);
		requested_path = NULL;
		return _403_Forbidden;
	}

	/* ok file exists, check if its a directory */
	check(!(s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG),
	    "stat(\"%s\") is not a file nor a directory.", requested_path)

	/* and update request string to a full path */
	free_me = *request;
	*request = concat(NULL, 1, requested_path + strlen(CONF.root_dir));
	free(free_me);
	free_me = NULL;

	free(requested_path);
	requested_path = NULL;

	return _200_OK;
}
