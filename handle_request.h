#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include "types.h"

void *handle_request(void *);
int read_request(int, char *, size_t);
int parse_request(char *, enum request_type *, char **);
void generate_response(struct data_store *);
void generate_200_file(struct data_store *, char *);
void generate_200_directory(struct data_store *, char *);
void generate_404(struct data_store *);
void generate_403(struct data_store *);

#endif
