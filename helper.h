#ifndef HELPER_H
#define HELPER_H

#include <sys/types.h>

enum request_type {PLAIN, HTTP};
enum body_type {DATA, TEXT, ERR_404, ERR_403};

/**
 * per request a data store is generated and then during execution filed
 */
struct data_store {
        char   ip[16];                /* ip from client */
        int    port;                  /* port from client */
        int    socket;                /* socket descriptor */
        char   *url;                  /* requested file */
        char   head[254];             /* response header */
        char   *body;                 /* response body, if file filename */
        size_t body_length;           /* length of response body */
        enum  request_type req_type;  /* requested type (PLAIN or HTTP) */
        enum  body_type    body_type; /* type of body (TEXT or DATA) */
};

/**
 * mallocs a new data_store and sets initial values
 */
struct data_store* create_data_store(void);

/**
 * savely frees the given datastore from memory
 */
void free_data_store(struct data_store*);

/**
 * if negative number is return, error occured
 *  0 : everything went fine.
 * -1 : could not write, client closed connection
 * -2 : could not write, 0 bytes written
 */
int send_text(int, char*, size_t);

/**
 * if negative number is return, error occured
 *  0 : everything went fine.
 * -1 : could not write, client closed connection
 * -2 : could not write, 0 bytes written
 */
int send_file(struct data_store *data);

/**
 * prints info (ip port socket) + given type and message to stdout
 */
void print_info(struct data_store*, char*, char*);

/**
 * will reallocated dst and strcat src onto it
 */
char* concat(char* dst, const char* src);

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int is_directory(const char*);

/**
 * prints error with given information and exits
 */
void test(const int, const char*, const char*, const char*);
/**
 * check if first string starts with second string
 */
int starts_with(const char*, const char*);

/**
 * reads in given file, saves length in given long,
 * returns pointer to content
 */
void* file_to_buf(const char*, size_t*);

/**
 * prints out formatted error and errorcode and exit's
 */
void mem_error(const char*, const char*, const ulong);

/**
 * mallocs given size but also checks if succeded, if not exits
 */
void *err_malloc(size_t);

/**
 * reallocs given size but also checks if succeded, if not exits
 */
void *err_realloc(void*, size_t);

/**
 * prints out given information to stderr and exits
 */
void err_quit(const char*, const int, const char*, const char*);

/**
 * returns content type of given file type
 * given "html" returns  "text/html"
 * given "gz"   returns  "application/gzip"
 * if type is not recognized, NULL will be returned
 */
char* get_content_encoding(char*);

#endif
