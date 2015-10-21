#ifndef HANDLE_GET_H
#define HANDLE_GET_H

#include "types.h"
#include "parse_http.h"

int handle_get(struct client_info *, struct http_header *);
enum response_type get_response_type(char **);

#endif
