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
get_html_from_dir(char* text, const struct dir *d)
{
        if (d->files == NULL) {
                return NULL;
        }
        if (text == NULL) {
                text = malloc(sizeof(char));
                if (text == NULL) {
                        mem_error("get_html_from_dir()", "text", sizeof(char));
                }
                text[0] = '\0';
        }

        int i;
        char buffer[512];

        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer,
                        "<tr><td><a href=\"%s/%s\">%s</a></td><td>%s</td></tr>",
                        d->name + 1, /* ignore away leading dot */
                        d->files[i]->name,
                        d->files[i]->name,
                        d->files[i]->is_dir ? "directory" : "file");
                text = _concat(text, buffer);
        }
        return text;
}

/**
 * adds a file to the given dir struct, usses realloc
 */
struct dir*
_add_file_to_dir(struct dir *d, struct dirent *dp)
{
        d = (struct dir *)realloc(d, sizeof(struct dir) + (ulong)((d->length + 1) * (int)sizeof(struct file*)));

        d->files[d->length] = malloc(sizeof(struct file));
        if (d->files[d->length] == NULL) {
                mem_error("_add_file_to_dir()", "d->files[d->length]", sizeof(struct file));
        }
        d->files[d->length]->name = malloc(sizeof(char) * strlen(dp->d_name) + 1);
        if (d->files[d->length]->name == NULL) {
                mem_error("_add_file_to_dir()", "d->files[d->length]->name",
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
                _quit("ERROR: get_dir()");
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
                result = _add_file_to_dir(result, dp);
        }

        closedir(dirp);

        return result;
}
