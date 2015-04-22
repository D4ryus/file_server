#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curses.h>

#include "ncurses_messages.h"
#include "helper.h"

/*
 * see config.h
 */
extern char *ROOT_DIR;
extern int PORT;
extern int USE_NCURSES;
extern int LOGGING_WINDOW_HEIGTH;

WINDOW *win_status  = NULL;
WINDOW *win_logging = NULL;
int last_win_heigth = 0;
int last_win_width  = 0;

void
ncurses_print_info(struct data_store *data, char *m_type, const char *message,
                                                                   int position)
{
        if (!USE_NCURSES) {
                return;
        }

        if (position < 0) {
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
                        "[%15s:%-5d - %3d]: %-15s - %s",
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        }
}

void
ncurses_init(void)
{
        if (!USE_NCURSES) {
                return;
        }

        int width, heigth;

        initscr();
        if (stdscr == NULL) {
                err_quit(ERR_INFO,  "initscr() != NULL"); }

        getmaxyx(stdscr, heigth, width);
        ncurses_init_windows(heigth, width);
        scrollok(win_logging, true);
}

void
ncurses_update_begin(int last_position)
{
        if (!USE_NCURSES) {
                return;
        }

        int win_heigth, win_width;

        getmaxyx(stdscr, win_heigth, win_width);
        if (win_heigth != last_win_heigth || win_width != last_win_width) {
                ncurses_init_windows(win_heigth, win_width);
                last_win_heigth = win_heigth;
                last_win_width = win_width;
        /* TODO: do this one right */
        } else if (last_position > 0) { /* only refresh if something changed */
                wclear(win_status);
        }
}

void
ncurses_update_end(int last_position)
{
        if (!USE_NCURSES) {
                return;
        }

        wrefresh(win_status);
}

void
ncurses_terminate()
{
        if (!USE_NCURSES) {
                return;
        }

        delwin(stdscr);
        endwin();
}

void
ncurses_init_windows(const int heigth, const int width)
{
        if (!USE_NCURSES) {
                return;
        }

        int status_heigth, i;
        int cur_stat_heigth, cur_stat_width;

        status_heigth = heigth - LOGGING_WINDOW_HEIGTH - 5; /* headers */

        /* if there is not space for status win, quit */
        if (status_heigth < 1) {
                status_heigth = 1;
        }

        if (win_status == NULL) {
                win_status = newwin(status_heigth, width, 3, 0);
                if (win_status == NULL) {
                        err_quit(ERR_INFO, "win_status is NULL");
                }
        }
        if (win_logging == NULL) {
                win_logging = newwin(LOGGING_WINDOW_HEIGTH, width,
                                             heigth - LOGGING_WINDOW_HEIGTH, 0);
                if (win_logging == NULL) {
                        err_quit(ERR_INFO, "win_logging is NULL");
                }
        }

        getmaxyx(win_status, cur_stat_heigth, cur_stat_width);
        if ((cur_stat_width != width) || (cur_stat_heigth != status_heigth)) {
                wresize(win_status,  status_heigth,         width);
                wresize(win_logging, LOGGING_WINDOW_HEIGTH, width);
                mvwin(win_logging, heigth - LOGGING_WINDOW_HEIGTH, 0);
        }

        wclear(win_status);
        start_color();

        init_pair(1, COLOR_BLACK, COLOR_WHITE);
        init_pair(2, COLOR_WHITE, COLOR_BLACK);

        attron(COLOR_PAIR(1));
        move(0, 0);
        clrtoeol();
        mvprintw(0, 0, "File Server version 0.1. shared directory: %s Port: %d",
                                                                ROOT_DIR, PORT);
        attron(COLOR_PAIR(2));
        move(1, 0);
        clrtoeol();
        mvprintw(1, 0, "Current file transfers:");
        move(heigth - LOGGING_WINDOW_HEIGTH - 2, 0);
        clrtoeol();
        mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 2, 0, "Log messages:");

        for (i = 0; i < width; i++) {
                mvprintw(2, i, "-");
                mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 1, i, "-");
        }

        refresh();
        wrefresh(win_status);
        wrefresh(win_logging);
}
