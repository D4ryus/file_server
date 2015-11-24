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
#include "handle_post.h"

/*
 * see globals.h
 */
extern char *UPLOAD_DIR;

int
handle_post(struct client_info *data, struct http_header *http_head)
{
	enum err_status error;
	uint64_t tmp;
	char message_buffer[MSG_BUFFER_SIZE];
	char fmt_size[7];

	error = STAT_OK;

	data->size = http_head->content_length;

	error = parse_post_body(data->sock, http_head->boundary,
	    &(data->requested_path), &(data->written), &(data->size));
	if (error) {
		return error;
	}

	error = send_201(data->sock, HTTP, &tmp);
	if (error) {
		return error;
	}

	snprintf(message_buffer,
	    (size_t)MSG_BUFFER_SIZE,
	    "%s received file: %s",
	    format_size(data->size, fmt_size),
	    data->requested_path);
	msg_print_log(data, FINISHED, message_buffer);

	return STAT_OK;
}

int
parse_post_body(int sock, char *boundary, char **requested_path,
    uint64_t *written, uint64_t *max_size)
{
	enum err_status error;
	char *buff;
	char *bound_buff;
	ssize_t read_from_socket;
	char *filename;
	char *full_filename;
	size_t file_head_size;
	char *free_me;
	FILE *fd;
	size_t bound_buff_length;
	ssize_t boundary_pos;
	ssize_t tmp;
	size_t file_written;
	ssize_t offset;

	buff = err_malloc((size_t)BUFFSIZE_READ);
	if (2 * HTTP_HEADER_LINE_LIMIT > BUFFSIZE_READ) {
		die(ERR_INFO,
		    "BUFFSIZE_READ should be > 2 * HTTP_HEADER_LINE_LIMIT");
	}

	error = STAT_OK;
	bound_buff = NULL;
	filename = NULL;
	full_filename = NULL;
	free_me = NULL;
	fd = NULL;
	(*written) = 0;
	memset(buff, '\0', (size_t)BUFFSIZE_READ);

	/* check for first boundary --[boundary]\r\n */
	read_from_socket = recv(sock, buff, strlen(boundary) + 4, 0);
	if (read_from_socket < 0
	    || (size_t)read_from_socket != strlen(boundary) + 4) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	(*written) += (uint64_t)read_from_socket;

	if ((memcmp(buff, "--", (size_t)2) != 0)
	    || (memcmp(buff + 2, boundary, strlen(boundary)) != 0)
	    || (memcmp(buff + 2 + strlen(boundary), "\r\n", (size_t)2) != 0))
	{
		error = WRONG_BOUNDRY;
		goto stop_transfer;
	}

	bound_buff = concat(concat(bound_buff, "\r\n--"), boundary);
	bound_buff_length = strlen(bound_buff);
	read_from_socket = recv(sock, buff, (size_t)BUFFSIZE_READ, 0);
	if (read_from_socket < 0) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	(*written) += (uint64_t)read_from_socket;
file_head:
	/* buff contains file head */
	if ((read_from_socket == 4) && memcmp(buff, "--\r\n", (size_t)4) == 0) {
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
	read_from_socket = offset;

	/* buff contains file_content */
	while ((*written) <= (*max_size)) {
		/* check buffer for boundry */
		error = buff_contains(sock, buff, (size_t)read_from_socket,
			    bound_buff, bound_buff_length, &boundary_pos);
		if (error) {
			goto stop_transfer;
		}
		if (boundary_pos == -1) {
			/* no boundary found */
			file_written = fwrite(buff, (size_t)1,
					   (size_t)read_from_socket, fd);
			if (file_written != (size_t)read_from_socket) {
				error = FILE_ERROR;
				goto stop_transfer;
			}
			read_from_socket = recv(sock, buff,
					       (size_t)BUFFSIZE_READ, 0);
			if (read_from_socket < 0) {
				error = CLOSED_CON;
				goto stop_transfer;
			}
			(*written) += (uint64_t)read_from_socket;
			continue;
		} else {
			/* boundary is found at pos */
			file_written = fwrite(buff, (size_t)1,
					   (size_t)boundary_pos, fd);
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
				tmp = recv(sock, buff, (size_t)BUFFSIZE_READ,
					  0);
				read_from_socket = tmp;
			} else {
				memmove(buff, buff + offset,
				    (size_t)(read_from_socket - offset));
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
	free(buff);

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
buff_contains(int sock, char *haystack, size_t haystack_size, char *needle,
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
		rec = recv(sock, rest, rest_size, MSG_PEEK);
		if (rec < 0) {
			free(rest);
			return CLOSED_CON;
		}
		if ((size_t)rec != rest_size) {
			die(ERR_INFO,
			    "peek did return less than rest_size");
		}
		if (memcmp(rest, needle + needle_matched,
		        strlen(needle + needle_matched)) == 0) {
			rec = recv(sock, rest, rest_size, 0);
			free(rest);
			if ((rec < 0) || ((size_t)rec != rest_size)) {
				die(ERR_INFO,
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

	/* not reached */
	die(ERR_INFO, "i != haystack_size, should not be possible");

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
		for (;number <= '9'; number++) {
			(*found_filename)[filename_length  + 1] = number;
			/* if not found -> should be a free filename */
			if (access((*found_filename), F_OK) == -1) {
				break;
			}
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
		die(ERR_INFO, "fopen()");
	}

	return STAT_OK;
}