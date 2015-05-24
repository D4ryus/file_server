#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "helper.h"
#include "types.h"

int send_200_file_head(int, enum request_type, uint64_t *, char *);
int send_200_directory(int, enum request_type, uint64_t *, char *);
int send_404(int, enum request_type, uint64_t *);
int send_403(int, enum request_type, uint64_t *);
int send_201(int, enum request_type, uint64_t *);

#endif
