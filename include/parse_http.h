#ifndef PARSE_HTTP_H
#define PARSE_HTTP_H

#include <stdint.h>

enum http_method {
	MISSING = 0,
	GET,
	POST
};

enum http_type {
	HTTP,
	PLAIN
};

struct http_header {
	enum http_method method;
	enum http_type type;
	char *url;
	char *host;
	uint64_t range_from;
	uint64_t range_to;
	uint64_t content_length;
	char *boundary;
	uint8_t range : 1;
};

void init_http_header(struct http_header *data);
int parse_header(struct http_header *, int);
void delete_http_header(struct http_header *);

#endif
