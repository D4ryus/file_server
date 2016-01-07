#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "config.h"
#include "misc.h"
#include "handle_request.h"
#include "msg.h"
#include "handle_post.h"

static int parse_post_body(int, int, const char *, char **, uint64_t);
static int buff_contains(int, const char *, size_t, const char *, size_t,
    ssize_t *, uint64_t *);
static int parse_file_header(const char *, size_t, size_t *, char **);
static int open_file(char **, FILE **, char *);

/*
 * html response for http status 201 (file created),
 * needed for POST messages
 */
static const char *RESPONSE_201=
"<!DOCTYPE html>"
"<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
	"<style>"
		"html, body {"
			"background-color: #303030;"
			"color: #888888;"
		"}"
		"h1, a:hover {"
			"color: #ffffff;"
		"}"
		"a {"
			"color: #44aaff;"
			"text-decoration: none;"
		"}"
	"</style>"
	"<center>"
		"<h1>its done!</h1>"
		"<body>"
		"<a href='/'>back to Main page</a><br>"
		"<a href='#' onclick='history.back()'>"
			"load up another file"
		"</a>"
		"</body>"
	"</center>"
"</html>";

int
handle_post(int msg_id, int sock, struct http_header *request)
{
	enum err_status error;
	char message_buffer[MSG_BUFFER_SIZE];
	char fmt_size[7];
	struct http_header response;
	size_t len;

	error = STAT_OK;

	if (!request->url) {
		return INV_POST_PATH;
	}

	error = parse_post_body(msg_id, sock, request->boundary, &request->url,
		    request->content_length);
	if (error) {
		return error;
	}

	len = strlen(RESPONSE_201);
	init_http_header(&response);
	response.method = RESPONSE;
	response.status = _201_Created;
	response.content_length = len;
	if (request->flags.keep_alive) {
		response.flags.keep_alive = 1;
	}
	if (request->flags.http) {
		response.content_type = TEXT_HTML;
		response.flags.http = 1;
	} else {
		response.content_type = TEXT_PLAIN;
		response.flags.http = 0;
	}

	if (request->flags.http) {
		char *header;

		header = print_header(&response);
		error = send_data(sock, header, strlen(header));
		free(header);
		if (error) {
			return error;
		}
	}
	error = send_data(sock, RESPONSE_201, len);
	if (error) {
		return error;
	}

	snprintf(message_buffer,
	    (size_t)MSG_BUFFER_SIZE,
	    "%s rx: %s",
	    format_size(request->content_length, fmt_size),
	    request->url);
	msg_print_log(msg_id, FINISHED, message_buffer);

	return STAT_OK;
}

static int
parse_post_body(int msg_id, int sock, const char *boundary, char **url,
    uint64_t max_size)
{
	enum err_status error;
	char *buff;
	char *bound_buff;
	ssize_t read_from_socket;
	char *filename;
	char *directory;
	size_t file_head_size;
	FILE *fd;
	size_t bound_buff_length;
	ssize_t boundary_pos;
	ssize_t tmp;
	size_t file_written;
	ssize_t offset;
	struct stat s;
	uint64_t *written;

	boundary_pos = 0;
	error = STAT_OK;
	bound_buff = NULL;
	filename = NULL;
	fd = NULL;

	if (memcmp(*url, "/\0", (size_t)2) == 0) {
		directory = concat(NULL, 2, CONF.root_dir, "/");
	} else {
		/* in case requested path is not / */
		buff = concat(NULL, 2, CONF.root_dir, *url);

		directory = realpath(buff, NULL);
		free(buff);
		buff = NULL;

		/* on gnux/linux this will return NULL on file not found */
		if (!directory) {
			return INV_POST_PATH;
		}

		/* but not on bsd, so check with stat */
		if (stat(directory, &s) != 0) {
			return INV_POST_PATH;
		}

		/* ok file exists, check if its a directory */
		if (!(s.st_mode & S_IFDIR)) {
			return INV_POST_PATH;
		}

		/* now check if file starts with CONF.root_dir path */
		if ((memcmp(directory, CONF.root_dir, strlen(CONF.root_dir)))
		    || strlen(CONF.root_dir) > strlen(directory)) {
			return INV_POST_PATH;
		}

		/* and update url to full path */
		free(*url);
		*url = concat(NULL, 1, directory + strlen(CONF.root_dir));
	}

	buff = malloc((size_t)BUFFSIZE_READ);
	check_mem(buff);
	memset(buff, '\0', (size_t)BUFFSIZE_READ);

	written = msg_hook_new_transfer(msg_id, *url, max_size, "rx");

	/* check for first boundary --[boundary]\r\n */
	read_from_socket = recv(sock, buff, strlen(boundary) + 4, 0);
	if (read_from_socket < 0
	    || (size_t)read_from_socket != strlen(boundary) + 4) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	*written += (uint64_t)read_from_socket;

	if ((memcmp(buff, "--", (size_t)2) != 0)
	    || (memcmp(buff + 2, boundary, strlen(boundary)) != 0)
	    || (memcmp(buff + 2 + strlen(boundary), "\r\n", (size_t)2) != 0))
	{
		error = WRONG_BOUNDRY;
		goto stop_transfer;
	}

	bound_buff = concat(bound_buff, 2, "\r\n--", boundary);
	bound_buff_length = strlen(bound_buff);
	read_from_socket = recv(sock, buff, (size_t)BUFFSIZE_READ, 0);
	if (read_from_socket < 0) {
		error = CLOSED_CON;
		goto stop_transfer;
	}
	*written += (uint64_t)read_from_socket;
file_head:
	/* buff contains file head */
	if ((read_from_socket == 4) && !memcmp(buff, "--\r\n", (size_t)4)) {
		/* upload finished */
		error = STAT_OK;
		goto stop_transfer;
	}
	error = parse_file_header(buff, (size_t)read_from_socket,
		    &file_head_size, &filename);
	if (error) {
		goto stop_transfer;
	}

	msg_hook_update_name(msg_id, filename);

	error = open_file(&filename, &fd, directory);
	if (error) {
		goto stop_transfer;
	}

	/* now move unread buff content to front and fill up with
	 * new data */
	offset = (read_from_socket - (ssize_t)file_head_size);
	memmove(buff, buff + file_head_size, (size_t)offset);
	read_from_socket = offset;

	/* buff contains file_content */
	while (*written <= max_size) {
		/* check buffer for boundry */
		error = buff_contains(sock, buff, (size_t)read_from_socket,
			    bound_buff, bound_buff_length, &boundary_pos,
			    written);
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
			*written += (uint64_t)read_from_socket;
			continue;
		} else {
			/* boundary is found at pos */
			file_written = fwrite(buff, (size_t)1,
					   (size_t)boundary_pos, fd);
			if (file_written != (size_t)boundary_pos) {
				error = FILE_ERROR;
				goto stop_transfer;
			}
			if (fd) {
				fclose(fd);
				fd = NULL;
			}

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
			*written += (uint64_t)tmp;
			goto file_head;
		}
	}
	if (*written > max_size) {
		error = CONTENT_LENGTH_EXT;
		goto stop_transfer;
	}

stop_transfer:
	if (fd) {
		fclose(fd);
		fd = NULL;
	}
	if (error) {
		unlink(filename);
	}
	if (filename) {
		free(filename);
		filename = NULL;
	}
	if (bound_buff) {
		free(bound_buff);
		bound_buff = NULL;
	}
	free(buff);
	buff = NULL;
	free(directory);
	directory = NULL;

	return error;
}

/*
 * will check given haystack for needle, if needle is matched at the end of
 * haystack the socket will be peeked, if needle is found, pos will be set on
 * its position and the rest of matched characters will be removed from socket.
 * if the needle is not found, pos will be -1
 * if error occurs its err_status will be returned, if not STAT_OK
 */
static int
buff_contains(int sock, const char *haystack, size_t haystack_size,
    const char *needle, size_t needle_size, ssize_t *pos, uint64_t *written)
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
				*pos = (ssize_t)(i - (needle_size - 1));
				return STAT_OK;
			}
		} else {
			/* if the cur pos did not match, check if first
			 * matches */
			if (haystack[i] == needle[0]) {
				needle_matched = 1;
			} else {
				needle_matched = 0;
			}
		}
	}
	/* if the loop went through and we did not match anything */
	if ((i == haystack_size) && (needle_matched == 0)) {
		*pos = -1;
		return STAT_OK;
	}

	/* if we found parts of the needle at the end of haystack */
	if ((i == haystack_size) && needle_matched) {
		rest_size = needle_size - needle_matched;
		rest = malloc(rest_size);
		check_mem(rest);
		rec = recv(sock, rest, rest_size, MSG_PEEK);
		if (rec < 0) {
			free(rest);
			rest = NULL;
			return CLOSED_CON;
		}
		check((size_t)rec != rest_size,
		    "peek (%ld) did return less than rest_size (%lu)",
			    (ulong)rec, (ulong)rest_size)

		if (!memcmp(rest, needle + needle_matched,
		        strlen(needle + needle_matched))) {
			rec = recv(sock, rest, rest_size, 0);
			free(rest);
			rest = NULL;

			check((rec < 0) || ((size_t)rec != rest_size),
			    "the data i just peeked is gone")

			*written += (uint64_t)rec;
			*pos = (ssize_t)(haystack_size - needle_matched);
			return STAT_OK;
		} else {
			free(rest);
			rest = NULL;
			*pos = -1;
			return STAT_OK;
		}
	}

	/* not reached */
	die("i != haystack_size, should not be possible");
}

/*
 * parses the http header of a file and sets filename accordingly.
 * content_read is counted up by size of read bytes..
 * if filename is missing or not found POST_NO_FILENAME is returned.
 */
static int
parse_file_header(const char *buff, size_t buff_size, size_t *file_head_size,
    char **filename)
{
	char *header;
	char *filename_start;
	char *filename_end;
	char *end_of_head;
	size_t str_len;

	header = NULL;
	filename_start = NULL;
	filename_end = NULL;
	end_of_head = NULL;
	str_len = 0;

	/* copy the buffer and terminate it with a \0 */
	header = malloc(buff_size + 1);
	check_mem(header);
	memcpy(header, buff, buff_size);
	header[buff_size] = '\0';

	/* lets search for the end of given header and terminate after it*/
	end_of_head = strstr(header, "\r\n\r\n");
	if (!end_of_head) {
		free(header);
		header = NULL;
		return FILE_HEAD_LINE_EXT;
	}
	end_of_head[4] = '\0';

	filename_start = strstr(header, "Content-Disposition: form-data;");
	if (!filename_start) {
		free(header);
		header = NULL;
		return NO_CONTENT_DISP;
	}

	/* found the line containing the filname, now extract it */

	filename_start = strstr(filename_start, "filename=\"");
	if (!filename_start) {
		free(header);
		header = NULL;
		return POST_NO_FILENAME;
	}
	/* point to first character of filename */
	filename_start += strlen("filename=\"");

	/* find the end of filename */
	filename_end = strchr(filename_start, '"');
	if (!filename_end) {
		free(header);
		header = NULL;
		return FILENAME_ERR;
	}

	str_len = (filename_end - filename_start) > 0 ?
	    (size_t)(filename_end - filename_start) : 0;
	if (str_len == 0) {
		free(header);
		header = NULL;
		return FILENAME_ERR;
	}

	/*
	 * ok we reach this point we found a correct filename and the end of
	 * header, now set filename and file_head_size and return
	 */

	if (*filename) {
		free(*filename);
	}
	*filename = malloc(str_len + 1);
	check_mem(*filename);
	memset(*filename, '\0', str_len + 1);
	memcpy(*filename, filename_start, str_len);

	*file_head_size = strlen(header);

	free(header);
	header = NULL;

	return STAT_OK;
}

static int
open_file(char **filename, FILE **fd, char *directory)
{
	char *found_filename;

	if (directory[strlen(directory)] == '/') {
		found_filename = concat(NULL, 2, directory, *filename);
	} else {
		found_filename = concat(NULL, 3, directory, "/", *filename);
	}

	/* in case file exists */
	if (access(found_filename, F_OK) == 0) {
		char number;
		size_t filename_length;

		filename_length = strlen(found_filename);
		found_filename = realloc(found_filename,
				     filename_length + 3);
		check_mem(found_filename);
		found_filename[filename_length] = '.';
		found_filename[filename_length + 2] = '\0';
		/* count up until a free filename is found */
		for (number = '0'; number <= '9'; number++) {
			found_filename[filename_length + 1] = number;
			/* if not found -> should be a free filename */
			if (access(found_filename, F_OK) == -1) {
				break;
			}
		}
		/* in case we went through and did not find a free name */
		if (number > '9') {
			free(found_filename);
			found_filename = NULL;
			*fd = NULL;
			return NO_FREE_SPOT;
		}
	}

	*fd = fopen(found_filename, "w+");
	check(!*fd, "fopen(\"%s\") returned NULL", found_filename);
	free(*filename);
	*filename = malloc(strlen(found_filename) + 1);
	check_mem(*filename);
	memcpy(*filename, found_filename, strlen(found_filename) + 1);
	free(found_filename);
	found_filename = NULL;

	return STAT_OK;
}
