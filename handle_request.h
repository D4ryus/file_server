#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <sys/types.h>

enum request_method {NON, OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE, CONNECT};

struct response {
        char* head;
        char* body;
        size_t body_length;
};

struct request {
        enum request_method method;
        char* url;
        char* Accept;
        char* Accept_Charset;
        char* Accept_Encoding;
        char* Accept_Language;
        char* Authorization;
        char* Expect;
        char* From;
        char* Host;
        char* If_Match;
        char* If_Modified_Since;
        char* If_None_Match;
        char* If_Range;
        char* If_Unmodified_Since;
        char* Max_Forwards;
        char* Proxy_Authorization;
        char* Range;
        char* Referer;
        char* TE;
        char* User_Agent;
};

void handle_request(int);
struct response* generate_response(char*);
struct request* parse_request(char*);
struct response* create_response(void);
void free_response(struct response*);
struct request* create_request(void);
void free_request(struct request*);
void parse_line(char*, struct request*);

#endif
