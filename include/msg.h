#ifndef MESSAGES_H
#define MESSAGES_H

#include <pthread.h>

#include "types.h"

void msg_init(pthread_t *, const pthread_attr_t *);
void msg_print_log(struct client_info *, const enum message_type, const char *);
void msg_print_status(const char *, int);
void msg_hook_add(struct client_info *);
void msg_hook_cleanup(struct client_info *);

#endif
