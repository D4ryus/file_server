#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "types.h"

struct dir *get_dir(char *);
void dir_to_table(struct data_store *, char *);
void free_dir(struct dir *);

struct dir *_add_file_to_dir(struct dir *, char *, char *);
int _compare_files(const void *, const void *);

#endif
