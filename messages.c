#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "messages.h"
#include "helper.h"
#ifdef NCURSES
#include "ncurses_messages.h"
#endif

/*
 * see config.h
 */
extern char *ROOT_DIR;
extern FILE *_LOG_FILE;
extern int VERBOSITY;
extern int COLOR;
extern size_t UPDATE_TIMEOUT;
#ifdef NCURSES
extern int USE_NCURSES;
#endif

static struct status_list_node *first = NULL;
static pthread_mutex_t status_list_mutex;
static pthread_mutex_t print_mutex;

void
init_messages(pthread_t *thread, const pthread_attr_t *attr)
{
	int error;

	error = 0;

#ifdef NCURSES
	ncurses_init();
	if (USE_NCURSES || VERBOSITY >= 3) {
#else
	if (VERBOSITY >= 3) {
#endif
		pthread_mutex_init(&status_list_mutex, NULL);
		pthread_mutex_init(&print_mutex, NULL);

		error = pthread_create(thread, attr, &print_loop, NULL);
		if (error != 0) {
			err_quit(ERR_INFO, "pthread_create() != 0");
		}
	}
}

void *
print_loop(void *ignored)
{
	struct status_list_node *cur;
	int position;
	position = 0;

	while (1) {
#ifdef NCURSES
		ncurses_update_begin(position);
#endif
		position = 0;
		pthread_mutex_lock(&status_list_mutex);
		if (first == NULL) {
			goto sleep;
		}
		for (cur = first; cur != NULL; cur = cur->next, position++) {
			format_and_print(cur, position);
			/* TODO: sum up size here */
		}
sleep:
		pthread_mutex_unlock(&status_list_mutex);
#ifdef NCURSES
		ncurses_update_end(position);
#endif
		sleep((uint)UPDATE_TIMEOUT);
	}
#ifdef NCURSES
	ncurses_terminate();
#endif
	pthread_mutex_destroy(&status_list_mutex);
	pthread_mutex_destroy(&print_mutex);
}

void
print_info(struct data_store *data, const enum message_type type,
    const char *message, int position)
{
	char str_time[20];
	FILE *stream;
	char *m_type;
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL) {
		err_quit(ERR_INFO, "localtime() returned NULL");
	}

	if (strftime(str_time, 20, "%Y-%m-%d %H:%M:%S", tmp) == 0) {
		err_quit(ERR_INFO, "strftime() returned 0");
	}

	switch (type) {
		case ERROR:
			m_type = "err";
			break;
		case SENT:
			m_type = "snt";
			break;
		case CONNECTED:
			m_type = "con";
			break;
		case TRANSFER:
			m_type = "trn";
			break;
		default:
			m_type = "";
			break;
	}

#ifdef NCURSES
	ncurses_print_info(data, m_type, str_time, message, position);
#endif

	switch (type) {
		case ERROR:
			if (VERBOSITY < 1) {
				return;
			}
			break;
		case SENT:
			if (VERBOSITY < 2) {
				return;
			}
			break;
		case CONNECTED:
		case TRANSFER:
			if (VERBOSITY < 3) {
				return;
			}
			break;
		default:
			break;
	}

	if (_LOG_FILE != NULL) {
		stream = _LOG_FILE;
	} else {
		stream = stdout;
	}

	pthread_mutex_lock(&print_mutex);
	if (COLOR) {
		if (position == -1) {
			position = 7;
		} else {
			/* first (0) color is black, last (7) color is white */
			position = (position % 6) + 1;
		}
		fprintf(stream, "\x1b[3%dm%-19s[%15s:%-5d - %3d]: %3s - %s\x1b[39;49m\n",
		    position,
		    str_time,
		    data->ip,
		    data->port,
		    data->socket,
		    m_type,
		    message);
	} else {
		position = 0;
		fprintf(stream, "%-19s [%15s:%-5d - %3d]: %3s - %s\n",
		    str_time,
		    data->ip,
		    data->port,
		    data->socket,
		    m_type,
		    message);
	}

	if (_LOG_FILE != NULL) {
		fflush(_LOG_FILE);
	}
	pthread_mutex_unlock(&print_mutex);
}

void
format_and_print(struct status_list_node *cur, const int position)
{
	/*
	 * later last_written is set and the initial written value is needed,
	 * to not read multiple times this value holds the inital value
	 */
	uint64_t synched_written;
	uint64_t written;
	uint64_t left;
	uint64_t size;
	uint64_t bytes_per_tval;

	char   fmt_written[7];
	char   fmt_left[7];
	char   fmt_size[7];
	char   fmt_bytes_per_tval[7];

	char   msg_buffer[256];

	/* read value only once from struct */
	synched_written = cur->data->written;
	written		= synched_written;
	left		= cur->data->body_length - synched_written;
	size		= cur->data->body_length;
	bytes_per_tval	= (synched_written - cur->data->last_written) / UPDATE_TIMEOUT;

	/* set last written to inital read value */
	cur->data->last_written = cur->data->written;

	format_size(written, fmt_written);
	format_size(left, fmt_left);
	format_size(size, fmt_size);
	format_size(bytes_per_tval, fmt_bytes_per_tval);

	sprintf(msg_buffer, "%3lu%% [%6s/%6s (%6s)] %6s/%lus - %s",
	    synched_written * 100 / cur->data->body_length,
	    fmt_written,
	    fmt_size,
	    fmt_left,
	    fmt_bytes_per_tval,
	    UPDATE_TIMEOUT,
	    cur->data->body + strlen(ROOT_DIR));

	print_info(cur->data, TRANSFER, msg_buffer, position);
}

void
add_hook(struct data_store *new_data)
{
	struct status_list_node *cur;
	struct status_list_node *new_node;

	new_node = err_malloc(sizeof(struct status_list_node));
	new_node->data = new_data;
	new_node->next = NULL;

	pthread_mutex_lock(&status_list_mutex);
	if (first == NULL) {
		first = new_node;
	} else {
		for (cur = first; cur->next != NULL; cur = cur->next)
			; /* nothing */ 
		cur->next = new_node;
	}
	pthread_mutex_unlock(&status_list_mutex);

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

	pthread_mutex_lock(&status_list_mutex);
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
	pthread_mutex_unlock(&status_list_mutex);
	free(cur);

	return;
}

