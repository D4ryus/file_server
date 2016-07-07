#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdint.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "handle_request.h"
#include "file_list.h"
#include "config.h"
#include "misc.h"
#include "msg.h"
#include "http.h"

static int parse_post_body(int, int, const char *, char **, uint64_t);
static int buff_contains(int, const char *, size_t, const char *, size_t,
    ssize_t *, uint64_t *);
static int parse_file_header(const char *, size_t, size_t *, char **);
static int open_file(char **, FILE **, char *);
static void generate_response_header(struct http_header *,
    struct http_header *);
static enum http_status get_response_status(char **);

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
		"</body>"
	"</center>"
"</html>";

/*
 * plaintext message which will be sent on 403 - Forbidden
 */
static const char *RESPONSE_403 =
	"403 - U better not go down this road!\r\n";

/*
 * plaintext message which will be sent on 404 - File not found
 */
static const char *RESPONSE_404 =
	"404 - Watcha pulling here buddy?\r\n";

/*
 * plaintext message which will be sent on 405 - Method Not Allowed
 */
static const char *RESPONSE_405 =
	"405 - ur Method is not allowed sir, if tried to "
	"POST, ask for permission.\r\n";

/*
 * function where each thread starts execution with given client_info
 */
void *
handle_request(void *ptr)
{
	struct client_info *data;
	enum err_status error;
	struct http_header request;
	struct http_header response;
	int msg_id;
	int sock;
	char ip[16];
	char fmt_size[7];
	char *filename;
	struct stat sb;
	uint64_t *written;
	char *dir_body;
	char *type;
	char *name;
	int is_dir;

	/* move values to stack and delete memory from heap */
	data = (struct client_info *)ptr;
	sock = data->sock;
	strncpy(ip, data->ip, (size_t)16);
	free(data);
	data = NULL;

	check(sock < 0, "socket in handle_request is %d", sock);

	written = NULL;
	type = NULL;
	name = NULL;
	filename = NULL;
	dir_body = NULL;

	msg_id = msg_hook_add(ip);
	msg_print_log(msg_id, 2, "connected.");

keep_alive:
	error = STAT_OK;
	init_http_header(&request);

	/* check if ip is blocked */
	if (!ip_matches(CONF.ip, ip)) {
		error = IP_BLOCKED;
		goto disconnect;
	}

	error = parse_header(&request, sock);
	if (error) {
		goto disconnect;
	}

	if (request.method == RESPONSE) {
		error = INV_REQ_TYPE;
		goto disconnect;
	}

	init_http_header(&response);
	generate_response_header(&request, &response);

	switch (request.method) {
	case GET:
	case HEAD:
		filename = concat(filename, 2, CONF.root_dir, request.url);
		break;
	case POST:
		if (ip_matches(CONF.upload_ip, ip)) {
			error = parse_post_body(msg_id, sock, request.boundary,
			    &request.url, request.content_length);
			if (error) {
				goto disconnect;
			}
			format_size(request.content_length, fmt_size);
			msg_print_log(msg_id, 1, "%s rx: %s", fmt_size,
			    request.url);
			response.status = _201_Created;
		} else {
			response.status = _405_Method_Not_Allowed;
		}
		break;
	default:
		error = INV_REQ_TYPE;
		goto disconnect;
	}

	/* check if its a regular file or a directory */
	if (response.status == _200_OK && is_directory(filename)) {
		is_dir = 1;
	} else {
		is_dir = 0;
	}

	/* this big _if_ statement will set the correct
	 * response.content_length, name and type, switch wont work
	 * cause of is_dir :( */
	if (is_dir) {
		response.content_type = TEXT_HTML;
		dir_body = dir_to_table(response.flags.http, request.url,
			   ip_matches(CONF.upload_ip, ip));
		response.content_length = strlen(dir_body);
		name = request.url;
		type = "tx";
	} else if (response.status == _200_OK) {
		check(stat(filename, &sb) == -1, "stat(\"%s\") returned -1",
		    filename);
		response.content_length = (uint64_t)sb.st_size;
		name = request.url;
		type = "tx";
	} else if (response.status ==_201_Created) {
		response.content_length = strlen(RESPONSE_201);
		response.content_type = TEXT_HTML;
		name = "201";
		type = "tx";
	} else if (response.status ==_206_Partial_Content) {
		check(stat(filename, &sb) == -1, "stat(\"%s\") returned -1",
		    filename);
		if (response.range.to == 0) {
			response.range.to = (uint64_t)(sb.st_size - 1);
		}
		if (response.range.size == 0) {
			response.range.size = (uint64_t)sb.st_size;
		}
		response.content_length = response.range.to
					- response.range.from + 1;
		response.range.size = (uint64_t)sb.st_size;
		name = request.url;
		type = "px";
	} else if (response.status ==_403_Forbidden) {
		response.content_length = strlen(RESPONSE_403);
		response.content_type = TEXT_PLAIN;
		name = "403";
		type = "tx";
	} else if (response.status ==_404_Not_Found) {
		response.content_length = strlen(RESPONSE_404);
		response.content_type = TEXT_PLAIN;
		name = "404";
		type = "tx";
	} else if (response.status ==_405_Method_Not_Allowed) {
		response.content_length = strlen(RESPONSE_405);
		response.content_type = TEXT_PLAIN;
		name = "405";
		type = "tx";
	} else {
		/* not reached */
		die("this should not happen, response.status: %d",
		    response.status);
	}

	/* now send out the response header if its http request */
	if (request.flags.http) {
		char *header;

		header = print_header(&response);
		error = send_data(sock, header, strlen(header), NULL);
		free(header);
		if (error) {
			goto disconnect;
		}
	}

	/* now add a transfer hook */
	if (request.method != HEAD) {
		written = msg_hook_new_transfer(msg_id, name,
		    response.content_length, type);
	}

	/* and send the actual response (body) */
	if (request.method == HEAD) {
		/* if method HEAD, dont send body, but dont forget to
		 * cleanup dir_body if it is a dir*/
		if (is_dir) {
			free(dir_body);
			dir_body = NULL;
		}
	} else if (is_dir) {
		error = send_data(sock, dir_body, response.content_length,
		    written);
		free(dir_body);
		dir_body = NULL;
	} else if (response.status == _200_OK) {
		error = send_file(sock, filename, written, (uint64_t)0,
			    response.content_length - 1);
	} else if (response.status == _201_Created) {
		error = send_data(sock, RESPONSE_201, response.content_length,
		    written);
	} else if (response.status ==_206_Partial_Content) {
		error = send_file(sock, filename, written, response.range.from,
			    response.range.to);
	} else if (response.status ==_403_Forbidden) {
		error = send_data(sock, RESPONSE_403, response.content_length,
		    written);
	} else if (response.status ==_404_Not_Found) {
		error = send_data(sock, RESPONSE_404, response.content_length,
		    written);
	} else if (response.status == _405_Method_Not_Allowed) {
		error = send_data(sock, RESPONSE_405, response.content_length,
		    written);
	} else {
		die("this should not happen, response.status: %d",
		    response.status);
	}

	if (error) {
		goto disconnect;
	}

	free(filename);
	filename = NULL;

	format_size(response.content_length, fmt_size);
	msg_print_log(msg_id, 1, "%s %s: %s", fmt_size, type, name);

	if (sock != 0 && request.flags.keep_alive) {
		delete_http_header(&request);
		delete_http_header(&response);
		goto keep_alive;
	}

disconnect:
	if (filename) {
		free(filename);
		filename = NULL;
	}
	if (error) {
		msg_print_log(msg_id, 2, "disconnected. (%s)",
		    get_err_msg(error));
	} else {
		msg_print_log(msg_id, 2, "disconnected.");
	}
	delete_http_header(&request);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	msg_hook_rem(msg_id);

	return NULL;
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

static void
generate_response_header(struct http_header *request,
    struct http_header *response)
{
	response->method = RESPONSE;
	if (request->method == GET
	    || request->method == HEAD) {
		response->status = get_response_status(&request->url);
		response->content_type = get_mime_type(request->url);
	} else {
		response->content_type = TEXT_HTML;
	}

	if (request->flags.range) {
		if (response->status == _200_OK) {
			response->status = _206_Partial_Content;
		}
		response->flags.range = 1;
		response->range.from = request->range.from;
		response->range.to = request->range.to;
		response->range.size = request->range.size;
	}

	if (request->flags.keep_alive) {
		response->flags.keep_alive = 1;
	}

	if (request->flags.http) {
		response->flags.http = 1;
	}
}

static enum http_status
get_response_status(char **request)
{
	char *full_requested_path;
	char *requested_path;
	char *free_me;
	struct stat s;

	if (!*request) {
		return _404_Not_Found;
	}
	if (memcmp(*request, "/\0", (size_t)2) == 0) {
		return _200_OK;
	}

	full_requested_path = concat(NULL, 2, CONF.root_dir, *request);

	requested_path = realpath(full_requested_path, NULL);
	free(full_requested_path);
	full_requested_path = NULL;

	/* on gnux/linux this will return NULL on file not found */
	if (!requested_path) {
		return _404_Not_Found;
	}

	/* but not on bsd, so check with stat */
	if (stat(requested_path, &s) != 0) {
		free(requested_path);
		requested_path = NULL;
		return _404_Not_Found;
	}

	/* now check if file starts with CONF.root_dir path */
	if ((memcmp(requested_path, CONF.root_dir, strlen(CONF.root_dir)) != 0)
	    || strlen(CONF.root_dir) > strlen(requested_path)) {
		free(requested_path);
		requested_path = NULL;
		return _403_Forbidden;
	}

	/* ok file exists, check if its a directory */
	check(!(s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG),
	    "stat(\"%s\") is not a file nor a directory.", requested_path)

	/* and update request string to a full path */
	free_me = *request;
	*request = concat(NULL, 1, requested_path + strlen(CONF.root_dir));
	free(free_me);
	free_me = NULL;

	free(requested_path);
	requested_path = NULL;

	return _200_OK;
}
