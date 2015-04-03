#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include "helper.h"
#include "handle_request.h"

int main(int, const char**);

int
main(int argc, const char *argv[])
{
        int server_socket;
        int port;
        int on;
        int error;
        pthread_t thread;
        pthread_attr_t attr;
        socklen_t clilen;
        struct sockaddr_in serv_addr;
        struct sockaddr_in cli_addr;
        struct data_store *data;
        int current_color;
        int colors[] = {0, 0, 0, 0, 0, 0, 0};
        int i;
        char root_dir[256];
        char* rp = NULL;

        root_dir[0] = '\0';

        for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
                        i++;
                        if (argc < i) {
                                err_quit(__FILE__, __LINE__, __func__,
                                                "user specified -p/--path "
                                                "without a path");
                        }
                        strncpy(root_dir, argv[i], 255);
                        if (strlen(root_dir) == 1 && root_dir[0] == '/') {
                                err_quit(__FILE__, __LINE__, __func__, "you tried to share /, no sir, thats not happening");
                        }
                        while (root_dir[strlen(root_dir) - 1] == '/') {
                                root_dir[strlen(root_dir) - 1] = '\0';
                        }
                }
        }

        if (strlen(root_dir) == 0) {
                rp = realpath(".", NULL);
                if (rp == NULL) {
                        err_quit(__FILE__, __LINE__, __func__, "realpath on . returned NULL");
                }
                strncpy(root_dir, rp, strlen(rp));
                root_dir[strlen(rp)] = '\0';
        }

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

        /* ignore sigpipe singal on write, since i cant catch it inside a thread */
        signal(SIGPIPE, SIG_IGN);

        port = 8283;
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
        serv_addr.sin_port = htons(port);

        /* bind socket */
        error = bind(server_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        if (error < 0) {
                err_quit(__FILE__, __LINE__, __func__, "bind() < 0");
        }

        /* set socket in listen mode */
        listen(server_socket, 5);
        clilen = sizeof(cli_addr);

        /* put each connection in a new detached thread with its own data_store */
        current_color = 1;
        while (1) {
                data = create_data_store();
                strncpy(data->root_dir, root_dir, 255);
                data->socket = accept(server_socket, (struct sockaddr *) &cli_addr, &clilen);
                current_color = current_color % 7 + 1;
                for (;current_color < 7; current_color++) {
                        if (colors[current_color] == 0) {
                                colors[current_color] = current_color;
                                data->color = &(colors[current_color]);
                                current_color++;
                                break;
                        }
                }
                strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), 16);
                data->port = ntohs(cli_addr.sin_port);

                error = pthread_create(&thread, &attr, &handle_request, data);
                if (error != 0) {
                        err_quit(__FILE__, __LINE__, __func__, "pthread_create() != 0");
                }
        }

        close(server_socket);
}
