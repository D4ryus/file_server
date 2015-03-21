#ifndef FILE_LIST_H
#define FILE_LIST_H

#include <dirent.h>

#include "helper.h"

struct file {
        char* name;
        char type[17];
        char time[20];
        off_t size;
};

struct dir {
        int length;
        char* name;
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
 * adds directory information to the given char*, uses realloc in text
 */
char* dir_to_html_table(char* text, const struct dir *d);

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir* add_file_to_dir(struct dir *d, struct dirent *dp);

/**
 * creates a dir stuct with from given directory
 */
struct dir* get_dir(char*);

int comp(const void*, const void*);

#endif
