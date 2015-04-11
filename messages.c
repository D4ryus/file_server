#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "messages.h"
#include "helper.h"

/**
 * see config.h
 */
extern char *ROOT_DIR;
extern int VERBOSITY;
extern size_t UPDATE_TIMEOUT;

static struct status_list_node *first = NULL;

void
add_hook(struct data_store *new_data)
{
        struct status_list_node *cur;
        struct status_list_node *new_node;

        new_node = err_malloc(sizeof(struct status_list_node));
        new_node->data = new_data;
        new_node->next = NULL;

        /* TODO: MUTEX LOCK */
        if (first == NULL) {
                first = new_node;
        } else {
                for (cur = first; cur->next != NULL; cur = cur->next) {
                }
                cur->next = new_node;
        }
        /* TODO: MUTEX LIFT */

        return;
}

void
remove_hook(struct data_store *del_data)
{
        struct status_list_node *cur;
        struct status_list_node *last;

        last = NULL;

        if (first == NULL || del_data == NULL) {
                return;
        }

        /* TODO: MUTEX LOCK */
        for (cur = first; cur != NULL; cur = cur->next) {
                if (cur->data == del_data) {
                        if (cur == first) {
                                first = cur->next;
                        } else {
                                last->next = cur->next;
                        }
                        break;
                }
                last = cur;
        }
        /* TODO: MUTEX LIFT */
        free(cur);

        return;
}

void
*print_loop(void *ptr)
{
        struct status_list_node *cur;
        char message_buffer[256];
        int position;
        size_t written;

        written = 0;
        position = 0;
#ifdef NCURSES
        /* TODO: NCURSES INIT? */
#endif

        while (1) {
                /* TODO: MUTEX LOCK */
                if (first == NULL) {
                        goto sleep;
                }
                for (cur = first; cur != NULL; cur = cur->next, position++) {
                        written = cur->data->written;
                        sprintf(message_buffer, "%-20s size: %12lub %12lub/%lus %3lu%%",
                                             cur->data->body + strlen(ROOT_DIR),
                                             cur->data->body_length,
                                             (written - cur->data->last_written) / UPDATE_TIMEOUT,
                                             UPDATE_TIMEOUT,
                                             written * 100 / cur->data->body_length);
                        print_info(cur->data, TRANSFER_STATUS, message_buffer);
#ifdef NCURSES
                        /* TODO: NCURSES PRINT */
#endif

                        cur->data->last_written = written;
                }
sleep:
                /* TODO: MUTEX LIFT */
                sleep((uint)UPDATE_TIMEOUT);
        }
}

void
print_info(struct data_store *data, enum message_type type, char *message)
{
#ifdef NCURSES
                        /* TODO: NCURSES PRINT */
#endif
        char *m_type;
        switch (type) {
                case ERROR:
                        if (VERBOSITY < 1) {
                                return;
                        }
                        m_type = "error";
                        break;
                case SENT:
                        if (VERBOSITY < 2) {
                                return;
                        }
                        m_type = "sent";
                        break;
                case ACCEPTED:
                        if (VERBOSITY < 3) {
                                return;
                        }
                        m_type = "accepted";
                        break;
                case TRANSFER_STATUS:
                        if (VERBOSITY < 3) {
                                return;
                        }
                        m_type = "transfer_status";
                        break;
                default:
                        m_type = "";
                        break;

        }

        if (data->color == NULL) {
                printf("[%15s:%-5d - %3d]: %-15s - %s\n",
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        } else {
                printf("\x1b[3%dm[%15s:%-5d - %3d]: %-15s - %s\x1b[39;49m\n",
                                (*data->color),
                                data->ip,
                                data->port,
                                data->socket,
                                m_type,
                                message);
        }
}