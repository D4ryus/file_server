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

void _quit(const char*);

int starts_with(const char*, const char*);

#endif
