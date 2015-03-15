#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

struct file {
        char* name; 
        int is_dir;
};

struct dir {
        int length;
        struct file *files[]; 
};

/**
 * free's dir struct + file list + file names
 */
void free_dir(struct dir *d);

/**
 * prints out dir to stdout
 */
void print_dir(const struct dir *d);

/**
 * will reallocated dst and strcat src onto it
 */
char* _concat(char* dst, const char* src);

/**
 * adds directory information to the given char*, uses realloc in text
 */
char* get_html_from_dir(char* text, const struct dir *d);

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir* _add_file_to_dir(struct dir *d, struct dirent *dp);

/**
 * creates a dir stuct with from given directory
 */
struct dir* get_dir(char*);

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int _is_directory(char*);

#endif
