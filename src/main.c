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

#include "handle_request.h"
#include "msg.h"
#include "types.h"
#include "helper.h"
#include "globals.h"

#ifdef NCURSES
#include "ncurses_msg.h"
#endif

FILE *_LOG_FILE = NULL;
char *UPLOAD_DIR = NULL;
int UPLOAD_ENABLED = 0;

#ifdef NCURSES
extern int USE_NCURSES;
extern int WINDOW_RESIZED;
#endif

int _main(int, const char **);
void parse_arguments(int, const char **);

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

	parse_arguments(argc, argv);

	/* init a pthread attribute struct */
	error = pthread_attr_init(&attr);
	if (error != 0) {
		die(ERR_INFO, "pthread_attr_init()");
	}

	/* set threads to be detached, so they dont need to be joined */
	error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (error != 0) {
		die(ERR_INFO,
		    "pthread_attr_setdetachstate():PTHREAD_CREATE_DETACHED");
	}

	msg_init(&thread, &attr);

	/* get a socket */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		die(ERR_INFO, "socket()");
	}

	on = 1;
	/* set socket options to make reuse of socket possible */
	error = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		    (const char *)&on, (socklen_t)sizeof(on));
	if (error == -1) {
		die(ERR_INFO, "setsockopt():SO_REUSEADDR");
	}

	memset((char *) &serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	/* bind socket */
	error = bind(server_socket, (struct sockaddr *) &serv_addr,
		    (socklen_t)sizeof(serv_addr));
	if (error < 0) {
		die(ERR_INFO, "bind()");
	}

	/* set socket in listen mode */
	error = listen(server_socket, 5);
	if (error == -1) {
		die(ERR_INFO, "listen()");
	}
	clilen = sizeof(cli_addr);

	/* put each conn in a new detached thread with its own data_store */
	while (1) {
		client_socket = accept(server_socket,
				   (struct sockaddr *)&cli_addr, &clilen);
#ifdef NCURSES
		/* ncurses uses signals on resize, so accept will continue */
		if (USE_NCURSES && WINDOW_RESIZED) {
			ncurses_organize_windows();
			continue;
		}
#endif
		/* check if ip is blocked */
		if (strlen(IP) > 0 &&
		    memcmp(IP, inet_ntoa(cli_addr.sin_addr), strlen(IP))) {
			printf("blocked\n");
			continue;
		}
		data = err_malloc(sizeof(struct client_info));
		data->sock = client_socket;
		strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), (size_t)16);
		data->port = ntohs(cli_addr.sin_port);

		error = pthread_create(&thread, &attr, &handle_request, data);
		if (error != 0) {
			die(ERR_INFO, "pthread_create()");
		}
	}

	/* not reached */
	// free(ROOT_DIR);
	// if (UPLOAD_DIR != NULL) {
	// 	free(UPLOAD_DIR);
	// }
	// close(server_socket);
}

void
parse_arguments(const int argc, const char *argv[])
{
	int i;
	int root_arg;
	int upload_arg;

	root_arg = 0;
	upload_arg = 0;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-c") == 0)
		    || (strcmp(argv[i], "--color") == 0)) {
			COLOR = 1;
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
				    " a path");
			}
			upload_arg = i;
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
			LOG_FILE = err_malloc(strlen(argv[i]) + 1);
			strncpy(LOG_FILE, argv[i], strlen(argv[i]) + 1);
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
			PORT = (uint16_t)atoi(argv[i]);
		} else if ((strcmp(argv[i], "-v") == 0)
		    || (strcmp(argv[i], "--verbosity") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0], "user specified "
				    "-v/--verbosity without a number (values "
				    "are [0] 1 2 3)");
			}
			VERBOSITY = (uint8_t)atoi(argv[i]);
		} else if ((strcmp(argv[i], "-i") == 0)
		    || (strcmp(argv[i], "--ip") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit(argv[0], "user specified "
				    "-i/--ip without a ip");
			}
			memcpy(IP, argv[i], strlen(argv[i]) + 1);
		} else {
			usage_quit(argv[0], "unknown argument specified");
		}
	}

	if (root_arg == 0) {
		ROOT_DIR = realpath(ROOT_DIR, NULL);
	} else {
		ROOT_DIR = realpath(argv[root_arg], NULL);
	}
	if (ROOT_DIR == NULL) {
		die(ERR_INFO, "realpath() on ROOT_DIR returned NULL");
	}
	if (strlen(ROOT_DIR) == 1 && ROOT_DIR[0] == '/') {
		die(ERR_INFO, "sharing / is not possible.");
	}

	if (upload_arg != 0) {
		UPLOAD_DIR = realpath(argv[upload_arg], NULL);
		if (UPLOAD_DIR == NULL) {
			die(ERR_INFO, "realpath on UPLOAD_DIR returned NULL");
		}
		UPLOAD_ENABLED = 1;
	}

	if (LOG_FILE != NULL) {
		_LOG_FILE = fopen(LOG_FILE, "a+");
		if (_LOG_FILE == NULL) {
			die(ERR_INFO, "could not open logfile");
		}
	}
}

