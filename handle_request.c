#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "helper.h"
#include "handle_request.h"
#include "file_list.h"
#include "messages.h"

/**
 * see config.h
 */
extern char *ROOT_DIR;
extern int VERBOSITY;
extern const size_t MIN_STATUS_SIZE;
extern const size_t BUFFSIZE_READ;
extern const char *HTTP_TOP;
extern const char *HTTP_BOT;
extern const char *RESPONSE_404;
extern const char *RESPONSE_403;

#ifdef NCURSES
extern int USE_NCURSES;
#endif

void
*handle_request(void *ptr)
{
        struct data_store *data;

        data = (struct data_store*)ptr;
        if (data->socket < 0) {
#ifdef NCURSES
                /* TODO: that aint nice */
                print_info(data, ERROR,
                      "socket is <0, could be due to window resize singal", -1);
                free_data_store(data);
                return(NULL);
#else
                err_quit(__FILE__, __LINE__, __func__,
                                             "socket in handle_request is < 0");
#endif
        }

        print_info(data, ACCEPTED, "", -1);

        char read_buffer[BUFFSIZE_READ];
        char message_buffer[64];
        enum err_status status;
        int added_hook = 0;

        status = STAT_OK;

        status = read_request(data->socket, read_buffer, BUFFSIZE_READ);
        if (status != STAT_OK) {
                goto exit;
        }

        status = parse_request(read_buffer, &(data->req_type), &(data->url));
        if (status != STAT_OK) {
                goto exit;
        }

        generate_response(data);

#ifdef NCURSES
        if ( (USE_NCURSES || VERBOSITY >= 3)
            && data->body_length >= MIN_STATUS_SIZE) {
#else
        if (VERBOSITY >= 3 && data->body_length >= MIN_STATUS_SIZE) {
#endif
                added_hook = 1;
                add_hook(data);
        }

        status = send_text(data->socket, data->head, strlen(data->head));
        if (status != STAT_OK) {
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
                        status = send_text(data->socket, data->body,
                                                             data->body_length);
                        break;
        }
        if (status != STAT_OK) {
                goto exit;
        }

        switch (data->body_type) {
                case DATA:
                case TEXT:
                        sprintf(message_buffer, "%-20s size: %12lub",
                                                  data->url, data->body_length);
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
        print_info(data, SENT, message_buffer, -1);

        /**
         * thread exit point, if status was set to error it will be printed,
         * socket will be closed and memory will be cleaned b4 thread exists
         */
exit:
        switch (status) {
                case WRITE_CLOSED:
                        print_info(data, ERROR,
                               "could not write, client closed connection", -1);
                        break;
                case ZERO_WRITTEN:
                        print_info(data, ERROR,
                                        "could not write, 0 bytes written", -1);
                        break;
                case READ_CLOSED:
                        print_info(data, ERROR, "could not read", -1);
                        break;
                case EMPTY_MESSAGE:
                        print_info(data, ERROR, "empty message", -1);
                        break;
                case INV_GET:
                        print_info(data, ERROR, "invalid GET", -1);
                        break;
                default:
                        break;
        }

        if (added_hook) {
                remove_hook(data);
        }
        close(data->socket);
        free_data_store(data);

        return NULL;
}

int
read_request(int socket, char *buffer, size_t size)
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

        return STAT_OK;
}

int
parse_request(char *request, enum request_type *req_type, char **url)
{
        char   *tmp;
        size_t length;

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

        (*url) = err_malloc(length + 1);
        memset((*url), '\0', length + 1);
        strncpy((*url), tmp, length);

        /* get requested type */
        tmp = strtok(NULL, " ");
        /* if none given go with PLAIN and return */
        if (tmp == NULL) {
                *req_type = PLAIN;
                return STAT_OK;
        }
        if ((starts_with(tmp, "HTTP/1.1") == 0)
                                       || (starts_with(tmp, "HTTP/1.0") == 0)) {
                *req_type = HTTP;
        } else {
                *req_type = PLAIN;
        }

        return STAT_OK;
}

void
generate_response(struct data_store *data)
{
        char *full_requested_path;
        char *requested_path;
        size_t length;

        if (data->url == NULL) {
                generate_404(data);
                return;
        }
        if (strcmp(data->url, "/") == 0) {
                generate_200_directory(data, ROOT_DIR);
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
generate_200_file(struct data_store *data, char *file)
{
        struct stat sb;
        char content_length[128];

        if (stat(file, &sb) == -1) {
                err_quit(__FILE__, __LINE__, __func__, "stat() retuned -1");
        }

        if (data->req_type == HTTP) {
                data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
                                                "Content-Type: ");
                data->head = concat(data->head,
                                      get_content_encoding(strrchr(file, '.')));
                sprintf(content_length, "\r\nContent-Length: %lu\r\n\r\n",
                                                            (size_t)sb.st_size);
                data->head = concat(data->head, content_length);
        }

        data->body = concat(data->body, file);
        data->body_length = (size_t)sb.st_size;
        data->body_type = DATA;

        return;
}

void
generate_200_directory(struct data_store *data, char *directory)
{
        if (data->req_type == HTTP) {
                data->head = concat(data->head, "HTTP/1.1 200 OK\r\n"
                                             "Content-Type: text/html\r\n\r\n");
                data->body = concat(data->body, HTTP_TOP);
        }

        dir_to_table(data, directory);

        if (data->req_type == HTTP) {
                data->body = concat(data->body, HTTP_BOT);
        }

        data->body_length = strlen(data->body);

        data->body_type = TEXT;

        return;
}

void
generate_404(struct data_store *data)
{
        if (data->req_type == HTTP) {
                data->head = concat(data->head, "HTTP/1.1 404 Not Found\r\n"
                                            "Content-Type: text/plain\r\n\r\n");
        }
        data->body = concat(data->body, RESPONSE_404);
        data->body_length = strlen(data->body);
        data->body_type = ERR_404;

        return;
}

void
generate_403(struct data_store *data)
{
        if (data->req_type == HTTP) {
                data->head = concat(data->head, "HTTP/1.1 403 Forbidden\r\n"
                                            "Content-Type: text/plain\r\n\r\n");
        }
        data->body = concat(data->body, RESPONSE_403);
        data->body_length = strlen(data->body);
        data->body_type = ERR_403;

        return;
}
