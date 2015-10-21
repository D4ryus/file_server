#ifndef HANDLE_POST_H
#define HANDLE_POST_H

#include "types.h"
#include "parse_http.h"

int handle_post(struct client_info *, struct http_header *);
int parse_post_body(int, char *, char **, uint64_t *, uint64_t *);
int buff_contains(int, char *, size_t, char *, size_t, ssize_t *);
int parse_file_header(char *, size_t, size_t *, char **);
int open_file(char *, FILE **, char **);

#endif
