#ifndef FILE_LIST_H
#define FILE_LIST_H

#include "types.h"
#include "parse_http.h"

struct dir {
	char *name;
	int length;
	struct file *files[];
};

struct dir *get_dir(char *);
char *dir_to_table(enum http_type, char *);
void free_dir(struct dir *);

#endif
