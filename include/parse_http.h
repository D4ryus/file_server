#ifndef PARSE_HTTP_H
#define PARSE_HTTP_H

#include <stdint.h>

enum http_method {
	GET,
	POST
};

struct http_header {
	enum http_method method;
	uint64_t host;
	uint64_t range_from;
	uint64_t range_to;
	uint64_t content_length;
	char *boundary;
	union {
		uint8_t R;
		struct {
			uint8_t missing:1; /* set if header field is missing */
			uint8_t plain:1;   /* set if plain request */
		} S;
	} flags;
};

struct http_keyword {
	char *key;
	int (*fn) (struct http_header *data, char *);
};

int parse_header(struct http_header *, int);
int get_line(int, char**);

int _parse_GET(struct http_header *, char *);
int _parse_POST(struct http_header *, char *);
int _parse_host(struct http_header *, char *);
int _parse_range(struct http_header *, char *);
int _parse_boundary(struct http_header *, char *);
int _parse_content_length(struct http_header *, char *);
int _parse_content_type(struct http_header *, char *);

#endif
