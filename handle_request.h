#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include <pthread.h>
#include <sys/types.h>

#include "helper.h"

/**
 * function where each thread starts execution with given data_store
 */
void *handle_request(void*);

/**
 * generates a response and saves it inside the data_store
 */
void generate_response(struct data_store*);

/**
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void generate_200_file(struct data_store*, char*);

/**
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void generate_200_directory(struct data_store*, char*);

/**
 * generates a 404 NOT FOUND HTTP response and saves it inside the data_store
 */
void generate_404(struct data_store*);

/**
 * generates a 403 FORBIDDEN HTTP response and saves it inside the data_store
 */
void generate_403(struct data_store*);

/**
 * parses given request and stores info inside given data_store
 */
void parse_request(struct data_store*, char*);

#endif
