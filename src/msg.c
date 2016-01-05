#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <inttypes.h>

#include "config.h"
#include "defines.h"
#include "msg.h"
#include "helper.h"

#ifdef NCURSES
#include "ncurses_msg.h"
#endif

static pthread_mutex_t print_mutex;

static void msg_print_status(const char *, int);
static void *msg_print_loop(void *);
static void format_status_msg(char *, size_t, int, int);
static const char *get_status_bar(uint8_t);

struct msg_hook {
	uint8_t in_use : 1;
	char ip[16];
	int port;
	/* dynamic array of transfer_hooks */
	struct {
		char name[NAME_LENGTH];
		char type[3]; /* tx, px or rx */
		uint64_t size;
		uint64_t written;
		uint64_t last_written;
	} trans;
};

/* dynamic array of msg_hooks */
static struct {
	pthread_mutex_t mutex;
	uint64_t left_over_tx;
	uint64_t left_over_rx;
	int used;
	int size;
	struct msg_hook *data;
} msg_hooks;

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

	check(CONF.update_timeout < 0.0f, "CONF.update_timeout < 0.0f");

#ifdef NCURSES
	ncurses_init(thread, attr);
	if (USE_NCURSES || CONF.verbosity >= 3) {
#else
	if (CONF.verbosity >= 3) {
#endif
		int i;

		pthread_mutex_init(&print_mutex, NULL);
		pthread_mutex_init(&msg_hooks.mutex, NULL);

		msg_hooks.left_over_tx = 0;
		msg_hooks.left_over_rx = 0;
		msg_hooks.used = 0;
		msg_hooks.size = 3;
		msg_hooks.data = err_calloc(sizeof(struct msg_hook), 3);
		for (i = 0; i < msg_hooks.size; i++) {
			msg_hooks.data[i].in_use = 0;
		}

		error = pthread_create(thread, attr, &msg_print_loop, NULL);
		check(error != 0, "pthread_create() returned %d", error);
	}
}

int
msg_hook_add(char ip[16], int port)
{
	int i;
	int msg_id;

	msg_id = 0;
	i = 0;
	pthread_mutex_lock(&msg_hooks.mutex);
	if (msg_hooks.used >= msg_hooks.size) {
		int next_free;

		/* if full, realloc new space and start search (i) there */
		next_free = msg_hooks.size;
		msg_hooks.size += 10;
		msg_hooks.data = err_realloc(msg_hooks.data,
		    sizeof(struct msg_hook) * (size_t)msg_hooks.size);
		for (i = next_free; i < msg_hooks.size; i++) {
			msg_hooks.data[i].in_use = 0;
		}
		i = next_free;
	}

	/* search for free spot */
	for (; i < msg_hooks.size; i++) {
		struct msg_hook *cur;

		if (msg_hooks.data[i].in_use) {
			continue;
		}
		msg_hooks.used++;
		msg_id = i;
		cur = &msg_hooks.data[msg_id];

		cur->in_use = 1;
		strncpy(cur->ip, ip, 16);
		cur->port = port;
		cur->trans.name[0] = '\0';
		cur->trans.type[0] = '\0';
		cur->trans.size = 0;
		cur->trans.written = 0;
		cur->trans.last_written = 0;
		break;
	}
	pthread_mutex_unlock(&msg_hooks.mutex);

	return msg_id;
}

void
msg_hook_rem(int msg_id)
{
	struct msg_hook *cur;

	pthread_mutex_lock(&msg_hooks.mutex);
	check(msg_id >= msg_hooks.size,
	    "given msg_id (%d) >= msg_hooks.size (%d)", msg_id,
	    msg_hooks.size);

	cur = &msg_hooks.data[msg_id];

	if (cur->trans.size != 0) {
		if (cur->trans.type[0] == 'r') {
			msg_hooks.left_over_rx +=
			    cur->trans.written - cur->trans.last_written;
		} else {
			msg_hooks.left_over_tx +=
			    cur->trans.written - cur->trans.last_written;
		}
	}

	cur->in_use = 0;
	msg_hooks.used--;
	pthread_mutex_unlock(&msg_hooks.mutex);
}

uint64_t *
msg_hook_new_transfer(int msg_id, char *name, uint64_t size, char type[3])
{
	struct msg_hook *cur;

	pthread_mutex_lock(&msg_hooks.mutex);
	check(msg_id >= msg_hooks.size,
	    "given msg_id (%d) >= msg_hooks.size (%d)", msg_id,
	    msg_hooks.size);

	cur = &msg_hooks.data[msg_id];

	/* in case there are leftovers */
	if (cur->trans.size != 0) {
		if (cur->trans.type[0] == 'r') {
			msg_hooks.left_over_rx +=
			    cur->trans.written - cur->trans.last_written;
		} else {
			msg_hooks.left_over_tx +=
			    cur->trans.written - cur->trans.last_written;
		}
	}
	strncpy(cur->trans.name, name, NAME_LENGTH);
	cur->trans.size = size;
	memcpy(cur->trans.type, type, 3);
	cur->trans.written = 0;
	pthread_mutex_unlock(&msg_hooks.mutex);

	return &cur->trans.written;
}

void
msg_hook_update_name(int msg_id, char *name)
{
	pthread_mutex_lock(&msg_hooks.mutex);
	check(msg_id >= msg_hooks.size,
	    "given msg_id (%d) >= msg_hooks.size (%d)",
	    msg_id, msg_hooks.size);
	strncpy(msg_hooks.data[msg_id].trans.name, name, NAME_LENGTH);
	pthread_mutex_unlock(&msg_hooks.mutex);
}

static void *
msg_print_loop(void *ignored)
{
	int position;
	uint64_t last_written;
	uint64_t tx;
	uint64_t rx;
	char msg_buffer[MSG_BUFFER_SIZE];
	struct timespec tsleep;
	int i;
	struct msg_hook *cur;

	/* will round down, what we want */
	tsleep.tv_sec = (long)CONF.update_timeout;
	tsleep.tv_nsec = (long)((CONF.update_timeout - (float)tsleep.tv_sec)
			* 1000000000L);
	position = 0;

	while (1) {
		pthread_mutex_lock(&msg_hooks.mutex);
#ifdef NCURSES
		ncurses_update_begin(position);
#endif
		position = 0;
		tx = 0;
		rx = 0;
		for (i = 0; i < msg_hooks.size; i++) {
			if (!msg_hooks.data[i].in_use) {
				continue;
			}

			cur = &msg_hooks.data[i];
			last_written = cur->trans.last_written;

			/* format_status_msg will update
			 * cur->trans.last_written */
			format_status_msg(msg_buffer, (size_t)MSG_BUFFER_SIZE,
			    i, position);
			msg_print_status(msg_buffer, position);

			if (cur->trans.type[0] == 'r') {
				rx += cur->trans.last_written - last_written;
			} else {
				tx += cur->trans.last_written - last_written;
			}

			/* finished */
			if (cur->trans.last_written == cur->trans.size) {
				cur->trans.size = 0;
				cur->trans.last_written = 0;
				cur->trans.written = 0;
				cur->trans.name[0] = '\0';
			}
			position++;
		}
		if (msg_hooks.left_over_rx) {
			rx += msg_hooks.left_over_rx;
			msg_hooks.left_over_rx = 0;
		}
		if (msg_hooks.left_over_tx) {
			tx += msg_hooks.left_over_tx;
			msg_hooks.left_over_tx = 0;
		}
		pthread_mutex_unlock(&msg_hooks.mutex);
#ifdef NCURSES
		ncurses_update_end(rx, tx, position);
#endif
		/* TODO: CLEANUP HERE */
		nanosleep(&tsleep, NULL);
	}
	/* not reached */
#ifdef NCURSES
	// ncurses_terminate();
#endif
	// pthread_mutex_destroy(&msg_hooks.mutex);
	// pthread_mutex_destroy(&print_mutex);

	return NULL;
}

/*
 * prints info (ip port socket) + given type and message to stdout
 */
void
msg_print_log(int msg_id, const enum message_type type, const char *message)
{
	char str_time[20];
	FILE *stream;
	time_t t;
	struct tm *tmp;
	char msg_buffer[MSG_BUFFER_SIZE];

#ifdef NCURSES
	if (!USE_NCURSES && type > CONF.verbosity) {
#else
	if (type > CONF.verbosity) {
#endif
		return;
	}

	t = time(NULL);
	check(t == -1, "time(NULL) returned -1");

	tmp = err_malloc(sizeof(struct tm));
	tmp = localtime_r(&t, tmp);

	check(!tmp, "localtime() returned NULL");

	check(strftime(str_time, (size_t)20, "%Y-%m-%d %H:%M:%S", tmp) == 0,
	    "strftime() returned NULL");
	free(tmp);
	tmp = NULL;

	snprintf(msg_buffer, (size_t)MSG_BUFFER_SIZE,
	    "%-19s [%15s]: %s",
	    str_time,
	    msg_hooks.data[msg_id].ip,
	    message ? message : "");

#ifdef NCURSES
	ncurses_print_log(msg_buffer);
#endif

	/* ncurses printed, check again if we also print to logfile */
	if (type > CONF.verbosity) {
		return;
	}

	if (CONF.log_file_d) {
		stream = CONF.log_file_d;
	} else {
		stream = stdout;
	}

	pthread_mutex_lock(&print_mutex);
	fprintf(stream, "%s\n", msg_buffer);

	if (CONF.log_file_d) {
		fflush(CONF.log_file_d);
	}
	pthread_mutex_unlock(&print_mutex);
}


/*
 * formats a data_list_node and calls print_info
 */
static void
format_status_msg(char *msg_buffer, size_t buff_size, int msg_id, int position)
{
	struct msg_hook *cur;
	/*
	 * later last_written is set and the initial written value is needed,
	 * to not read multiple times this value holds the inital value
	 */
	uint64_t written;
	uint64_t left;
	uint64_t size;
	uint64_t bytes_per_tval;
	uint8_t percent;

	char fmt_written[7];
	char fmt_left[7];
	char fmt_size[7];
	char fmt_bytes_per_tval[7];

	cur = &msg_hooks.data[msg_id];
	/* read value only once from struct */
	written = cur->trans.written;
	size = cur->trans.size;
	left = size - written;
	bytes_per_tval = (uint64_t)((float)(written - cur->trans.last_written)
				/ CONF.update_timeout);

	/* set last written to inital read value */
	cur->trans.last_written = written;
	percent = (uint8_t)(written * 100
		      / (cur->trans.size > 0 ? cur->trans.size : 1));

	format_size(written, fmt_written);
	format_size(left, fmt_left);
	format_size(size, fmt_size);
	format_size(bytes_per_tval, fmt_bytes_per_tval);

	snprintf(msg_buffer, buff_size,
	    "%15s: %s [%s|%3u%% (%6s/%6s @ %6s/s)] %s",
	    cur->ip,
	    cur->trans.type,
	    get_status_bar(percent),
	    percent,
	    fmt_written,
	    fmt_size,
	    /* fmt_left, */
	    fmt_bytes_per_tval,
	    cur->trans.name);

	return;
}

static const char *
get_status_bar(uint8_t percent)
{
	uint8_t pos;
	static const char *bars[] = {
		"          ", ".         ", "o         ",
		"O         ", "O.        ", "Oo        ",
		"OO        ", "OO.       ", "OOo       ",
		"OOO       ", "OOO.      ", "OOOo      ",
		"OOOO      ", "OOOO.     ", "OOOOo     ",
		"OOOOO     ", "OOOOO.    ", "OOOOOo    ",
		"OOOOOO    ", "OOOOOO.   ", "OOOOOOo   ",
		"OOOOOOO   ", "OOOOOOO.  ", "OOOOOOOo  ",
		"OOOOOOOO  ", "OOOOOOOO. ", "OOOOOOOOo ",
		"OOOOOOOOO ", "OOOOOOOOO.", "OOOOOOOOOo",
		"OOOOOOOOOO",
	};

	pos = (uint8_t)((float)percent / 3.33f + 0.5f);
	if (pos > 30) {
		pos = 30;
	}
	return bars[pos];
}

void
msg_print_status(const char *msg, int position)
{
	FILE *stream;

#ifdef NCURSES
	ncurses_print_status(msg, position);
#endif
	if (CONF.verbosity < 3) {
		return;
	}

	if (CONF.log_file_d) {
		stream = CONF.log_file_d;
	} else {
		stream = stdout;
	}

	pthread_mutex_lock(&print_mutex);
	if (CONF.color) {
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

	if (CONF.log_file_d) {
		fflush(CONF.log_file_d);
	}
	pthread_mutex_unlock(&print_mutex);
}
