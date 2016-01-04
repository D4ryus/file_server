#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "types.h"
#include "http.h"

struct dir {
	char *name;
	int length;
	struct file *files[];
};

struct dir *get_dir(const char *);
char *dir_to_table(int, const char *, int);
void free_dir(struct dir *);

#endif
