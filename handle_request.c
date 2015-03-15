#include "handle_request.h"

void
handle_request(int socket)
{
        if (socket < 0) {
                error("ERROR on accept");
        }

        int n;
        size_t BUFFSIZE = 2048;
        char buffer[BUFFSIZE];
        struct dir *d;
        struct request *req;
        char *url;
        char *result;

        memset(buffer, '\0', BUFFSIZE);

        n = read(socket, buffer, BUFFSIZE - 1);
        if (n < 0) {
                error("ERROR reading from socket");
        }

        printf("%s\n", buffer);
        req = parse_request(buffer);

        url = malloc(sizeof(char) * (strlen(req->url) + 2));
        memset(url, '\0', sizeof(char) * (strlen(req->url) + 2));
        url[0] = '.';
        strncat(url, req->url, strlen(req->url));

        result = malloc(sizeof(char));
        result[0] = '\0';

        if (_is_directory(url)) {
                d = get_dir(url);
                result = _concat(result, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
                result = _concat(result, "<!DOCTYPE html><html><head>");
                result = _concat(result, "<meta http-equiv=\"content-type\"content=\"text/html;charset=UTF-8\"/>");
                result = _concat(result, "</head><body><table style=\"width:30%\">");
                result = get_html_from_dir(result, d);
                result = _concat(result, "</table></body></html>");
                free_dir(d);
        } else {
                result = _concat(result, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
                result = _concat(result, "yo nigga, its a file");
        }
        free(url);

        n = write(socket, result, strlen(result));
        if (n < 0) {
                error("ERROR writing to socket");
        }

        _free_request(req);
        free(result);

        close(socket);
        /* char buffer[123]; */
        /* printf("%s -> %s\n", ".", realpath(".", buffer)); */
        /* printf("%s -> %s\n", "..", realpath("..", buffer)); */
        /* printf("%s -> %s\n", "blub/../../", realpath("blub/../../", buffer)); */

        /* printf("%s -> %s\n", ".", _is_directory(".") ? "Dir" : "File"); */
        /* printf("%s -> %s\n", "test.html", _is_directory("test.html") ? "Dir" : "File"); */
}

void
error(const char *msg)
{
        perror(msg);
        exit(1);
}

int
starts_with(const char *line, const char *prefix)
{
    while (*prefix) {
        if (*prefix++ != *line++) {
            return 0;
        }
    }

    return 1;
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
