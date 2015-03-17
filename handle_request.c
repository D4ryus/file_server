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

        printf("%s\n", buffer);
        req = parse_request(buffer);
        if (req->url == NULL) {
                printf("requested with non valid http request, abort.\n");
                close(socket);
                return;
        }

        /* TODO: thats ugly */
        url = malloc(sizeof(char) * (strlen(req->url) + 2));
        memset(url, '\0', sizeof(char) * (strlen(req->url) + 2));
        url[0] = '.';
        strncat(url, req->url, strlen(req->url));

        requested_path = realpath(url, NULL);

        res = generate_response(requested_path);

        response_buffer = malloc(strlen(res->head) + res->body_length);

        memcpy(response_buffer, res->head, strlen(res->head));
        memcpy(response_buffer + strlen(res->head), res->body, res->body_length);

        n = write(socket, response_buffer, strlen(res->head) + res->body_length);
        printf("did send: %u\n", n);

        free(response_buffer);
        free(url);
        free(requested_path);
        _free_response(res);
        _free_request(req);

        close(socket);
}

struct response*
generate_response(char* file)
{
        struct response *res = _create_response();
        char *accepted_path;

        accepted_path = realpath(".", NULL);
        printf("file: %s, accpeted: %s\n", file, accepted_path);

        if (accepted_path == NULL) {
                _quit("ERROR: realpath()");
        }

        if (file == NULL) {
                res->head = _concat(res->head, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n");
                res->body = _concat(res->body, "404 - Watcha pulling here buddy?");
                res->body_length = strlen(res->body);
        } else if (!starts_with(file, accepted_path)) {
                printf("file: %s, accpeted: %s\n", file, accepted_path);
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
                char* extension = strrchr(file, '.');
                res->head = _concat(res->head, get_content_encoding(extension));
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
        struct request* res = _create_request();

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
        res->head = malloc(sizeof(char));
        res->body = malloc(sizeof(char));
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
}

struct request*
_create_request(void)
{
        struct request *res = malloc(sizeof(struct request));

        res->method = NON;
        res->url = NULL;
        res->Accept = NULL;
        res->Accept_Charset = NULL;
        res->Accept_Encoding = NULL;
        res->Accept_Language = NULL;
        res->Authorization = NULL;
        res->Expect = NULL;
        res->From = NULL;
        res->Host = NULL;
        res->If_Match = NULL;
        res->If_Modified_Since = NULL;
        res->If_None_Match = NULL;
        res->If_Range = NULL;
        res->If_Unmodified_Since = NULL;
        res->Max_Forwards = NULL;
        res->Proxy_Authorization = NULL;
        res->Range = NULL;
        res->Referer = NULL;
        res->TE = NULL;
        res->User_Agent = NULL;

        return res;
}

void
_free_request(struct request *res)
{
        if (res->url != NULL) free(res->url);
        if (res->Accept != NULL) free(res->Accept);
        if (res->Accept_Charset != NULL) free(res->Accept_Charset);
        if (res->Accept_Encoding != NULL) free(res->Accept_Encoding);
        if (res->Accept_Language != NULL) free(res->Accept_Language);
        if (res->Authorization != NULL) free(res->Authorization);
        if (res->Expect != NULL) free(res->Expect);
        if (res->From != NULL) free(res->From);
        if (res->Host != NULL) free(res->Host);
        if (res->If_Match != NULL) free(res->If_Match);
        if (res->If_Modified_Since != NULL) free(res->If_Modified_Since);
        if (res->If_None_Match != NULL) free(res->If_None_Match);
        if (res->If_Range != NULL) free(res->If_Range);
        if (res->If_Unmodified_Since != NULL) free(res->If_Unmodified_Since);
        if (res->Max_Forwards != NULL) free(res->Max_Forwards);
        if (res->Proxy_Authorization != NULL) free(res->Proxy_Authorization);
        if (res->Range != NULL) free(res->Range);
        if (res->Referer != NULL) free(res->Referer);
        if (res->TE != NULL) free(res->TE);
        if (res->User_Agent != NULL) free(res->User_Agent);

        free(res);
}

void
_parse_line(char *line, struct request *res)
{
        if (starts_with(line, "GET ")) {
                char* tmp = strtok(line, " ");
                tmp = strtok(NULL, " ");
                res->url = malloc(sizeof(char) * (strlen(tmp) + 1));
                memset(res->url, '\0', sizeof(char) * (strlen(tmp) + 1));
                strncpy(res->url, tmp, strlen(tmp));
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
}
