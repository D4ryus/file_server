#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "config.h"
#include "handle_request.h"
#include "msg.h"
#include "misc.h"

#ifdef NCURSES
#include "ncurses_msg.h"
#endif

static void *sig_handler(void *);
static void parse_arguments(int, const char **);

int
main(const int argc, const char *argv[])
{
	int server_socket;
	int client_socket;
	int on;
	int error;
	pthread_t thread;
	pthread_attr_t attr;
	socklen_t clilen;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	struct client_info *data;
	sigset_t set;

	parse_arguments(argc, argv);

	/* block all specified signals, they will also be blocked on all created
	 * pthreads. handle these signals @sig_handler below */
	sigemptyset(&set);
#ifdef NCURSES
	sigaddset(&set, SIGWINCH);
#endif
	sigaddset(&set, SIGKILL);
	error = pthread_sigmask(SIG_BLOCK, &set, NULL);
	check(error, "pthread_sigmask(SIG_BLOCK)");

	/* init a pthread attribute struct */
	error = pthread_attr_init(&attr);
	check(error, "pthread_attr_init() returned %d", error)

	/* set threads to be detached, so they dont need to be joined */
	error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	check(error != 0, "pthread_attr_setdetachstate(\"%s\")",
	    "PTHREAD_CREATE_DETACHED");

	/* start extra thread to handle signals */
	error = pthread_create(&thread, &attr, &sig_handler, (void *)&set);
	check(error, "pthread_create(sig_handler)");

	msg_init(&thread, &attr);

	/* get a socket */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	check(server_socket < 0, "socket() returned %d", server_socket);

	on = 1;
	/* set socket options to make reuse of socket possible */
	error = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		    (const char *)&on, (socklen_t)sizeof(on));
	check(error == -1, "setsockopt():SO_REUSEADDR");

	memset((char *) &serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(CONF.port);

	/* bind socket */
	error = bind(server_socket, (struct sockaddr *) &serv_addr,
		    (socklen_t)sizeof(serv_addr));
	check(error < 0, "bind() returned %d", error);

	/* set socket in listen mode */
	error = listen(server_socket, 5);
	check(error == -1, "listen() returned %d", error);
	clilen = sizeof(cli_addr);

	/* put each conn in a new detached thread with its own data_store */
	while (1) {
		client_socket = accept(server_socket,
				   (struct sockaddr *)&cli_addr, &clilen);
#ifdef NCURSES
		/* ncurses uses signals on resize, so accept() will return */
		if (USE_NCURSES && client_socket == -1) {
			continue;
		}
#endif

		data = malloc(sizeof(struct client_info));
		check_mem(data);
		data->sock = client_socket;
		strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), (size_t)16);

		error = pthread_create(&thread, &attr, &handle_request, data);
		check(error, "pthread_create(handle_request)");
	}

	/* not reached */
	// free(CONF.root_dir);
	// close(server_socket);
}

/*
 * handles all signals
 */
static void *
sig_handler(void *arg)
{
	sigset_t *set;
	int sig;
	int err;

	set = (sigset_t *)arg;
	for (;;) {
		err = sigwait(set, &sig);
		check(err, "sigwait");
		printf("sig: %d\n", sig);
		switch (sig) {
#ifdef NCURSES
		case SIGWINCH:
			ncurses_organize_windows(1);
			break;
#endif
		case SIGKILL:
#ifdef NCURSES
			ncurses_terminate();
#endif
			exit(EXIT_SUCCESS);
			break;
		default:
			break;
		}
	}
}

static void
parse_arguments(const int argc, const char *argv[])
{
	int i;
	int root_arg;
	char *tmp;

	root_arg = 0;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-c") == 0)
		    || (strcmp(argv[i], "--color") == 0)) {
			CONF.color = 1;
		} else if ((strcmp(argv[i], "-d") == 0)
		    || (strcmp(argv[i], "--dir") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0],
				    "user specified -d/--dir without a path");
			}
			root_arg = i;
		} else if ((strcmp(argv[i], "-u") == 0)
		    || (strcmp(argv[i], "--upload") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0],
				    "user specified -u/--upload without"
				    " a ip");
			}
			memcpy(CONF.upload_ip, argv[i], strlen(argv[i]));
		} else if ((strcmp(argv[i], "-h") == 0)
		    || (strcmp(argv[i], "--help") == 0)) {
			usage_quit(argv[0], NULL);
		} else if ((strcmp(argv[i], "-l") == 0)
		    || (strcmp(argv[i], "--log_file") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0],
				    "user specified -l/--log_file without a "
				    "file");
			}
			CONF.log_file = malloc(strlen(argv[i]) + 1);
			check_mem(CONF.log_file);
			strncpy(CONF.log_file, argv[i], strlen(argv[i]) + 1);
#ifdef NCURSES
		} else if ((strcmp(argv[i], "-n") == 0)
		    || (strcmp(argv[i], "--ncurses") == 0)) {
			USE_NCURSES = 1;
#endif
		} else if ((strcmp(argv[i], "-p") == 0)
		    || (strcmp(argv[i], "--port") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0],
				    "user specified -p/--port without a port");
			}
			CONF.port = (uint16_t)atoi(argv[i]);
		} else if ((strcmp(argv[i], "-v") == 0)
		    || (strcmp(argv[i], "--verbosity") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0], "user specified "
				    "-v/--verbosity without a number (values "
				    "are [0] 1 2 3)");
			}
			CONF.verbosity = (uint8_t)atoi(argv[i]);
		} else if ((strcmp(argv[i], "-i") == 0)
		    || (strcmp(argv[i], "--ip") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0], "user specified "
				    "-i/--ip without a ip");
			}
			if (strlen(argv[i]) > 15) {
				usage_quit(argv[0], "user specified "
				    "-i/--ip with ip length > 15.");
			}
			normalize_ip(CONF.ip, argv[i]);
		} else {
			usage_quit(argv[0], "unknown argument specified");
		}
	}

	if (root_arg == 0) {
		tmp = realpath(CONF.root_dir, NULL);
		check(!tmp, "realpath(\"%s\") returned NULL", CONF.root_dir);
	} else {
		tmp = realpath(argv[root_arg], NULL);
		check(!tmp, "realpath(\"%s\") returned NULL", argv[root_arg]);
	}
	CONF.root_dir = tmp;

	check(strlen(CONF.root_dir) == 1 && CONF.root_dir[0] == '/',
		"sharing / is not possible.");

	if (CONF.log_file) {
		CONF.log_file_d = fopen(CONF.log_file, "a+");
		check(!CONF.log_file_d, "fopen(\"%s\") returned NULL",
		    CONF.log_file);
	}
}
