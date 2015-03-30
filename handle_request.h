#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <pthread.h>
#include <sys/types.h>

enum request_type {PLAIN, HTTP};
enum body_type {TEXT, DATA};

struct response {
        char  head[254];
        char* body;
        enum body_type type;
        size_t body_length;
        size_t head_length;
};

struct request {
        char* url;
        enum request_type type;
};

struct thread_info {
        char ip[16];
        int port;
        int socket;
        unsigned long thread_id;
};

void print_info(struct thread_info*, char*, char*);
void handle_request(struct thread_info*);
int send_text(struct thread_info*, struct response*);
int send_file(struct thread_info*, struct response*);
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
