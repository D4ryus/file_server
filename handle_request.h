#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <sys/types.h>

enum request_type {PLAIN, HTTP};

struct response {
        char* head;
        char* body;
        size_t body_length;
};

struct request {
        char* url;
        enum request_type type;
};

void handle_request(int);
struct response* generate_response(char*);
struct request* parse_request(char*);
struct response* create_response(void);
void free_response(struct response*);
struct request* create_request(void);
void free_request(struct request*);

#endif
