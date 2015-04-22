#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include "types.h"

/*
 * function where each thread starts execution with given data_store
 */
void *handle_request(void*);

/*
 * reads from socket size bytes and write them to buffer
 * if negative number is returned, error occured
 * STAT_OK       ( 0) : everything went fine.
 * READ_CLOSED   (-3) : client closed connection
 * EMPTY_MESSAGE (-4) : nothing was read from socket
 */
int read_request(int, char*, size_t);

/*
 * parses given request
 * allocs given *url with size of requested filename
 * if negative number is returned, error occured
 * STAT_OK       ( 0) : everything went fine.
 * INV_GET       (-5) : parse error
 */
int parse_request(char*, enum request_type*, char**);

/*
 * generates a response and saves it inside the data_store
 */
void generate_response(struct data_store*);

/*
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void generate_200_file(struct data_store*, char*);

/*
 * generates a 200 OK HTTP response and saves it inside the data_store
 */
void generate_200_directory(struct data_store*, char*);

/*
 * generates a 404 NOT FOUND HTTP response and saves it inside the data_store
 */
void generate_404(struct data_store*);

/*
 * generates a 403 FORBIDDEN HTTP response and saves it inside the data_store
 */
void generate_403(struct data_store*);

#endif
