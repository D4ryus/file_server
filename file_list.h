#ifndef FILE_LIST_H
#define FILE_LIST_H

#include <dirent.h>

#include "helper.h"

struct file {
        char  *name;
        char  type[17];
        char  time[20];
        off_t size;
};

struct dir {
        int  length;
        char *name;
        struct file *files[];
};

/**
 * free's dir struct + file list + file names
 */
void free_dir(struct dir*);

/**
 * opens given directory and adds data to given data_store
 */
void dir_to_table(struct data_store*, char*);

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir* add_file_to_dir(struct dir*, char*, char*);

/**
 * creates a dir stuct with from given directory
 */
struct dir* get_dir(char*);

/**
 * compare function to compare file structs, used for qsort
 */
int compare_files(const void*, const void*);

#endif
