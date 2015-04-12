#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "messages.h"
#include "helper.h"
#ifdef NCURSES
#include "ncurses_messages.h"
#endif

/**
 * see config.h
 */
extern char *ROOT_DIR;
extern FILE *_LOG_FILE;
extern int VERBOSITY;
extern int COLOR;
extern size_t UPDATE_TIMEOUT;
#ifdef NCURSES
extern int USE_NCURSES;
#endif

static struct status_list_node *first = NULL;
static pthread_mutex_t status_list_mutex;

void
init_messages(pthread_t *thread, const pthread_attr_t *attr)
{
        int error;

        error = 0;

#ifdef NCURSES
        ncurses_init();
        if (USE_NCURSES || VERBOSITY >= 3) {
#else
        if (VERBOSITY >= 3) {
#endif
                pthread_mutex_init(&status_list_mutex, NULL);
                error = pthread_create(thread, attr, &print_loop, NULL);
                if (error != 0) {
                        err_quit(__FILE__, __LINE__, __func__, "pthread_create() != 0");
                }
        }
}

void
*print_loop(void *ignored)
{
        struct status_list_node *cur;
        int position;
        position = 0;

        while (1) {
#ifdef NCURSES
                ncurses_update_begin(position);
#endif
                position = 0;
                pthread_mutex_lock(&status_list_mutex);
                if (first == NULL) {
                        goto sleep;
                }
                for (cur = first; cur != NULL; cur = cur->next, position++) {
                        format_and_print(cur, position);
                        /* TODO: sum up size here */
                }
sleep:
                pthread_mutex_unlock(&status_list_mutex);
#ifdef NCURSES
                ncurses_update_end(position);
#endif
                sleep((uint)UPDATE_TIMEOUT);
        }
#ifdef NCURSES
        ncurses_terminate();
#endif
        pthread_mutex_destroy(&status_list_mutex);
}

void
print_info(struct data_store *data, const enum message_type type, const char *message, int position)
{
        FILE *stream;
        char *m_type;

        switch (type) {
                case ERROR:
                        m_type = "error";
                        break;
                case SENT:
                        m_type = "sent";
                        break;
                case ACCEPTED:
                        m_type = "accepted";
                        break;
                case TRANSFER_STATUS:
                        m_type = "transfer_status";
                        break;
                default:
                        m_type = "";
                        break;
        }

#ifdef NCURSES
        ncurses_print_info(data, m_type, message, position);
#endif

        switch (type) {
                case ERROR:
                        if (VERBOSITY < 1) {
                                return;
                        }
                        break;
                case SENT:
                        if (VERBOSITY < 2) {
                                return;
                        }
                        break;
                case ACCEPTED:
                case TRANSFER_STATUS:
                        if (VERBOSITY < 3) {
                                return;
                        }
                default:
                        break;
        }

        if (_LOG_FILE != NULL) {
                stream = _LOG_FILE;
        } else {
                stream = stdout;
        }

        if (COLOR) {
                if (position == -1) {
                        position = 7;
                } else {
                        position = (position % 6) + 1; /* first (0) color is black, last (7) color is white */
                }
                fprintf(stream, "\x1b[3%dm[%15s:%-5d - %3d]: %-15s - %s\x1b[39;49m\n",
                                position,
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        } else {
                position = 0;
                fprintf(stream, "[%15s:%-5d - %3d]: %-15s - %s\n",
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        }

        if (_LOG_FILE != NULL) {
                fflush(_LOG_FILE);
        }

        return;
}

void
format_and_print(struct status_list_node *cur, const int position)
{
        /**
         * later last_written is set and the initial written value is needed,
         * to not read multiple times this value holds the inital value
         */
        size_t synched_written;
        size_t one;
        size_t written;
        size_t left;
        size_t size;
        size_t bytes_per_tval;
        char   *written_type;
        char   *left_type;
        char   *size_type;
        char   *bytes_per_tval_type;
        char   message_buffer[256];

        one = 1;
        synched_written = cur->data->written; /* read value only once from struct */

        written        = synched_written;
        left           = cur->data->body_length - synched_written;
        size           = cur->data->body_length;
        bytes_per_tval = (synched_written - cur->data->last_written) / UPDATE_TIMEOUT;
        /* set last written to inital read value */
        cur->data->last_written = cur->data->written;

        if (written > (one << 33)) { /* 8gb */
                written = written >> 30;
                written_type = "gb";
        } else if (written > (one << 23)) { /* 8mb */
                written = written >> 20;
                written_type = "mb";
        } else if (written > (one << 13)) { /* 8kb */
                written = written >> 10;
                written_type = "kb";
        } else {
                written_type = "b";
        }

        if (left > (one << 33)) { /* 8gb */
                left = left >> 30;
                left_type = "gb";
        } else if (left > (one << 23)) { /* 8mb */
                left = left >> 20;
                left_type = "mb";
        } else if (left > (one << 13)) { /* 8kb */
                left = left >> 10;
                left_type = "kb";
        } else {
                left_type = "b";
        }

        if (size > (one << 33)) { /* 8gb */
                size = size >> 30;
                size_type = "gb";
        } else if (size > (one << 23)) { /* 8mb */
                size = size >> 20;
                size_type = "mb";
        } else if (size > (one << 13)) { /* 8kb */
                size = size >> 10;
                size_type = "kb";
        } else {
                size_type = "b";
        }

        if (bytes_per_tval > (one << 33)) { /* 8gb */
                bytes_per_tval = bytes_per_tval >> 30;
                bytes_per_tval_type = "gb";
        } else if (bytes_per_tval > (one << 23)) { /* 8mb */
                bytes_per_tval = bytes_per_tval >> 20;
                bytes_per_tval_type = "mb";
        } else if (bytes_per_tval > (one << 13)) { /* 8kb */
                bytes_per_tval = bytes_per_tval >> 10;
                bytes_per_tval_type = "kb";
        } else {
                bytes_per_tval_type = "b";
        }

        sprintf(message_buffer, "%3lu%% [%4lu%2s/%4lu%2s (%4lu%2s)] %4lu%2s/%lus - %s",
                             synched_written * 100 / cur->data->body_length,
                             written,
                             written_type,
                             size,
                             size_type,
                             left,
                             left_type,
                             bytes_per_tval,
                             bytes_per_tval_type,
                             UPDATE_TIMEOUT,
                             cur->data->body + strlen(ROOT_DIR));

        print_info(cur->data, TRANSFER_STATUS, message_buffer, position);
}

void
add_hook(struct data_store *new_data)
{
        struct status_list_node *cur;
        struct status_list_node *new_node;

        new_node = err_malloc(sizeof(struct status_list_node));
        new_node->data = new_data;
        new_node->next = NULL;

        pthread_mutex_lock(&status_list_mutex);
        if (first == NULL) {
                first = new_node;
        } else {
                for (cur = first; cur->next != NULL; cur = cur->next) {
                }
                cur->next = new_node;
        }
        pthread_mutex_unlock(&status_list_mutex);

        return;
}

void
remove_hook(struct data_store *del_data)
{
        struct status_list_node *cur;
        struct status_list_node *last;

        last = NULL;

        if (first == NULL || del_data == NULL) {
                return;
        }

        pthread_mutex_lock(&status_list_mutex);
        for (cur = first; cur != NULL; cur = cur->next) {
                if (cur->data == del_data) {
                        if (cur == first) {
                                first = cur->next;
                        } else {
                                last->next = cur->next;
                        }
                        break;
                }
                last = cur;
        }
        pthread_mutex_unlock(&status_list_mutex);
        free(cur);

        return;
}

