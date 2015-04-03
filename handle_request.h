#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <pthread.h>
#include <sys/types.h>

#include "helper.h"

void *handle_request(void*);
void generate_response(struct data_store*);
void generate_200_file(struct data_store*, char*);
void generate_200_directory(struct data_store*, char*);
void generate_404(struct data_store*);
void generate_403(struct data_store*);
void parse_request(struct data_store*, char*);
struct data_store* create_data_store(void);
void free_data_store(struct data_store*);

#endif
