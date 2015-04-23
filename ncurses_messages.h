#ifndef MESSAGES_NCURSES_H
#define MESSAGES_NCURSES_H

#include "types.h"

/*
 * handles resize signal
 */
void _ncurses_resize_handler(int);

/*
 * initialises ncurses main window
 */
void ncurses_init(void);

/*
 * prints given info inside ncurses window
 */
void ncurses_print_info(struct data_store *, char *, const char *, const char *, int);

/*
 * start of printing status messages
 */
void ncurses_update_begin(int);

/*
 * end of printing status messages
 */
void ncurses_update_end(int);

/*
 * terminates ncurses window
 */
void ncurses_terminate(void);

/*
 * initializes ncurses windows, called on startup and resize
 */
void ncurses_init_windows(int, int);

#endif
