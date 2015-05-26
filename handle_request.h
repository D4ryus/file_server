#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include "types.h"

void *handle_request(void *);
int handle_get(struct client_info *, char *);
int handle_post(struct client_info *, char *);
int parse_post_header(int, char **, char **, char **);
int parse_post_body(int, char *, char **, uint64_t *, uint64_t *);
int parse_file_header(char *, size_t, size_t *, char **);
int open_file(char *, FILE **, char **);
int read_request(int, char *, size_t);
int parse_get(char *, enum request_type *, char **);
int get_line(int, char**);
enum response_type get_response_type(char **);

#endif
