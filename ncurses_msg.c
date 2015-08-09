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
extern uint16_t PORT;
extern const size_t UPDATE_TIMEOUT;

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
static int log_heigth;

static char **log_buf;
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
		die(ERR_INFO, "initscr()");
	}

	if (has_colors()) {
		start_color();
		init_pair((short)1, (short)COLOR_WHITE, (short)COLOR_BLUE);
		wbkgd(stdscr, COLOR_PAIR(1));
	}

	log_heigth = 10;
	log_buf_pos = 0;
	log_buf = err_malloc(sizeof(char *) * (size_t)NCURSES_LOG_LINES);
	for (i = 0; i < NCURSES_LOG_LINES; i++) {
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

	terminal_heigth = 0;
	terminal_width = 0;
	/* ncurses_organize_windows will rearrange windows */
	WINDOW_RESIZED = 1;
	ncurses_organize_windows();
	if (win_logging) {
		scrollok(win_logging, (bool)TRUE);
	} else {
		endwin();
		delwin(stdscr);
		die(ERR_INFO, "window to small");
	}

	/* if upload is enabled, handle keys from keyboard */
	error = pthread_create(thread, attr, &ncurses_handle_keyboard, NULL);
	if (error != 0) {
		die(ERR_INFO, "pthread_create()");
	}
}

void *
ncurses_handle_keyboard(void *ptr)
{
	char ch;
	int upload_allowed;
	int resize_lines;

	noecho();
	nodelay(stdscr, (bool)FALSE);

	if (UPLOAD_ENABLED) {
		upload_allowed = 1;
	} else {
		upload_allowed = 0;
	}

	resize_lines = 0;
	while (1) {
		ch = 0;
		ch = (char)getch();
		switch (ch) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (resize_lines == 0) {
				resize_lines = ch - 48;
			} else {
				resize_lines = (resize_lines * 10) + (ch - 48);
			}
			break;
		case 'u':
			if (!upload_allowed) {
				break;
			}
			UPLOAD_ENABLED = UPLOAD_ENABLED ? 0 : 1;
			pthread_mutex_lock(&ncurses_mutex);
			if ((size_t)terminal_width > strlen(head_data) + 5) {
				mvwprintw(stdscr, 0, (int)terminal_width
				    - ((int)strlen(head_data) + 5),
				    UPLOAD_ENABLED ? "(on)-" : "     ");
				refresh();
			}
			pthread_mutex_unlock(&ncurses_mutex);
			break;
		case 'k':
			resize_lines = resize_lines ? resize_lines : 1;
			if ((status_heigth - resize_lines) >= 2) {
				log_heigth += resize_lines;
				ncurses_organize_windows();
			} else {
				log_heigth += (status_heigth - 2);
			}
			ncurses_organize_windows();
			resize_lines = 0;
			break;
		case 'j':
			resize_lines = resize_lines ? resize_lines : 1;
			if ((log_heigth - resize_lines) >= 2) {
				log_heigth -= resize_lines;
			} else {
				log_heigth = 2;
			}
			ncurses_organize_windows();
			resize_lines = 0;
			break;
		case 'r':
			ncurses_organize_windows();
			break;
		case 'q':
			ncurses_terminate();
			exit(0);
			break;
		default:
			break;
		}
	}

	return NULL;
}

/*
 * appends given msg to log window
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
		wmove(win_logging, log_heigth - 2, 0);
		wclrtoeol(win_logging);
		mvwprintw(win_logging, log_heigth - 2, 1, "%s", msg);
		_ncurses_draw_logging_box();
		wrefresh(win_logging);
	}
	pthread_mutex_unlock(&ncurses_mutex);

	_ncurses_push_log_buf(msg);
}

/*
 * writes given msg to status window at given pos
 */
void
ncurses_print_status(const char *msg, int pos)
{
	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_lock(&ncurses_mutex);
	if (win_status && pos + 1 < status_heigth - 1) {
		mvwprintw(win_status, pos + 1, 1, "%s", msg);
	}
	pthread_mutex_unlock(&ncurses_mutex);
}

/*
 * start of printing status messages
 */
void
ncurses_update_begin(int last_pos)
{
	if (!USE_NCURSES) {
		return;
	}

	/* if there where no messages last time, dont erase screen */
	if (last_pos > 0) {
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
	int width_changed;

	if (!USE_NCURSES) {
		return;
	}

	width_changed = 0;
	pthread_mutex_lock(&ncurses_mutex);
	/* reinitialize ncurses after resize */
	if (WINDOW_RESIZED) {
		int old_terminal_width;

		WINDOW_RESIZED = 0;
		old_terminal_width = terminal_width;
		endwin();
		refresh();
		getmaxyx(stdscr, terminal_heigth, terminal_width);
		if (old_terminal_width != terminal_width) {
			width_changed = 1;
		}
	}

	/* header */
	status_heigth = terminal_heigth - log_heigth - 1;

	/*
	 * if terminal is to small to display information delete both windows,
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
			die(ERR_INFO, "newwin()");
		}
	} else {
		wresize(win_status, status_heigth, terminal_width);
	}
	if (win_logging == NULL) {
		win_logging = newwin(log_heigth, terminal_width,
				  terminal_heigth - log_heigth, 0);
		if (win_logging == NULL) {
			die(ERR_INFO, "newwin()");
		}
	} else {
		wresize(win_logging, log_heigth, terminal_width);
	}

	wclear(win_status);
	wclear(win_logging);
	mvwin(win_logging, terminal_heigth - log_heigth, 0);
	for (i = 0; i < log_heigth - 2; i++) {
		mvwprintw(win_logging, log_heigth - (2 + i), 1,
		    "%s", log_buf[(log_buf_pos - i + NCURSES_LOG_LINES)
		    % NCURSES_LOG_LINES]);
	}

	if (width_changed) {
		/* rootpath, port and upload_enabled */
		move(0, 0);
		clrtoeol();
		if ((size_t)terminal_width > strlen(head_data) + 1) {
			mvwprintw(stdscr, 0,
			    (int)terminal_width - (int)strlen(head_data),
			    "%s", head_data);
		}
		if (UPLOAD_ENABLED
		    && (size_t)terminal_width > strlen(head_data) + 5) {
			mvwprintw(stdscr, 0,
			    (int)terminal_width - ((int)strlen(head_data) + 5),
			    "(on)-");
			refresh();
		}

		/* name and version */
		if ((size_t)terminal_width > strlen(head_info)
		    + strlen(head_data) + 6) {
			mvwprintw(stdscr, 0, 0, "%s", head_info);
		}
	}

	_ncurses_draw_logging_box();
	_ncurses_draw_status_box();

	if (width_changed) {
		refresh();
	}
	wrefresh(win_status);
	wrefresh(win_logging);
	pthread_mutex_unlock(&ncurses_mutex);
}

void
_ncurses_push_log_buf(char *new_msg)
{
	pthread_mutex_lock(&ncurses_mutex);

	log_buf_pos++;
	if (log_buf_pos >= NCURSES_LOG_LINES) {
		log_buf_pos -= NCURSES_LOG_LINES;
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
	box(win_logging, ACS_VLINE, ACS_HLINE);
	if ((size_t)terminal_width > strlen(head_body_log) + 2) {
		mvwprintw(win_logging, 0, 2, "%s", head_body_log);
	}
}

void
_ncurses_draw_status_box()
{
	box(win_status, ACS_VLINE, ACS_HLINE);
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
