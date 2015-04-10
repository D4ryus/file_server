#ifndef MESSAGES_H
#define MESSAGES_H

#include "types.h"
/**
 * prints info (ip port socket) + given type and message to stdout
 */
void print_info(struct data_store*, enum message_type, char*);

#endif
