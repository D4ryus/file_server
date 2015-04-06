#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "handle_request.h"
#include "file_list.h"
#include "config.h"

// BUFFSIZE_READ - 1 bytes are read from socket
#define BUFFSIZE_READ 2048

void
*handle_request(void *ptr)
{
        struct data_store *data;

        data = (struct data_store*)ptr;
        if (data->socket < 0) {
                err_quit(__FILE__, __LINE__, __func__, "socket in handle_request is < 0");
        }

        print_info(data, "accepted", "");

        char read_buffer[BUFFSIZE_READ];
        char message_buffer[64];
        enum err_status status;

        status = OK;

        status = read_request(data->socket, read_buffer, BUFFSIZE_READ);
        if (status != OK) {
                goto exit;
        }

        status = parse_request(read_buffer, &(data->req_type), data->url, 255);
        if (status != OK) {
                goto exit;
        }

        generate_response(data);

        status = send_text(data->socket, data->head, strlen(data->head));
        if (status != OK) {
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
                        status = send_text(data->socket, data->body, data->body_length);
                        break;
        }
        if (status != OK) {
                goto exit;
        }

        switch (data->body_type) {
                case DATA:
                case TEXT:
                        sprintf(message_buffer, "%-20s size: %12lub",
                                             data->url,
                                             data->body_length);
                        break;
                case ERR_404:
                        sprintf(message_buffer, "404 requested: %-20s",
                                             data->url);
                        break;
                case ERR_403:
                        sprintf(message_buffer, "403 requested: %-20s",
                                             data->url);
                        break;
                default:
                        strncpy(message_buffer, "no body_type set", 17);
                        break;
        }
        print_info(data, "sent", message_buffer);

        /**
         * thread exit point, if status was set to error it will be printed,
         * socket will be closed and memory will be cleaned b4 thread exists
         */
exit:
        switch (status) {
                case WRITE_CLOSED:
                        print_info(data, "error", "could not write, client closed connection");
                        break;
                case ZERO_WRITTEN:
                        print_info(data, "error", "could not write, 0 bytes written");
                        break;
                case READ_CLOSED:
                        print_info(data, "error", "could not read");
                        break;
                case EMPTY_MESSAGE:
                        print_info(data, "error", "empty message");
                        break;
                case INV_GET:
                        print_info(data, "error", "invalid GET");
                        break;
                default:
                        break;
        }

        close(data->socket);
        free_data_store(data);

        return NULL;
}

int
read_request(int socket, char* buffer, size_t size)
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

        return OK;
}

int
parse_request(char *request, enum request_type *req_type, char* url, const size_t size)
{
        char   *tmp;
        size_t length;

        memset(url, '\0', size);

        tmp = strtok(request, "\n"); /* get first line */

        /* check if GET is valid */
        if (tmp == NULL || !starts_with(tmp, "GET /")) {
                *req_type = PLAIN;
                return INV_GET;
        }
        tmp = strtok(tmp, " ");  /* split first line */
        tmp = strtok(NULL, " "); /* get requested url */

        length = strlen(tmp);
        /* remove trailing / from request */
        while (length > 1 && tmp[length - 1] == '/') {
                tmp[length - 1] = '\0';
                length = strlen(tmp);
        }

        strncpy(url, tmp, size);

        /* get requested type */
        tmp = strtok(NULL, " ");
        /* if none given go with PLAIN and return */
        if (tmp == NULL) {
                *req_type = PLAIN;
                return OK;
        }
        if (starts_with(tmp, "HTTP/1.1") == 0 || starts_with(tmp, "HTTP/1.0") == 0) {
                *req_type = HTTP;
        } else {
                *req_type = PLAIN;
        }

        return OK;
}

void
generate_response(struct data_store *data)
{
        char *full_requested_path;
        char *requested_path;
        size_t length;

        if (strcmp(data->url, "/") == 0) {
                generate_200_directory(data, ROOT_DIR);
                return;
        }
        if (strlen(data->url) == 0) {
                generate_404(data);
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

void
generate_200_file(struct data_store *data, char* file)
{
        struct stat sb;
        char content_length[128];

        if (stat(file, &sb) == -1) {
                err_quit(__FILE__, __LINE__, __func__, "stat() retuned -1");
        }

        data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: ");
        data->head = concat(data->head, get_content_encoding(strrchr(file, '.')));
        sprintf(content_length, "\r\nContent-Length: %lu\r\n\r\n", (size_t)sb.st_size);
        data->head = concat(data->head, content_length);

        data->body = concat(data->body, file);
        data->body_length = (size_t)sb.st_size;
        data->body_type = DATA;

        return;
}

void
generate_200_directory(struct data_store *data, char* directory)
{
        if (data->req_type == HTTP) {
                data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
                                                "Content-Type: text/html\r\n\r\n");
                data->body = concat(data->body, "<!DOCTYPE html><html><head>"
                               "<link href='http://fonts.googleapis.com/css?family=Iceland'"
                                     "rel='stylesheet'"
                                     "type='text/css'>"
                               "<meta http-equiv='content-type'"
                                     "content='text/html;"
                                     "charset=UTF-8'/>"
                               "</head>"
                               "<body>");
        } else {
                data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
                                                "Content-Type: text/plain\r\n\r\n");
        }

        dir_to_table(data, directory);

        if (data->req_type == HTTP) {
                data->body = concat(data->body, "</body></html>");
        }

        data->body_length = strlen(data->body);

        data->body_type = TEXT;

        return;
}

void
generate_404(struct data_store *data)
{
        data->head = concat(data->head, "HTTP/1.1 404 Not Found\r\n"
                                        "Content-Type: text/plain\r\n\r\n");
        data->body = concat(data->body, "404 - Watcha pulling here buddy?\r\n");
        data->body_length = strlen(data->body);
        data->body_type = ERR_404;

        return;
}

void
generate_403(struct data_store *data)
{
        data->head = concat(data->head, "HTTP/1.1 403 Forbidden\r\n"
                                        "Content-Type: text/plain\r\n\r\n");
        data->body = concat(data->body, "403 - U better not go down this road!\r\n");
        data->body_length = strlen(data->body);
        data->body_type = ERR_403;

        return;
}
