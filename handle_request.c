#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "helper.h"
#include "handle_request.h"
#include "file_list.h"
#include "msg.h"

/*
 * see config.h
 */
extern char *ROOT_DIR;
extern int VERBOSITY;
extern const uint32_t BUFFSIZE_READ;
extern const char *HTTP_TOP;
extern const char *HTTP_BOT;
extern const char *RESPONSE_404;
extern const char *RESPONSE_403;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

/*
 * function where each thread starts execution with given data_store
 */
void *
handle_request(void *ptr)
{
	struct data_store *data;

	data = (struct data_store *)ptr;
	if (data->socket < 0) {
		err_quit(ERR_INFO, "socket in handle_request is < 0");
	}

	msg_print_info(data, CONNECTED, "connection established", -1);

	char read_buffer[BUFFSIZE_READ];
	size_t message_buffer_size = 64;
	char message_buffer[message_buffer_size];
	enum err_status status;
	int added_hook = 0;
	char fmt_size[7];

	status = STAT_OK;

	status = read_request(data->socket, read_buffer, BUFFSIZE_READ);
	if (status != STAT_OK) {
		goto exit;
	}

	status = parse_request(read_buffer, &(data->req_type), &(data->url));
	if (status != STAT_OK) {
		goto exit;
	}

	generate_response(data);

#ifdef NCURSES
	if ((USE_NCURSES || VERBOSITY >= 3) && (data->body_type == DATA)) {
#else
	if ((VERBOSITY >= 3) && (data->body_type == DATA)) {
#endif
		added_hook = 1;
		msg_hook_add(data);
	}

	status = send_text(data->socket, data->head, strlen(data->head));
	if (status != STAT_OK) {
		goto exit;
	}

	switch (data->body_type) {
		case DATA:
			status = send_file(data);
			break;
		case TEXT:
		case ERR_404:
		case ERR_403:
		default:
			status = send_text(data->socket, data->body,
			    data->body_length);
			break;
	}
	if (status != STAT_OK) {
		goto exit;
	}

	switch (data->body_type) {
		case DATA:
		case TEXT:
			snprintf(message_buffer, message_buffer_size, "%s %s",
			   format_size(data->body_length, fmt_size), data->url);
			break;
		case ERR_404:
			snprintf(message_buffer, message_buffer_size, "err404 %s",
			    data->url);
			break;
		case ERR_403:
			snprintf(message_buffer, message_buffer_size, "err403 %s",
			    data->url);
			break;
		default:
			strncpy(message_buffer, "no body_type set", 17);
			break;
	}
	msg_print_info(data, SENT, message_buffer, -1);

	/*
	 * thread exit point, if status was set to error it will be printed,
	 * socket will be closed, memory will be cleaned and status hook will be
	 * removed before thread exit's
	 */
exit:
	switch (status) {
		case WRITE_CLOSED:
			msg_print_info(data, ERROR,
			    "could not write, client closed connection", -1);
			break;
		case ZERO_WRITTEN:
			msg_print_info(data, ERROR,
			    "could not write, 0 bytes written", -1);
			break;
		case READ_CLOSED:
			msg_print_info(data, ERROR, "could not read", -1);
			break;
		case EMPTY_MESSAGE:
			msg_print_info(data, ERROR, "empty message", -1);
			break;
		case INV_GET:
			msg_print_info(data, ERROR, "invalid GET", -1);
			break;
		default:
			break;
	}

	close(data->socket);
	if (added_hook) {
		msg_hook_cleanup(data);
	} else {
		free_data_store(data);
	}

	return NULL;
}

/*
 * reads from socket size bytes and write them to buffer
 * if negative number is returned, error occured
 * STAT_OK	 ( 0) : everything went fine.
 * READ_CLOSED	 (-3) : client closed connection
 * EMPTY_MESSAGE (-4) : nothing was read from socket
 */
int
read_request(int socket, char *buffer, size_t size)
{
	ssize_t n;

	memset(buffer, '\0', size);
	n = read(socket, buffer, size - 1);
	if (n < 0) {
		return READ_CLOSED;
	}
	if (n == 0) {
		return EMPTY_MESSAGE;
	}

	return STAT_OK;
}

/*
 * parses given request
 * allocs given *url with size of requested filename
 * if negative number is returned, error occured
 * STAT_OK	 ( 0) : everything went fine.
 * INV_GET	 (-5) : parse error
 */
int
parse_request(char *request, enum request_type *req_type, char **url)
{
	char *tmp;
	size_t length;
	size_t i;
	size_t url_pos;

	tmp = strtok(request, "\n"); /* get first line */

	/* check if GET is valid */
	if (tmp == NULL || !starts_with(tmp, "GET /")) {
		*req_type = PLAIN;
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

	(*url) = err_malloc(length + 1);
	memset((*url), '\0', length + 1);

	for (i = 0, url_pos = 0; i < length; i++, url_pos++) {
		if (*(tmp + i) != '%') {
			(*url)[url_pos] = tmp[i];
			continue;
		}
		/* check for url encoding */
		if (starts_with(tmp + i, "%20")) {
			(*url)[url_pos] = ' ';
			i += 2;
		} else if (starts_with(tmp + i, "%21")) {
			(*url)[url_pos] = '!';
			i += 2;
		} else if (starts_with(tmp + i, "%23")) {
			(*url)[url_pos] = '#';
			i += 2;
		} else if (starts_with(tmp + i, "%24")) {
			(*url)[url_pos] = '$';
			i += 2;
		} else if (starts_with(tmp + i, "%25")) {
			(*url)[url_pos] = '%';
			i += 2;
		} else if (starts_with(tmp + i, "%26")) {
			(*url)[url_pos] = '&';
			i += 2;
		} else if (starts_with(tmp + i, "%27")) {
			(*url)[url_pos] = '\'';
			i += 2;
		} else if (starts_with(tmp + i, "%28")) {
			(*url)[url_pos] = '(';
			i += 2;
		} else if (starts_with(tmp + i, "%29")) {
			(*url)[url_pos] = ')';
			i += 2;
		} else if (starts_with(tmp + i, "%2B")) {
			(*url)[url_pos] = '+';
			i += 2;
		} else if (starts_with(tmp + i, "%2C")) {
			(*url)[url_pos] = ',';
			i += 2;
		} else if (starts_with(tmp + i, "%2D")) {
			(*url)[url_pos] = '-';
			i += 2;
		} else if (starts_with(tmp + i, "%2E")) {
			(*url)[url_pos] = '.';
			i += 2;
		} else if (starts_with(tmp + i, "%3B")) {
			(*url)[url_pos] = ';';
			i += 2;
		} else if (starts_with(tmp + i, "%3D")) {
			(*url)[url_pos] = '=';
			i += 2;
		} else if (starts_with(tmp + i, "%40")) {
			(*url)[url_pos] = '@';
			i += 2;
		} else if (starts_with(tmp + i, "%5B")) {
			(*url)[url_pos] = '[';
			i += 2;
		} else if (starts_with(tmp + i, "%5D")) {
			(*url)[url_pos] = ']';
			i += 2;
		} else if (starts_with(tmp + i, "%5E")) {
			(*url)[url_pos] = '^';
			i += 2;
		} else if (starts_with(tmp + i, "%5F")) {
			(*url)[url_pos] = '_';
			i += 2;
		} else if (starts_with(tmp + i, "%60")) {
			(*url)[url_pos] = '`';
			i += 2;
		} else if (starts_with(tmp + i, "%7B")) {
			(*url)[url_pos] = '{';
			i += 2;
		} else if (starts_with(tmp + i, "%7D")) {
			(*url)[url_pos] = '}';
			i += 2;
		} else if (starts_with(tmp + i, "%7E")) {
			(*url)[url_pos] = '~';
			i += 2;
		} else {
			(*url)[url_pos] = tmp[i];
		}
	}

	/* get requested type */
	tmp = strtok(NULL, " ");
	/* if none given go with PLAIN and return */
	if (tmp == NULL) {
		*req_type = PLAIN;
		return STAT_OK;
	}
	if ((starts_with(tmp, "HTTP/1.1") == 0)
	    || (starts_with(tmp, "HTTP/1.0") == 0)) {
		*req_type = HTTP;
	} else {
		*req_type = PLAIN;
	}

	return STAT_OK;
}

/*
 * generates a response and saves it inside the data_store
 */
void
generate_response(struct data_store *data)
{
	char *full_requested_path;
	char *requested_path;
	size_t length;

	if (data->url == NULL) {
		generate_404(data);
		return;
	}
	if (strcmp(data->url, "/") == 0) {
		generate_200_directory(data, ROOT_DIR);
		return;
	}

	length = strlen(ROOT_DIR) + strlen(data->url);
	full_requested_path = err_malloc(length + 1);
	strncpy(full_requested_path, ROOT_DIR, length + 1);
	strncat(full_requested_path, data->url, strlen(data->url));
	full_requested_path[length] = '\0';

	requested_path = realpath(full_requested_path, NULL);
	if (requested_path == NULL) {
		generate_404(data);
	} else if (!starts_with(requested_path, ROOT_DIR)) {
		generate_403(data);
	} else if (is_directory(requested_path)) {
		generate_200_directory(data, requested_path);
	} else {
		generate_200_file(data, requested_path);
	}

	free(requested_path);
	free(full_requested_path);

	return;
}

/*
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void
generate_200_file(struct data_store *data, char *file)
{
	struct stat sb;
	size_t content_length_size = 128;
	char content_length[content_length_size];

	if (stat(file, &sb) == -1) {
		err_quit(ERR_INFO, "stat() retuned -1");
	}

	if (data->req_type == HTTP) {
		data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
		    "Content-Type: ");
		data->head = concat(data->head,
		    get_content_encoding(file));
		snprintf(content_length, content_length_size,
		    "\r\nContent-Length: %llu\r\n\r\n",
		    (long long unsigned int)sb.st_size);
		data->head = concat(data->head, content_length);
	}

	data->body = concat(data->body, file);
	data->body_length = (uint64_t)sb.st_size;
	data->body_type = DATA;

	return;
}

/*
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void
generate_200_directory(struct data_store *data, char *directory)
{
	if (data->req_type == HTTP) {
		data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
		    "Content-Type: text/html\r\n\r\n");
		data->body = concat(data->body, HTTP_TOP);
	}

	dir_to_table(data, directory);

	if (data->req_type == HTTP) {
		data->body = concat(data->body, HTTP_BOT);
	}

	data->body_length = strlen(data->body);

	data->body_type = TEXT;

	return;
}

/*
 * generates a 404 NOT FOUND HTTP response and saves it inside the data_store
 */
void
generate_404(struct data_store *data)
{
	if (data->req_type == HTTP) {
		data->head = concat(data->head, "HTTP/1.1 404 Not Found\r\n"
		    "Content-Type: text/plain\r\n\r\n");
	}
	data->body = concat(data->body, RESPONSE_404);
	data->body_length = strlen(data->body);
	data->body_type = ERR_404;

	return;
}

/*
 * generates a 403 FORBIDDEN HTTP response and saves it inside the data_store
 */
void
generate_403(struct data_store *data)
{
	if (data->req_type == HTTP) {
		data->head = concat(data->head, "HTTP/1.1 403 Forbidden\r\n"
		    "Content-Type: text/plain\r\n\r\n");
	}
	data->body = concat(data->body, RESPONSE_403);
	data->body_length = strlen(data->body);
	data->body_type = ERR_403;

	return;
}
