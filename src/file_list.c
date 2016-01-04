#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "globals.h"
#include "file_list.h"
#include "helper.h"
#include "defines.h"

struct file {
	char *name;
	off_t size;
	char time[20];
	uint8_t is_dir;
};

static struct dir *add_file_to_dir(struct dir *, const char *, const char *);
static int compare_files(const void *, const void *);

/*
 * creates a dir stuct with from given directory.
 * returns NULL if permission is denied.
 */
struct dir *
get_dir(const char *directory)
{
	DIR *dirp;
	int i;
	struct dirent *dp;
	struct dir *result;

	dirp = opendir(directory);
	if (!dirp) {
		if (errno == EACCES) {
			return NULL;
		}
		die(ERR_INFO, "opendir()");
	}

	result = (struct dir *)err_malloc(sizeof(struct dir));
	result->length = 0;
	result->name = err_malloc(strlen(directory) + 1);
	strncpy(result->name, directory, strlen(directory) + 1);

	for (i = 0; (dp = (struct dirent *)readdir(dirp)); i++) {
		result = add_file_to_dir(result, dp->d_name, directory);
	}

	closedir(dirp);

	qsort(result->files, (size_t)result->length, sizeof(struct file *),
	    compare_files);

	return result;
}

/*
 * returns given directory as formated string
 */
char *
dir_to_table(int http, const char *dir, int upload)
{
	int i;
	char buffer[TABLE_BUFFER_SIZE];
	struct dir *d;
	char fmt_size[7];
	const char **table_ptr;
	char *table_buffer;
	char *directory;
	const char *requ;

	table_buffer = NULL;
	if (http) {
		table_buffer = concat(table_buffer, HTTP_TOP);
	}

	if (strcmp(dir, "/") == 0) {
		requ = "";
	} else {
		requ = dir;
	}

	directory = NULL;
	directory = concat(concat(directory, ROOT_DIR), requ);

	d = get_dir(directory);

	if (!d) {
		die(ERR_INFO, "cannot open directory");
	}

	if (!d->files) {
		die(ERR_INFO, "get_dir()->files are NULL");
	}

	if (http) {
		table_ptr = TABLE_HTML;
	} else {
		table_ptr = TABLE_PLAIN;
	}

	memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);
	/* table head, see globals.h */
	snprintf(buffer, (size_t)TABLE_BUFFER_SIZE, table_ptr[0],
	    "Last_modified",
	    "Size",
	    "Filename");

	table_buffer = concat(table_buffer, buffer);
	memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);

	for (i = 0; i < d->length; i++) {
		if (!d->files[i]->name) {
			continue;
		}
		/* table body */
		snprintf(buffer, (size_t)TABLE_BUFFER_SIZE, table_ptr[1],
		    d->files[i]->time,
		    format_size((uint64_t)d->files[i]->size, fmt_size),
		    requ,
		    d->files[i]->name,
		    d->files[i]->is_dir ? "/" : "");
		table_buffer = concat(table_buffer, buffer);
		memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);
	}

	table_buffer = concat(table_buffer, table_ptr[2]); /* table end */

	free_dir(d);
	d = NULL;

	if (http) {
		/* if upload allowed, print upload form */
		if (upload) {
			char *buff;
			size_t length;

			length = strlen(HTTP_UPLOAD) + strlen(directory) + 1;
			buff = err_malloc(length);
			snprintf(buff, length, HTTP_UPLOAD, directory);
			table_buffer = concat(table_buffer, buff);
			free(buff);
			buff = NULL;
		}
		table_buffer = concat(table_buffer, HTTP_BOT);
	}

	free(directory);
	directory = NULL;


	return table_buffer;
}

/*
 * free's dir struct + file list + file names
 */
void
free_dir(struct dir *d)
{
	int i;

	if (!d) {
		return;
	}

	if (d->name) {
		free(d->name);
		d->name = NULL;
	}

	for (i = 0; i < d->length; i++) {
		if (d->files[i]->name) {
			free(d->files[i]->name);
			d->files[i]->name = NULL;
		}
		free(d->files[i]);
		d->files[i] = NULL;
	}

	free(d);
	d = NULL;

	return;
}

/*
 * adds a file to the given dir struct, usses realloc
 */
static struct dir *
add_file_to_dir(struct dir *d, const char *file, const char *directory)
{
	char *combined_path;
	struct stat sb;
	struct file *new_file;
	struct tm *tmp;

	if (!file) {
		die(ERR_INFO, "tried to add file which was NULL");
		return d;
	}

	combined_path = err_malloc(strlen(directory) + strlen(file) + 2);
	memcpy(combined_path, directory, strlen(directory) + 1);
	combined_path[strlen(directory)] = '/';
	memcpy(combined_path + strlen(directory) + 1, file, strlen(file) + 1);
	if (stat(combined_path, &sb) == -1) {
		die(ERR_INFO,
		    "stat() returned -1 (could be a dead symbolic link)");
	}
	free(combined_path);
	combined_path = NULL;

	new_file = err_malloc(sizeof(struct file));
	new_file->name = err_malloc(strlen(file) + 1);
	strncpy(new_file->name, file, strlen(file) + 1);

	if ((sb.st_mode & S_IFMT) == S_IFDIR) {
		new_file->is_dir = 1;
	} else {
		new_file->is_dir = 0;
	}

	tmp = localtime(&sb.st_mtime);
	if (!tmp) {
		die(ERR_INFO, "localtime()");
	}
	if (!strftime(new_file->time, (size_t)20, "%Y-%m-%d %H:%M:%S", tmp)) {
		die(ERR_INFO, "strftime()");
	}

	new_file->size = sb.st_size;

	/* remalloc directory struct to fit the new filepointer */
	d = (struct dir *)err_realloc(d, sizeof(struct dir) +
			    ((size_t)(d->length + 1) * sizeof(struct file *)));

	/* set new ptr to new_file */
	d->files[d->length] = new_file;

	d->length++;

	return d;
}

/*
 * compare function to compare file structs, used for qsort
 */
static int
compare_files(const void *elem1, const void *elem2)
{
	const struct file *file1 = *(struct file * const *)elem1;
	const struct file *file2 = *(struct file * const *)elem2;

	return strcmp(file1->name, file2->name);
}
