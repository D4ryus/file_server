#ifndef MESSAGES_H
#define MESSAGES_H

#include <pthread.h>

#include "types.h"

void msg_init(pthread_t *, const pthread_attr_t *);
void msg_print_log(struct client_info *, const enum message_type, const char *);
void msg_print_status(const char *, int);
void msg_hook_add(struct client_info *);
void msg_hook_cleanup(struct client_info *);

void *_msg_print_loop(void *);
void _format_status_msg(char *, size_t, struct status_list_node *, const int);
void _msg_hook_delete(void);

#endif
