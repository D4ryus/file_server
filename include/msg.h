#ifndef MESSAGES_H
#define MESSAGES_H

#include <pthread.h>

void msg_init(pthread_t *, const pthread_attr_t *);
void msg_print_log(int, int, const char *);
int msg_hook_add(char[16]);
void msg_hook_rem(int);
uint64_t *msg_hook_new_transfer(int, char *, uint64_t, char[3]);
void msg_hook_update_name(int, char *);

#endif
