#ifndef MESSAGES_NCURSES_H
#define MESSAGES_NCURSES_H

#include "types.h"

void ncurses_init(pthread_t *, const pthread_attr_t *);
void *handle_keyboard(void *);
void ncurses_print_log(char *);
void ncurses_print_status(const char *message, int position);
void ncurses_update_begin(int);
void ncurses_update_end(uint64_t, uint64_t, int);
void ncurses_terminate(void);
void ncurses_organize_windows(void);

void _ncurses_push_log_buf(char *);
void _ncurses_draw_logging_box(void);
void _ncurses_draw_status_box(void);
void _ncurses_resize_handler(int);

#endif
