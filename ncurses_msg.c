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
static int status_heigth;

static char **log_buf;
int log_buf_size;
int log_buf_pos;

static char *head_body_status = "Current file transfers: ";
static char *head_body_log = "Log messages: ";
static char *head_info = "File Server version 0.1";
static char *head_data;

/*
 * initialises ncurses main window
 */
void
ncurses_init(void)
{
	if (!USE_NCURSES) {
		return;
	}

	int i;
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = _ncurses_resize_handler;
	sigaction(SIGWINCH, &sa, NULL);

	pthread_mutex_init(&ncurses_mutex, NULL);

	initscr();

	if (stdscr == NULL) {
		err_quit(ERR_INFO, "initscr() == NULL");
	}

	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_WHITE, COLOR_BLUE);
		wbkgd(stdscr, COLOR_PAIR(1));
	}

	log_buf_size = LOGGING_WINDOW_HEIGTH - 2;
	log_buf_pos = 0;
	log_buf = err_malloc(sizeof(char *) * (size_t)log_buf_size);
	for (i = 0; i < log_buf_size; i++) {
		log_buf[i] = err_malloc(sizeof(char));
		log_buf[i][0] = '\0';
	}

	head_data = (char *)err_malloc(strlen(ROOT_DIR) + 23);
	/* port(5) + braces(4) + space(1) */
	snprintf(head_data, strlen(ROOT_DIR) + 23,
	    "Shared: %s on Port: %d", ROOT_DIR, PORT);

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
	size_t msg_buffer_size = 512;
	char msg_buffer[msg_buffer_size];
	memset(msg_buffer, '\0', msg_buffer_size);

	pthread_mutex_lock(&ncurses_mutex);
	if (position < 0) {
		if (win_logging) {
			scroll(win_logging);
			wmove(win_logging, LOGGING_WINDOW_HEIGTH - 2, 0);
			wclrtoeol(win_logging);
			snprintf(msg_buffer, msg_buffer_size,
			    "%-19s [%15s:%-5d - %3d]: %-3s - %s",
			    time,
			    data->ip,
			    data->port,
			    data->socket,
			    m_type,
			    message);
			mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - 2, 1,
			    "%s", msg_buffer);
			_ncurses_draw_logging_box();
			wrefresh(win_logging);
		}
	} else {
		/* TODO: Implement Scrolling for status window */
		if (win_status && position + 1 < status_heigth - 1) {
			mvwprintw(win_status, position + 1, 1,
			    "[%15s:%-5d - %3d]: %s",
			    data->ip,
			    data->port,
			    data->socket,
			    message);
		}
	}
	pthread_mutex_unlock(&ncurses_mutex);


	if (strlen(msg_buffer) > 0) {
		_ncurses_push_log_buf(msg_buffer);
	}
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
ncurses_update_end(uint64_t written, int clients)
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

	snprintf(msg_buffer, msg_buffer_size, "(%d)-(%6s)-(%6s/%us)",
	    clients,
	    fmt_written_all_time,
	    fmt_bytes_per_tval,
	    (unsigned int)UPDATE_TIMEOUT);

	pthread_mutex_lock(&ncurses_mutex);
	if ((size_t)terminal_width > strlen(msg_buffer) && win_status) {
		_ncurses_draw_status_box();
		mvwprintw(win_status,
		    0, terminal_width - (int)strlen(msg_buffer) - 2,
		    "%s", msg_buffer);
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
	delwin(win_status);
	delwin(win_logging);
	endwin();
	delwin(stdscr);
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
	int cur_stat_heigth;
	int cur_stat_width;

	/* reinitialize ncurses after resize */
	endwin();
	refresh();
	getmaxyx(stdscr, terminal_heigth, terminal_width);

	/* header */
	status_heigth = terminal_heigth - LOGGING_WINDOW_HEIGTH - 1;

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
		win_status = newwin(status_heigth, terminal_width, 1, 0);
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

	/* check if terminal was just resized, if so reprint log window */
	getmaxyx(win_status, cur_stat_heigth, cur_stat_width);
	if ((cur_stat_width != terminal_width) || (cur_stat_heigth != status_heigth)) {
		wresize(win_status, status_heigth, terminal_width);
		wclear(win_status);
		wresize(win_logging, LOGGING_WINDOW_HEIGTH, terminal_width);
		wclear(win_logging);
		mvwin(win_logging, terminal_heigth - LOGGING_WINDOW_HEIGTH, 0);
		for (i = 0; i < log_buf_size; i++) {
			mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - (2 + i), 1,
			    "%s", log_buf[(log_buf_pos - i + log_buf_size)
			    % log_buf_size]);
		}
	}

	/* name and version */
	move(0, 0);
	clrtoeol();
	if ((size_t)terminal_width > strlen(head_info) + 2) {
		mvwprintw(stdscr, 0, 0, "%s", head_info);
	}
	/* rootpath and port */
	if ((size_t)terminal_width > strlen(head_info) + strlen(head_data) + 3) {
		mvwprintw(stdscr, 0,
		    (int)terminal_width - (int)strlen(head_data),
		    "%s", head_data);
	}

	_ncurses_draw_logging_box();
	_ncurses_draw_status_box();

	refresh();
	wrefresh(win_status);
	wrefresh(win_logging);
	pthread_mutex_unlock(&ncurses_mutex);
}

void
_ncurses_push_log_buf(char *new_msg)
{
	pthread_mutex_lock(&ncurses_mutex);

	log_buf_pos++;
	if (log_buf_pos >= log_buf_size) {
		log_buf_pos -= log_buf_size;
	}

	if (log_buf[log_buf_pos] != NULL) {
		free(log_buf[log_buf_pos]);
	}

	log_buf[log_buf_pos] = err_malloc(strlen(new_msg) + 1);
	memset(log_buf[log_buf_pos], '\0', strlen(new_msg) + 1);
	strncpy(log_buf[log_buf_pos], new_msg, strlen(new_msg));

	pthread_mutex_unlock(&ncurses_mutex);

	return;
}

void
_ncurses_draw_logging_box()
{
	box(win_logging, 0, 0);
	if ((size_t)terminal_width > strlen(head_body_log) + 2) {
		mvwprintw(win_logging, 0, 2, "%s", head_body_log);
	}
}

void
_ncurses_draw_status_box()
{
	box(win_status, 0, 0);
	if ((size_t)terminal_width > strlen(head_body_status) + 2) {
		mvwprintw(win_status, 0, 2, "%s", head_body_status);
	}
}

/*
 * handles resize signal
 */
void
_ncurses_resize_handler(int sig)
{
	WINDOW_RESIZED = 1;
}
