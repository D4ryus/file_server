#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

#include "types.h"

void init_client_info(struct client_info *);
void *handle_request(void *);

#endif
