#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "defines.h"
#include "helper.h"
#include "handle_request.h"
#include "http_response.h"
#include "msg.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern char *UPLOAD_DIR;
extern int VERBOSITY;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

/*
 * corresponding error messages to err_status, see types.h
 */
const char *err_msg[] =
{
/* STAT_OK            */ "no error detected",
/* WRITE_CLOSED       */ "could not write, client closed connection",
/* ZERO_WRITTEN       */ "could not write, 0 bytes written",
/* CLOSED_CON         */ "client closed connection",
/* EMPTY_MESSAGE      */ "empty message",
/* INV_REQ_TYPE       */ "invalid Request Type",
/* INV_GET            */ "invalid GET",
/* INV_POST           */ "invalid POST",
/* CON_LENGTH_MISSING */ "Content_Length missing",
/* BOUNDARY_MISSING   */ "boundary missing",
/* REFERER_MISSING    */ "referer missing",
/* FILESIZE_ZERO      */ "filesize is 0 or error ocurred",
/* WRONG_BOUNDRY      */ "wrong boundry specified",
/* LINE_LIMIT_EXT     */ "http header extended line limit",
/* POST_NO_FILENAME   */ "missing filename in post message",
/* NO_FREE_SPOT       */ "the posted filename already exists (10 times)",
/* FILE_ERROR         */ "could not write the post content to file"
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

	data = (struct client_info *)ptr;

	if (data->socket < 0) {
		err_quit(ERR_INFO, "socket in handle_request is < 0");
	}
	msg_print_info(data, CONNECTED, "connection established", -1);

	error = get_line(data->socket, &cur_line);
	if (error) {
		msg_print_info(data, ERROR,
		    err_msg[error],
		    -1);
		free(data);
		return NULL;
	}

	data->requested_path = NULL;
	data->size = 0;
	data->written = 0;
	data->last_written = 0;

#ifdef NCURSES
	if (USE_NCURSES || VERBOSITY >= 3) {
#else
	if (VERBOSITY >= 3) {
#endif
		msg_hook_add(data);
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

	if (error) {
		msg_print_info(data, ERROR,
		    err_msg[error],
		    -1);
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
	char *requ_path_tmp;

	error = STAT_OK;

	requ_path_tmp = NULL;
	error = parse_get(request, &(req_type), &(requ_path_tmp));
	if (error) {
		return error;
	}

	res_type = get_response_type(&(requ_path_tmp));

	if (res_type == FILE_200  || res_type == DIR_200) {
		data->requested_path = requ_path_tmp;
		requ_path_tmp = NULL;
	}

	switch (res_type) {
		case FILE_200:
			error = send_200_file_head(data->socket,
				    req_type,
				    &(data->size),
				    data->requested_path);
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
				    &(data->size),
				    data->requested_path);
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "%s %s",
				    format_size(data->size, fmt_size),
				    data->requested_path);
				data->written = data->size;
			}
			break;
		case TXT_403:
			error = send_403(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "err403");
				data->written = data->size;
			}
			break;
		case TXT_404:
			error = send_404(data->socket, req_type,
				    &(data->size));
			if (!error) {
				snprintf(message_buffer,
				    MSG_BUFFER_SIZE,
				    "err404");
				data->written = data->size;
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
	char *filename;
	char *free_me;
	uint64_t tmp;

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
	free(referer);
	/* TODO: USE REFERER */

	data->size = err_string_to_val(content_length);
	free(content_length);
	if (data->size == 0) {
		free(boundary);
		return FILESIZE_ZERO;
	}

	/* END of HEADER */

	error = get_line(data->socket, &cur_line);
	if (error) {
		free(boundary);
		return error;
	}

	if (!starts_with(cur_line, "--")
	         || (strcmp(cur_line + 2, boundary) != 0)) {
		free(boundary);
		return WRONG_BOUNDRY;
	}
	data->written += strlen(cur_line) + 2;
	free(cur_line);

	/*
	 * go through all sent files and parse their headers and then save
	 * file_content to disc
	 */
	do {
		error = parse_file_header(data->socket, &(filename),
			    &(data->written));
		if (error) {
			free(boundary);
			return error;
		}
		/*
		 * updated requested_path to current filename, and delete the
		 * old one
		 */
		if (data->requested_path != NULL) {
			free_me = data->requested_path;
			data->requested_path = filename;
			free(free_me);
		} else {
			data->requested_path = filename;
		}
		error = save_file_from_post(data->socket, filename, boundary,
			    &(data->written), data->size);
		if (error) {
			free(boundary);
			return error;
		}
		error = get_line(data->socket, &cur_line);
		if (error) {
			free(boundary);
			return error;
		}
		data->written += strlen(cur_line) + 2;
	} while (strlen(cur_line) == 0
		    && strcmp(cur_line, "--") != 0);

	free(cur_line);
	free(boundary);

	error = send_201(data->socket, HTTP, &(tmp));

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
			str_len = (file_end - file_start) > 0 ?
			    (size_t)(file_end - file_start) :
			    0;
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
	char number;
	size_t i;
	FILE *fd;
	size_t filename_length;
	char cur_char;
	size_t file_written;
	ssize_t err;
	char buff[BUFFSIZE_READ];
	size_t cur_buf_pos;
	char *bound_buff;
	size_t cur_bound_buf_pos;

	error = STAT_OK;
	full_filename = NULL;
	full_filename = concat(concat(concat(full_filename, UPLOAD_DIR), "/"),
			    filename);

	/* in case file exists */
	number = '0';
	if (access(full_filename, F_OK) == 0) {
		filename_length = strlen(full_filename);
		full_filename = err_realloc(full_filename, filename_length + 2);
		full_filename[filename_length] = '_';
		full_filename[filename_length + 1] = number;
		full_filename[filename_length + 2] = '\0';
		/* count up until a free filename is found */
		while (access(full_filename, F_OK) == 0 && number <= '9') {
			full_filename[filename_length  + 1] = ++number;
		}
		if (number > '9') {
			free(full_filename);
			return NO_FREE_SPOT;
		}
	}

	fd = fopen(full_filename, "w+");
	if (fd == NULL) {
		err_quit(ERR_INFO, "fopen() retuned NULL");
	}

	bound_buff = NULL;
	bound_buff = concat(concat(bound_buff, "\r\n--"), boundary);

	cur_buf_pos = 0;
	cur_bound_buf_pos = 0;

	/* read as long as we are not extending max_length */
	while ((*content_read) < max_length) {
		err = recv(socket, &cur_char, (size_t)1, 0);
		(*content_read)++;
		if (err < 1) {
			error = CLOSED_CON;
			goto stop_transfer;
		}
continue_without_read:
		/* check if current character is matching the bound string */
		if (cur_char == bound_buff[cur_bound_buf_pos]) {
			cur_bound_buf_pos++;
			/*
			 * if every character has matched the bound_buff string
			 * stop
			 */
			if (cur_bound_buf_pos == strlen(bound_buff)) {
				error = STAT_OK;
				goto stop_transfer;
			}
			continue;
		/*
		 * if the cur_char is not matching write all matched (if any)
		 * to buffer
		 */
		} else if (cur_bound_buf_pos != 0) {
			for (i = 0; i < cur_bound_buf_pos; i++) {
				/* write to buffer */
				buff[cur_buf_pos] = bound_buff[i];
				/*
				 * lets see if we extend the buffer, if so write
				 * content to file
				 */
				if (++cur_buf_pos == BUFFSIZE_READ) {
					file_written = fwrite(buff, 1, cur_buf_pos, fd);
					if (file_written != cur_buf_pos) {
						error = FILE_ERROR;
						goto stop_transfer;
					}
					cur_buf_pos = 0;
				}
			}
			cur_bound_buf_pos = 0;
			goto continue_without_read;
		}
		buff[cur_buf_pos] = cur_char;
		/*
		 * lets see if we extend the buffer, if so write
		 * content to file
		 */
		if (++cur_buf_pos == BUFFSIZE_READ) {
			file_written = fwrite(buff, 1, cur_buf_pos, fd);
			if (file_written != cur_buf_pos) {
				error = FILE_ERROR;
				goto stop_transfer;
			}
			cur_buf_pos = 0;
		}
	}
	/*
	 * if content_read >= max_length a error occured, since there
	 * should be a boundry before we read content_length from stream.
	 * and there is a -- after the last boundary
	 */
	error = WRONG_BOUNDRY;

stop_transfer:
	free(bound_buff);
	/* check if data is left in buffer */
	if (cur_buf_pos != 0) {
		file_written = fwrite(buff, 1, cur_buf_pos, fd);
		if (file_written != cur_buf_pos) {
			error = FILE_ERROR;
			goto stop_transfer;
		}
	}
	fclose(fd);
	if (error) {
		unlink(full_filename);
	}
	free(full_filename);

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
		err = recv(socket, &cur_char, (size_t)1, 0);
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

