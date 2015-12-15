#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "globals.h"
#include "http_response.h"
#include "types.h"
#include "helper.h"
#include "file_list.h"
#include "defines.h"

/*
 * sends a 200 OK HTTP header response
 * if type == PLAIN no http header will be sent (only size will be set)
 * size is set to content_length
 */
int
send_200_file_head(int socket, enum http_type type, uint64_t *size,
    char *filename)
{
	struct stat sb;
	char *head;
	enum err_status error;
	char *full_path;
	char content_length[64];

	full_path = NULL;
	full_path = concat(concat(full_path, ROOT_DIR), filename);

	if (stat(full_path, &sb) == -1) {
		die(ERR_INFO, "stat()");
	}
	free(full_path);
	*size = (uint64_t)sb.st_size;

	if (type != HTTP) {
		return STAT_OK;
	}

	snprintf(content_length, (size_t)64,
	    "\r\nContent-Length: %llu\r\n\r\n",
	    (long long unsigned int)sb.st_size);

	head = NULL;
	head = concat(concat(concat(
		   head, "HTTP/1.1 200 OK\r\n"
			 "Accept-Ranges: bytes\r\n"
			 "Cache-Control: no-cache\r\n"
			 "Connection: close\r\n"
			 "Content-Type: "), get_content_encoding(filename)),
			 content_length);

	error = send_data(socket, head, (uint64_t)strlen(head));
	free(head);
	if (error) {
		return error;
	}


	return STAT_OK;
}

/*
 * sends a 206 Partial Content HTTP header response
 * if type == PLAIN no http header will be sent (only size will be set)
 * size is set to content_length
 */
int
send_206_file_head(int socket, enum http_type type, uint64_t *size,
    char *filename, uint64_t from, uint64_t to)
{
	struct stat sb;
	char *head;
	enum err_status error;
	char *full_path;
	char content_length[64];
	char content_range[64];
	uint64_t file_size;

	if (type != HTTP) {
		return STAT_OK;
	}

	full_path = NULL;
	full_path = concat(concat(full_path, ROOT_DIR), filename);

	if (stat(full_path, &sb) == -1) {
		die(ERR_INFO, "stat()");
	}
	free(full_path);
	file_size = (uint64_t)sb.st_size;

	if (to == 0) {
	    to = file_size - 1;
	}

	*size = to - from + 1;

	snprintf(content_length, (size_t)64,
	    "\r\nContent-Length: %" PRIu64 "\r\n",
	    (to - from) + 1);

	snprintf(content_range, (size_t)64,
	    "Content-Range: bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64 "\r\n\r\n",
	    from, to, file_size);

	head = NULL;
	head = concat(concat(concat(concat(
		   head, "HTTP/1.1 206 Partial Content\r\n"
			 "Accept-Ranges: bytes\r\n"
			 "Connection: close\r\n"
			 "Content-Type: "), get_content_encoding(filename)),
			 content_length),
			 content_range);

	error = send_data(socket, head, (uint64_t)strlen(head));
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
send_200_directory(int socket, enum http_type type, uint64_t *size,
    char *directory, char ip[16])
{
	enum err_status error;
	char *body;
	char *table;
	char *head;
	char content_length[64];

	head = NULL;
	body = NULL;

	if (type == HTTP) {
		body = concat(body, HTTP_TOP);
		/* if upload allowed, print upload form */
		if (ip_matches(UPLOAD_IP, ip)) {
			body = concat(concat(concat(body,
				"<form action='"), directory), "'"
				      "method='post'"
				      "enctype='multipart/form-data'>"
					"<br><input type='file' name='file[]' multiple='true'>"
					"<br><button type='submit'>Upload</button>"
				"</form>");
		}
	}
	table = dir_to_table(type, directory);
	body = concat(body, table);
	free(table);
	if (type == HTTP) {
		body = concat(body, HTTP_BOT);
	}

	*size = (uint64_t)strlen(body);
	if (type == HTTP) {
		snprintf(content_length, (size_t)64,
		    "Content-Length: %llu\r\n\r\n",
		    (long long unsigned int)*size);

		head = concat(concat(head, "HTTP/1.1 200 OK\r\n"
					   "Cache-Control: no-cache\r\n"
					   "Connection: close\r\n"
					   "Content-Type: text/html; "
					   "charset=utf-8\r\n"),
					   content_length);
		error = send_data(socket, head, (uint64_t)strlen(head));
		free(head);
		if (error) {
			return error;
		}
	}

	error = send_data(socket, body, *size);
	free(body);

	return error;
}

/*
 * sends a 403 FORBIDDEN HTTP response with attached RESPONSE_403 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_403(int socket, enum http_type type, uint64_t *size)
{
	enum err_status error;
	char *head;
	char content_length[64];

	head = NULL;

	*size = (uint64_t)strlen(RESPONSE_403);
	if (type == HTTP) {
		snprintf(content_length, (size_t)64,
		    "Content-Length: %llu\r\n\r\n",
		    (long long unsigned int)*size);

		head = concat(concat(head, "HTTP/1.1 403 Forbidden\r\n"
					   "Cache-Control: no-cache\r\n"
					   "Connection: close\r\n"
					   "Content-Type: text/plain; "
					   "charset=utf-8\r\n"),
					   content_length);
		error = send_data(socket, head, (uint64_t)strlen(head));
		if (error) {
			return error;
		}
	}

	error = send_data(socket, RESPONSE_403, *size);

	return error;
}

/*
 * sends a 404 NOT FOUND HTTP response with attached RESPONSE_404 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_404(int socket, enum http_type type, uint64_t *size)
{
	enum err_status error;
	char *head;
	char content_length[64];

	head = NULL;

	*size = (uint64_t)strlen(RESPONSE_404);
	if (type == HTTP) {
		snprintf(content_length, (size_t)64,
		    "Content-Length: %llu\r\n\r\n",
		    (long long unsigned int)*size);

		head = concat(concat(head, "HTTP/1.1 404 Not Found\r\n"
					   "Cache-Control: no-cache\r\n"
					   "Connection: close\r\n"
					   "Content-Type: text/plain; "
					   "charset=utf-8\r\n"),
					   content_length);
		error = send_data(socket, head, (uint64_t)strlen(head));
		free(head);
		if (error) {
			return error;
		}
	}

	error = send_data(socket, RESPONSE_404, *size);

	return error;
}

/*
 * sends a 405 METHOD NOT ALLOWED HTTP response with attached RESPONSE_405 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_405(int socket, enum http_type type, uint64_t *size)
{
	enum err_status error;
	char *head;
	char content_length[64];

	head = NULL;

	*size = (uint64_t)strlen(RESPONSE_405);
	if (type == HTTP) {
		snprintf(content_length, (size_t)64,
		    "Content-Length: %llu\r\n\r\n",
		    (long long unsigned int)*size);

		head = concat(concat(head, "HTTP/1.1 405 Method Not Allowed\r\n"
					   "Cache-Control: no-cache\r\n"
					   "Connection: close\r\n"
					   "Content-Type: text/plain; "
					   "charset=utf-8\r\n"),
					   content_length);
		error = send_data(socket, head, (uint64_t)strlen(head));
		if (error) {
			return error;
		}
	}

	error = send_data(socket, RESPONSE_405, *size);

	return error;
}

/*
 * sends a 201 CREATED HTTP response with attached RESPONSE_201 msg
 * if type == PLAIN no http header will be sent
 * size is set to content_length
 */
int
send_201(int socket, enum http_type type, uint64_t *size)
{
	enum err_status error;
	char *head;
	char content_length[64];

	head = NULL;

	*size = (uint64_t)strlen(RESPONSE_201);
	if (type == HTTP) {
		snprintf(content_length, (size_t)64,
		    "Content-Length: %llu\r\n\r\n",
		    (long long unsigned int)*size);

		head = concat(concat(head, "HTTP/1.1 201 Created\r\n"
					   "Cache-Control: no-cache\r\n"
					   "Connection: close\r\n"
					   "Content-Type: text/html; "
					   "charset=utf-8\r\n"),
					   content_length);
		error = send_data(socket, head, (uint64_t)strlen(head));
		free(head);
		if (error) {
			return error;
		}
	}

	error = send_data(socket, RESPONSE_201, *size);

	return error;
}

