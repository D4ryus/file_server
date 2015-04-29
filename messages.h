#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"

void init_messages(pthread_t *, const pthread_attr_t *);
void *print_loop(void *);
void print_info(struct data_store *, const enum message_type, const char *, int);
void format_and_print(struct status_list_node *, const int);
void add_hook(struct data_store *);
void remove_hook(struct data_store *);

#endif
