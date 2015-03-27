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

void
handle_request(struct thread_info* info)
{
        if (info->socket < 0) {
                quit("ERROR: accept()");
        }

        info->thread_id = (unsigned long)pthread_self();
        printf("Accpepted - [IP: %15s, Port: %5i, Thread ID: %12lu, Socket ID: %5i]\n",
                        info->ip,
                        info->port,
                        info->thread_id,
                        info->socket);

        struct request *req;
        struct response *res;
        size_t buffsize = 2048;
        char read_buffer[buffsize];
        ssize_t n;

        memset(read_buffer, '\0', buffsize);
        n = read(info->socket, read_buffer, buffsize - 1);
        if (n < 0) {
                printf("encounterd a error on read(), will ignore request.\n");
                close(info->socket);
                return;
        }
        if (n == 0) {
                printf("empty request, will ignore request\n");
                close(info->socket);
                return;
        }
        req = parse_request(read_buffer);
        if (req->url == NULL) {
                printf("requested with non valid http request, abort.\n");
                close(info->socket);
                return;
        }
        res = generate_response(req);
        write(info->socket, res->head, res->head_length);
        if (res->type == TEXT) {
                send_text(info, res);
        } else {
                send_file(info, res);
        }
        free_request(req);
        free_response(res);
        close(info->socket);
        free(info);

        return;
}

void
send_text(struct thread_info *info, struct response *res)
{
        ssize_t write_res;
        int sending;
        size_t sent;

        write_res = 0;
        sent = 0;
        sending = 1;
        while (sending) {
                write_res = write(info->socket, res->body + sent, res->body_length - sent);
                if (write_res == -1) {
                        printf("encounterd a error on write(), client closed connection.\n");
                        return;
                } else if (write_res == 0) {
                        printf("0 bytes written, ignoring request.\n");
                }
                sent = sent + (size_t)write_res;
                if (sent != res->body_length) {
                        continue;
                }
                sending = 0;
        }
}

void
send_file(struct thread_info *info, struct response *res)
{
        ssize_t write_res;
        int     sending;
        size_t  read;
        size_t  sent;
        size_t  written;
        size_t  last_written;
        size_t  buffsize;
        char    *buffer;
        FILE    *f;
        time_t  last_time;
        time_t  current_time;

        buffsize = 8 << 10; /* 8kB */

        buffer = malloc(buffsize);
        if (buffer == NULL) {
                mem_error("send_file()", "buffer", buffsize);
        }
        f = fopen(res->body, "rb");
        if (!f) {
                quit("ERROR: send_file()");
        }

        write_res = 0;
        last_time = 0;
        sending = 1;
        written = 0;
        last_written = 0;
        while (sending) {
                sent = 0;
                read = fread(buffer, 1, buffsize, f);
                if (read < buffsize) {
                        sending = 0;
                }
                while (sent < read) {
                        write_res = write(info->socket, buffer + sent, read - sent);
                        if (write_res == -1) {
                                printf("encounterd a error on write(), client closed connection.\n");
                                sending = 0;
                        } else if (write_res == 0) {
                                printf("0 bytes written, ignoring request.\n");
                                sending = 0;
                        }
                        sent = sent + (size_t)write_res;
                }
                written += sent;
                current_time = time(NULL);
                if ((current_time - last_time) > 1 || !sending) {
                        printf("ip: %15s requested: %s size: %8lukb written: %8lukb remaining: %8lukb %3lu%% %8lukb/s\n",
                                             info->ip,
                                             res->body, /* contains filename */
                                             res->body_length >> 10,
                                             written >> 10,
                                             (res->body_length  >> 10) - (written >> 10),
                                             written * 100 / res->body_length,
                                             (written >> 10) - (last_written >> 10) / (!sending ? 1 : (size_t)((current_time  >> 10) - (last_time >> 10))));
                        last_time = current_time;
                        last_written = written;
                }
        }

        fclose(f);
}

struct response*
generate_response(struct request *req)
{
        struct response *res;
        char* requested_path;
        char* accepted_path;

        if (strcmp(req->url, "/") == 0) {
                if (req->type == PLAIN) {
                        res = generate_200_directory_plain(".");
                } else {
                        res = generate_200_directory(".");
                }
                return res;
        }
        if (req->url == NULL) {
                res = generate_404();
                return res;
        }

        accepted_path = realpath(".", NULL);
        if (accepted_path == NULL) {
                quit("ERROR: realpath(accepted_path)");
        }

        requested_path = realpath(req->url + 1, NULL);
        if (requested_path == NULL) {
                res = generate_404();
                return res;
        }

        if (!starts_with(requested_path, accepted_path)) {
                res = generate_403();
        } else if (is_directory(req->url + 1)) {
                if (req->type == PLAIN) {
                        res = generate_200_directory_plain(req->url + 1);
                } else {
                        res = generate_200_directory(req->url + 1);
                }
        } else {
                res = generate_200_file(req->url + 1);
        }

        free(requested_path);
        free(accepted_path);

        return res;
}

struct response*
generate_200_file(char* file)
{
        struct response *res;
        struct stat sb;
        char length[32];

        if (stat(file, &sb) == -1) {
                quit("ERROR: generate_200_file()");
        }

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: ");
        res->head = concat(res->head, get_content_encoding(strrchr(file, '.')));
        res->head = concat(res->head, "\r\nContent-Length: ");
        sprintf(length, "%lu\r\n\r\n", (size_t)sb.st_size);
        res->head = concat(res->head, length);
        res->head_length = strlen(res->head);
        res->body_length = (size_t)sb.st_size;
        res->body = concat(res->body, file);
        res->type = DATA;

        return res;
}

struct response*
generate_200_directory_plain(char* directory)
{
        struct response *res;

        res = create_response();
        struct dir *d = get_dir(directory);
        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
        res->head_length = strlen(res->head);
        res->body = dir_to_plain_table(res->body, d);
        res->body_length = strlen(res->body);
        free_dir(d);
        res->type = TEXT;

        return res;
}

struct response*
generate_200_directory(char* directory)
{
        struct response *res;
        struct dir *d;

        res = create_response();
        d = get_dir(directory);

        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        res->head_length = strlen(res->head);

        res->body = concat(res->body, "<!DOCTYPE html><html><head>");
        res->body = concat(res->body, "<link href='http://fonts.googleapis.com/css?family=Iceland' rel='stylesheet' type='text/css'>");
        res->body = concat(res->body, "<meta http-equiv='content-type'content='text/html;charset=UTF-8'/>");
        res->body = concat(res->body, "</head>");
        res->body = concat(res->body, "<body>");
        res->body = dir_to_html_table(res->body, d);
        res->body = concat(res->body, "</body></html>");
        res->body_length = strlen(res->body);
        res->type = TEXT;

        free_dir(d);

        return res;
}

struct response*
generate_404(void)
{
        struct response *res;

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n");
        res->head_length = strlen(res->head);
        res->body = concat(res->body, "404 - Watcha pulling here buddy?");
        res->body_length = strlen(res->body);
        res->type = TEXT;

        return res;
}

struct response*
generate_403(void)
{
        struct response *res;

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\n");
        res->head_length = strlen(res->head);
        res->body = concat(res->body, "404 - Watcha pulling here buddy?");
        res->body = concat(res->body, "403 - U better not go down this road!");
        res->body_length = strlen(res->body);
        res->type = TEXT;

        return res;
}

struct request*
parse_request(char* request)
{
        char *tmp;
        struct request* req;
        size_t length;

        req = create_request();
        tmp = strtok(request, "\n"); /* get first line */

        if (tmp == NULL || !starts_with(tmp, "GET ")) {
                req->url = NULL;
                req->type = PLAIN;
                return req;
        }
        tmp = strtok(tmp, " ");  /* split first line */
        tmp = strtok(NULL, " "); /* get requested url */

        length = strlen(tmp);
        req->url = malloc(sizeof(char) * (length + 1));
        if (req->url == NULL) {
                mem_error("parse_line()", "req->url", sizeof(char) * (length + 1));
        }
        memset(req->url, '\0', sizeof(char) * (length + 1));
        strncpy(req->url, tmp, length);

        /* get requested type */
        tmp = strtok(NULL, " ");
        /* if none given go with PLAIN and return */
        if (tmp == NULL) {
                req->type = PLAIN;
                return req;
        }
        if (starts_with(tmp, "HTTP/1.1") == 0 || starts_with(tmp, "HTTP/1.0") == 0) {
                req->type = HTTP;
        } else {
                req->type = PLAIN;
        }

        return req;
}

struct response*
create_response(void)
{
        struct response *res;

        res = malloc(sizeof(struct response));
        if (res == NULL) {
                mem_error("create_response()", "res", sizeof(struct response));
        }
        res->head = malloc(sizeof(char));
        if (res->head == NULL) {
                mem_error("create_response()", "res->head", sizeof(char));
        }
        res->body = malloc(sizeof(char));
        if (res->body == NULL) {
                mem_error("create_response()", "res->body", sizeof(char));
        }
        res->head[0] = '\0';
        res->head_length = 0;
        res->body[0] = '\0';
        res->body_length = 0;
        res->type = TEXT;

        return res;
}

void
free_response(struct response *res)
{
        if (res->head != NULL) {
                free(res->head);
        }
        if (res->body != NULL) {
                free(res->body);
        }
        free(res);

        return;
}

struct request*
create_request(void)
{
        struct request *req;

        req = malloc(sizeof(struct request));
        if (req == NULL) {
                mem_error("create_request()", "req", sizeof(char));
        }
        req->url = NULL;
        req->type = PLAIN;

        return req;
}

void
free_request(struct request *req)
{
        if (req == NULL) {
                return;
        }

        if (req->url != NULL) free(req->url);
        free(req);

        return;
}
