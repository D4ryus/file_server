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
extern int LOGGING_WINDOW_HEIGTH;
#endif

#ifdef NCURSES
#include <signal.h>
#include <curses.h>
#endif

static struct status_list_node *first = NULL;
static pthread_mutex_t status_list_mutex;
#ifdef NCURSES
WINDOW *win_status  = NULL;
WINDOW *win_logging = NULL;
#endif

void
init_messages(pthread_t *thread, const pthread_attr_t *attr)
{
        int error;

        error = 0;

#ifdef NCURSES
        if (USE_NCURSES) {
                initscr();
                if (stdscr == NULL) {
                        err_quit(__FILE__, __LINE__, __func__, "initscr() != NULL");
                }

                int width, heigth;
                getmaxyx(stdscr, heigth, width);
                init_ncurses_windows(heigth, width);
                scrollok(win_logging, true);
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
        int win_heigth, win_width;
        int last_win_heigth, last_win_width;

        getmaxyx(stdscr, last_win_heigth, last_win_width);
#endif
        written = 0;

        while (1) {
#ifdef NCURSES
                getmaxyx(stdscr, win_heigth, win_width);
                if (win_heigth != last_win_heigth || win_width != last_win_width) {
                        init_ncurses_windows(win_heigth, win_width);
                        last_win_heigth = win_heigth;
                        last_win_width = win_width;
                }
                wclear(win_status);
#endif
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
                wrefresh(win_status);
#endif
                sleep((uint)UPDATE_TIMEOUT);
        }
#ifdef NCURSES
        delwin(stdscr);
        endwin();
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
                if (type != TRANSFER_STATUS) {
                        scroll(win_logging);
                        mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - 1, 0,
                                "[%15s:%-5d - %3d]: %-15s - %s",
                                        data->ip,
                                        data->port,
                                        data->socket,
                                        m_type,
                                        message);
                        wrefresh(win_logging);
                } else  {
                        mvwprintw(win_status, position, 0,
                                "[%15s:%-5d - %3d]: %-15s - %s\n",
                                        data->ip,
                                        data->port,
                                        data->socket,
                                        m_type,
                                        message);
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
init_ncurses_windows(int heigth, int width)
{
        int status_heigth, i;
        int cur_stat_heigth, cur_stat_width;

        status_heigth = heigth - LOGGING_WINDOW_HEIGTH - 5; /* headers */

        /* if there is not space for status win, quit */
        if (status_heigth < 1) {
                err_quit(__FILE__, __LINE__, __func__, "terminal height way to little (< 10 rows)");
        }

        if (win_status == NULL) {
                win_status = newwin(status_heigth, width, 3, 0);
                if (win_status == NULL) {
                        err_quit(__FILE__, __LINE__, __func__, "win_status is NULL");
                }
        }
        if (win_logging == NULL) {
                win_logging = newwin(LOGGING_WINDOW_HEIGTH, width, heigth - LOGGING_WINDOW_HEIGTH, 0);
                if (win_logging == NULL) {
                        err_quit(__FILE__, __LINE__, __func__, "win_logging is NULL");
                }
        }

        getmaxyx(win_status, cur_stat_heigth, cur_stat_width);
        if ((cur_stat_width != width) || (cur_stat_heigth != status_heigth)) {
                wresize(win_status,  status_heigth,         width);
                wresize(win_logging, LOGGING_WINDOW_HEIGTH, width);
                mvwin(win_logging, heigth - LOGGING_WINDOW_HEIGTH, 0);
        }

        start_color();

        init_pair(1, COLOR_BLACK, COLOR_WHITE);
        init_pair(2, COLOR_WHITE, COLOR_BLACK);

        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "File Server version 0.1");
        attron(COLOR_PAIR(2));
        mvprintw(1, 0, "Current file transfers:");
        mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 2, 0, "Log messages:");

        for (i = 0; i < width; i++) {
                mvprintw(2, i, "-");
                mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 1, i, "-");
        }

        refresh();
        wrefresh(win_status);
        wrefresh(win_logging);
}

#endif

