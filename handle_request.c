#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <limits.h>

#include "defines.h"
#include "helper.h"
#include "handle_request.h"
#include "file_list.h"
#include "msg.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern char *UPLOAD_DIR;
extern int VERBOSITY;
extern const size_t BUFFSIZE_READ;
extern const char *HTTP_TOP;
extern const char *HTTP_BOT;
extern const char *RESPONSE_404;
extern const char *RESPONSE_403;
extern const char *RESPONSE_201;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

#define MSG_BUFFER_SIZE 256

/*
 * function where each thread starts execution with given client_info
 */
void *
handle_request(void *ptr)
{
	struct client_info *data;
	char *cur_line;
	enum err_status error;

	data = (struct client_info *)ptr;

	if (data->socket < 0) {
		err_quit(ERR_INFO, "socket in handle_request is < 0");
	}
	msg_print_info(data, CONNECTED, "connection established", -1);

	/* set everything to 0 */
	data->requested_path = NULL;
	data->size = 0;
	data->written = 0;
	data->last_written = 0;

	error = get_line(data->socket, &cur_line);
	if (error) {
		goto exit_thread;
	}

	if (starts_with(cur_line, "POST")) {
		if (UPLOAD_DIR != NULL) {
			error = handle_post(data, cur_line);
		} else {
			error = INV_REQ_TYPE;
		}
	} else if (starts_with(cur_line, "GET")) {
		error = handle_get(data, cur_line);
	} else {
		error = INV_REQ_TYPE;
	}
	free(cur_line);
	cur_line = NULL;

	/*
	 * exit of thread, closes socket and prints out error msg
	 */
exit_thread:
	close(data->socket);

	switch (error) {
		case WRITE_CLOSED:
			msg_print_info(data, ERROR,
			    "could not write, client closed connection",
			    -1);
			break;
		case ZERO_WRITTEN:
			msg_print_info(data, ERROR,
			    "could not write, 0 bytes written",
			    -1);
			break;
		case CLOSED_CON:
			msg_print_info(data, ERROR,
			    "Client closed connection",
			    -1);
			break;
		case EMPTY_MESSAGE:
			msg_print_info(data, ERROR,
			    "empty message",
			    -1);
			break;
		case INV_REQ_TYPE:
			msg_print_info(data, ERROR,
			    "invalid Request Type",
			    -1);
			break;
		case INV_GET:
			msg_print_info(data, ERROR,
			    "invalid GET",
			    -1);
			break;
		case INV_POST:
			msg_print_info(data, ERROR,
			    "invalid POST",
			    -1);
			break;
		case CON_LENGTH_MISSING:
			msg_print_info(data, ERROR,
			    "Content_Length missing",
			    -1);
			break;
		case BOUNDARY_MISSING:
			msg_print_info(data, ERROR,
			    "boundary missing",
			    -1);
			break;
		case REFERER_MISSING:
			msg_print_info(data, ERROR,
			    "referer missing",
			    -1);
			break;
		case FILESIZE_ZERO:
			msg_print_info(data, ERROR,
			    "filesize is 0 or error ocurred",
			    -1);
			break;
		case WRONG_BOUNDRY:
			msg_print_info(data, ERROR,
			    "wrong boundry specified",
			    -1);
			break;
		case LINE_LIMIT_EXT:
			msg_print_info(data, ERROR,
			    "http header extended line limit",
			    -1);
			break;
		case POST_NO_FILENAME:
			msg_print_info(data, ERROR,
			    "missing filename in post message",
			    -1);
			break;
		case NO_FREE_SPOT:
			msg_print_info(data, ERROR,
			    "the posted filename already exists (10 times)",
			    -1);
			break;
		case FILE_ERROR:
			msg_print_info(data, ERROR,
			    "could not write to post content to file",
			    -1);
			break;
		default:
			/* not reached */
			break;
	}

	msg_hook_cleanup(data);

	return NULL;
}

int
handle_get(struct client_info *data, char *request)
{
	char message_buffer[MSG_BUFFER_SIZE];
	enum err_status error;
	char fmt_size[7];
	enum response_type res_type;
	enum request_type req_type;

	error = STAT_OK;

	error = parse_get(request, &(req_type), &(data->requested_path));
	if (error) {
		return error;
	}

	res_type = get_response_type(&(data->requested_path));

	switch (res_type) {
		case FILE_200:
#ifdef NCURSES
			if (USE_NCURSES || VERBOSITY >= 3) {
#else
			if (VERBOSITY >= 3) {
#endif
				msg_hook_add(data);
			}

			error = send_200_file_head(data->socket,
				    req_type,
				    data->requested_path,
				    &(data->size));
			if (error) {
				return error;
			}

			error = send_file(data->socket, data->requested_path,
				    &(data->written));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "%s %s",
				    format_size(data->size, fmt_size),
				    data->requested_path);
			}
			break;
		case DIR_200:
			error = send_200_directory(data->socket,
				    req_type,
				    data->requested_path,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "%s %s",
				    format_size(data->size, fmt_size),
				    data->requested_path);
			}
			break;
		case TXT_403:
			error = send_403(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "err403");
			}
			break;
		case TXT_404:
			error = send_404(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "err404");
			}
			break;
		default:
			/* not readched */
			break;
	}

	if (error) {
		return error;
	}
	msg_print_info(data, SENT, message_buffer, -1);

	/*
	 * thread exit point
	 * socket will be closed, memory will be cleaned and error hook will be
	 * removed before thread exit's
	 * error is returned
	 */
	return STAT_OK;
}

int
handle_post(struct client_info *data, char *request)
{
	char *cur_line;
	enum err_status error;
	char *boundary;
	char *referer;
	char *content_length;
	uint64_t max_length;
	uint64_t content_read;

	cur_line = NULL;
	error = STAT_OK;

	if (strcmp(request, "POST / HTTP/1.1") != 0) {
		return INV_POST;
	}

	error = parse_post_header(data->socket, &boundary, &referer,
		    &content_length);
	if (error) {
		return error;
	}

	max_length = err_string_to_val(content_length);
	if (max_length == 0) {
		return FILESIZE_ZERO;
	}

	/* END of HEADER */

	content_read = 0;
	error = get_line(data->socket, &cur_line);
	if (error) {
		return error;
	}
	content_read += strlen(cur_line) + 2;

	if (!starts_with(cur_line, "--")
	         || (strcmp(cur_line + 2, boundary) != 0)) {
		return WRONG_BOUNDRY;
	}
	free(cur_line);

	do {
		char *filename;
		error = parse_file_header(data->socket, &filename,
			    &content_read);
		if (error) {
			return error;
		}
		error = save_file_from_post(data->socket, filename, boundary,
			    &content_read, max_length);
		if (error) {
			return error;
		}
		error = get_line(data->socket, &cur_line);
		if (error) {
			return error;
		}
		content_read += strlen(cur_line) + 2;
	} while (strlen(cur_line) == 0
		    && strcmp(cur_line, "--") != 0)

	error = send_201(data->socket);
	msg_print_info(data, POST, "someone posted!", -1);

	/*
	 * thread exit point
	 * socket will be closed, memory will be cleaned and error hook will be
	 * removed before thread exit's
	 * error is returned
	 */

	return error;
}

/*
 * given file is created inside the UPLOAD_DIR folder, if exists file with name
 * filename_0 is created, if also exists filename_1 is created (up to filename_9)
 * if no free fileame is found NO_FREE_SPOT is returned
 */
int
save_file_from_post(int socket, char *filename, char *boundary,
    uint64_t *content_read, uint64_t max_length)
{
	enum err_status error;
	char *full_filename;
	char str_number[2];
	int number;
	int i;
	FILE *fd;

	ssize_t err;
	size_t written;

	char buff[BUFFSIZE_READ];
	int cur_buf_pos;

	char *bound_buff;
	int cur_bound_buf_pos;

	error = STAT_OK;
	full_filename = NULL;
	full_filename = concat(concat(concat(full_filename, UPLOAD_DIR), "/"),
			    filename);

	/* in case file exists */
	number = 0;
	if (access(full_filename, F_OK) != -1) {
		snprintf(str_number, "%i", 2, number++);
		full_filename = concat(concat(full_filename, "_"), str_number);
	}
	/* count up until a free filename is found */
	while (access(full_filename, F_OK) != -1 && number < 10) {
		snprintf(str_number, "%i", 2, number++);
		full_filename[strlen(full_filename) - 1] = str_number[0];
	}
	if (number >= 10) {
		free(full_filename);
		return NO_FREE_SPOT;
	}

	fd = fopen(full_filename, "rb");

	bound_buff = NULL;
	bound_buff = concat(concat(bound_buff, "\r\n--"), boundary);

	cur_buf_pos = 0;
	cur_bound_buf_pos = 0;

	/* read as long as we are not extending max_length */
	while ((*content_read) < max_length) {
		/* read 1 character from socket */
		err = read(socket, &cur_char, (size_t)1);
		(*content_read)++;
		if (err < 1) {
			error = CLOSED_CON;
			goto stop_transfer;
		}
		/* check if current character is matching the bound string */
		if (cur_char == bound_buff[cur_bound_buf_pos]) {
			cur_bound_buf_pos++;
			/* if every character has matched the bound_buff string stop */
			if (cur_bound_buf_pos == strlen(bound_buff)) {
				error = STAT_OK;
				goto stop_transfer;
			}
			continue;
		/*
		 * if it didn't match check if it has matched some, if so
		 * write them to buff
		 */
		}
		for (i = 0; i < cur_bound_buf_pos; i++) {
			buff[cur_buf_pos] = bound_buff[i];
			/*
			 * lets see if we extend the buffer, if so write
			 * content to file 
			 */
			if (++cur_buf_pos == BUFFSIZE_READ) {
				written = 0;
				while (written != cur_buf_pos) {
					err = write(fd, buff, cur_buf_pos);
					if (err < 1) {
						error = FILE_ERROR;
						goto stop_transfer;
					}
					written += (size_t)err;
				}
				cur_buf_bos = 0;
			}
		}
		buff[cur_buf_pos] = cur_char;
		/*
		 * lets see if we extend the buffer, if so write
		 * content to file
		 */
		if (++cur_buf_pos == BUFFSIZE_READ) {
			written = 0;
			while (written != cur_buf_pos) {
				err = write(fd, buff, cur_buf_pos);
				if (err < 1) {
					error = FILE_ERROR;
					goto stop_transfer;
				}
				written += (size_t)err;
			}
			cur_buf_bos = 0;
		}
	}

stop_transfer:
	free(bound_buff);
	fclose(fd);
	if (error) {
		unlink(full_filename);
	}
	free(full_filename);

	return error;
}

/*
 * parses the http header of a file and sets filename accordingly.
 * content_read is counted up by size of read bytes..
 * if filename is missing or not found POST_NO_FILENAME is returned.
 */
int
parse_file_header(int socket, char **filename, uint64_t *content_read)
{
	char *cur_line;
	char *STR_DIS;
	size_t str_len;
	enum err_status error;
	char *file_start;
	char *file_end;

	STR_DIS = "Content-Disposition: form-data;";
	error = STAT_OK;
	(*filename) = NULL;

	error = get_line(socket, &cur_line);
	if (error) {
		return error;
	}
	(*content_read) += strlen(cur_line) + 2;

	while (strlen(cur_line) != 0) {
		if (error) {
			return error;
		}
		if (starts_with(cur_line, STR_DIS)) {
			/* got that string */
			file_start = strstr(cur_line, "filename=\"");
			if (file_start == NULL) {
				error = POST_NO_FILENAME;
				break;
			}
			file_start += strlen("filename=\"");
			file_end = strchr(file_start, '"');
			if (file_end == NULL) {
				error = POST_NO_FILENAME;
				break;
			}
			str_len = (file_end - file_start) - 1;
			if (str_len == 0) {
				error = POST_NO_FILENAME;
				break;
			}
			(*filename) = err_malloc(str_len + 1);
			memset((*filename), '\0', str_len + 1);
			strncpy((*filename), file_start, str_len);
		}
		free(cur_line);
		cur_line = NULL;
		error = get_line(socket, &cur_line);
		if (error) {
			break;
		}
		(*content_read) += strlen(cur_line) + 2;
	}
	if (cur_line != NULL) {
		free(cur_line);
	}

	if (error) {
		if ((*filename) != NULL) {
			free((*filename));
			(*filename) = NULL;
		}
	}
	if  ((*filename) == NULL) {
		error = POST_NO_FILENAME;
	}

	return error;
}

int
parse_post_header(int socket, char **boundary, char **referer,
    char **content_length)
{
	char *cur_line;
	enum err_status error;
	size_t str_len;
	char *STR_REF;
	char *STR_COL;
	char *STR_BOU;

	STR_REF = "Referer: ";
	STR_COL = "Content-Length: ";
	STR_BOU = "Content-Type: multipart/form-data; boundary=";
	error = STAT_OK;
	(*boundary) = NULL;
	(*referer) = NULL;
	(*content_length) = NULL;

	error = get_line(socket, &cur_line);
	if (error) {
		return error;
	}

	while (strlen(cur_line) != 0) {
		if (starts_with(cur_line, STR_COL)) {
			str_len = strlen(cur_line + strlen(STR_COL));
			if (str_len - strlen(STR_COL) < 1) {
				error = CON_LENGTH_MISSING;
				break;
			}
			(*content_length) = err_malloc(str_len + 1);
			strncpy((*content_length), cur_line
			    + strlen(STR_COL), str_len + 1);
		}
		if (starts_with(cur_line, STR_BOU)) {
			str_len = strlen(cur_line + strlen(STR_BOU));
			if (str_len - strlen(STR_BOU) < 1) {
				error = BOUNDARY_MISSING;
				break;
			}
			(*boundary) = err_malloc(str_len + 1);
			strncpy((*boundary), cur_line
			    + strlen(STR_BOU), str_len + 1);
		}
		if (starts_with(cur_line, STR_REF)) {
			str_len = strlen(cur_line + strlen(STR_REF));
			if (str_len - strlen(STR_REF) < 1) {
				error = REFERER_MISSING;
				break;
			}
			(*referer) = err_malloc(str_len + 1);
			strncpy((*referer), cur_line
			    + strlen(STR_REF), str_len + 1);
		}
		free(cur_line);
		cur_line = NULL;
		error = get_line(socket, &cur_line);
		if (error) {
			break;
		}
	}
	if (cur_line != NULL) {
		free(cur_line);
	}

	if  ((*content_length) == NULL) {
		error = CON_LENGTH_MISSING;
	}
	if ((*boundary) == NULL) {
		error = BOUNDARY_MISSING;
	}
	if ((*referer) == NULL) {
		error = REFERER_MISSING;
	}

	if (error) {
		if ((*content_length) != NULL) {
			free((*content_length));
			(*content_length) = NULL;
		}
		if ((*boundary) != NULL) {
			free((*boundary));
			(*boundary) = NULL;
		}
		if ((*referer) != NULL) {
			free((*referer));
			(*referer) = NULL;
		}
	}

	return error;
}

/*
 * parses given request line
 * allocs given *requested_path with size of requested filename
 * if negative number is returned, error occured
 * STAT_OK: everything went fine.
 * INV_GET: parse error
 */
int
parse_get(char *request, enum request_type *req_type, char **requested_path)
{
	char *tmp;
	size_t length;
	size_t i;
	size_t url_pos;

	tmp = strtok(request, "\n"); /* get first line */

	/* check if GET is valid */
	if (tmp == NULL || !starts_with(tmp, "GET /")) {
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

	(*requested_path) = err_malloc(length + 1);
	memset((*requested_path), '\0', length + 1);

	for (i = 0, url_pos = 0; i < length; i++, url_pos++) {
		if (*(tmp + i) != '%') {
			(*requested_path)[url_pos] = tmp[i];
			continue;
		}
		/* check for requested_path encoding */
		if (starts_with(tmp + i, "%20")) {
			(*requested_path)[url_pos] = ' ';
			i += 2;
		} else if (starts_with(tmp + i, "%21")) {
			(*requested_path)[url_pos] = '!';
			i += 2;
		} else if (starts_with(tmp + i, "%23")) {
			(*requested_path)[url_pos] = '#';
			i += 2;
		} else if (starts_with(tmp + i, "%24")) {
			(*requested_path)[url_pos] = '$';
			i += 2;
		} else if (starts_with(tmp + i, "%25")) {
			(*requested_path)[url_pos] = '%';
			i += 2;
		} else if (starts_with(tmp + i, "%26")) {
			(*requested_path)[url_pos] = '&';
			i += 2;
		} else if (starts_with(tmp + i, "%27")) {
			(*requested_path)[url_pos] = '\'';
			i += 2;
		} else if (starts_with(tmp + i, "%28")) {
			(*requested_path)[url_pos] = '(';
			i += 2;
		} else if (starts_with(tmp + i, "%29")) {
			(*requested_path)[url_pos] = ')';
			i += 2;
		} else if (starts_with(tmp + i, "%2B")) {
			(*requested_path)[url_pos] = '+';
			i += 2;
		} else if (starts_with(tmp + i, "%2C")) {
			(*requested_path)[url_pos] = ',';
			i += 2;
		} else if (starts_with(tmp + i, "%2D")) {
			(*requested_path)[url_pos] = '-';
			i += 2;
		} else if (starts_with(tmp + i, "%2E")) {
			(*requested_path)[url_pos] = '.';
			i += 2;
		} else if (starts_with(tmp + i, "%3B")) {
			(*requested_path)[url_pos] = ';';
			i += 2;
		} else if (starts_with(tmp + i, "%3D")) {
			(*requested_path)[url_pos] = '=';
			i += 2;
		} else if (starts_with(tmp + i, "%40")) {
			(*requested_path)[url_pos] = '@';
			i += 2;
		} else if (starts_with(tmp + i, "%5B")) {
			(*requested_path)[url_pos] = '[';
			i += 2;
		} else if (starts_with(tmp + i, "%5D")) {
			(*requested_path)[url_pos] = ']';
			i += 2;
		} else if (starts_with(tmp + i, "%5E")) {
			(*requested_path)[url_pos] = '^';
			i += 2;
		} else if (starts_with(tmp + i, "%5F")) {
			(*requested_path)[url_pos] = '_';
			i += 2;
		} else if (starts_with(tmp + i, "%60")) {
			(*requested_path)[url_pos] = '`';
			i += 2;
		} else if (starts_with(tmp + i, "%7B")) {
			(*requested_path)[url_pos] = '{';
			i += 2;
		} else if (starts_with(tmp + i, "%7D")) {
			(*requested_path)[url_pos] = '}';
			i += 2;
		} else if (starts_with(tmp + i, "%7E")) {
			(*requested_path)[url_pos] = '~';
			i += 2;
		} else {
			(*requested_path)[url_pos] = tmp[i];
		}
	}

	/* get requested type */
	tmp = strtok(NULL, " ");
	/* if none given go with PLAIN and return */
	if (tmp == NULL) {
		(*req_type) = PLAIN;
		return STAT_OK;
	}
	if ((starts_with(tmp, "HTTP/1.1") == 0)
	    || (starts_with(tmp, "HTTP/1.0") == 0)) {
		(*req_type) = HTTP;
	} else {
		(*req_type) = PLAIN;
	}

	return STAT_OK;
}

/*
 * gets a line from socket, and writes it to buffer
 * buffer will be err_malloc'ed by get_line.
 * returns STAT_OK CLOSED_CON or LINE_LIMIT_EXT
 * buff will be NULL and free'd error
 */
int
get_line(int socket, char **buff)
{
	size_t buf_size;
	size_t bytes_read;
	ssize_t err;
	char cur_char;

	buf_size = 128;
	(*buff) = (char *)err_malloc(buf_size);
	memset((*buff), '\0', buf_size);

	for (bytes_read = 0 ;; bytes_read++) {
		err = read(socket, &cur_char, (size_t)1);
		if (err < 1) {
			free((*buff));
			(*buff) = NULL;
			return CLOSED_CON;
		}

		if (bytes_read >= buf_size) {
			buf_size += 128;
			if (buf_size > HTTP_HEADER_LINE_LIMIT) {
				free((*buff));
				(*buff) = NULL;
				return LINE_LIMIT_EXT;
			}
			(*buff) = err_realloc((*buff), buf_size);
			memset((*buff) + buf_size - 128, '\0', 128);
		}

		if (cur_char == '\n') {
			break;
		}

		if (cur_char == '\r') {
			continue;
		}

		(*buff)[bytes_read] = cur_char;
	}

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
	} else if (!starts_with(requested_path, ROOT_DIR)
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

/*
 * generates a 200 OK HTTP response and saves it inside the client_info
 */
int
send_200_file_head(int socket, enum request_type type, char *filename,
    uint64_t *size)
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
 * sends a 200 OK resposne with attached directory table
 */
int
send_200_directory(int socket, enum request_type type, char *directory,
    uint64_t *size)
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
 * generates a 404 NOT FOUND HTTP response and saves it inside the client_info
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
 * generates a 403 FORBIDDEN HTTP response and saves it inside the client_info
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
 * will send a 201 Created
 */
int
send_201(int socket)
{
	enum err_status error;
	char *head;
	uint64_t size;

	head = "HTTP/1.1 201 Created\r\n"
	       "Content-Type: text/html\r\n";
	size = (uint64_t)strlen(RESPONSE_201);

	error = send_http_head(socket, head, size);
	if (error) {
		return error;
	}

	error = send_text(socket, RESPONSE_201, size);

	return error;
}

/*
 * sends a http header with added Content_Length and Content_Type
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
