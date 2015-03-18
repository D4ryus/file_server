#ifndef HELPER_H
#define HELPER_H

#include <sys/types.h>

/**
 * will reallocated dst and strcat src onto it
 */
char* concat(char* dst, const char* src);

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int is_directory(char*);

/**
 * prints error and quits
 */
void quit(const char*);

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

#endif
