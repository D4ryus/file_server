#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"

void msg_init(pthread_t *, const pthread_attr_t *);
void *_msg_print_loop(void *);
void msg_print_info(struct data_store *, const enum message_type, const char *, int);
void hook_add(struct data_store *);
void hook_cleanup(struct data_store *);

void _format_and_print(struct status_list_node *, const int);
void _hook_delete(void);

#endif
