#ifndef HELPER_H
#define HELPER_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

/**
 * will reallocated dst and strcat src onto it
 */
char* _concat(char* dst, const char* src);

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int _is_directory(char*);

/**
 * prints error and quits
 */
void _quit(const char*);

/**
 * check if first string starts with second string
 */
int starts_with(const char*, const char*);

/**
 * reads in given file, saves length in given long,
 * returns pointer to content
 */
void* _file_to_buf(const char*, size_t*);

#endif
