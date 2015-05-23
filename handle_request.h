#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include "types.h"

void *handle_request(void *);
int handle_get(struct client_info *, char *);
int handle_post(struct client_info *, char *);
int read_file_from_post(int, char *, uint64_t *);
int parse_post_header(int, char **, char **, char **);
int read_request(int, char *, size_t);
int parse_get(char *, enum request_type *, char **);
int get_line(int, char**);
enum response_type get_response_type(char **);
int send_200_file_head(int, enum request_type, char *, uint64_t *);
int send_200_directory(int, enum request_type, char *, uint64_t *);
int send_404(int, enum request_type, uint64_t *);
int send_403(int, enum request_type, uint64_t *);
int send_201(int);
int send_http_head(int, char *, uint64_t);

#endif
