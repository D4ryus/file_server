#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "handle_request.h"
#include "file_list.h"

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

        char    read_buffer[BUFFSIZE_READ];
        ssize_t n;
        char    message_buffer[64];
        int     status;

        memset(read_buffer, '\0', BUFFSIZE_READ);
        n = read(data->socket, read_buffer, BUFFSIZE_READ - 1);
        if (n < 0) {
                print_info(data, "error", "could not read(), request ignored");
                close(data->socket);
                return NULL;
        }
        if (n == 0) {
                print_info(data, "error", "empty message, request ignored");
                close(data->socket);
                return NULL;
        }
        data->url = parse_request(read_buffer, &(data->req_type));
        if (data->url == NULL) {
                print_info(data, "error", "invalid GET, request ignored");
                close(data->socket);
                return NULL;
        }
        generate_response(data);
        write(data->socket, data->head, strlen(data->head));
        status = 0;

        switch (data->body_type) {
                case DATA:
                        status = send_file(data);
                        break;
                case TEXT:
                case ERR_404:
                case ERR_403:
                default:
                        status = send_text(data->socket, data->body, data->body_length);
        }

        if (status == 0) {
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
                }
                print_info(data, "sent", message_buffer);
        } else if (status == -1) {
                print_info(data, "error", "could not write(), client closed connection");
        } else if (status == -2) {
                print_info(data, "error", "could not write(), 0 bytes written");
        }

        close(data->socket);
        free_data_store(data);

        return NULL;
}

char
*parse_request(char *request, enum request_type *req_type)
{
        char   *tmp;
        char   *url;
        size_t length;

        tmp = strtok(request, "\n"); /* get first line */

        if (tmp == NULL || !starts_with(tmp, "GET /")) {
                *req_type = PLAIN;
                return NULL;
        }
        tmp = strtok(tmp, " ");  /* split first line */
        tmp = strtok(NULL, " "); /* get requested url */

        length = strlen(tmp);
        /* remove trailing / from request */
        while (length > 1 && tmp[length - 1] == '/') {
                tmp[length - 1] = '\0';
                length = strlen(tmp);
        }
        url = err_malloc(sizeof(char) * (length + 1));
        memset(url, '\0', sizeof(char) * (length + 1));
        strncpy(url, tmp, length);

        /* get requested type */
        tmp = strtok(NULL, " ");
        /* if none given go with PLAIN and return */
        if (tmp == NULL) {
                *req_type = PLAIN;
                return url;
        }
        if (starts_with(tmp, "HTTP/1.1") == 0 || starts_with(tmp, "HTTP/1.0") == 0) {
                *req_type = HTTP;
        } else {
                *req_type = PLAIN;
        }

        return url;
}

void
generate_response(struct data_store *data)
{
        char*  requested_path;
        char*  accepted_path;

        if (strcmp(data->url, "/") == 0) {
                generate_200_directory(data, ".");
                return;
        }
        if (data->url == NULL) {
                generate_404(data);
                return;
        }

        accepted_path = realpath(".", NULL);
        if (accepted_path == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "realpath() retuned NULL");
        }

        requested_path = realpath(data->url + 1, NULL);
        if (requested_path == NULL) {
                generate_404(data);
        } else if (!starts_with(requested_path, accepted_path)) {
                generate_403(data);
        } else if (is_directory(data->url + 1)) {
                generate_200_directory(data, data->url + 1);
        } else {
                generate_200_file(data, data->url + 1);
        }

        free(requested_path);
        free(accepted_path);

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

        strcat(data->head, "HTTP/1.1 200 OK\r\n"
                           "Content-Type: ");
        strcat(data->head, get_content_encoding(strrchr(file, '.')));
        sprintf(content_length, "\r\nContent-Length: %lu\r\n\r\n", (size_t)sb.st_size);
        strcat(data->head, content_length);

        data->body = concat(data->body, file);
        data->body_length = (size_t)sb.st_size;
        data->body_type = DATA;

        return;
}

void
generate_200_directory(struct data_store *data, char* directory)
{
        if (data->req_type == HTTP) {
                strcat(data->head, "HTTP/1.1 200 OK\r\n"
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
                strcat(data->head, "HTTP/1.1 200 OK\r\n"
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
        strcat(data->head, "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/plain\r\n\r\n");
        data->body = concat(data->body, "404 - Watcha pulling here buddy?\r\n");
        data->body_length = strlen(data->body);
        data->body_type = ERR_404;

        return;
}

void
generate_403(struct data_store *data)
{
        strcat(data->head, "HTTP/1.1 403 Forbidden\r\n"
                           "Content-Type: text/plain\r\n\r\n");
        data->body = concat(data->body, "403 - U better not go down this road!\r\n");
        data->body_length = strlen(data->body);
        data->body_type = ERR_403;

        return;
}
