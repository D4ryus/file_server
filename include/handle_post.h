#ifndef HANDLE_POST_H
#define HANDLE_POST_H

#include "types.h"
#include "parse_http.h"

int handle_post(struct client_info *, struct http_header *);

#endif
