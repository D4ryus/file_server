#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"

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
void *print_loop(size_t refresh_time);
/**
 * prints info (ip port socket) + given type and message to stdout
 */
void print_info(struct data_store*, enum message_type, char*);

#endif
