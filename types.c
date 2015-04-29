#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "helper.h"

/*
 * mallocs a new data_store and sets initial values
 */
struct data_store *
create_data_store(void)
{
	struct data_store *data;

	data = err_malloc(sizeof(struct data_store));

	data->ip[0]   = '\0';
	data->port    = -1;
	data->socket  = -1;
	data->url = NULL;
	data->head = err_malloc(sizeof(char));
	data->head[0] = '\0';
	data->body = err_malloc(sizeof(char));
	data->body[0] = '\0';
	data->body_length = 0;
	data->req_type = TEXT;
	data->body_type = PLAIN;
	data->written = 0;
	data->last_written = 0;

	return data;
}

/*
 * savely frees the given datastore from memory
 */
void
free_data_store(struct data_store *data)
{
	if (data == NULL) {
		return;
	}
	if (data->url != NULL) {
		free(data->url);
	}
	if (data->head != NULL) {
		free(data->head);
	}
	if (data->body != NULL) {
		free(data->body);
	}
	free(data);

	return;
}

