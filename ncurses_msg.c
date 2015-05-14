#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>

#include "ncurses_msg.h"
#include "helper.h"

/*
 * see config.h
 */
extern char *ROOT_DIR;
extern int PORT;
extern int LOGGING_WINDOW_HEIGTH;
extern size_t UPDATE_TIMEOUT;

int USE_NCURSES;
int WINDOW_RESIZED;

WINDOW *win_status = NULL;
WINDOW *win_logging = NULL;

static pthread_mutex_t ncurses_mutex;

static uint64_t written_all_time = 0;

static int terminal_heigth;
static int terminal_width;

/*
 * initialises ncurses main window
 */
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

	pthread_mutex_init(&ncurses_mutex, NULL);

	initscr();

	start_color();
	attron(COLOR_PAIR(1));
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_WHITE, COLOR_BLACK);

	if (stdscr == NULL) {
		err_quit(ERR_INFO, "initscr() == NULL");
	}

	ncurses_organize_windows();
	if (win_logging) {
		scrollok(win_logging, true);
	}
}

/*
 * prints given info inside ncurses window
 * if position is < 1 its a log message
 */
void
ncurses_print_info(struct data_store *data, char *m_type, const char *time,
    const char *message, int position)
{
	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	if (position < 0) {
		if (win_logging) {
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
		}
	} else {
		if (win_status) {
			mvwprintw(win_status, position, 0,
			    "[%15s:%-5d - %3d]: %s",
			    data->ip,
			    data->port,
			    data->socket,
			    message);
		}
	}
	pthread_mutex_unlock(&ncurses_mutex);
}

/*
 * start of printing status messages
 */
void
ncurses_update_begin(int last_position)
{
	if (!USE_NCURSES) {
		return;
	}

	/* if there where no messages last time, dont erase screen */
	if (last_position > 0) {
		pthread_mutex_lock(&ncurses_mutex);
		if (win_status) {
			werase(win_status);
		}
		pthread_mutex_unlock(&ncurses_mutex);
	}
}

/*
 * end of printing status messages
 */
void
ncurses_update_end(uint64_t written)
{
	if (!USE_NCURSES) {
		return;
	}

	size_t msg_buffer_size = 256;
	char msg_buffer[msg_buffer_size];
	char fmt_written_all_time[7];
	char fmt_bytes_per_tval[7];

	written_all_time += written;

	format_size(written_all_time, fmt_written_all_time);
	format_size(written / UPDATE_TIMEOUT, fmt_bytes_per_tval);

	snprintf(msg_buffer, msg_buffer_size, "(%6s)_(%6s/%us)",
	    fmt_written_all_time,
	    fmt_bytes_per_tval,
	    (unsigned int)UPDATE_TIMEOUT);

	pthread_mutex_lock(&ncurses_mutex);
	if ((size_t)terminal_width > strlen(msg_buffer) && win_status) {
		mvwprintw(stdscr, 1, terminal_width - (int)strlen(msg_buffer), "%s",
		    msg_buffer);
		refresh();
		wrefresh(win_status);
	}
	pthread_mutex_unlock(&ncurses_mutex);
}

/*
 * terminates ncurses window
 */
void
ncurses_terminate()
{
	if (!USE_NCURSES) {
		return;
	}

	/* TODO: fix cleanup */
	pthread_mutex_lock(&ncurses_mutex);
	delwin(stdscr);
	endwin();
	pthread_mutex_unlock(&ncurses_mutex);
	pthread_mutex_destroy(&ncurses_mutex);
}

/*
 * initializes ncurses windows, called on startup and resize
 */
void
ncurses_organize_windows()
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

	/* reinitialize ncurses after resize */
	endwin();
	refresh();
	getmaxyx(stdscr, terminal_heigth, terminal_width);

	status_heigth = terminal_heigth - LOGGING_WINDOW_HEIGTH - 3; /* headers */

	head_info = "File Server_(version 0.1)";
	head_body_status = "Current file transfers:";
	head_body_log = "Log messages:";
	/* port(5) + braces(4) + space(1) */
	head_data = (char *)err_malloc(strlen(ROOT_DIR) + 10);
	snprintf(head_data, strlen(ROOT_DIR) + 10, "(%s)_(%d)", ROOT_DIR, PORT);

	/*
	 * if terminal is to small to display information delete both windows set
	 * them to NULL and return.
	 */
	if (status_heigth < 1) {
		pthread_mutex_lock(&ncurses_mutex);
		if (win_status != NULL) {
			delwin(win_status);
			win_status = NULL;
		}
		if (win_logging != NULL) {
			delwin(win_logging);
			win_logging = NULL;
		}
		pthread_mutex_unlock(&ncurses_mutex);
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	/* check if windows are initialized, if not initialize them */
	if (win_status == NULL) {
		win_status = newwin(status_heigth, terminal_width, 2, 0);
		if (win_status == NULL) {
			err_quit(ERR_INFO, "win_status is NULL");
		}
	}
	if (win_logging == NULL) {
		win_logging = newwin(LOGGING_WINDOW_HEIGTH, terminal_width,
				  terminal_heigth - LOGGING_WINDOW_HEIGTH, 0);
		if (win_logging == NULL) {
			err_quit(ERR_INFO, "win_logging is NULL");
		}
	}

	/* check if terminal was just resized, than move windows */
	getmaxyx(win_status, cur_stat_heigth, cur_stat_width);
	if ((cur_stat_width != terminal_width) || (cur_stat_heigth != status_heigth)) {
		wresize(win_status, status_heigth, terminal_width);
		wclear(win_status);
		wresize(win_logging, LOGGING_WINDOW_HEIGTH, terminal_width);
		mvwin(win_logging, terminal_heigth - LOGGING_WINDOW_HEIGTH, 0);
	}

	/* print new info strings */
	for (i = 0; i < terminal_width; i++) {
		mvprintw(0, i, "_");
		mvprintw(1, i, "_");
		mvprintw(terminal_heigth - LOGGING_WINDOW_HEIGTH - 1, i, "_");
	}
	if ((size_t)terminal_width > strlen(head_info)) {
		mvaddnstr(0, 0, head_info, terminal_width);
	}
	if ((size_t)terminal_width > strlen(head_info) + strlen(head_data) + 1) {
		mvaddnstr(0, (int)terminal_width  - (int)strlen(head_data),
		    head_data, terminal_width);
	}
	if ((size_t)terminal_width > strlen(head_body_status)) {
		mvaddnstr(1, 0, head_body_status, terminal_width);
	}
	if ((size_t)terminal_width > strlen(head_body_log)) {
		mvaddnstr(terminal_heigth - LOGGING_WINDOW_HEIGTH - 1, 0,
		    head_body_log, terminal_width);
	}

	refresh();
	wrefresh(win_status);
	wrefresh(win_logging);
	pthread_mutex_unlock(&ncurses_mutex);
}

/*
 * handles resize signal
 */
void
_ncurses_resize_handler(int sig)
{
	WINDOW_RESIZED = 1;
}
