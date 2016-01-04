#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdint.h>

#include "globals.h"
#include "handle_post.h"
#include "handle_get.h"
#include "defines.h"
#include "helper.h"
#include "handle_request.h"
#include "msg.h"
#include "http.h"

/*
 * function where each thread starts execution with given client_info
 */
void *
handle_request(void *ptr)
{
	struct client_info *data;
	enum err_status error;
	char con_msg[128];
	struct http_header request;
	int msg_id;
	int sock;
	int port;
	char ip[16];

	/* move values to stack and delete memory from heap */
	data = (struct client_info *)ptr;
	sock = data->sock;
	port = data->port;
	strncpy(ip, data->ip, 16);
	free(data);
	data = NULL;

	if (sock < 0) {;
		die(ERR_INFO, "socket in handle_request is < 0");
	}

	msg_id = msg_hook_add(ip, port);

	snprintf(con_msg, (size_t)128, "connected to port %d.", port);
	msg_print_log(msg_id, CONNECTED, con_msg);

	/* check if ip is blocked */
	if (!ip_matches(IP, ip)) {
		error = IP_BLOCKED;
		goto disconnect;
	}

keep_alive:
	init_http_header(&request);
	error = parse_header(&request, sock);
	if (error) {
		goto disconnect;
	}

	switch (request.method) {
	case GET:
		error = handle_get(msg_id, sock, &request,
			    ip_matches(UPLOAD_IP, ip));
		break;
	case POST:
		if (!ip_matches(UPLOAD_IP, ip)) {
			error = POST_DISABLED;
			break;
		}
		error = handle_post(msg_id, sock, &request);
		break;
	default:
		/* not reached */
		error = INV_REQ_TYPE;
		break;
	}
	if (error) {
		msg_print_log(msg_id, ERROR, get_err_msg(error));
		error = STAT_OK;
	}

	if (sock != 0 && request.flags.keep_alive) {
		delete_http_header(&request);
		goto keep_alive;
	}

disconnect:
	if (error) {
		msg_print_log(msg_id, ERROR, get_err_msg(error));
	}
	delete_http_header(&request);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	msg_hook_rem(msg_id);

	return NULL;
}
