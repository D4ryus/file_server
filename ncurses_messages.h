#ifndef MESSAGES_NCURSES_H
#define MESSAGES_NCURSES_H

#include "types.h"

void ncurses_init(void);
void ncurses_print_info(struct data_store *, char *, const char *, const char *, int);
void ncurses_update_begin(int);
void ncurses_update_end(void);
void ncurses_terminate(void);

void _ncurses_init_windows(int, int);
void _ncurses_resize_handler(int);

#endif
