#include <stdio.h>
#include <stdlib.h>

#include "messages.h"

void
print_info(struct data_store *data, enum message_type type, char *message)
{
        char *m_type;
        switch (type) {
                case ACCEPTED:
                        m_type = "accepted";
                        break;
                case SENT:
                        m_type = "sent";
                        break;
                case ERROR:
                        m_type = "error";
                        break;
                case TRANSFER_STATUS:
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
