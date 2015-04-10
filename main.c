#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "handle_request.h"
#include "types.h"
#include "helper.h"
#include "config.h"

int use_color = 0;
int current_color = 0;
int colors[] = {0, 0, 0, 0, 0, 0, 0, 0};

int main(int, const char**);
int *get_color(void);
void parse_arguments(int, const char**);

int
main(int argc, const char *argv[])
{
        int server_socket;
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
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_init() != 0");
        }

        /* set threads to be detached, so they dont need to be joined */
        error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (error != 0) {
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_setdetachstate() != 0");
        }

        size_t stack_size;
        error =  pthread_attr_getstacksize(&attr, &stack_size);
        if (error != 0) {
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_getstacksize() != 0");
        }

        stack_size = PTHREAD_STACK_MIN << 1;
        printf("set pthread stacksize to %lu\n", stack_size);
        error = pthread_attr_setstacksize(&attr, stack_size);
        if (error != 0) {
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_setstacksize() != 0");
        }

        /* ignore sigpipe singal on write, since i cant catch it inside a thread */
        signal(SIGPIPE, SIG_IGN);

        /* get a socket */
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
                err_quit(__FILE__, __LINE__, __func__, "socket() retuned < 0");
        }

        on = 1;
        /* set socket options to make reuse of socket possible */
        error = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
        if (error == -1) {
                err_quit(__FILE__, __LINE__, __func__, "setsockopt() retuned -1");
        }

        memset((char *) &serv_addr, '\0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(PORT);

        /* bind socket */
        error = bind(server_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        if (error < 0) {
                err_quit(__FILE__, __LINE__, __func__, "bind() < 0");
        }

        /* set socket in listen mode */
        listen(server_socket, 5);
        clilen = sizeof(cli_addr);

        /* put each connection in a new detached thread with its own data_store */
        while (1) {
                data = create_data_store();
                data->socket = accept(server_socket, (struct sockaddr *) &cli_addr, &clilen);
                if (use_color) {
                        data->color = get_color();
                }
                strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), 16);
                data->port = ntohs(cli_addr.sin_port);

                error = pthread_create(&thread, &attr, &handle_request, data);
                if (error != 0) {
                        err_quit(__FILE__, __LINE__, __func__, "pthread_create() != 0");
                }
        }

        free(ROOT_DIR);
        close(server_socket);
}

int
*get_color(void)
{
        int i;
        int *ret_color = NULL;

        for (i = 0; i < 8; i++, current_color++) {
                current_color = current_color % 8;
                if (current_color == 0) { /* ignore black */
                        continue;
                }
                if (colors[current_color] == 0) {
                        colors[current_color] = current_color;
                        ret_color = &(colors[current_color]);
                        current_color++;
                        break;
                }
        }

        return ret_color;
}

void
parse_arguments(int argc, const char *argv[])
{
        int i;
        int root_arg = 0;

        for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--dir") == 0)) {
                        i++;
                        if (argc < i) {
                                err_quit(__FILE__, __LINE__, __func__,
                                                "user specified -d/--dir "
                                                "without a path");
                        }
                        root_arg = i;
                } else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--port") == 0)) {
                        i++;
                        if (argc < i) {
                                err_quit(__FILE__, __LINE__, __func__,
                                                "user specified -p/--port "
                                                "without a port");
                        }
                        PORT = atoi(argv[i]);
                } else if ((strcmp(argv[i], "-c") == 0) || (strcmp(argv[i], "--color") == 0)) {
                        use_color = 1;
                }
        }

        if (root_arg == 0) {
                ROOT_DIR = realpath(ROOT_DIR, NULL);
        } else {
                ROOT_DIR = realpath(argv[root_arg], NULL);
        }
        if (ROOT_DIR == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "realpath on ROOT_DIR returned NULL");
        }
        if (strlen(ROOT_DIR) == 1 && ROOT_DIR[0] == '/') {
                err_quit(__FILE__, __LINE__, __func__, "you tried to share /, no sir, thats not happening");
        }
}

