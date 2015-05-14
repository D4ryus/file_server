#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "handle_request.h"
#include "msg.h"
#include "types.h"
#include "helper.h"
#include "config.h"

#ifdef NCURSES
#include "ncurses_msg.h"
#endif

FILE *_LOG_FILE = NULL;

#ifdef NCURSES
extern int USE_NCURSES;
extern int WINDOW_RESIZED;
#endif

int main(int, const char **);
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
	struct data_store *data;

	parse_arguments(argc, argv);

	/* init a pthread attribute struct */
	error = pthread_attr_init(&attr);
	if (error != 0) {
		err_quit(ERR_INFO, "pthread_attr_init() != 0");
	}

	/* set threads to be detached, so they dont need to be joined */
	error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (error != 0) {
		err_quit(ERR_INFO, "pthread_attr_setdetachstate() != 0");
	}

	size_t stack_size;
	error =  pthread_attr_getstacksize(&attr, &stack_size);
	if (error != 0) {
		err_quit(ERR_INFO, "pthread_attr_getstacksize() != 0");
	}

	stack_size = PTHREAD_STACK_MIN << 1;
	/* printf("set pthread stacksize to %lu\n", stack_size); */
	error = pthread_attr_setstacksize(&attr, stack_size);
	if (error != 0) {
		err_quit(ERR_INFO, "pthread_attr_setstacksize() != 0");
	}

	/* ignore sigpipe singal on write, since i cant catch it inside a thread */
	signal(SIGPIPE, SIG_IGN);

	msg_init(&thread, &attr);

	/* get a socket */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		err_quit(ERR_INFO, "socket() retuned < 0");
	}

	on = 1;
	/* set socket options to make reuse of socket possible */
	error = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		    (const char *) &on, sizeof(on));
	if (error == -1) {
		err_quit(ERR_INFO, "setsockopt() retuned -1");
	}

	memset((char *) &serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	/* bind socket */
	error = bind(server_socket, (struct sockaddr *) &serv_addr,
		    sizeof(serv_addr));
	if (error < 0) {
		err_quit(ERR_INFO, "bind() < 0");
	}

	/* set socket in listen mode */
	listen(server_socket, 5);
	clilen = sizeof(cli_addr);

	/* put each connection in a new detached thread with its own data_store */
	while (1) {
		client_socket = accept(server_socket,
				   (struct sockaddr *) &cli_addr, &clilen);
#ifdef NCURSES
		/* ncurses uses signals on resize, so accept will continue */
		if (USE_NCURSES && WINDOW_RESIZED) {
			WINDOW_RESIZED = 0;
			ncurses_organize_windows();
			continue;
		}
#endif
		data = create_data_store();
		data->socket = client_socket;
		strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), 16);
		data->port = ntohs(cli_addr.sin_port);

		error = pthread_create(&thread, &attr, &handle_request, data);
		if (error != 0) {
			err_quit(ERR_INFO, "pthread_create() != 0");
		}
	}

	free(ROOT_DIR);
	close(server_socket);
}

void
parse_arguments(const int argc, const char *argv[])
{
	int i;
	int root_arg = 0;

	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-c") == 0)
		    || (strcmp(argv[i], "--color") == 0)) {
			COLOR = 1;
		} else if ((strcmp(argv[i], "-d") == 0)
		    || (strcmp(argv[i], "--dir") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit("user specified -d/--dir without a path");
			}
			root_arg = i;
		} else if ((strcmp(argv[i], "-h") == 0)
		    || (strcmp(argv[i], "--help") == 0)) {
			usage_quit(NULL);
		} else if ((strcmp(argv[i], "-l") == 0)
		    || (strcmp(argv[i], "--log_file") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit("user specified -l/--log_file "
				    "without a file");
			}
			LOG_FILE = malloc(strlen(argv[i]) + 1);
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
				usage_quit("user specified -p/--port "
				    "without a port");
			}
			PORT = (uint16_t)atoi(argv[i]);
		} else if ((strcmp(argv[i], "-v") == 0)
		    || (strcmp(argv[i], "--verbosity") == 0)) {
			i++;
			if (argc <= i) {
				usage_quit("user specified -v/--verbosity "
				    "without a number (values are [0] 1 2 3)");
			}
			VERBOSITY = atoi(argv[i]);
		} else {
			usage_quit("unknown argument specified");
		}
	}

	if (root_arg == 0) {
		ROOT_DIR = realpath(ROOT_DIR, NULL);
	} else {
		ROOT_DIR = realpath(argv[root_arg], NULL);
	}
	if (ROOT_DIR == NULL) {
		err_quit(ERR_INFO, "realpath on ROOT_DIR returned NULL");
	}
	if (strlen(ROOT_DIR) == 1 && ROOT_DIR[0] == '/') {
		err_quit(ERR_INFO, "you tried to share /, no sir, thats not happening");
	}

	if (LOG_FILE != NULL) {
		_LOG_FILE = fopen(LOG_FILE, "a+");
		if (_LOG_FILE == NULL) {
			err_quit(ERR_INFO, "could not open logfile");
		}
	}
}

