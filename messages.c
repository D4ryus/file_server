#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "messages.h"
#include "helper.h"

/**
 * see config.h
 */
extern char *ROOT_DIR;
extern int VERBOSITY;
extern int COLOR;
extern size_t UPDATE_TIMEOUT;
#ifdef NCURSES
extern int USE_NCURSES;
#endif

#ifdef NCURSES
#include <signal.h>
#include <curses.h>
#endif

static struct status_list_node *first = NULL;
static pthread_mutex_t status_list_mutex;
#ifdef NCURSES
WINDOW *ncurses_window = NULL;
int resized = 0;;
#endif

void init_messages(pthread_t *thread, const pthread_attr_t *attr)
{
        int error;

        error = 0;

#ifdef NCURSES
        if (USE_NCURSES) {
                ncurses_window = initscr();
                if (ncurses_window == NULL) {
                        err_quit(__FILE__, __LINE__, __func__, "initscr() != NULL");
                }
                struct sigaction sa;
                memset(&sa, 0, sizeof(struct sigaction));
                sa.sa_handler = resize_handler;
                sigaction(SIGWINCH, &sa, NULL);
                refresh();
        }

        if (USE_NCURSES || VERBOSITY >= 3) {
#else
        if (VERBOSITY >= 3) {
#endif
                pthread_mutex_init(&status_list_mutex, NULL);
                /* start up a extra thread to print status, see message.c */
                error = pthread_create(thread, attr, &print_loop, NULL);
                if (error != 0) {
                        err_quit(__FILE__, __LINE__, __func__, "pthread_create() != 0");
                }
        }
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

void
*print_loop(void *ptr)
{
        struct status_list_node *cur;
        char message_buffer[256];
        int position;
        size_t written;
#ifdef NCURSES
        int i;
        int row;
        int col;
#endif 

        written = 0;

        while (1) {
                position = 0;
                pthread_mutex_lock(&status_list_mutex);
                if (first == NULL) {
                        goto sleep;
                }
                for (cur = first; cur != NULL; cur = cur->next, position++) {
                        written = cur->data->written;
                        sprintf(message_buffer, "%-20s size: %12lub %12lub/%lus %3lu%%",
                                             cur->data->body + strlen(ROOT_DIR),
                                             cur->data->body_length,
                                             (written - cur->data->last_written) / UPDATE_TIMEOUT,
                                             UPDATE_TIMEOUT,
                                             written * 100 / cur->data->body_length);
                        print_info(cur->data, TRANSFER_STATUS, message_buffer, position);
                        cur->data->last_written = written;
                }
sleep:
                pthread_mutex_unlock(&status_list_mutex);
#ifdef NCURSES
                getmaxyx(ncurses_window, row, col);

                for (i = position + 1; i < row; i++) {
                        move(i, 0);
                        clrtoeol();
                }
                refresh();
#endif
                sleep((uint)UPDATE_TIMEOUT);
        }
#ifdef NCURSES
        delwin(ncurses_window);
        endwin();
        refresh();
#endif
        pthread_mutex_destroy(&status_list_mutex);
}

void
print_info(struct data_store *data, enum message_type type, char *message, int position)
{
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
        if (USE_NCURSES) {
                if (resized) {
                        resized = 0;
                        endwin();
                        refresh();
                        clear();
                }
                mvprintw(position + 1, 0, "[%15s:%-5d - %3d]: %-15s - %s\n",
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
                if (type != TRANSFER_STATUS) {
                        refresh();
                }
        }
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

        if (COLOR) {
                if (position == -1) {
                        position = 7;
                } else {
                        position = (position % 6) + 1; /* first (0) color is black, last (7) color is white */
                }
                printf("\x1b[3%dm[%15s:%-5d - %3d]: %-15s - %s\x1b[39;49m\n",
                                position,
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        } else {
                printf("[%15s:%-5d - %3d]: %-15s - %s\n",
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        }
}

#ifdef NCURSES
void
resize_handler(int x)
{
        (void)x;
        resized = 1;
}
#endif
