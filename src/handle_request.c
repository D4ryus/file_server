#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdint.h>

#include "handle_post.h"
#include "handle_get.h"
#include "defines.h"
#include "helper.h"
#include "handle_request.h"
#include "http_response.h"
#include "msg.h"
#include "parse_http.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern char *UPLOAD_DIR;
extern int UPLOAD_ENABLED;
extern uint8_t VERBOSITY;
extern int UPLOAD_ENABLED;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

/*
 * corresponding error messages to err_status, see types.h
 */
const char *err_msg[] =
{
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
/* INV_RANGE          */ "error - invalid Range"
};

/*
 * function where each thread starts execution with given client_info
 */
void *
handle_request(void *ptr)
{
	struct client_info *data;
	enum err_status error;
	char con_msg[128];
	struct http_header http_head;
	size_t length;

	data = (struct client_info *)ptr;

	if (data->sock < 0) {
		die(ERR_INFO, "socket in handle_request is < 0");
	}
	snprintf(con_msg, (size_t)128, "connected to port %d.",
			data->port);
	msg_print_log(data, CONNECTED, con_msg);

	error = parse_header(&http_head, data->sock);
	if (error) {
		shutdown(data->sock, SHUT_RDWR);
		close(data->sock);
		msg_print_log(data, ERROR, err_msg[error]);
		msg_hook_cleanup(data);

		return NULL;
	}

	data->written = 0;
	data->last_written = 0;

	switch (http_head.method) {
	case MISSING:
		error = INV_REQ_TYPE;
		break;
	case GET:
		if (http_head.range) {
			data->type = PARTIAL;
		} else {
			data->type = DOWNLOAD;
		}
		length = strlen(http_head.url);
		data->requested_path = err_malloc(length + 1);
		memcpy(data->requested_path, http_head.url, length + 1);
		msg_hook_add(data);
		error = handle_get(data, &http_head);
		break;
	case POST:
		if (!UPLOAD_ENABLED) {
			error = POST_DISABLED;
			break;
		}
		data->type = UPLOAD;
		msg_hook_add(data);
		error = handle_post(data, &http_head);
		break;
	default:
		/* not reached */
		break;
	}
	if (error) {
		msg_print_log(data, ERROR, err_msg[error]);
	}

	delete_http_header(&http_head);
	shutdown(data->sock, SHUT_RDWR);
	close(data->sock);
	msg_hook_cleanup(data);

	return NULL;
}
