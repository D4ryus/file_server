#include "helper.h"

/**
 * will reallocated dst and strcat src onto it
 */
char*
_concat(char* dst, const char* src)
{
        dst = realloc(dst, (sizeof(char) * strlen(dst)) + (sizeof(char) * strlen(src)) + 1);
        if (dst == NULL) {
                _quit("ERROR! Could not allocate memory");
        }
        strncat(dst, src, strlen(src));

        return dst;
}

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int
_is_directory(char *path)
{
        struct stat s;
        if (stat(path,&s) == 0) {
                if (s.st_mode & S_IFDIR) {
                        return 1;
                } else if (s.st_mode & S_IFREG) {
                        return 0;
                } else {
                        _quit("ERROR: _is_directory() (its not a file, nor a dir)");
                }
        } else {
                _quit("ERROR: _is_directory()");
        }

        return 0;
}

void
_quit(const char *msg)
{
        perror(msg);
        exit(1);
}

int
starts_with(const char *line, const char *prefix)
{
        while (*prefix) {
                if (*prefix++ != *line++) {
                        return 0;
                }
        }

        return 1;
}

void*
_file_to_buf(const char *file, size_t *length)
{
        FILE *fptr;
        char *buf;

        fptr = fopen(file, "rb");
        if (!fptr) {
                return NULL;
        }
        fseek(fptr, 0, SEEK_END);
        *length = (size_t)ftell(fptr);
        buf = (void*)malloc(*length + 1);
        if (buf == NULL) {
                mem_error("_file_to_buf()", "buf", *length + 1);
        }
        fseek(fptr, 0, SEEK_SET);
        fread(buf, (*length), 1, fptr);
        fclose(fptr);
        buf[*length] = '\0';

        return buf;
}

void
mem_error(const char* func, const char* var, const ulong size)
{
        fprintf(stderr, "ERROR: could not malloc. func: %s, variable: %s, size: %lu",
                        func, var, size);
        perror(NULL);
        exit(1);
}
