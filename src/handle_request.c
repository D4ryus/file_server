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
extern char *IP;
extern char *ROOT_DIR;
extern char *UPLOAD_DIR;
extern int UPLOAD_ENABLED;
extern uint8_t VERBOSITY;
extern int UPLOAD_ENABLED;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

void
init_client_info(struct client_info *data)
{
	data->size = 0;
	data->written = 0;
	data->last_written = 0;
	data->requested_path = NULL;
	data->type = PLAIN;
}

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

	init_client_info(data);

	/* check if ip is blocked */
	if (strlen(IP) > 0 &&
	    memcmp(IP, data->ip, strlen(IP))) {
		shutdown(data->sock, SHUT_RDWR);
		close(data->sock);
		msg_print_log(data, ERROR, "ip blocked");
		msg_hook_cleanup(data);

		return NULL;
	}

	error = parse_header(&http_head, data->sock);
	if (error) {
		shutdown(data->sock, SHUT_RDWR);
		close(data->sock);
		msg_print_log(data, ERROR, get_err_msg(error));
		msg_hook_cleanup(data);

		return NULL;
	}

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
		msg_print_log(data, ERROR, get_err_msg(error));
	}

	delete_http_header(&http_head);
	shutdown(data->sock, SHUT_RDWR);
	close(data->sock);
	msg_hook_cleanup(data);

	return NULL;
}
