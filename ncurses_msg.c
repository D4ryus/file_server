#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>

#include "ncurses_msg.h"
#include "helper.h"
#include "defines.h"

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern char *UPLOAD_DIR;
extern int UPLOAD_ENABLED;
extern int PORT;
extern int LOGGING_WINDOW_HEIGTH;
extern size_t UPDATE_TIMEOUT;

int USE_NCURSES;
int WINDOW_RESIZED;

WINDOW *win_status = NULL;
WINDOW *win_logging = NULL;

static pthread_mutex_t ncurses_mutex;

static uint64_t downloaded = 0;
static uint64_t uploaded = 0;

static int terminal_heigth;
static int terminal_width;
static int status_heigth;

static char **log_buf;
int log_buf_size;
int log_buf_pos;

static char *head_body_status = "Current file transfers: ";
static char *head_body_log = "Log messages: ";
static char *head_info = "File Server version 0.2";
static char *head_data;
static char *status_data;

/*
 * initialises ncurses main window
 */
void
ncurses_init(pthread_t *thread, const pthread_attr_t *attr)
{
	int i;
	struct sigaction sa;
	enum err_status error;

	if (!USE_NCURSES) {
		return;
	}

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
		init_pair((short)1, (short)COLOR_WHITE, (short)COLOR_BLUE);
		wbkgd(stdscr, COLOR_PAIR(1));
	}

	log_buf_size = LOGGING_WINDOW_HEIGTH - 2;
	log_buf_pos = 0;
	log_buf = err_malloc(sizeof(char *) * (size_t)log_buf_size);
	for (i = 0; i < log_buf_size; i++) {
		log_buf[i] = err_malloc(sizeof(char));
		log_buf[i][0] = '\0';
	}

	head_data = (char *)err_malloc(strlen(ROOT_DIR) + 11);
	snprintf(head_data, strlen(ROOT_DIR) + 11,
	    "(%s)-(%d)", ROOT_DIR, PORT);

	status_data = (char *)err_malloc((size_t)MSG_BUFFER_SIZE);
	strncpy(status_data, "(0)-(   0b |   0b )-(   0b/1s|   0b/1s)",
	    (size_t)39);
	status_data[39] = '\0';

	ncurses_organize_windows();
	if (win_logging) {
		scrollok(win_logging, (bool)TRUE);
	} else {
		err_quit(ERR_INFO, "window to small");
	}

	/* if upload is enabled, handle keys from keyboard */
	if (UPLOAD_ENABLED) {
		error = pthread_create(thread, attr, &handle_keyboard, NULL);
		if (error != 0) {
			err_quit(ERR_INFO, "pthread_create() != 0");
		}
	}
}

void *
handle_keyboard(void *ptr)
{
	char ch;
	
	noecho();
	nodelay(stdscr, (bool)FALSE);
	UPLOAD_ENABLED = 0;

	pthread_mutex_lock(&ncurses_mutex);
	if (((size_t)terminal_width > strlen(head_data) + 6)) {
		mvwprintw(stdscr, 0,
		    (int)terminal_width - ((int)strlen(head_data) + 6),
		    "%s)-",
		    UPLOAD_ENABLED ? " (on" : "(off");
		refresh();
	}
	pthread_mutex_unlock(&ncurses_mutex);

	while (1) {
		ch = 0;
		ch = (char)getch();
		if (ch != 'u') {
			continue;
		}
		UPLOAD_ENABLED = UPLOAD_ENABLED ? 0 : 1;
		pthread_mutex_lock(&ncurses_mutex);
		if (((size_t)terminal_width > strlen(head_data) + 2)) {
		mvwprintw(stdscr, 0,
		    (int)terminal_width - ((int)strlen(head_data) + 6),
		    "%s)-",
		    UPLOAD_ENABLED ? " (on" : "(off");
			refresh();
		}
		pthread_mutex_unlock(&ncurses_mutex);
	}

	return NULL;
}

/*
 * prints given info inside ncurses window
 * if position is < 1 its a log message
 */
void
ncurses_print_log(char *msg)
{
	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	if (win_logging) {
		scroll(win_logging);
		wmove(win_logging, LOGGING_WINDOW_HEIGTH - 2, 0);
		wclrtoeol(win_logging);
		mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - 2, 1,
		    "%s", msg);
		_ncurses_draw_logging_box();
		wrefresh(win_logging);
	}
	pthread_mutex_unlock(&ncurses_mutex);

	_ncurses_push_log_buf(msg);
}

/*
 * prints given info inside ncurses window
 * if position is < 1 its a log message
 */
void
ncurses_print_status(const char *message, int position)
{
	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	if (win_status && position + 1 < status_heigth - 1) {
		mvwprintw(win_status, position + 1, 1, "%s", message);
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
ncurses_update_end(uint64_t up, uint64_t down, int clients)
{
	char fmt_uploaded[7];
	char fmt_downloaded[7];
	char fmt_bytes_per_tval_up[7];
	char fmt_bytes_per_tval_down[7];

	if (!USE_NCURSES) {
		return;
	}

	uploaded += up;
	downloaded += down;

	format_size(uploaded, fmt_uploaded);
	format_size(downloaded, fmt_downloaded);
	format_size(up / UPDATE_TIMEOUT, fmt_bytes_per_tval_up);
	format_size(down / UPDATE_TIMEOUT, fmt_bytes_per_tval_down);

	snprintf(status_data, (size_t)MSG_BUFFER_SIZE,
	    "(%d)-(%6s|%6s)-(%6s/%us|%6s/%us)",
	    clients,
	    fmt_uploaded,
	    fmt_downloaded,
	    fmt_bytes_per_tval_up,
	    (unsigned int)UPDATE_TIMEOUT,
	    fmt_bytes_per_tval_down,
	    (unsigned int)UPDATE_TIMEOUT);

	pthread_mutex_lock(&ncurses_mutex);
	if ((size_t)terminal_width > strlen(status_data) + 2 && win_status) {
		_ncurses_draw_status_box();
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
	int i;

	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	/* reinitialize ncurses after resize */
	endwin();
	refresh();
	getmaxyx(stdscr, terminal_heigth, terminal_width);

	/* header */
	status_heigth = terminal_heigth - LOGGING_WINDOW_HEIGTH - 1;

	/*
	 * if terminal is to small to display information delete both windows
	 * set them to NULL and return.
	 */
	if (status_heigth < 1) {
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

	/* check if windows are initialized, if not initialize them */
	if (win_status == NULL) {
		win_status = newwin(status_heigth, terminal_width, 1, 0);
		if (win_status == NULL) {
			err_quit(ERR_INFO, "win_status is NULL");
		}
	} else {
		wresize(win_status, status_heigth, terminal_width);
	}
	if (win_logging == NULL) {
		win_logging = newwin(LOGGING_WINDOW_HEIGTH, terminal_width,
				  terminal_heigth - LOGGING_WINDOW_HEIGTH, 0);
		if (win_logging == NULL) {
			err_quit(ERR_INFO, "win_logging is NULL");
		}
	} else {
		wresize(win_logging, LOGGING_WINDOW_HEIGTH, terminal_width);
	}

	wclear(win_status);
	wclear(win_logging);
	mvwin(win_logging, terminal_heigth - LOGGING_WINDOW_HEIGTH, 0);
	for (i = 0; i < log_buf_size; i++) {
		mvwprintw(win_logging, LOGGING_WINDOW_HEIGTH - (2 + i), 1,
		    "%s", log_buf[(log_buf_pos - i + log_buf_size)
		    % log_buf_size]);
	}

	/* rootpath, port and upload_enabled */
	move(0, 0);
	clrtoeol();
	if ((size_t)terminal_width > strlen(head_data) + 1) {
		mvwprintw(stdscr, 0,
		    (int)terminal_width - (int)strlen(head_data),
		    "%s", head_data);
	}

	/* name and version */
	if ((size_t)terminal_width > strlen(head_info)
	    + strlen(head_data) + 1) {
		mvwprintw(stdscr, 0, 0, "%s", head_info);
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
	if ((size_t)terminal_width > strlen(status_data) + 4) {
		mvwprintw(win_status, 0,
		    (int)terminal_width - ((int)strlen(status_data) + 2),
		    "%s", status_data);
	}

	if ((size_t)terminal_width > strlen(status_data)
	    + strlen(head_body_status) + 4) {
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
