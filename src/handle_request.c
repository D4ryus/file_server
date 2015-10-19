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
/* HEADER_LINES_EXT   */ "error - too many headerlines"
};

/*
 * function where each thread starts execution with given client_info
 */
void *
handle_request(void *ptr)
{
	struct client_info *data;
	char *cur_line;
	enum err_status error;
	char con_msg[128];

	data = (struct client_info *)ptr;

	if (data->sock < 0) {
		die(ERR_INFO, "socket in handle_request is < 0");
	}
	snprintf(con_msg, (size_t)128, "connected to port %d.",
			data->port);
	msg_print_log(data, CONNECTED, con_msg);

	error = get_line(data->sock, &cur_line);
	if (error) {
		msg_print_log(data, ERROR, err_msg[error]);
		free(data);
		return NULL;
	}

	data->requested_path = NULL;
	data->size = 0;
	data->written = 0;
	data->last_written = 0;

	if (memcmp(cur_line, "POST", (size_t)4) == 0) {
		if (UPLOAD_ENABLED) {
			data->type = UPLOAD;
			msg_hook_add(data);
			error = handle_post(data, cur_line);
		} else {
			send_405(data->sock, HTTP, &(data->size));
			error = POST_DISABLED;
		}
	} else if (memcmp(cur_line, "GET", (size_t)3) == 0) {
		data->type = DOWNLOAD;
		msg_hook_add(data);
		error = handle_get(data, cur_line);
	} else {
		error = INV_REQ_TYPE;
	}
	free(cur_line);
	cur_line = NULL;

	if (error) {
		msg_print_log(data, ERROR, err_msg[error]);
	}

	shutdown(data->sock, SHUT_RDWR);
	close(data->sock);
	msg_hook_cleanup(data);

	return NULL;
}
