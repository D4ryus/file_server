#ifndef HANDLE_GET_H
#define HANDLE_GET_H

#include "types.h"

int handle_get(struct client_info *, char *);
int parse_get(char *, enum request_type *, char **);
enum response_type get_response_type(char **);

#endif
