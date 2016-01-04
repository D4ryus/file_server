#ifndef MESSAGES_H
#define MESSAGES_H

#include <pthread.h>

#include "types.h"

/*
 * message verbosity
 */
enum message_type {
	ERROR = 1,
	CONNECTED = 2,
	FINISHED = 2
};

void msg_init(pthread_t *, const pthread_attr_t *);
void msg_print_log(int, const enum message_type, const char *);
int msg_hook_add(char[16], int);
void msg_hook_rem(int);
uint64_t *msg_hook_new_transfer(int, char *, uint64_t, char[3]);
void msg_hook_update_name(int, char *);

#endif
