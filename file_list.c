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
char*
dir_to_plain_table(char* text, const struct dir *d)
{
        if (d->files == NULL) {
                return NULL;
        }
        if (text == NULL) {
                text = malloc(sizeof(char));
                if (text == NULL) {
                        mem_error("dir_to_plain_table()", "text", sizeof(char));
                }
                text[0] = '\0';
        }

        int  i;
        char buffer[512];

        sprintf(buffer,
                "%-19s %-17s %-12s %s\n",
                "Last modified",
                "Type",
                "Size",
                "Filename");
        text = concat(text, buffer);
        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer, "%19s %17s %12li /%s%s%s\n",
                        d->files[i]->time,
                        d->files[i]->type,
                        (long)d->files[i]->size,
                        strcmp(d->name, ".") == 0 ? "" : d->name,
                        strcmp(d->name, ".") == 0 ? "" : "/",
                        d->files[i]->name);
                text = concat(text, buffer);
        }

        return text;
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

        int  i;
        char buffer[512];

        text = concat(text, "<style>"
                "table, td, th {"
                                "text-align: right;"
                        "}"
                "tbody tr:nth-child(odd) {"
                                "background: #eee;"
                        "}"
        "</style>"
        "<table style size='100%'>"
                "<tbody>"
                "<thead>"
                        "<tr>"
                                "<th>Filename</th>"
                                "<th>Type</th>"
                                "<th>Last modified</th>"
                                "<th>Size</th>"
                        "</tr>"
                "</thead>");

        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer,
                        "<tr>"
                                "<td><a href='/%s%s%s'>%s</a></td>"
                                "<td>%s</td>"
                                "<td>%s</td>"
                                "<td>%12li</td>"
                        "</tr>",
                        strcmp(d->name, ".") == 0 ? "" : d->name,
                        strcmp(d->name, ".") == 0 ? "" : "/",
                        d->files[i]->name,
                        d->files[i]->name,
                        d->files[i]->type,
                        d->files[i]->time,
                        (long)d->files[i]->size);
                text = concat(text, buffer);
        }
        text = concat(text, "</tbody></table>");

        return text;
}

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir*
add_file_to_dir(struct dir *d, char *file, char* directory)
{
        if (file == NULL) {
                printf("file is null, dunno\n");
                return d;
        }

        struct stat sb;
        struct file *tmp;
        char        *combined_path;

        d = (struct dir *)realloc(d, sizeof(struct dir) + ((size_t)(d->length + 1) * sizeof(struct file*)));

        d->files[d->length] = malloc(sizeof(struct file));
        if (d->files[d->length] == NULL) {
                mem_error("add_file_to_dir()", "d->files[d->length]", sizeof(struct file));
        }
        tmp = d->files[d->length];
        d->length++;
        tmp->name = malloc(strlen(file) + 1);
        if (tmp->name == NULL) {
                mem_error("add_file_to_dir()", "tmp->name",
                                strlen(file) + 1);
        }
        strncpy(tmp->name, file, strlen(file) + 1);

        combined_path = malloc(strlen(directory) + strlen(file) + 2);
        if (combined_path == NULL) {
                mem_error("add_file_to_dir()", "combined_path", strlen(directory) + strlen(file) + 2);
        }
        combined_path[0] = '\0';
        memcpy(combined_path, directory, strlen(directory) + 1);
        combined_path[strlen(directory)] = '/';
        memcpy(combined_path + strlen(directory) + 1, file, strlen(file) + 1);
        if (stat(combined_path, &sb) == -1) {
                quit("ERROR: add_file_to_dir()");
        }
        free(combined_path);

        /* TODO: replace this 17 with a config variable, see file struct */
        switch (sb.st_mode & S_IFMT) {
                case S_IFREG:  strncpy(tmp->type, "regular file"    , 17); break;
                case S_IFDIR:  strncpy(tmp->type, "directory"       , 17); break;
                case S_IFLNK:  strncpy(tmp->type, "symlink"         , 17); break;
                case S_IFBLK:  strncpy(tmp->type, "block device"    , 17); break;
                case S_IFCHR:  strncpy(tmp->type, "character device", 17); break;
                case S_IFIFO:  strncpy(tmp->type, "fifo/pipe"       , 17); break;
                case S_IFSOCK: strncpy(tmp->type, "socket"          , 17); break;
                default:       strncpy(tmp->type, "unknown"         , 17); break;
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
        DIR           *dirp;
        struct dirent *dp;
        struct dir    *result;
        int           i;

        dirp = opendir(directory);
        if (dirp == NULL) {
                quit("ERROR: get_dir()");
        }

        result = (struct dir*)malloc(sizeof(struct dir));
        if (result == NULL) {
                mem_error("get_dir()", "result", sizeof(struct  dir));
        }
        result->length = 0;
        result->name = malloc(strlen(directory) + 1);
        if (result->name == NULL) {
                mem_error("get_dir()", "result->name",
                                strlen(directory) + 1);
        }
        strncpy(result->name, directory, strlen(directory) + 1);

        for (i = 0; (dp = (struct dirent *)readdir(dirp)) != NULL; i++) {
                result = add_file_to_dir(result, dp->d_name, directory);
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
