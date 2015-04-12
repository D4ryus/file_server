#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"

/**
 * initialize messages subsystem, which will create its own thread
 * and init mutex variables. if NCURSES is enabled ncurses will be
 * started also
 */
void init_messages(pthread_t*, const pthread_attr_t*);

/**
 * adds a hook to the status print thread
 */
void add_hook(struct data_store*);

/**
 * removes a hook to the status print thread
 */
void remove_hook(struct data_store*);

/**
 * wont end, will print every refresh_time seconds
 * each element from the linkedlist
 */
void *print_loop(void*);
/**
 * prints info (ip port socket) + given type and message to stdout
 */
void print_info(struct data_store*, enum message_type, char*, int);

#ifdef NCURSES
/**
 * handles signal if window is resized
 */
void resize_handler(int);

/**
 * initializes ncurses windows, called on startup and resize
 */
void init_ncurses_windows(int, int);
#endif

#endif
