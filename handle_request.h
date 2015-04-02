#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <pthread.h>
#include <sys/types.h>

enum request_type {PLAIN, HTTP};
enum body_type {DATA, TEXT, ERR_404, ERR_403};

/**
 * per request a data store is generated and then during execution filed
 */
struct data_store {
        char   ip[16];                /* ip from client */
        int    port;                  /* port from client */
        int    socket;                /* socket descriptor */
        ulong  thread_id;             /* thread id */
        char   *url;                  /* requested file */
        char   head[254];             /* response header */
        char   *body;                 /* response body, if file filename */
        size_t body_length;           /* length of response body */
        size_t head_length;           /* length of response head */
        enum  request_type req_type;  /* requested type (PLAIN or HTTP) */
        enum  body_type    body_type; /* type of body (TEXT or DATA) */
};

void print_info(struct data_store*, char*, char*);
void *handle_request(void*);
int send_text(struct data_store*);
int send_file(struct data_store*);
void generate_response(struct data_store*);
void generate_200_file(struct data_store*, char*);
void generate_200_directory(struct data_store*, char*);
void generate_200_directory_plain(struct data_store*, char*);
void generate_404(struct data_store*);
void generate_403(struct data_store*);
void parse_request(struct data_store*, char*);
struct data_store* create_data_store(void);
void free_data_store(struct data_store*);

#endif
