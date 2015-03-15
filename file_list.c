#include "file_list.h"

void
free_dir(struct dir *d)
{
        if (d == NULL) {
                return;
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
 * will reallocated dst and strcat src onto it
 */
char*
_concat(char* dst, const char* src)
{
        dst = realloc(dst, (sizeof(char) * strlen(dst)) + (sizeof(char) * strlen(src)) + 1);
        if (dst == NULL) {
                printf("ERROR! Could not allocate memory, exiting.\n");
                exit(1);
        }
        strncat(dst, src, strlen(src));

        return dst;
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
                text[0] = '\0';
        }

        int i;
        char buffer[512];

        for (i = 0; i < d->length; i++) {
                if (d->files[i]->name == NULL) {
                        continue;
                }
                sprintf(buffer,
                        "<tr><td><a href=\"/%s\">%s</a></td><td>%s</td></tr>",
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
        d->files[d->length]->name = malloc(sizeof(char) * strlen(dp->d_name) + 1);
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
                printf("not found\n");
                return NULL;
        }

        struct dirent *dp;
        struct dir *result = (struct dir*)malloc(sizeof(struct dir));
        result->length = 0;

        int i;
        for (i = 0; (dp = (struct dirent *)readdir(dirp)) != NULL; i++) {
                result = _add_file_to_dir(result, dp);
        }

        (void)closedir(dirp);

        return result;
}

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int
_is_directory(char *f)
{
        struct stat st;
        lstat(f, &st);
        return (S_ISDIR(st.st_mode));
}
