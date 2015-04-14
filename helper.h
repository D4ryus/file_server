#ifndef HELPER_H
#define HELPER_H

#include "types.h"

/**
 * if negative number is return, error occured
 * STAT_OK      ( 0) : everything went fine.
 * WRITE_CLOSED (-1) : could not write, client closed connection
 * ZERO_WRITTEN (-2) : could not write, 0 bytes written
 */
int send_text(int, char*, size_t);

/**
 * if negative number is return, error occured
 * STAT_OK      ( 0) : everything went fine.
 * WRITE_CLOSED (-1) : could not write, client closed connection
 * ZERO_WRITTEN (-2) : could not write, 0 bytes written
 */
int send_file(struct data_store*);

/**
 * will reallocated dst and strcat src onto it
 */
char* concat(char*, const char*);

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int is_directory(const char*);
/**
 * check if first string starts with second string
 */
int starts_with(const char*, const char*);

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
 * prints usage and quits
 */
void usage_quit(const char*);

/**
 * returns content type of given file type
 * given "html" returns  "text/html"
 * given "gz"   returns  "application/gzip"
 * if type is not recognized, NULL will be returned
 */
char* get_content_encoding(const char*);

#endif
