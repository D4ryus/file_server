#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "http_response.h"
#include "types.h"
#include "helper.h"
#include "file_list.h"
#include "defines.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern const char *RESPONSE_404;
extern const char *RESPONSE_403;
extern const char *RESPONSE_201;
extern const char *HTTP_TOP;
extern const char *HTTP_BOT;

/*
 * sends the given http header with added Content_Length attribute
 */
int
send_http_head(int socket, char* head, uint64_t content_length)
{
	enum err_status error;
	char tmp[MSG_BUFFER_SIZE];

	snprintf(tmp, MSG_BUFFER_SIZE,
	    "Content-Length: %llu\r\n\r\n",
	    (long long unsigned int)content_length);

	error = send_text(socket, head, (uint64_t)strlen(head));
	if (error) {
		return error;
	}

	error = send_text(socket, tmp,
	    (uint64_t)strlen(tmp));
	if (error) {
		return error;
	}

	return STAT_OK;
}

/*
 * sends a 200 OK HTTP header response
 * if type == PLAIN no http header will be sent (only size will be set)
 * size is set to content_length
 */
int
send_200_file_head(int socket, enum request_type type, uint64_t *size,
    char *filename)
{
	struct stat sb;
	char *head;
	enum err_status error;
	char *full_path;

	full_path = NULL;
	full_path = concat(concat(full_path, ROOT_DIR), filename);

	if (stat(full_path, &sb) == -1) {
		err_quit(ERR_INFO, "stat() retuned -1");
	}
	free(full_path);
	(*size) = (uint64_t)sb.st_size;

	if (type != HTTP) {
		return STAT_OK;
	}

	head = NULL;
	head = concat(
		   concat(
		       concat(head, "HTTP/1.1 200 OK\r\n" "Content-Type: "),
		       get_content_encoding(filename)),
		   "\r\n");
	error = send_http_head(socket, head, (*size));
	free(head);
	if (error) {
		return error;
	}


	return STAT_OK;
}

/*
 * sends a 200 OK HTTP header resposne with attached directory table
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_200_directory(int socket, enum request_type type, uint64_t *size,
    char *directory)
{
	enum err_status error;
	char *body;
	char *table;
	char *head;
	    
	head = "HTTP/1.1 200 OK\r\n"
	       "Content-Type: text/html\r\n";
	body = NULL;

	if (type == HTTP) {
		body = concat(body, HTTP_TOP);
	}
	table = dir_to_table(type, directory);
	body = concat(body, table);
	free(table);
	if (type == HTTP) {
		body = concat(body, HTTP_BOT);
	}
	(*size) = strlen(body);

	if (type == HTTP) {
		error = send_http_head(socket, head, strlen(body));
		if (error) {
			return error;
		}
	}

	error = send_text(socket, body, (*size));
	free(body);

	return error;
}

/*
 * sends a 404 NOT FOUND HTTP response with attached RESPONSE_404 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_404(int socket, enum request_type type, uint64_t *size)
{
	enum err_status error;
	char *head;

	head = "HTTP/1.1 404 Not Found\r\n"
	       "Content-Type: text/plain\r\n";
	(*size) = strlen(RESPONSE_404);

	if (type == HTTP) {
		error = send_http_head(socket, head, strlen(RESPONSE_404));
		if (error) {
			return error;
		}
	}

	error = send_text(socket, RESPONSE_404, (*size));

	return error;
}

/*
 * sends a 403 FORBIDDEN HTTP response with attached RESPONSE_403 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_403(int socket, enum request_type type, uint64_t *size)
{
	enum err_status error;
	char *head;

	head = "HTTP/1.1 403 Forbidden\r\n"
	       "Content-Type: text/plain\r\n";
	(*size) = strlen(RESPONSE_403);

	if (type == HTTP) {
		error = send_http_head(socket, head, strlen(RESPONSE_403));
		if (error) {
			return error;
		}
	}

	error = send_text(socket, RESPONSE_403, (*size));

	return error;
}

/*
 * sends a 201 CREATED HTTP response with attached RESPONSE_201 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_201(int socket, enum request_type type, uint64_t *size)
{
	enum err_status error;
	char *head;

	head = "HTTP/1.1 201 Created\r\n"
	       "Content-Type: text/html\r\n";
	(*size) = (uint64_t)strlen(RESPONSE_201);

	if (type == HTTP) {
		error = send_http_head(socket, head, (*size));
		if (error) {
			return error;
		}
	}

	error = send_text(socket, RESPONSE_201, (*size));

	return error;
}

