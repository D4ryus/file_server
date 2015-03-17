#include "handle_request.h"
#include "content_encoding.h"

void
handle_request(int socket)
{
        if (socket < 0) {
                _quit("ERROR: accept()");
        }

        struct request *req;
        struct response *res;
        ssize_t n;
        char *requested_path;
        size_t BUFFSIZE = 2048;
        char buffer[BUFFSIZE];
        char *url;
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
        /* TODO: thats ugly */
        url = malloc(sizeof(char) * (strlen(req->url) + 2));
        if (url == NULL) {
                mem_error("handle_request()", "url",
                                sizeof(char) * (strlen(req->url) + 2));
        }
        memset(url, '\0', sizeof(char) * (strlen(req->url) + 2));
        url[0] = '.';
        strncat(url, req->url, strlen(req->url));
        requested_path = realpath(url, NULL);
        res = generate_response(requested_path);
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
        free(url);
        free(requested_path);
        _free_request(req);
        _free_response(res);
        close(socket);

        return;
}

struct response*
generate_response(char* file)
{
        struct response *res;
        char *accepted_path;

        res = _create_response();
        accepted_path = realpath(".", NULL);
        if (accepted_path == NULL) {
                _quit("ERROR: realpath()");
        }
        if (file == NULL) {
                res->head = _concat(res->head, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n");
                res->body = _concat(res->body, "404 - Watcha pulling here buddy?");
                res->body_length = strlen(res->body);
        } else if (!starts_with(file, accepted_path)) {
                res->head = _concat(res->head, "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\n");
                res->body = _concat(res->body, "403 - U better not go down this road!");
                res->body_length = strlen(res->body);
        } else if (_is_directory(file)) {
                file[strlen(accepted_path) - 1] = '.';
                struct dir *d = get_dir(file + strlen(accepted_path) - 1);
                res->head = _concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
                res->body = _concat(res->body, "<!DOCTYPE html><html><head>");
                res->body = _concat(res->body, "<meta http-equiv=\"content-type\"content=\"text/html;charset=UTF-8\"/>");
                res->body = _concat(res->body, "</head><body><table style=\"width:30%\">");
                res->body = get_html_from_dir(res->body, d);
                res->body = _concat(res->body, "</table></body></html>");
                res->body_length = strlen(res->body);
                free_dir(d);
        } else {
                res->head = _concat(res->head, "HTTP/1.1 200 OK\r\nContent-Type: ");
                res->head = _concat(res->head, get_content_encoding(strrchr(file, '.')));
                res->head = _concat(res->head, "\r\n\r\n");
                if (res->body != NULL) {
                        free(res->body);
                }
                res->body = _file_to_buf(file, &(res->body_length));
        }
        free(accepted_path);

        return res;
}

struct request*
parse_request(char* request)
{
        char *tmp;
        int length;
        struct request* res;

        res = _create_request();
        tmp = strtok(request, "\n");
        for (length = 0; tmp != NULL; length++) {
                _parse_line(tmp, res);
                tmp = strtok(NULL, "\n");
        }

        return res;
}

struct response*
_create_response(void)
{
        struct response *res;

        res = malloc(sizeof(struct response));
        if (res == NULL) {
                mem_error("_create_response()", "res", sizeof(struct response));
        }
        res->head = malloc(sizeof(char));
        if (res->head == NULL) {
                mem_error("_create_response()", "res->head", sizeof(char));
        }
        res->body = malloc(sizeof(char));
        if (res->body == NULL) {
                mem_error("_create_response()", "res->body", sizeof(char));
        }
        res->head[0] = '\0';
        res->body[0] = '\0';
        res->body_length = 0;

        return res;
}

void
_free_response(struct response *res)
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
_create_request(void)
{
        struct request *req;

        req = malloc(sizeof(struct request));
        if (req == NULL) {
                mem_error("_create_request()", "req", sizeof(char));
        }
        req->method = NON;
        req->url = NULL;
        req->Accept = NULL;
        req->Accept_Charset = NULL;
        req->Accept_Encoding = NULL;
        req->Accept_Language = NULL;
        req->Authorization = NULL;
        req->Expect = NULL;
        req->From = NULL;
        req->Host = NULL;
        req->If_Match = NULL;
        req->If_Modified_Since = NULL;
        req->If_None_Match = NULL;
        req->If_Range = NULL;
        req->If_Unmodified_Since = NULL;
        req->Max_Forwards = NULL;
        req->Proxy_Authorization = NULL;
        req->Range = NULL;
        req->Referer = NULL;
        req->TE = NULL;
        req->User_Agent = NULL;

        return req;
}

void
_free_request(struct request *req)
{
        if (req->url != NULL) free(req->url);
        if (req->Accept != NULL) free(req->Accept);
        if (req->Accept_Charset != NULL) free(req->Accept_Charset);
        if (req->Accept_Encoding != NULL) free(req->Accept_Encoding);
        if (req->Accept_Language != NULL) free(req->Accept_Language);
        if (req->Authorization != NULL) free(req->Authorization);
        if (req->Expect != NULL) free(req->Expect);
        if (req->From != NULL) free(req->From);
        if (req->Host != NULL) free(req->Host);
        if (req->If_Match != NULL) free(req->If_Match);
        if (req->If_Modified_Since != NULL) free(req->If_Modified_Since);
        if (req->If_None_Match != NULL) free(req->If_None_Match);
        if (req->If_Range != NULL) free(req->If_Range);
        if (req->If_Unmodified_Since != NULL) free(req->If_Unmodified_Since);
        if (req->Max_Forwards != NULL) free(req->Max_Forwards);
        if (req->Proxy_Authorization != NULL) free(req->Proxy_Authorization);
        if (req->Range != NULL) free(req->Range);
        if (req->Referer != NULL) free(req->Referer);
        if (req->TE != NULL) free(req->TE);
        if (req->User_Agent != NULL) free(req->User_Agent);

        free(req);

        return;
}

void
_parse_line(char *line, struct request *req)
{
        if (starts_with(line, "GET ")) {
                char* tmp = strtok(line, " ");
                tmp = strtok(NULL, " ");
                req->url = malloc(sizeof(char) * (strlen(tmp) + 1));
                if (req->url == NULL) {
                        mem_error("_parse_line()", "req->url",
                                        sizeof(char) * (strlen(tmp) + 1));
                }

                memset(req->url, '\0', sizeof(char) * (strlen(tmp) + 1));
                strncpy(req->url, tmp, strlen(tmp));
        /* TODO: add other header settings here */
        } else if (starts_with(line, "Accept: ")) {
        } else if (starts_with(line, "Accept-Charset:")) {
        } else if (starts_with(line, "Accept-Encoding:")) {
        } else if (starts_with(line, "Accept-Language:")) {
        } else if (starts_with(line, "Authorization:")) {
        } else if (starts_with(line, "Expect:")) {
        } else if (starts_with(line, "From:")) {
        } else if (starts_with(line, "Host:")) {
        } else if (starts_with(line, "If-Match:")) {
        } else if (starts_with(line, "If-Modified-Since:")) {
        } else if (starts_with(line, "If-None-Match:")) {
        } else if (starts_with(line, "If-Range:")) {
        } else if (starts_with(line, "If-Unmodified-Since:")) {
        } else if (starts_with(line, "Max-Forwards:")) {
        } else if (starts_with(line, "Proxy-Authorization:")) {
        } else if (starts_with(line, "Range:")) {
        } else if (starts_with(line, "Referer:")) {
        } else if (starts_with(line, "TE:")) {
        } else if (starts_with(line, "User-Agent:")) {
        } else {
        }

        return;
}
