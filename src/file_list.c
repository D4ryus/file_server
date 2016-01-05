#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
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
 * Http table config
 * -----------------
 */

/*
 * string which will be at the top of http table response
 */
const char *HTTP_TOP =
"<!DOCTYPE html>"
"<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>"
"<style>"
	"html, body, tbody {"
		"background-color: #303030;"
		"color: #888888;"
	"}"
	"th, h3, h2, a:hover {"
		"color: #ffffff;"
	"}"
	"a {"
		"color: #44aaff;"
		"text-decoration: none;"
	"}"
	"table.download {"
		"margin-top: -18px;"
	"}"
	"form.upload {"
		"margin-top: -8px;"
	"}"
	"h3.download, h3.upload {"
		"left: 0;"
		"border-bottom: 1px solid #ffffff;"
		"width: 100%;"
		"padding: 0.2em;"
	"}"
"</style>"
"<html>"
	"<head>"
		"<title>file_server version 0.3</title>"
	"</head>"
	"<center>"
	"<table><tbody><tr><td>"
		"<center>"
			"<h2>file_server version 0.3</h2>"
		"</center>"
		"<div align='left'>"
			"<h3 class='download'>Download</h3>"
		"</div>"
	"</td></tr><tr><td>"
		"<body>";

/*
 * table head values which will be filed in at %s
 * given values are:
 * "Last modified" "Size" "Filename"
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]" "[filesize]" "[directory]" "[filename]"
 * the buffer limit is defined in defines.h
 */
const char *TABLE_PLAIN[3] = {
	/* table head */
	"%-19s %-6s %s\n"
	"-----------------------------------\n",
	/* table body */
	"%19s %6s %s/%s%s\n",
	/* table end */
	""};

const char *TABLE_HTML[3] = {
/* table head */
"<table class='download'>"
	"<thead>"
		"<tr>"
			"<th align='left'>%s</th>"
			"<th align='left'>%s</th>"
			"<th align='left'>%s</th>"
		"</tr>"
	"</thead>"
	"<tbody>",
/* table body */
		"<tr>"
			"<td align='center'>%s</td>"
			"<td align='right'>%s</td>"
			"<td align='left'><a href=\"%s/%s\">%4$s%s</a></td>"
		"</tr>",
/* table end */
	"</tbody>"
"</table>"};

/*
 * if current ip is allowed to upload this will be displayed b4 HTTP_BOT,
 * %s will be replaced with upload directory
 */
const char *HTTP_UPLOAD =
			"<h3 class='upload'>Upload</h3>"
			"<form class='upload' action='%s'"
				     "method='post'"
				     "enctype='multipart/form-data'>"
				"<input type='file' name='file[]' multiple='true'>"
				"<button type='submit'>Upload</button>"
			"</form>";

/*
 * string which will be at the bottom of http table response
 */
const char *HTTP_BOT =
		"</body>"
	"</td></tr></tbody></table>"
	"<div align='right'>"
		"Powered by "
		"<a href='https://freedns.afraid.org/'>freedns.afraid.org</a>"
		" and "
		"<a href='http://www.vim.org/'>Vim</a>"
	"</div>"
	"</center>"
"</html>";

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
		check(errno != EACCES, "opendir(%s) returned NULL",
		    directory);
		return NULL;
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

	if (http) {
		table_buffer = concat(NULL, 1, HTTP_TOP);
	} else {
		table_buffer = NULL;
	}

	if (strcmp(dir, "/") == 0) {
		requ = "";
	} else {
		requ = dir;
	}

	directory = concat(NULL, 2, CONF.root_dir, requ);

	d = get_dir(directory);

	check(!d, "get_dir(\"%s\") returned NULL.", directory);
	check(!d->files, "get_dir(\"%s\")->files is NULL", directory);

	if (http) {
		table_ptr = TABLE_HTML;
	} else {
		table_ptr = TABLE_PLAIN;
	}

	memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);

	snprintf(buffer, (size_t)TABLE_BUFFER_SIZE, table_ptr[0],
	    "Last_modified",
	    "Size",
	    "Filename");

	table_buffer = concat(table_buffer, 1, buffer);
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
		table_buffer = concat(table_buffer, 1, buffer);
		memset(buffer, '\0', (size_t)TABLE_BUFFER_SIZE);
	}

	table_buffer = concat(table_buffer, 1, table_ptr[2]); /* table end */

	free_dir(d);
	d = NULL;

	if (http) {
		/* if upload allowed, print upload form */
		if (upload) {
			char *buff;
			size_t length;

			length = strlen(HTTP_UPLOAD) + strlen(dir) + 1;
			buff = err_malloc(length);
			snprintf(buff, length, HTTP_UPLOAD, dir);
			table_buffer = concat(table_buffer, 1, buff);
			free(buff);
			buff = NULL;
		}
		table_buffer = concat(table_buffer, 1, HTTP_BOT);
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

	check(!file, "tried to add file which was NULL");

	combined_path = err_malloc(strlen(directory) + strlen(file) + 2);
	memcpy(combined_path, directory, strlen(directory) + 1);
	combined_path[strlen(directory)] = '/';
	memcpy(combined_path + strlen(directory) + 1, file, strlen(file) + 1);

	check(stat(combined_path, &sb) == -1,
	    "stat(%s) returned -1 (could be a dead symbolic link)",
		    combined_path)

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
	check(!tmp, "localtime() returned NULL");

	check(!strftime(new_file->time, (size_t)20, "%Y-%m-%d %H:%M:%S", tmp),
	    "strftime() returned NULL");

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
