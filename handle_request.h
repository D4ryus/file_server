#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <sys/types.h>

enum request_type {PLAIN, HTTP};
enum body_type {TEXT, DATA};

struct response {
        char* head;
        char* body;
        enum body_type type;
        size_t body_length;
        size_t head_length;
};

struct request {
        char* url;
        enum request_type type;
};

void handle_request(int*);
void send_text(int, struct response*);
void send_file(int, struct response*);
struct response* generate_response(struct request*);
struct response* generate_200_file(char*);
struct response* generate_200_directory(char*);
struct response* generate_200_directory_plain(char*);
struct response* generate_404(void);
struct response* generate_403(void);
struct request* parse_request(char*);
struct response* create_response(void);
void free_response(struct response*);
struct request* create_request(void);
void free_request(struct request*);

#endif
