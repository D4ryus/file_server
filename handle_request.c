#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#include "handle_request.h"
#include "content_encoding.h"
#include "helper.h"
#include "file_list.h"

// BUFFSIZE_READ - 1 bytes are read from socket
#define BUFFSIZE_READ 2048

// BUFFSIZE_WRITE bytes are written to socket
#define BUFFSIZE_WRITE 8192

void
print_info(struct data_store *data, char *type, char *message)
{
        printf("[%15s:%-5d - ID: %5d]: %-10s - %s\n",
                        data->ip,
                        data->port,
                        data->socket,
                        type,
                        message);
}

void
*handle_request(void *ptr)
{
        struct data_store *data;

        data = (struct data_store*)ptr;
        if (data->socket < 0) {
                quit("ERROR: accept()");
        }

        data->thread_id = (unsigned long)pthread_self();
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
        parse_request(data, read_buffer);
        if (data->url == NULL) {
                print_info(data, "error", "invalid GET, request ignored");
                close(data->socket);
                return NULL;
        }
        generate_response(data);
        write(data->socket, data->head, data->head_length);
        if (data->body_type == TEXT) {
                status = send_text(data);
        } else {
                status = send_file(data);
        }

        if (status == 0) {
                sprintf(message_buffer, "%-20s size: %12lub",
                                     data->url,
                                     data->body_length);
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

/**
 * if negative number is return, error occured
 *  0 : everything went fine.
 * -1 : could not write, client closed connection
 * -2 : could not write, 0 bytes written
 */
int
send_text(struct data_store *data)
{
        ssize_t write_res;
        int     sending;
        size_t  sent;
        int     ret_status;

        write_res = 0;
        sent = 0;
        sending = 1;
        ret_status = 0;

        while (sending) {
                write_res = write(data->socket, data->body + sent, data->body_length - sent);
                if (write_res == -1) {
                        ret_status = -1;
                        break;
                } else if (write_res == 0) {
                        ret_status = -2;
                        break;
                }
                sent = sent + (size_t)write_res;
                if (sent != data->body_length) {
                        continue;
                }
                sending = 0;
        }

        return ret_status;
}

/**
 * if negative number is return, error occured
 *  0 : everything went fine.
 * -1 : could not write, client closed connection
 * -2 : could not write, 0 bytes written
 */
int
send_file(struct data_store *data)
{
        ssize_t write_res;
        int     sending;
        size_t  read;
        size_t  sent;
        size_t  written;
        size_t  last_written;
        char    *buffer;
        FILE    *f;
        time_t  last_time;
        time_t  current_time;
        char    message_buffer[128];
        int     ret_status;

        buffer = malloc(BUFFSIZE_WRITE);
        if (buffer == NULL) {
                mem_error("send_file()", "buffer", BUFFSIZE_WRITE);
        }
        f = fopen(data->body, "rb");
        if (!f) {
                quit("ERROR: send_file()");
        }

        write_res = 0;
        last_time = 0;
        sending = 1;
        written = 0;
        last_written = 0;
        ret_status = 0;

        while (sending) {
                sent = 0;
                read = fread(buffer, 1, BUFFSIZE_WRITE, f);
                if (read < BUFFSIZE_WRITE) {
                        sending = 0;
                }
                while (sent < read) {
                        write_res = write(data->socket, buffer + sent, read - sent);
                        if (write_res == -1) {
                                sending = 0;
                                ret_status = -1;
                                break;
                        } else if (write_res == 0) {
                                sending = 0;
                                ret_status = -2;
                                break;
                        }
                        sent = sent + (size_t)write_res;
                }
                written += sent;
                current_time = time(NULL);
                if ((current_time - last_time) > 1 || !sending) {
                        sprintf(message_buffer, "/%-19s size: %12lub written: %12lub remaining: %12lub %12lub/s %3lu%%",
                                             data->body,
                                             data->body_length,
                                             written,
                                             data->body_length - written,
                                             (written - last_written) / (!sending ? 1 : (size_t)(current_time - last_time)),
                                             written * 100 / data->body_length);
                        print_info(data, "requested", message_buffer);
                        last_time = current_time;
                        last_written = written;
                }
        }
        fclose(f);

        return ret_status;
}

void
generate_response(struct data_store *data)
{
        char*  requested_path;
        char*  accepted_path;

        if (strcmp(data->url, "/") == 0) {
                if (data->req_type == PLAIN) {
                        generate_200_directory_plain(data, ".");
                } else {
                        generate_200_directory(data, ".");
                }
                return;
        }
        if (data->url == NULL) {
                generate_404(data);
                return;
        }

        accepted_path = realpath(".", NULL);
        if (accepted_path == NULL) {
                quit("ERROR: realpath(accepted_path)");
        }

        requested_path = realpath(data->url + 1, NULL);
        if (requested_path == NULL) {
                generate_404(data);
                return;
        }

        if (!starts_with(requested_path, accepted_path)) {
                generate_403(data);
        } else if (is_directory(data->url + 1)) {
                if (data->req_type == PLAIN) {
                        generate_200_directory_plain(data, data->url + 1);
                } else {
                        generate_200_directory(data, data->url + 1);
                }
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
        char   length[32];

        if (stat(file, &sb) == -1) {
                quit("ERROR: generate_200_file()");
        }
        sprintf(length, "%lu\r\n\r\n", (size_t)sb.st_size);

        strcat(data->head, "HTTP/1.1 200 OK\r\n"
                           "Content-Type: ");
        strcat(data->head, get_content_encoding(strrchr(file, '.')));
        strcat(data->head, "\r\nContent-Length: ");
        strcat(data->head, length);
        data->head_length = strlen(data->head);
        data->body_length = (size_t)sb.st_size;
        data->body = concat(data->body, file);
        data->body_type = DATA;

        return;
}

void
generate_200_directory_plain(struct data_store *data, char* directory)
{
        struct dir *d = get_dir(directory);
        strcat(data->head, "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n\r\n");
        data->head_length = strlen(data->head);
        data->body = dir_to_plain_table(data->body, d);
        data->body_length = strlen(data->body);
        free_dir(d);
        data->body_type = TEXT;

        return;
}

void
generate_200_directory(struct data_store *data, char* directory)
{
        struct dir *d;

        d = get_dir(directory);

        strcat(data->head, "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n\r\n");
        data->head_length = strlen(data->head);

        data->body = concat(data->body, "<!DOCTYPE html><html><head>"
                       "<link href='http://fonts.googleapis.com/css?family=Iceland'"
                             "rel='stylesheet'"
                             "type='text/css'>"
                       "<meta http-equiv='content-type'"
                             "content='text/html;"
                             "charset=UTF-8'/>"
                       "</head>"
                       "<body>");
        data->body = dir_to_html_table(data->body, d);
        data->body = concat(data->body, "</body></html>");
        data->body_length = strlen(data->body);
        data->body_type = TEXT;

        free_dir(d);

        return;
}

void
generate_404(struct data_store *data)
{
        strcat(data->head, "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/plain\r\n\r\n");
        data->head_length = strlen(data->head);
        data->body = concat(data->body, "404 - Watcha pulling here buddy?");
        data->body_length = strlen(data->body);
        data->body_type = TEXT;

        return;
}

void
generate_403(struct data_store *data)
{
        strcat(data->head, "HTTP/1.1 403 Forbidden\r\n"
                           "Content-Type: text/plain\r\n\r\n");
        data->head_length = strlen(data->head);
        data->body = concat(data->body, "403 - U better not go down this road!");
        data->body_length = strlen(data->body);
        data->body_type = TEXT;

        return;
}

void
parse_request(struct data_store *data, char *request)
{
        char   *tmp;
        size_t length;

        tmp = strtok(request, "\n"); /* get first line */

        if (tmp == NULL || !starts_with(tmp, "GET /")) {
                data->url = NULL;
                data->req_type = PLAIN;
                return;
        }
        tmp = strtok(tmp, " ");  /* split first line */
        tmp = strtok(NULL, " "); /* get requested url */

        length = strlen(tmp);
        data->url = malloc(sizeof(char) * (length + 1));
        if (data->url == NULL) {
                mem_error("parse_line()", "data->url", sizeof(char) * (length + 1));
        }
        memset(data->url, '\0', sizeof(char) * (length + 1));
        strncpy(data->url, tmp, length);

        /* get requested type */
        tmp = strtok(NULL, " ");
        /* if none given go with PLAIN and return */
        if (tmp == NULL) {
                data->req_type = PLAIN;
                return;
        }
        if (starts_with(tmp, "HTTP/1.1") == 0 || starts_with(tmp, "HTTP/1.0") == 0) {
                data->req_type = HTTP;
        } else {
                data->req_type = PLAIN;
        }

        return;
}

struct data_store*
create_data_store(void)
{
        struct data_store *data;

        data = malloc(sizeof(struct data_store));
        if (data == NULL) {
                mem_error("create_data_store()", "data", sizeof(struct data_store));
        }

        data->port = -1;
        data->socket = -1;
        data->thread_id = 0;
        data->url = NULL;
        data->body = malloc(sizeof(char));
        if (data->body == NULL) {
                mem_error("create_data_store()", "data->body", sizeof(char));
        }
        data->head[0] = '\0';
        data->head_length = 0;
        data->body[0] = '\0';
        data->body_length = 0;
        data->req_type = TEXT;
        data->body_type = PLAIN;

        return data;
}

void
free_data_store(struct data_store *data)
{
        if (data == NULL) {
                return;
        }

        if (data->url != NULL) {
                free(data->url);
        }

        if (data->body != NULL) {
                free(data->body);
        }
        free(data);

        return;
}
