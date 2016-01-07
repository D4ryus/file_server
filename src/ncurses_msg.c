#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>

#include "config.h"
#include "ncurses_msg.h"
#include "helper.h"

#define HEADER_COLOR_ID 1

/* global flag, is set to 1 if ncurses is active */
int USE_NCURSES;

static WINDOW *win_status = NULL;
static WINDOW *win_logging = NULL;

/* mutex to prevent cluttering of terminal */
static pthread_mutex_t ncurses_mutex;
#define LOCK_TERM pthread_mutex_lock(&ncurses_mutex)
#define UNLOCK_TERM pthread_mutex_unlock(&ncurses_mutex)

static uint64_t downloaded = 0;
static uint64_t uploaded = 0;

/* containing terminal and window dimensions */
static int terminal_heigth;
static int terminal_width;
static int status_heigth;
static int log_heigth;

static char *head_body_status = "Current connections: ";
static char *head_body_log = "Log messages: ";
static char *head_info = "File Server version " VERSION;

/* contains header string with rootpath and port */
static char *head_data;

/* contains status header string with current connections, up/download size
 * since startup current up/download speed */
static char status_data[MSG_BUFFER_SIZE] =
		"(0)-(   0b |   0b )-(   0b/1s|   0b/1s)";

/* will hold log messages */
#define LOG_LINES 128
static struct {
	pthread_mutex_t mutex;
	int pos;
	char *data[LOG_LINES];
} log_buf;
static void ncurses_init_log(void);
static void ncurses_push_log(const char *);
static char *ncurses_get_log(int);
static void ncurses_delete_log(void);

static void *ncurses_handle_keyboard(void *);
static void ncurses_draw_header(void);
static void ncurses_draw_logging_box(void);
static void ncurses_draw_status_box(void);

/*
 * initialises ncurses
 */
void
ncurses_init(pthread_t *thread, const pthread_attr_t *attr)
{
	enum err_status error;

	if (!USE_NCURSES) {
		return;
	}

	pthread_mutex_init(&ncurses_mutex, NULL);

	initscr();

	check(!stdscr, "initscr() failed");

	if (has_colors()) {
		start_color();
		init_pair((short)HEADER_COLOR_ID,
		    (short)COLOR_WHITE, (short)COLOR_BLUE);
	}

	log_heigth = 10;

	ncurses_init_log();

	head_data = (char *)malloc(strlen(CONF.root_dir) + 11);
	check_mem(head_data);
	snprintf(head_data, strlen(CONF.root_dir) + 11,
	    "(%s)-(%d)", CONF.root_dir, CONF.port);

	terminal_heigth = 0;
	terminal_width = 0;
	/* ncurses_organize_windows will rearrange windows */
	ncurses_organize_windows(1);

	/* if upload is enabled, handle keys from keyboard */
	error = pthread_create(thread, attr, &ncurses_handle_keyboard, NULL);
	check(error != 0,
	    "pthread_create(ncurses_handle_keyboard) returned %d", error);
}

static void *
ncurses_handle_keyboard(void *ptr)
{
	char ch;
	int upload_allowed;
	int resize_lines;
	char upload_ip_buff[16];
	int ret;
	struct timespec tsleep;

	/* will round down, what we want */
	tsleep.tv_sec = (long)KEYBOARD_TIMEOUT;
	tsleep.tv_nsec = (long)((KEYBOARD_TIMEOUT - (float)tsleep.tv_sec)
			* 1000000000L);

	noecho();
	nodelay(stdscr, (bool)TRUE);

	upload_allowed = 0;
	resize_lines = 0;
	while (1) {
		ch = 0;
		LOCK_TERM;
		if (!stdscr) {
			ret = ERR;
		} else {
			ret = (char)wgetch(stdscr);
		}
		UNLOCK_TERM;
		if (ret == ERR) {
			/* sleep here */
			nanosleep(&tsleep, NULL);
			continue;
		}
		ch = (char)ret;
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
			/* offset for j/k resize, 10j -> resize by 10 lines */
			if (resize_lines == 0) {
				resize_lines = ch - 48;
			} else {
				resize_lines = (resize_lines * 10) + (ch - 48);
			}
			break;
		case 'u':
			/* enable upload for all ip's */
			upload_allowed = !upload_allowed;
			if (upload_allowed) {
				memcpy(upload_ip_buff, CONF.upload_ip,
				    (size_t)16);
				memcpy(CONF.upload_ip, "x", (size_t)2);
			} else {
				memcpy(CONF.upload_ip, upload_ip_buff,
				    (size_t)16);
			}
			LOCK_TERM;
			ncurses_draw_header();
			UNLOCK_TERM;
			break;
		case 'k':
			/* resize log window (bigger) */
			resize_lines = resize_lines ? resize_lines : 1;
			if ((status_heigth - resize_lines) >= 2) {
				log_heigth += resize_lines;
			} else {
				log_heigth += (status_heigth - 2);
			}
			ncurses_organize_windows(0);
			resize_lines = 0;
			break;
		case 'j':
			/* resize log window (smaller) */
			resize_lines = resize_lines ? resize_lines : 1;
			if ((log_heigth - resize_lines) >= 2) {
				log_heigth -= resize_lines;
			} else {
				log_heigth = 2;
			}
			ncurses_organize_windows(0);
			resize_lines = 0;
			break;
		case 'r':
			/* refresh terminal */
			wclear(win_status);
			wclear(win_logging);
			ncurses_organize_windows(1);
			break;
		case 'q':
			/* quit */
			ncurses_terminate();
			exit(0);
			break;
		default:
			break;
		}
	}
}

/*
 * appends given msg to log window
 */
void
ncurses_print_log(const char *msg)
{
	if (!USE_NCURSES) {
		return;
	}

	ncurses_push_log(msg);

	/* in case there is no window or its too small to display anything */
	if (!win_logging || log_heigth < 3) {
		return;
	}

	LOCK_TERM;
	scroll(win_logging);
	wmove(win_logging, log_heigth - 2, 0);
	wclrtoeol(win_logging);
	mvwaddnstr(win_logging, log_heigth - 2, 1, msg,
	    terminal_width - 2);
	ncurses_draw_logging_box();
	wrefresh(win_logging);
	UNLOCK_TERM;
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

	LOCK_TERM;
	if (win_status && pos + 1 < status_heigth - 1) {
		mvwaddnstr(win_status, pos + 1, 1, msg, terminal_width - 2);
	}
	UNLOCK_TERM;
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
	if (last_pos == 0) {
		return;
	}

	LOCK_TERM;
	if (win_status) {
		werase(win_status);
	}
	UNLOCK_TERM;
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
	format_size((uint64_t)((float)up / UPDATE_TIMEOUT),
	    fmt_bytes_per_tval_up);
	format_size((uint64_t)((float)down / UPDATE_TIMEOUT),
	    fmt_bytes_per_tval_down);

	snprintf(status_data, (size_t)MSG_BUFFER_SIZE,
	    "(%d)-(%6s|%6s)-(%6s/s|%6s/s)",
	    clients,
	    fmt_uploaded,
	    fmt_downloaded,
	    fmt_bytes_per_tval_up,
	    fmt_bytes_per_tval_down);

	LOCK_TERM;
	ncurses_draw_status_box();
	wrefresh(win_status);
	UNLOCK_TERM;
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
	LOCK_TERM;
	delwin(win_status);
	delwin(win_logging);
	endwin();
	delwin(stdscr);
	UNLOCK_TERM;
	pthread_mutex_destroy(&ncurses_mutex);

	ncurses_delete_log();
}

/*
 * initializes ncurses windows, called on startup and resize
 */
void
ncurses_organize_windows(int resized)
{
	int i;
	int width_changed;

	if (!USE_NCURSES) {
		return;
	}

	width_changed = 0;

	LOCK_TERM;
	/* reinitialize ncurses after resize */
	if (resized) {
		int old_terminal_width;

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
	if (status_heigth < 2) {
		if (win_status) {
			delwin(win_status);
			win_status = NULL;
		}
		if (win_logging) {
			delwin(win_logging);
			win_logging = NULL;
		}
		werase(stdscr);
		mvwaddnstr(stdscr, 0, 0,
		    "terminal heigth to small, pls resize.",
		    terminal_width);
		wrefresh(stdscr);
		UNLOCK_TERM;
		return;
	}

	/*
	 * if width changed or screen is getting initialized
	 */
	if (width_changed || (!win_status && !win_logging)) {
		ncurses_draw_header();
	}

	/* check if windows are initialized, if not initialize them */
	if (!win_status) {
		win_status = newwin(status_heigth, terminal_width, 1, 0);
		check(!win_status, "newwin(win_stats) returned NULL");
	} else {
		wresize(win_status, status_heigth, terminal_width);
		werase(win_status);
	}

	if (!win_logging) {
		win_logging = newwin(log_heigth, terminal_width,
				  terminal_heigth - log_heigth, 0);
		check(!win_logging, "newwin(win_logging) returned NULL");
		scrollok(win_logging, (bool)TRUE);
	} else {
		wresize(win_logging, log_heigth, terminal_width);
		werase(win_logging);
	}

	/* move logging window into place, then write log */
	mvwin(win_logging, terminal_heigth - log_heigth, 0);
	/* bottom to top */
	pthread_mutex_lock(&log_buf.mutex);
	for (i = 0; i < log_heigth - 2; i++) {
		mvwaddnstr(win_logging, log_heigth - (i + 2), 1,
		    ncurses_get_log(-i),
		    terminal_width - 2);
	}
	pthread_mutex_unlock(&log_buf.mutex);

	/* draw boxes and header info */
	ncurses_draw_status_box();
	ncurses_draw_logging_box();

	wrefresh(win_status);
	wrefresh(win_logging);

	UNLOCK_TERM;
}

static void
ncurses_init_log()
{
	int i;

	pthread_mutex_init(&log_buf.mutex, NULL);
	log_buf.pos = 0;
	for (i = 0; i < LOG_LINES; i++) {
		log_buf.data[i] = malloc(sizeof(char));
		check_mem(log_buf.data[i]);
		log_buf.data[i][0] = '\0';
	}
}

static void
ncurses_push_log(const char *new_msg)
{
	pthread_mutex_lock(&log_buf.mutex);

	/* ++ and modulo to prevent overruns */
	log_buf.pos = (log_buf.pos + 1) % LOG_LINES;

	if (log_buf.data[log_buf.pos]) {
		char *free_me;

		free_me = log_buf.data[log_buf.pos];
		log_buf.data[log_buf.pos] = NULL;
		free(free_me);
		free_me = NULL;
	}

	log_buf.data[log_buf.pos] = malloc(strlen(new_msg) + 1);
	check_mem(log_buf.data[log_buf.pos]);
	memset(log_buf.data[log_buf.pos], '\0', strlen(new_msg) + 1);
	strncpy(log_buf.data[log_buf.pos], new_msg, strlen(new_msg));

	pthread_mutex_unlock(&log_buf.mutex);

	return;
}

static char *
ncurses_get_log(int offset)
{
	int pos;

	/* normalize offset */
	offset = offset - (offset / LOG_LINES) * LOG_LINES;

	/* prevent negative numbers */
	pos = (log_buf.pos + offset + LOG_LINES) % LOG_LINES;

	return log_buf.data[pos];
}

static void
ncurses_delete_log()
{
	int i;

	for (i = 0; i < LOG_LINES; i++) {
		free(log_buf.data[i]);
		log_buf.data[i] = NULL;
	}
	pthread_mutex_destroy(&log_buf.mutex);
}

static void
ncurses_draw_header()
{
	char tmp[19] = "";
	size_t head_info_l;
	size_t head_data_l;
	size_t head_upload_l;

	strcat(tmp, "(");
	if (!memcmp(CONF.upload_ip, "x", (size_t)2)) {
		strcat(tmp, "all");
	} else if (!memcmp(CONF.upload_ip, "-", (size_t)2)) {
		strcat(tmp, "none");
	} else {
		strcat(tmp, CONF.upload_ip);
	}
	strcat(tmp, ")-");

	head_info_l = strlen(head_info);
	head_data_l = strlen(head_data);
	head_upload_l = strlen(tmp);

	/* clear line */
	move(0, 0);
	clrtoeol();

	/* rootpath and port */
	if ((size_t)terminal_width > head_data_l + 1) {
		mvwaddnstr(stdscr, 0, terminal_width - (int)head_data_l,
		    head_data, -1);
	}

	/* upload ip */
	if ((size_t)terminal_width > head_data_l + head_upload_l + 1) {
		mvwaddnstr(stdscr, 0, terminal_width
		    - (int)(head_data_l + head_upload_l), tmp, -1);
	}

	/* name and version */
	if ((size_t)terminal_width > head_info_l + head_data_l
	    + head_upload_l + 1) {
		mvwaddnstr(stdscr, 0, 0, head_info, -1);
	}

	mvchgat(0, 0, -1, A_NORMAL, HEADER_COLOR_ID, NULL);
	refresh();
}

static void
ncurses_draw_logging_box()
{
	box(win_logging, ACS_VLINE, ACS_HLINE);
	if ((size_t)terminal_width > strlen(head_body_log) + 2) {
		mvwaddstr(win_logging, 0, 2, head_body_log);
	}
}

static void
ncurses_draw_status_box()
{
	box(win_status, ACS_VLINE, ACS_HLINE);
	if ((size_t)terminal_width > strlen(status_data) + 4) {
		mvwaddstr(win_status, 0,
		    (int)terminal_width - ((int)strlen(status_data) + 2),
		    status_data);
	}

	if ((size_t)terminal_width > strlen(status_data)
	    + strlen(head_body_status) + 4) {
		mvwaddstr(win_status, 0, 2, head_body_status);
	}
}
