#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "helper.h"

// BUFFSIZE_WRITE bytes are written to socket
#define BUFFSIZE_WRITE 8192

int
send_text(int socket, char* text, size_t length)
{
        ssize_t write_res;
        int     sending;
        size_t  sent;
        int     ret_status;

        write_res = 0;
        sent = 0;
        sending = 1;
        ret_status = 0;

        while (sending) {
                write_res = write(socket, text + sent, length - sent);
                if (write_res == -1) {
                        ret_status = -1;
                        break;
                } else if (write_res == 0) {
                        ret_status = -2;
                        break;
                }
                sent = sent + (size_t)write_res;
                if (sent != length) {
                        continue;
                }
                sending = 0;
        }

        return ret_status;
}

int
send_file(struct data_store *data)
{
        ssize_t write_res;
        int     sending;
        size_t  read;
        size_t  sent;
        size_t  written;
        size_t  last_written;
        char    buffer[BUFFSIZE_WRITE];
        FILE    *f;
        time_t  last_time;
        time_t  current_time;
        char    message_buffer[256];
        int     ret_status;

        f = fopen(data->body, "rb");
        if (f == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "fopen() retuned NULL");
        }

        write_res = 0;
        last_time = time(NULL);
        sending = 1;
        written = 0;
        last_written = 0;
        ret_status = 0;

        while (sending) {
                read = fread(buffer, 1, BUFFSIZE_WRITE, f);
                if (read < BUFFSIZE_WRITE) {
                        sending = 0;
                }
                sent = 0;
                while (sent < read) {
                        write_res = write(data->socket, buffer + sent, read - sent);
                        if (write_res == -1) {
                                sending = 0;
                                ret_status = -1;
                                break;
                        } else if (write_res == 0) {
                                sending = 0;
                                ret_status = -2;
                                break;
                        }
                        sent = sent + (size_t)write_res;
                }
                written += sent;
                current_time = time(NULL);
                if ((current_time - last_time) > 1) {
                        sprintf(message_buffer, "/%-19s size: %12lub written: %12lub remaining: %12lub %12lub/s %3lu%%",
                                             data->body,
                                             data->body_length,
                                             written,
                                             data->body_length - written,
                                             (written - last_written) / (!sending ? 1 : (size_t)(current_time - last_time)),
                                             written * 100 / data->body_length);
                        print_info(data, "requested", message_buffer);
                        last_time = current_time;
                        last_written = written;
                }
        }
        fclose(f);

        return ret_status;
}

void
print_info(struct data_store *data, char *type, char *message)
{
        printf("[%15s:%-5d - ID: %5d]: %-10s - %s\n",
                        data->ip,
                        data->port,
                        data->socket,
                        type,
                        message);
}

char*
concat(char* dst, const char* src)
{
        dst = err_realloc(dst, strlen(dst) + strlen(src) + 1);
        strncat(dst, src, strlen(src));

        return dst;
}

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int
is_directory(const char *path)
{
        struct stat s;

        if (stat(path, &s) == 0) {
                if (s.st_mode & S_IFDIR) {
                        return 1;
                } else if (s.st_mode & S_IFREG) {
                        return 0;
                } else {
                        err_quit(__FILE__, __LINE__, __func__, "stat() has no file nor a directory");
                }
        } else {
                err_quit(__FILE__, __LINE__, __func__, "stat() retuned -1");
        }

        return 0;
}

int
starts_with(const char *line, const char *prefix)
{
        if (line == NULL || prefix == NULL) {
                return 0;
        }
        while (*prefix) {
                if (*prefix++ != *line++) {
                        return 0;
                }
        }

        return 1;
}

void*
file_to_buf(const char *file, size_t *length)
{
        FILE *fptr;
        char *buf;

        fptr = fopen(file, "rb");
        if (!fptr) {
                return NULL;
        }
        fseek(fptr, 0, SEEK_END);
        *length = (size_t)ftell(fptr);
        buf = (void*)err_malloc(*length + 1);
        fseek(fptr, 0, SEEK_SET);
        fread(buf, (*length), 1, fptr);
        fclose(fptr);
        buf[*length] = '\0';

        return buf;
}

void
*err_malloc(size_t size)
{
        void *tmp;

        tmp = malloc(size);
        if (tmp == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "could not malloc");
        }

        return tmp;
}

void
*err_realloc(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
        if (ptr == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "could not realloc");
        }

        return ptr;
}

void
err_quit(const char *file, const int line, const char *function, const char *msg)
{
        fprintf(stderr, "%s:%d:%s: error: ", file, line, function);
        perror(msg);
        exit(1);
}
