#ifndef MESSAGES_H
#define MESSAGES_H

#include <pthread.h>

#include "types.h"

void msg_init(pthread_t *, const pthread_attr_t *);
void msg_print_info(struct data_store *, const enum message_type, const char *,
         int);
void msg_hook_add(struct data_store *);
void msg_hook_cleanup(struct data_store *);

void *_msg_print_loop(void *);
void _msg_format_and_print(struct status_list_node *, const int);
void _msg_hook_delete(void);

#endif
