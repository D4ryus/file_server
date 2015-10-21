#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "types.h"
#include "parse_http.h"

struct dir *get_dir(char *);
char *dir_to_table(enum http_type, char *);
void free_dir(struct dir *);

struct dir *_add_file_to_dir(struct dir *, char *, char *);
int _compare_files(const void *, const void *);

#endif
