#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"

/*
 * initialize messages subsystem, which will create its own thread
 * and init mutex variables. if NCURSES is enabled ncurses will be
 * started also
 */
void init_messages(pthread_t *, const pthread_attr_t *);

/*
 * wont end, will print every refresh_time seconds
 * each element from the linkedlist
 */
void *print_loop(void *);

/*
 * prints info (ip port socket) + given type and message to stdout
 */
void print_info(struct data_store *, const enum message_type, const char *, int);

/*
 * formats a data_list_node and calls print_info
 */
void format_and_print(struct status_list_node *, const int);

/*
 * adds a hook to the status print thread
 */
void add_hook(struct data_store *);

/*
 * removes a hook to the status print thread
 */
void remove_hook(struct data_store *);

#endif
