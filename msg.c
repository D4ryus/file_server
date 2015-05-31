#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>

#include "defines.h"
#include "msg.h"
#include "helper.h"
#ifdef NCURSES
#include "ncurses_msg.h"
#endif

/*
 * see globals.h
 */
extern char *ROOT_DIR;
extern FILE *_LOG_FILE;
extern uint8_t VERBOSITY;
extern int COLOR;
extern size_t UPDATE_TIMEOUT;
#ifdef NCURSES
extern int USE_NCURSES;
#endif

static struct status_list_node *first = NULL;
static pthread_mutex_t status_list_mutex;
static pthread_mutex_t print_mutex;
int msg_enabled = 0;

/*
 * initialize messages subsystem, which will create its own thread
 * and init mutex variables. if NCURSES is enabled ncurses will be
 * started also
 */
void
msg_init(pthread_t *thread, const pthread_attr_t *attr)
{
	enum err_status error;

	error = 0;

	if (UPDATE_TIMEOUT <= 0) {
		err_quit(ERR_INFO, "UPDATE_TIMEOUT < 0");
	}

#ifdef NCURSES
	ncurses_init(thread, attr);
	if (USE_NCURSES || VERBOSITY >= 3) {
#else
	if (VERBOSITY >= 3) {
#endif
		msg_enabled = 1;
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
	uint64_t down;
	uint64_t up;
	char msg_buffer[MSG_BUFFER_SIZE];

	position = 0;

	while (1) {
		pthread_mutex_lock(&status_list_mutex);
#ifdef NCURSES
		ncurses_update_begin(position);
#endif
		position = 0;
		down = 0;
		up = 0;
		for (cur = first; cur != NULL; cur = cur->next, position++) {
			last_written = cur->data->last_written;

			_format_status_msg(msg_buffer, (size_t)MSG_BUFFER_SIZE,
			    cur, position);
			msg_print_status(msg_buffer, position);

			if (cur->data->type == DOWNLOAD) {
				down += cur->data->last_written - last_written;
			} else {
				up += cur->data->last_written - last_written;
			}
		}
		pthread_mutex_unlock(&status_list_mutex);
#ifdef NCURSES
		ncurses_update_end(up, down, position);
#endif
		_msg_hook_delete();
		sleep((unsigned int)UPDATE_TIMEOUT);
	}
#ifdef NCURSES
	ncurses_terminate();
#endif
	pthread_mutex_destroy(&status_list_mutex);
	pthread_mutex_destroy(&print_mutex);

	return NULL;
}

/*
 * prints info (ip port socket) + given type and message to stdout
 */
void
msg_print_log(struct client_info *data, const enum message_type type,
    const char *message)
{
	char str_time[20];
	FILE *stream;
	time_t t;
	struct tm *tmp;
	char msg_buffer[MSG_BUFFER_SIZE];

#ifdef NCURSES
	if (!USE_NCURSES && type > VERBOSITY) {
#else
	if (type > VERBOSITY) {
#endif
		return;
	}

	t = time(NULL);
	if (t == -1) {
		err_quit(ERR_INFO, "time() returned - 1");
	}

	tmp = err_malloc(sizeof(struct tm));
	tmp = localtime_r(&t, tmp);

	if (tmp == NULL) {
		err_quit(ERR_INFO, "localtime() returned NULL");
	}

	if (strftime(str_time, (size_t)20, "%Y-%m-%d %H:%M:%S", tmp) == 0) {
		err_quit(ERR_INFO, "strftime() returned 0");
	}
	free(tmp);

	snprintf(msg_buffer, MSG_BUFFER_SIZE,
	    "%-19s [%15s:%-5d - %3d]: %s",
	    str_time,
	    data->ip,
	    data->port,
	    data->socket,
	    message == NULL ? "" : message);

#ifdef NCURSES
	ncurses_print_log(msg_buffer);
#endif

	/* ncurses printed, check again if we also print to logfile */
	if (type > VERBOSITY) {
		return;
	}

	if (_LOG_FILE != NULL) {
		stream = _LOG_FILE;
	} else {
		stream = stdout;
	}

	pthread_mutex_lock(&print_mutex);
	fprintf(stream, "%s\n", msg_buffer);

	if (_LOG_FILE != NULL) {
		fflush(_LOG_FILE);
	}
	pthread_mutex_unlock(&print_mutex);
}

/*
 * adds a hook to the status_list_node
 */
void
msg_hook_add(struct client_info *new_data)
{
	struct status_list_node *cur;
	struct status_list_node *new_node;

	if (!msg_enabled) {
		return;
	}

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
msg_hook_cleanup(struct client_info *rem_data)
{
	struct status_list_node *cur;
	int found;

	found = 0;

	for (cur = first; cur != NULL; cur = cur->next) {
		if (cur->data == rem_data) {
			cur->remove_me = 1;
			found = 1;
		}
	}

	if (!found) {
		if (rem_data->requested_path != NULL) {
			free(rem_data->requested_path);
		}
		free(rem_data);
	}
}

/*
 * formats a data_list_node and calls print_info
 */
void
_format_status_msg(char *msg_buffer, size_t buff_size,
    struct status_list_node *cur, const int position)
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

	/* read value only once from struct */
	written		= cur->data->written;
	left		= cur->data->size - written;
	size		= cur->data->size;
	bytes_per_tval	= (written - cur->data->last_written) / UPDATE_TIMEOUT;

	/* set last written to inital read value */
	cur->data->last_written = written;

	format_size(written, fmt_written);
	format_size(left, fmt_left);
	format_size(size, fmt_size);
	format_size(bytes_per_tval, fmt_bytes_per_tval);

	snprintf(msg_buffer, buff_size,
	    "[%15s:%-5d - %3d]: %3u%% [%6s/%6s (%6s)] %6s/%us %s %s",
	    cur->data->ip,
	    cur->data->port,
	    cur->data->socket,
	    (unsigned int)(written * 100 /
		(cur->data->size > 0 ? cur->data->size : 1)),
	    fmt_written,
	    fmt_size,
	    fmt_left,
	    fmt_bytes_per_tval,
	    (unsigned int)UPDATE_TIMEOUT,
	    (cur->data->type == DOWNLOAD) ? "down" : "up",
	    (cur->data->requested_path == NULL) ?
	        "-" :
	        cur->data->requested_path
	    );

	return;
}

void
msg_print_status(const char *msg, int position)
{
	FILE *stream;

#ifdef NCURSES
	ncurses_print_status(msg, position);
#endif
	if (VERBOSITY < 3) {
		return;
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
		fprintf(stream, "\x1b[3%dm%s\x1b[39;49m\n", position, msg);
	} else {
		fprintf(stream, "%s\n", msg);
	}

	if (_LOG_FILE != NULL) {
		fflush(_LOG_FILE);
	}
	pthread_mutex_unlock(&print_mutex);
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
			if (tmp->data->requested_path != NULL) {
				free(tmp->data->requested_path);
			}
			free(tmp->data);
			free(tmp);
		} else {
			last = cur;
			cur = cur->next;
		}
	}
	pthread_mutex_unlock(&status_list_mutex);

	return;
}

