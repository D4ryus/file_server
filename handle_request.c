#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "handle_request.h"
#include "content_encoding.h"
#include "helper.h"
#include "file_list.h"

void
handle_request(int *sock)
{
        int socket = *sock;
        if (socket < 0) {
                quit("ERROR: accept()");
        }

        struct request *req;
        struct response *res;
        ssize_t n;
        size_t BUFFSIZE = 2048;
        char buffer[BUFFSIZE];
        char* response_buffer;
        size_t response_length;

        memset(buffer, '\0', BUFFSIZE);
        n = read(socket, buffer, BUFFSIZE - 1);
        if (n < 0) {
                printf("encounterd a error on read(), will ignore request.\n");
                close(socket);
                return;
        }
        if (n == 0) {
                printf("empty request, will ignore request\n");
                close(socket);
                return;
        }
        req = parse_request(buffer);
        if (req->url == NULL) {
                printf("requested with non valid http request, abort.\n");
                close(socket);
                return;
        }
        res = generate_response(req);
        response_length = strlen(res->head) + res->body_length;
        response_buffer = malloc(sizeof(char) * response_length);
        if (response_buffer == NULL) {
                mem_error("handle_request()", "response_buffer",
                                sizeof(char) * response_length);
        }
        memcpy(response_buffer, res->head, strlen(res->head));
        memcpy(response_buffer + strlen(res->head), res->body, res->body_length);
        n = write(socket, response_buffer, response_length);
        if (n < 0) {
                printf("encounterd a error on write(), will ignore request.\n");
        } else if (n == 0) {
                printf("0 bytes have been written, will ignore request\n");
        } else if ((size_t)n != response_length) {
                printf("write should have written %u bytes, but instead only %u have been written.\n",
                                (uint)response_length, (uint)n);
        }
        free(response_buffer);
        free_request(req);
        free_response(res);
        close(socket);

        return;
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
                quit("ERROR: realpath(requested_path)");
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

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: ");
        res->head = concat(res->head, get_content_encoding(strrchr(file, '.')));
        res->head = concat(res->head, "\r\n\r\n");
        if (res->body != NULL) {
                free(res->body);
        }
        res->body = file_to_buf(file, &(res->body_length));

        return res;
}

struct response*
generate_200_directory_plain(char* directory)
{
        struct response *res;

        res = create_response();
        struct dir *d = get_dir(directory);
        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
        res->body = dir_to_plain_table(res->body, d);
        res->body_length = strlen(res->body);
        free_dir(d);

        return res;
}

struct response*
generate_200_directory(char* directory)
{
        struct response *res;

        res = create_response();
        struct dir *d = get_dir(directory);
        res->head = concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        res->body = concat(res->body, "<!DOCTYPE html><html><head>");
        res->body = concat(res->body, "<link href='http://fonts.googleapis.com/css?family=Iceland' rel='stylesheet' type='text/css'>");
        res->body = concat(res->body, "<meta http-equiv='content-type'content='text/html;charset=UTF-8'/>");
        res->body = concat(res->body, "</head>");
        res->body = concat(res->body, "<body>");
        res->body = dir_to_html_table(res->body, d);
        res->body = concat(res->body, "</body></html>");
        res->body_length = strlen(res->body);
        free_dir(d);

        return res;
}

struct response*
generate_404(void)
{
        struct response *res;

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n");
        res->body = concat(res->body, "404 - Watcha pulling here buddy?");
        res->body_length = strlen(res->body);

        return res;
}

struct response*
generate_403(void)
{
        struct response *res;

        res = create_response();
        res->head = concat(res->head, "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\n");
        res->body = concat(res->body, "403 - U better not go down this road!");
        res->body_length = strlen(res->body);

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

        if (!starts_with(tmp, "GET ")) {
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
        res->body[0] = '\0';
        res->body_length = 0;

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
