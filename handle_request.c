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
/* FILESIZE_ZERO      */ "filesize is 0 or error ocurred",
/* WRONG_BOUNDRY      */ "wrong boundry specified",
/* HTTP_HEAD_LINE_EXT */ "http header extended line limit",
/* FILE_HEAD_LINE_EXT */ "file header extended line limit",
/* POST_NO_FILENAME   */ "missing filename in post message",
/* NO_FREE_SPOT       */ "the posted filename already exists (10 times)",
/* FILE_ERROR         */ "could not write the post content to file",
/* NO_CONTENT_DISP    */ "Content-Disposition missing",
/* FILENAME_ERR       */ "could not parse filename",
/* CONTENT_LENGTH_EXT */ "content length extended"
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
	int call_back_socket;

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

	if (starts_with(cur_line, "POST", 4)) {
		if (UPLOAD_DIR != NULL) {
			error = handle_post(data, cur_line);
		} else {
			error = INV_REQ_TYPE;
		}
	} else if (starts_with(cur_line, "GET", 3)) {
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

	call_back_socket = data->socket;
	msg_hook_cleanup(data);

	sleep(5);
	close(call_back_socket);

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
	enum err_status error;
	char *boundary;
	char *content_length;
	uint64_t tmp;

	error = STAT_OK;

	if (strcmp(request, "POST / HTTP/1.1") != 0) {
		return INV_POST;
	}

	error = parse_post_header(data->socket, &boundary, &content_length);
	if (error) {
		return error;
	}

	data->size = err_string_to_val(content_length);
	free(content_length);
	if (data->size == 0) {
		free(boundary);
		return FILESIZE_ZERO;
	}

	error = parse_post_body(data->socket, boundary, &(data->requested_path),
		    &(data->written), &(data->size));
	free(boundary);
	if (error) {
		return error;
	}
	/* END */

	error = send_201(data->socket, HTTP, &(tmp));
	if (error) {
		return error;
	}

	msg_print_info(data, SENT, "POST successfull", -1);

	return STAT_OK;
}

int
parse_post_header(int socket, char **boundary, char **content_length)
{
	char *cur_line;
	enum err_status error;
	size_t str_len;
	char *STR_COL;
	char *STR_BOU;

	STR_COL = "Content-Length: ";
	STR_BOU = "Content-Type: multipart/form-data; boundary=";
	error = STAT_OK;
	(*boundary) = NULL;
	(*content_length) = NULL;

	error = get_line(socket, &cur_line);
	if (error) {
		return error;
	}

	while (strlen(cur_line) != 0) {
		if (starts_with(cur_line, STR_COL, strlen(STR_COL))) {
			str_len = strlen(cur_line + strlen(STR_COL));
			if (str_len - strlen(STR_COL) < 1) {
				error = CON_LENGTH_MISSING;
				break;
			}
			(*content_length) = err_malloc(str_len + 1);
			strncpy((*content_length), cur_line
			    + strlen(STR_COL), str_len + 1);
		}
		if (starts_with(cur_line, STR_BOU, strlen(STR_BOU))) {
			str_len = strlen(cur_line + strlen(STR_BOU));
			if (str_len - strlen(STR_BOU) < 1) {
				error = BOUNDARY_MISSING;
				break;
			}
			(*boundary) = err_malloc(str_len + 1);
			strncpy((*boundary), cur_line
			    + strlen(STR_BOU), str_len + 1);
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

	if (error) {
		if ((*content_length) != NULL) {
			free((*content_length));
			(*content_length) = NULL;
		}
		if ((*boundary) != NULL) {
			free((*boundary));
			(*boundary) = NULL;
		}
	}

	return error;
}

int
parse_post_body(int socket, char *boundary, char **requested_path,
    uint64_t *written, uint64_t *max_size)
{
	enum err_status error;
	char buff[BUFFSIZE_READ];
	char *bound_buff;
	ssize_t read_from_socket;
	char *filename;
	char *full_filename;
	size_t file_head_size;
	char *free_me;
	FILE *fd;
	size_t bound_buff_length;
	ssize_t boundary_pos;
	/* just a tmp variable, feel free to use it */
	ssize_t tmp;
	size_t file_written;
	ssize_t offset;

	if (2 * HTTP_HEADER_LINE_LIMIT > BUFFSIZE_READ) {
		err_quit(ERR_INFO,
		    "BUFFSIZE_READ should be > 2 * HTTP_HEADER_LINE_LIMIT");
	}

	error = STAT_OK;
	bound_buff = NULL;
	filename = NULL;
	full_filename = NULL;
	free_me = NULL;
	fd = NULL;
	(*written) = 0;
	memset(buff, '\0', BUFFSIZE_READ);

	/* check for first boundary --[boundary]\r\n */
	read_from_socket = recv(socket, buff, strlen(boundary) + 4, 0);
	if (read_from_socket < 0
	    || (size_t)read_from_socket != strlen(boundary) + 4) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	(*written) += (uint64_t)read_from_socket;

	if (!starts_with(buff, "--", 2)
	    || !starts_with(buff + 2, boundary, strlen(boundary))
	    || !starts_with(buff + 2 + strlen(boundary), "\r\n", 2))
	{
		error = WRONG_BOUNDRY;
		goto stop_transfer;
	}

	bound_buff = concat(concat(bound_buff, "\r\n--"), boundary);
	bound_buff_length = strlen(bound_buff);
	read_from_socket = recv(socket, buff, BUFFSIZE_READ, 0);
	if (read_from_socket < 0) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	(*written) += (uint64_t)read_from_socket;
file_head:
	/* buff contains file head */
	if ((read_from_socket == 4) && starts_with(buff, "--\r\n", 4)) {
		error = STAT_OK;
		goto stop_transfer;
	}
	error = parse_file_header(buff, (size_t)read_from_socket,
		    &file_head_size, &filename);
	if (error) {
		goto stop_transfer;
	}

	if (full_filename != NULL) {
		free(full_filename);
		full_filename = NULL;
	}
	error = open_file(filename, &fd, &full_filename);
	if (error) {
		goto stop_transfer;
	}
	/* set the requested_path, if there was already one free it savely */
	if ((*requested_path) != NULL) {
		free_me = (*requested_path);
		(*requested_path) = filename;
		free(free_me);
		filename = NULL;
	} else {
		(*requested_path) = filename;
		filename = NULL;
	}

	/* now move unread buff content to front and fill up with new data */
	offset = (read_from_socket - (ssize_t)file_head_size);
	memmove(buff, buff + file_head_size, (size_t)offset);
	tmp = recv(socket, buff + offset, BUFFSIZE_READ - (size_t)offset, 0);
	if (tmp < 0) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	(*written) += (uint64_t)tmp;
	read_from_socket = offset + tmp;

	/* buff contains file_content */
	while ((*written) <= (*max_size)) {
		/* check buffer for boundry */
		error = buff_contains(socket, buff, (size_t)read_from_socket,
			    bound_buff, bound_buff_length, &(boundary_pos));
		if (error) {
			goto stop_transfer;
		}
		if (boundary_pos == -1) {
			/* no boundary found */
			file_written = fwrite(buff, 1, (size_t)read_from_socket, fd);
			if (file_written != (size_t)read_from_socket) {
				error = FILE_ERROR;
				goto stop_transfer;
			}
			read_from_socket = recv(socket, buff, BUFFSIZE_READ, 0);
			if (read_from_socket < 0) {
				error = CLOSED_CON;
				goto stop_transfer;
			}
			(*written) += (uint64_t)read_from_socket;
			continue;
		} else {
			/* boundary is found at pos */
			file_written = fwrite(buff, 1, (size_t)boundary_pos, fd);
			if (file_written != (size_t)boundary_pos) {
				error = FILE_ERROR;
				goto stop_transfer;
			}
			fclose(fd);
			fd = NULL;
			free(full_filename);
			full_filename = NULL;

			/* buff + offset is first byte from new file */
			offset = boundary_pos + (ssize_t)bound_buff_length;
			if (read_from_socket <= offset) {
				tmp = recv(socket, buff, BUFFSIZE_READ, 0);
				read_from_socket = tmp;
			} else {
				memmove(buff, buff + offset, (size_t)(read_from_socket - offset));
				tmp = 0;
				read_from_socket = read_from_socket - offset;
			}
			if (tmp < 0) {
				error = CLOSED_CON;
				goto stop_transfer;
			}
			(*written) += (uint64_t)tmp;
			goto file_head;
		}
	}
	if ((*written) > (*max_size)) {
		error = CONTENT_LENGTH_EXT;
		goto stop_transfer;
	}

stop_transfer:
	if (full_filename != NULL) {
		unlink(full_filename);
		free(full_filename);
	}
	if (fd != NULL) {
		fclose(fd);
	}
	if (bound_buff != NULL) {
		free(bound_buff);
	}
	if (filename != NULL) {
		free(filename);
	}

	return error;
}

/*
 * will check given haystack for needle, if needle is matched at the end of
 * haystack the socket will be peeked, if needle is found, pos will be set on
 * its position and the rest of matched characters will be removed from socket.
 * if the needle is not found, pos will be -1
 * if error occurs its err_status will be returned, if not STAT_OK
 */
int
buff_contains(int socket, char *haystack, size_t haystack_size, char *needle,
    size_t needle_size, ssize_t *pos)
{
	size_t i;
	size_t needle_matched;
	size_t rest_size;
	char *rest;
	ssize_t rec;

	for (i = 0, needle_matched = 0; i < haystack_size; i++) {
		/* if positions match, we found a match */
		if (haystack[i] == needle[needle_matched]) {
			needle_matched++;
			/* if we found the full needle inside the haystack */
			if (needle_matched == needle_size) {
				(*pos) = (ssize_t)(i - (needle_size - 1));
				return STAT_OK;
			}
		} else {
			/* if the cur pos didnt match, check if first matches */
			if (haystack[i] == needle[0]) {
				needle_matched = 1;
			} else {
				needle_matched = 0;
			}
		}
	}
	/* if the loop went through and we did not match anything */
	if ((i == haystack_size) && (needle_matched == 0)) {
		(*pos) = -1;
		return STAT_OK;
	}

	/* if we found parts of the needle at the end of haystack */
	if ((i == haystack_size) && (needle_matched != 0)) {
		rest_size = needle_size - needle_matched;
		rest = err_malloc(rest_size);
		rec = recv(socket, rest, rest_size, MSG_PEEK);
		/* check for error */
		if (rec < 0) {
			free(rest);
			return CLOSED_CON;
		}
		if ((size_t)rec != rest_size) {
			err_quit(ERR_INFO,
			    "peek did return less than rest_size");
		}
		if (starts_with(rest, needle + needle_matched,
		        strlen(needle + needle_matched))) {
			rec = recv(socket, rest, rest_size, 0);
			free(rest);
			if ((rec < 0) || ((size_t)rec != rest_size)) {
				err_quit(ERR_INFO,
				    "the data i just peeked is gone");
			}
			(*pos) = (ssize_t)(haystack_size - needle_matched);
			return STAT_OK;
		} else {
			free(rest);
			(*pos) = -1;
			return STAT_OK;
		}
	}

	err_quit(ERR_INFO, "i != haystack_size, should not be possible");
	/* not reached */

	return STAT_OK;
}

/*
 * parses the http header of a file and sets filename accordingly.
 * content_read is counted up by size of read bytes..
 * if filename is missing or not found POST_NO_FILENAME is returned.
 */
int
parse_file_header(char *buff, size_t buff_size, size_t *file_head_size,
    char **filename)
{
	char *header;
	char *filename_start;
	char *filename_end;
	char *end_of_head;
	size_t str_len;

	(*filename) = NULL;
	header = NULL;
	filename_start = NULL;
	filename_end = NULL;
	end_of_head = NULL;
	str_len = 0;

	/* copy the buffer and terminate it with a \0 */
	header = err_malloc(buff_size + 1);
	memcpy(header, buff, buff_size);
	header[buff_size] = '\0';

	/* lets search for the end of given header and terminate after it*/
	end_of_head = strstr(header, "\r\n\r\n");
	if (end_of_head == NULL) {
		free(header);
		return FILE_HEAD_LINE_EXT;
	}
	end_of_head[4] = '\0';

	filename_start = strstr(header, "Content-Disposition: form-data;");
	if (filename_start == NULL) {
		free(header);
		return NO_CONTENT_DISP;
	}

	/* found the line containing the filname, now extract it */

	filename_start = strstr(filename_start, "filename=\"");
	if (filename_start == NULL) {
		free(header);
		return POST_NO_FILENAME;
	}
	/* point to first character of filename */
	filename_start += strlen("filename=\"");

	/* find the end of filename */
	filename_end = strchr(filename_start, '"');
	if (filename_end == NULL) {
		free(header);
		return FILENAME_ERR;
	}

	str_len = (filename_end - filename_start) > 0 ?
	    (size_t)(filename_end - filename_start) : 0;
	if (str_len == 0) {
		free(header);
		return FILENAME_ERR;
	}

	/*
	 * ok we reach this point we found a correct filename and the end of
	 * header, now set filename and file_head_size and return
	 */

	(*filename) = err_malloc(str_len + 1);
	memset((*filename), '\0', str_len + 1);
	memcpy((*filename), filename_start, str_len);

	(*file_head_size) = strlen(header);

	free(header);

	return STAT_OK;
}

int
open_file(char *filename, FILE **fd, char **found_filename)
{
	char number;
	size_t filename_length;

	(*found_filename) = NULL;
	(*found_filename) = concat(concat(concat((*found_filename),
					      UPLOAD_DIR), "/"),
				       filename);

	/* in case file exists */
	if (access((*found_filename), F_OK) == 0) {
		number = '0';
		filename_length = strlen((*found_filename));
		(*found_filename) = err_realloc((*found_filename),
				        filename_length + 2);
		(*found_filename)[filename_length] = '_';
		(*found_filename)[filename_length + 1] = number;
		(*found_filename)[filename_length + 2] = '\0';
		/* count up until a free filename is found */
		while (access((*found_filename), F_OK) == 0 && number <= '9') {
			(*found_filename)[filename_length  + 1] = ++number;
		}
		if (number > '9') {
			free((*found_filename));
			(*found_filename) = NULL;
			fd = NULL;
			return NO_FREE_SPOT;
		}
	}

	(*fd) = fopen((*found_filename), "w+");
	if ((*fd) == NULL) {
		err_quit(ERR_INFO, "fopen() retuned NULL");
	}

	return STAT_OK;
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
	if (tmp == NULL || !starts_with(tmp, "GET /", 5)) {
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
		if (starts_with(tmp + i, "%20", 3)) {
			(*requested_path)[url_pos] = ' ';
			i += 2;
		} else if (starts_with(tmp + i, "%21", 3)) {
			(*requested_path)[url_pos] = '!';
			i += 2;
		} else if (starts_with(tmp + i, "%23", 3)) {
			(*requested_path)[url_pos] = '#';
			i += 2;
		} else if (starts_with(tmp + i, "%24", 3)) {
			(*requested_path)[url_pos] = '$';
			i += 2;
		} else if (starts_with(tmp + i, "%25", 3)) {
			(*requested_path)[url_pos] = '%';
			i += 2;
		} else if (starts_with(tmp + i, "%26", 3)) {
			(*requested_path)[url_pos] = '&';
			i += 2;
		} else if (starts_with(tmp + i, "%27", 3)) {
			(*requested_path)[url_pos] = '\'';
			i += 2;
		} else if (starts_with(tmp + i, "%28", 3)) {
			(*requested_path)[url_pos] = '(';
			i += 2;
		} else if (starts_with(tmp + i, "%29", 3)) {
			(*requested_path)[url_pos] = ')';
			i += 2;
		} else if (starts_with(tmp + i, "%2B", 3)) {
			(*requested_path)[url_pos] = '+';
			i += 2;
		} else if (starts_with(tmp + i, "%2C", 3)) {
			(*requested_path)[url_pos] = ',';
			i += 2;
		} else if (starts_with(tmp + i, "%2D", 3)) {
			(*requested_path)[url_pos] = '-';
			i += 2;
		} else if (starts_with(tmp + i, "%2E", 3)) {
			(*requested_path)[url_pos] = '.';
			i += 2;
		} else if (starts_with(tmp + i, "%3B", 3)) {
			(*requested_path)[url_pos] = ';';
			i += 2;
		} else if (starts_with(tmp + i, "%3D", 3)) {
			(*requested_path)[url_pos] = '=';
			i += 2;
		} else if (starts_with(tmp + i, "%40", 3)) {
			(*requested_path)[url_pos] = '@';
			i += 2;
		} else if (starts_with(tmp + i, "%5B", 3)) {
			(*requested_path)[url_pos] = '[';
			i += 2;
		} else if (starts_with(tmp + i, "%5D", 3)) {
			(*requested_path)[url_pos] = ']';
			i += 2;
		} else if (starts_with(tmp + i, "%5E", 3)) {
			(*requested_path)[url_pos] = '^';
			i += 2;
		} else if (starts_with(tmp + i, "%5F", 3)) {
			(*requested_path)[url_pos] = '_';
			i += 2;
		} else if (starts_with(tmp + i, "%60", 3)) {
			(*requested_path)[url_pos] = '`';
			i += 2;
		} else if (starts_with(tmp + i, "%7B", 3)) {
			(*requested_path)[url_pos] = '{';
			i += 2;
		} else if (starts_with(tmp + i, "%7D", 3)) {
			(*requested_path)[url_pos] = '}';
			i += 2;
		} else if (starts_with(tmp + i, "%7E", 3)) {
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
	if ((starts_with(tmp, "HTTP/1.1", 8))
	    || (starts_with(tmp, "HTTP/1.0", 8))) {
		(*req_type) = HTTP;
	} else {
		(*req_type) = PLAIN;
	}

	return STAT_OK;
}

/*
 * gets a line from socket, and writes it to buffer
 * buffer will be err_malloc'ed by get_line.
 * returns STAT_OK CLOSED_CON or HTTP_HEAD_LINE_EXT
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
		if (err < 0) {
			free((*buff));
			(*buff) = NULL;
			return CLOSED_CON;
		}

		if (bytes_read >= buf_size) {
			buf_size += 128;
			if (buf_size > HTTP_HEADER_LINE_LIMIT) {
				free((*buff));
				(*buff) = NULL;
				return HTTP_HEAD_LINE_EXT;
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
	} else if (!starts_with(requested_path, ROOT_DIR, strlen(ROOT_DIR))
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

