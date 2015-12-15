#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "helper.h"
#include "types.h"
#include "parse_http.h"

int send_200_file_head(int, enum http_type, uint64_t *, char *);
int send_206_file_head(int, enum http_type, uint64_t *, char *, uint64_t,
    uint64_t);
int send_200_directory(int, enum http_type, uint64_t *, char *, char[16]);
int send_403(int, enum http_type, uint64_t *);
int send_404(int, enum http_type, uint64_t *);
int send_405(int, enum http_type, uint64_t *);
int send_201(int, enum http_type, uint64_t *);

#endif
