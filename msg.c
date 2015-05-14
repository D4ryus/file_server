#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "msg.h"
#include "helper.h"
#ifdef NCURSES
#include "ncurses_msg.h"
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

/*
 * initialize messages subsystem, which will create its own thread
 * and init mutex variables. if NCURSES is enabled ncurses will be
 * started also
 */
void
msg_init(pthread_t *thread, const pthread_attr_t *attr)
{
	int error;

	error = 0;

	if (UPDATE_TIMEOUT <= 0) {
		err_quit(ERR_INFO, "UPDATE_TIMEOUT < 0");
	}

#ifdef NCURSES
	ncurses_init();
	if (USE_NCURSES || VERBOSITY >= 3) {
#else
	if (VERBOSITY >= 3) {
#endif
		pthread_mutex_init(&status_list_mutex, NULL);
		pthread_mutex_init(&print_mutex, NULL);

		error = pthread_create(thread, attr, &_msg_print_loop, NULL);
		if (error != 0) {
			err_quit(ERR_INFO, "pthread_create() != 0");
		}
	}
}

/*
 * wont end, will print every refresh_time seconds
 * each element from the linkedlist
 */
void *
_msg_print_loop(void *ignored)
{
	struct status_list_node *cur;
	int position;
	uint64_t last_written;
	uint64_t written;

	position = 0;

	while (1) {
#ifdef NCURSES
		ncurses_update_begin(position);
#endif
		position = 0;
		written = 0;
		pthread_mutex_lock(&status_list_mutex);
		if (first == NULL) {
			goto sleep;
		}
		for (cur = first; cur != NULL; cur = cur->next, position++) {
			last_written = cur->data->last_written;
			_msg_format_and_print(cur, position);

			written += cur->data->last_written - last_written;
		}
sleep:
		pthread_mutex_unlock(&status_list_mutex);
#ifdef NCURSES
		ncurses_update_end(written);
#endif
		_msg_hook_delete();
		sleep((uint)UPDATE_TIMEOUT);
	}
#ifdef NCURSES
	ncurses_terminate();
#endif
	pthread_mutex_destroy(&status_list_mutex);
	pthread_mutex_destroy(&print_mutex);
}

/*
 * prints info (ip port socket) + given type and message to stdout
 */
void
msg_print_info(struct data_store *data, const enum message_type type,
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

/*
 * adds a hook to the status_list_node
 */
void
msg_hook_add(struct data_store *new_data)
{
	struct status_list_node *cur;
	struct status_list_node *new_node;

	new_node = err_malloc(sizeof(struct status_list_node));
	new_node->remove_me = 0;
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

/*
 * flags given data object for deletion (delete_me)
 */
void
msg_hook_cleanup(struct data_store *rem_data)
{
	struct status_list_node *cur;

	for (cur = first; cur != NULL; cur = cur->next) {
		if (cur->data == rem_data) {
			cur->remove_me = 1;
		}
	}
}

/*
 * formats a data_list_node and calls print_info
 */
void
_msg_format_and_print(struct status_list_node *cur, const int position)
{
	/*
	 * later last_written is set and the initial written value is needed,
	 * to not read multiple times this value holds the inital value
	 */
	uint64_t written;
	uint64_t left;
	uint64_t size;
	uint64_t bytes_per_tval;

	char fmt_written[7];
	char fmt_left[7];
	char fmt_size[7];
	char fmt_bytes_per_tval[7];

	size_t msg_buffer_size = 256;
	char msg_buffer[msg_buffer_size];

	/* read value only once from struct */
	written		= cur->data->written;
	left		= cur->data->body_length - written;
	size		= cur->data->body_length;
	bytes_per_tval	= (written - cur->data->last_written) / UPDATE_TIMEOUT;

	/* set last written to inital read value */
	cur->data->last_written = written;

	format_size(written, fmt_written);
	format_size(left, fmt_left);
	format_size(size, fmt_size);
	format_size(bytes_per_tval, fmt_bytes_per_tval);

	snprintf(msg_buffer, msg_buffer_size, "%3u%% [%6s/%6s (%6s)] %6s/%us - %s",
	    (unsigned int)(written * 100 /
		(cur->data->body_length > 0 ? cur->data->body_length : 1)),
	    fmt_written,
	    fmt_size,
	    fmt_left,
	    fmt_bytes_per_tval,
	    (unsigned int)UPDATE_TIMEOUT,
	    cur->data->body + strlen(ROOT_DIR));

	msg_print_info(cur->data, TRANSFER, msg_buffer, position);

	return;
}

/*
 * removes all flagged (remove_me) data hooks
 */
void
_msg_hook_delete()
{
	struct status_list_node *cur;
	struct status_list_node *tmp;
	struct status_list_node *last;

	if (first == NULL) {
		return;
	}

	last = NULL;

	pthread_mutex_lock(&status_list_mutex);
	for (cur = first; cur != NULL;) {
		if (cur->remove_me) {
			if (cur == first) {
				first = cur->next;
			} else {
				last->next = cur->next;
			}
			tmp = cur;
			cur = cur->next;
			free_data_store(tmp->data);
			free(tmp);
		} else {
			last = cur;
			cur = cur->next;
		}
	}
	pthread_mutex_unlock(&status_list_mutex);

	return;
}

