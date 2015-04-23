#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curses.h>
#include <string.h>

#include "ncurses_messages.h"
#include "helper.h"

/*
 * see config.h
 */
extern char *ROOT_DIR;
extern int PORT;
extern int USE_NCURSES;
extern int LOGGING_WINDOW_HEIGTH;
extern int WINDOW_RESIZED;

WINDOW *win_status  = NULL;
WINDOW *win_logging = NULL;

void
_ncurses_resize_handler(int sig)
{
	WINDOW_RESIZED = 1;
	endwin();
	refresh();
	ncurses_init_windows(0, 0);
}

void
ncurses_print_info(struct data_store *data, char *m_type, const char *time,
    const char *message, int position)
{
	if (!USE_NCURSES) {
		return;
	}

	if (position < 0) {
		/* TODO: NCURSES MUTEX LOCK */
		scroll(win_logging);
		mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - 1, 0,
		    "%-19s [%15s:%-5d - %3d]: %-3s - %s",
		    time,
		    data->ip,
		    data->port,
		    data->socket,
		    m_type,
		    message);
		wrefresh(win_logging);
		/* TODO: NCURSES MUTEX UNLOCK */
	} else {
		/* TODO: NCURSES MUTEX LOCK */
		mvwprintw(win_status, position, 0,
		    "[%15s:%-5d - %3d]: %s",
		    data->ip,
		    data->port,
		    data->socket,
		    message);
		/* TODO: NCURSES MUTEX UNLOCK */
	}
}

void
ncurses_init(void)
{
	if (!USE_NCURSES) {
		return;
	}

	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = _ncurses_resize_handler;
	sigaction(SIGWINCH, &sa, NULL);

	initscr();

	start_color();
	attron(COLOR_PAIR(1));
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_WHITE, COLOR_BLACK);

	if (stdscr == NULL) {
		err_quit(ERR_INFO,  "initscr() != NULL");
	}

	ncurses_init_windows(0, 0);
	scrollok(win_logging, true);
}

void
ncurses_update_begin(int last_position)
{
	if (!USE_NCURSES) {
		return;
	}

	/* TODO: do this one right */
	if (last_position > 0) { /* only refresh if something changed */
		/* TODO: NCURSES MUTEX LOCK */
		werase(win_status);
		/* TODO: NCURSES MUTEX UNLOCK */
	}
}

void
ncurses_update_end(int last_position)
{
	if (!USE_NCURSES) {
		return;
	}

	/* TODO: NCURSES MUTEX LOCK */
	wrefresh(win_status);
	/* TODO: NCURSES MUTEX UNLOCK */
}

void
ncurses_terminate()
{
	if (!USE_NCURSES) {
		return;
	}

	/* TODO: fix cleanup */
	/* TODO: NCURSES MUTEX LOCK */
	delwin(stdscr);
	endwin();
	/* TODO: NCURSES MUTEX UNLOCK */
}

void
ncurses_init_windows(int heigth, int width)
{
	if (!USE_NCURSES) {
		return;
	}

	int i;
	int status_heigth;
	int cur_stat_heigth;
	int cur_stat_width;
	char *head_info;
	char *head_data;
	char *head_body_status;
	char *head_body_log;

	if (heigth == 0 && width == 0) {
		getmaxyx(stdscr, heigth, width);
	}

	status_heigth = heigth - LOGGING_WINDOW_HEIGTH - 3; /* headers */

	head_info = "File Server_(version 0.1)";
	head_body_status = "Current file transfers:";
	head_body_log = "Log messages:";
	/* port(5) + braces(4) + space(1) */
	head_data = (char *)malloc(strlen(ROOT_DIR) + 10);
	sprintf(head_data, "(%s)_(%d)", ROOT_DIR, PORT);

	if (status_heigth < 1) {
		status_heigth = 1;
	}

	/* TODO: NCURSES MUTEX LOCK */
	if (win_status == NULL) {
		win_status = newwin(status_heigth, width, 2, 0);
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
		wresize(win_status,  status_heigth,	    width);
		wresize(win_logging, LOGGING_WINDOW_HEIGTH, width);
		mvwin(win_logging, heigth - LOGGING_WINDOW_HEIGTH, 0);
	}

	werase(win_status);

	for (i = 0; i < width; i++) {
		mvprintw(0, i, "_");
		mvprintw(1, i, "_");
		mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 1, i, "_");
	}
	if ((size_t)width > strlen(head_info)) {
		/* mvprintw(0, 0, head_info); */
		mvaddnstr(0, 0, head_info, width);
	}
	if ((size_t)width > strlen(head_info) + strlen(head_data) + 1) {
		/* mvprintw(0, (int)width  - (int)strlen(head_data), head_data); */
		mvaddnstr(0, (int)width  - (int)strlen(head_data), head_data, width);
	}
	if ((size_t)width > strlen(head_body_status)) {
		/* mvprintw(1, 0, head_body_status); */
		mvaddnstr(1, 0, head_body_status, width);
	}
	if ((size_t)width > strlen(head_body_log)) {
		/* mvprintw(heigth - LOGGING_WINDOW_HEIGTH - 1, 0, head_body_log); */
		mvaddnstr(heigth - LOGGING_WINDOW_HEIGTH - 1, 0, head_body_log, width);
	}

	refresh();
	wrefresh(win_status);
	wrefresh(win_logging);
	/* TODO: NCURSES MUTEX UNLOCK */
}
