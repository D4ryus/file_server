#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                                d->files[i]->is_dir ? "directory" : "file",
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
        text = concat(text, "table, td, th { font-family: 'Iceland', cursive; font-size:140%;}");
        text = concat(text, "tbody tr:nth-child(odd) { background: #eee; }");
        text = concat(text, "</style><table style='width:30%' sortable>");
        text = concat(text, "<tbody>");
        text = concat(text, "<thead><tr><th>Filename</th><th>Type</th></tr></thead>");
        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer,
                        "<tr><td><a href='%s/%s'>%s</a></td><td>%s</td></tr>",
                        d->name + 1, /* ignore away leading dot */
                        d->files[i]->name,
                        d->files[i]->name,
                        d->files[i]->is_dir ? "directory" : "file");
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
        d = (struct dir *)realloc(d, sizeof(struct dir) + (ulong)((d->length + 1) * (int)sizeof(struct file*)));

        d->files[d->length] = malloc(sizeof(struct file));
        if (d->files[d->length] == NULL) {
                mem_error("add_file_to_dir()", "d->files[d->length]", sizeof(struct file));
        }
        d->files[d->length]->name = malloc(sizeof(char) * strlen(dp->d_name) + 1);
        if (d->files[d->length]->name == NULL) {
                mem_error("add_file_to_dir()", "d->files[d->length]->name",
                                sizeof(char) * strlen(dp->d_name) + 1);
        }
        strncpy(d->files[d->length]->name, dp->d_name, strlen(dp->d_name) + 1);
        d->files[d->length]->is_dir = dp->d_type == 4 ? 1 : 0;
        d->length++;

        return d;
}

/**
 * creates a dir stuct from given directory
 */
struct dir*
get_dir(char *directory)
{
        DIR *dirp = opendir(directory);
        if (dirp == NULL) {
                quit("ERROR: get_dir()");
        }

        struct dirent *dp;
        struct dir *result = (struct dir*)malloc(sizeof(struct dir));
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
