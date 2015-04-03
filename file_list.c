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

void
dir_to_table(struct data_store *data, char* directory)
{
        int  i;
        char buffer[512];
        struct dir *d;

        d = get_dir(directory);

        if (d->files == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "get_dir()->files are NULL");
        }

        if (data->req_type == HTTP) {
                data->body = concat(data->body, "<style>"
                        "tbody tr:nth-child(odd) {"
                                "background: #eee;"
                        "}"
                "</style>"
                "<table style size='100%'>"
                        "<tbody>"
                        "<thead>"
                                "<tr>"
                                        "<th align='left'>Type</th>"
                                        "<th align='left'>Last modified</th>"
                                        "<th align='left'>Size</th>"
                                        "<th align='left'>Filename</th>"
                                "</tr>"
                        "</thead>");
        } else {
                sprintf(buffer,
                        "%-19s %-17s %-12s %s\n",
                        "Last modified",
                        "Type",
                        "Size",
                        "Filename");
                data->body = concat(data->body, buffer);
        }

        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                if (data->req_type == HTTP) {
                        sprintf(buffer,
                                "<tr>"
                                        "<td align='center'>%s</td>"
                                        "<td align='center'>%s</td>"
                                        "<td align='right'>%12li</td>"
                                        "<td align='left'><a href='%s/%s'>%s/%s</a></td>"
                                "</tr>",
                                d->files[i]->time,
                                d->files[i]->type,
                                (long)d->files[i]->size,
                                directory + strlen(data->root_dir),
                                d->files[i]->name,
                                directory + strlen(data->root_dir),
                                d->files[i]->name);
                } else {
                        sprintf(buffer, "%19s %17s %12li %s/%s\n",
                                d->files[i]->time,
                                d->files[i]->type,
                                (long)d->files[i]->size,
                                directory + strlen(data->root_dir),
                                d->files[i]->name);
                }
                data->body = concat(data->body, buffer);
        }

        if (data->req_type == HTTP) {
                data->body = concat(data->body, "</tbody></table>");
        }

        data->body_length = strlen(data->body);

        free_dir(d);

        return;
}

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

        d = (struct dir *)err_realloc(d, sizeof(struct dir) + ((size_t)(d->length + 1) * sizeof(struct file*)));

        d->files[d->length] = err_malloc(sizeof(struct file));
        tmp = d->files[d->length];
        d->length++;
        tmp->name = err_malloc(strlen(file) + 1);
        strncpy(tmp->name, file, strlen(file) + 1);

        combined_path = err_malloc(strlen(directory) + strlen(file) + 2);
        memcpy(combined_path, directory, strlen(directory) + 1);
        combined_path[strlen(directory)] = '/';
        memcpy(combined_path + strlen(directory) + 1, file, strlen(file) + 1);
        if (stat(combined_path, &sb) == -1) {
                err_quit(__FILE__, __LINE__, __func__, "stat() retuned -1");
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

struct dir*
get_dir(char *directory)
{
        DIR           *dirp;
        struct dirent *dp;
        struct dir    *result;
        int           i;

        dirp = opendir(directory);
        if (dirp == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "opendir() returned NULL");
        }

        result = (struct dir*)err_malloc(sizeof(struct dir));
        result->length = 0;
        result->name = err_malloc(strlen(directory) + 1);
        strncpy(result->name, directory, strlen(directory) + 1);

        for (i = 0; (dp = (struct dirent *)readdir(dirp)) != NULL; i++) {
                result = add_file_to_dir(result, dp->d_name, directory);
        }

        closedir(dirp);

        qsort(result->files, (size_t)result->length, sizeof(struct file *), compare_files);

        return result;
}

int
compare_files(const void *elem1, const void *elem2) 
{
        const struct file *file1 = *(struct file * const *)elem1;
        const struct file *file2 = *(struct file * const *)elem2;

        return strcmp(file1->name, file2->name);
}
