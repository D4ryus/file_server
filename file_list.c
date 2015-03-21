#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>

#include "file_list.h"

void
free_dir(struct dir *d)
{
        if (d == NULL) {
                return;
        }

        if (d->name != NULL) {
                free(d->name);
        }

        int i;
        for (i = 0; i < d->length; i++) {
                if (d->files[i] == NULL) {
                        continue;
                }
                if (d->files[i]->name != NULL) {
                        free(d->files[i]->name);
                }
                free(d->files[i]);
        }
        free(d);
}

/**
 * prints out dir to stdout
 */
void
print_dir(const struct dir *d)
{
        int i;
        printf("dir: %s\n", d->name);
        for (i = 0; i < d->length; i++) {
                if (d->files[i] == NULL) {
                        continue;
                }
                if (d->files[i]->name == NULL) {
                        continue;
                }
                printf("(%9s) - %s\n",
                                d->files[i]->type,
                                d->files[i]->name);
        }
}

/**
 * adds directory information to the given char*, uses realloc in text
 */
char*
dir_to_html_table(char* text, const struct dir *d)
{
        if (d->files == NULL) {
                return NULL;
        }
        if (text == NULL) {
                text = malloc(sizeof(char));
                if (text == NULL) {
                        mem_error("dir_to_html_table()", "text", sizeof(char));
                }
                text[0] = '\0';
        }

        int i;
        char buffer[512];
        text = concat(text, "<style>");
        text = concat(text, "table, td, th { font-family: 'Iceland', cursive; font-size:130%; text-align: right;}");
        text = concat(text, "tbody tr:nth-child(odd) { background: #eee; }");
        text = concat(text, "</style><table style='width:50%'>");
        text = concat(text, "<tbody>");
        text = concat(text, "<thead><tr><th>Filename</th><th>Type</th><th>Last modified</th><th>Size</th></tr></thead>");
        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer,
                        "<tr><td><a href='%s/%s'>%s</a></td><td>%s</td><td>%s</td><td>%d</td></tr>",
                        d->name + 1, /* ignore away leading dot */
                        d->files[i]->name,
                        d->files[i]->name,
                        d->files[i]->type,
                        d->files[i]->time,
                        d->files[i]->size);
                text = concat(text, buffer);
        }
        text = concat(text, "</tbody></table>");
        return text;
}

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir*
add_file_to_dir(struct dir *d, struct dirent *dp)
{
        struct stat sb;
        struct file* tmp;

        d = (struct dir *)realloc(d, sizeof(struct dir) + (ulong)((d->length + 1) * (int)sizeof(struct file*)));

        d->files[d->length] = malloc(sizeof(struct file));
        if (d->files[d->length] == NULL) {
                mem_error("add_file_to_dir()", "d->files[d->length]", sizeof(struct file));
        }
        tmp = d->files[d->length];
        d->length++;
        tmp->name = malloc(sizeof(char) * strlen(dp->d_name) + 1);
        if (tmp->name == NULL) {
                mem_error("add_file_to_dir()", "tmp->name",
                                sizeof(char) * strlen(dp->d_name) + 1);
        }
        strcpy(tmp->name, dp->d_name);

        if (stat(realpath(dp->d_name, NULL), &sb) == -1) {
                quit("ERROR: add_file_to_dir()");
        }

        switch (sb.st_mode & S_IFMT) {
                case S_IFREG:  strcpy(tmp->type, "regular file"    ); break;
                case S_IFDIR:  strcpy(tmp->type, "directory"       ); break;
                case S_IFLNK:  strcpy(tmp->type, "symlink"         ); break;
                case S_IFBLK:  strcpy(tmp->type, "block device"    ); break;
                case S_IFCHR:  strcpy(tmp->type, "character device"); break;
                case S_IFIFO:  strcpy(tmp->type, "fifo/pipe"       ); break;
                case S_IFSOCK: strcpy(tmp->type, "socket"          ); break;
                default:       strcpy(tmp->type, "unknown"         ); break;
        }
        strftime(tmp->time, 20, "%Y-%m-%d %H:%M:%S", localtime(&sb.st_mtime));
        tmp->size = sb.st_size;

        return d;
}

/**
 * creates a dir stuct from given directory
 */
struct dir*
get_dir(char *directory)
{
        DIR *dirp;
        struct dirent *dp;
        struct dir *result;

        dirp = opendir(directory);
        if (dirp == NULL) {
                quit("ERROR: get_dir()");
        }

        result = (struct dir*)malloc(sizeof(struct dir));
        if (result == NULL) {
                mem_error("get_dir()", "result", sizeof(struct  dir));
        }
        result->length = 0;
        result->name = malloc(sizeof(char) * (strlen(directory) + 1));
        if (result->name == NULL) {
                mem_error("get_dir()", "result->name",
                                sizeof(char) * (strlen(directory) + 1));
        }
        strncpy(result->name, directory, sizeof(char) * (strlen(directory) + 1));

        int i;
        for (i = 0; (dp = (struct dirent *)readdir(dirp)) != NULL; i++) {
                result = add_file_to_dir(result, dp);
        }

        closedir(dirp);

        qsort(result->files, (size_t)result->length, sizeof(struct file *), comp);

        return result;
}

int
comp(const void *elem1, const void *elem2) 
{
        const struct file *file1 = *(struct file * const *)elem1;
        const struct file *file2 = *(struct file * const *)elem2;

        return strcmp(file1->name, file2->name);
}
