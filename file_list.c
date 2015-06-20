#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "file_list.h"
#include "helper.h"
#include "defines.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern const char *TABLE_PLAIN[3];
extern const char *TABLE_HTML[3];

/*
 * creates a dir stuct with from given directory.
 * returns NULL if permission is denied.
 */
struct dir *
get_dir(char *directory)
{
	DIR *dirp;
	int i;
	struct dirent *dp;
	struct dir *result;

	dirp = opendir(directory);
	if (dirp == NULL) {
		if (errno == EACCES) {
			return NULL;
		}
		die(ERR_INFO, "opendir()");
	}

	result = (struct dir *)err_malloc(sizeof(struct dir));
	result->length = 0;
	result->name = err_malloc(strlen(directory) + 1);
	strncpy(result->name, directory, strlen(directory) + 1);

	for (i = 0; (dp = (struct dirent *)readdir(dirp)) != NULL; i++) {
		result = _add_file_to_dir(result, dp->d_name, directory);
	}

	closedir(dirp);

	qsort(result->files, (size_t)result->length, sizeof(struct file *),
	    _compare_files);

	return result;
}

/*
 * returns given directory as formated string
 */
char *
dir_to_table(enum request_type type, char *dir)
{
	int i;
	char buffer[TABLE_BUFFER_SIZE];
	struct dir *d;
	char fmt_size[7];
	const char **table_ptr;
	char *table_buffer;
	char *directory;
	char *requ;

	if (strcmp(dir, "/") == 0) {
		requ = "";
	} else {
		requ = dir;
	}

	directory = NULL;
	directory = concat(concat(directory, ROOT_DIR), requ);

	d = get_dir(directory);

	free(directory);

	if (d == NULL) {
		die(ERR_INFO, "cannot open directory");
	}

	if (d->files == NULL) {
		die(ERR_INFO, "get_dir()->files are NULL");
	}

	if (type == HTTP) {
		table_ptr = TABLE_HTML;
	} else {
		table_ptr = TABLE_PLAIN;
	}

	memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);
	/* table head, see globals.h */
	snprintf(buffer, (size_t)TABLE_BUFFER_SIZE, table_ptr[0],
	    "Last_modified",
	    "Type",
	    "Size",
	    "Filename");

	table_buffer = NULL;
	table_buffer = concat(table_buffer, buffer);
	memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);

	for (i = 0; i < d->length; i++) {
		if (d->files[i]->name == NULL) {
			continue;
		}
		/* table body */
		snprintf(buffer, (size_t)TABLE_BUFFER_SIZE, table_ptr[1],
		    d->files[i]->time,
		    d->files[i]->type,
		    format_size((uint64_t)d->files[i]->size, fmt_size),
		    requ,
		    d->files[i]->name);
		table_buffer = concat(table_buffer, buffer);
		memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);
	}

	table_buffer = concat(table_buffer, table_ptr[2]); /* table end */

	free_dir(d);

	return table_buffer;
}

/*
 * free's dir struct + file list + file names
 */
void
free_dir(struct dir *d)
{
	int i;

	if (d == NULL) {
		return;
	}

	if (d->name != NULL) {
		free(d->name);
	}

	for (i = 0; i < d->length; i++) {
		if (d->files[i]->name != NULL) {
			free(d->files[i]->name);
		}
		free(d->files[i]);
	}

	free(d);

	return;
}

/*
 * adds a file to the given dir struct, usses realloc
 */
struct dir *
_add_file_to_dir(struct dir *d, char *file, char *directory)
{
	char *combined_path;
	struct stat sb;
	struct file *new_file;
	struct tm *tmp;
	size_t n;

	if (file == NULL) {
		die(ERR_INFO, "tried to add file which was NULL");
		return d;
	}

	combined_path = err_malloc(strlen(directory) + strlen(file) + 2);
	memcpy(combined_path, directory, strlen(directory) + 1);
	combined_path[strlen(directory)] = '/';
	memcpy(combined_path + strlen(directory) + 1, file, strlen(file) + 1);
	if (stat(combined_path, &sb) == -1) {
		warning(ERR_INFO,
		    "stat() returned -1 (could be a dead symbolic link)");
		free(combined_path);
		return d;
	}
	free(combined_path);

	new_file = err_malloc(sizeof(struct file));
	new_file->name = err_malloc(strlen(file) + 1);
	strncpy(new_file->name, file, strlen(file) + 1);

	n = 11;
	switch (sb.st_mode & S_IFMT) {
		case S_IFREG:  strncpy(new_file->type, "file"      , n); break;
		case S_IFDIR:  strncpy(new_file->type, "directory" , n); break;
		case S_IFLNK:  strncpy(new_file->type, "symlink"   , n); break;
		case S_IFBLK:  strncpy(new_file->type, "blk_device", n); break;
		case S_IFCHR:  strncpy(new_file->type, "chr_device", n); break;
		case S_IFIFO:  strncpy(new_file->type, "fifo_pipe" , n); break;
		case S_IFSOCK: strncpy(new_file->type, "socket"    , n); break;
		default:       strncpy(new_file->type, "unknown"   , n); break;
	}

	tmp = localtime(&sb.st_mtime);
	if (tmp == NULL) {
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
int
_compare_files(const void *elem1, const void *elem2)
{
	const struct file *file1 = *(struct file * const *)elem1;
	const struct file *file2 = *(struct file * const *)elem2;

	return strcmp(file1->name, file2->name);
}
