#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "helper.h"
#include "handle_request.h"

int main(int, const char**);

int
main(int argc, const char *argv[])
{
        int sockfd;
        int portno;
        int on;
        int error;
        pthread_t thread;
        pthread_attr_t attr;
        socklen_t clilen;
        struct sockaddr_in serv_addr;
        struct sockaddr_in cli_addr;
        struct data_store *data;

        portno = 8283;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
                err_quit(__FILE__, __LINE__, __func__, "socket() retuned < 0");
        }

        on = 1;
        error = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
        if (error == -1) {
                err_quit(__FILE__, __LINE__, __func__, "setsockopt() retuned -1");
        }

        error = pthread_attr_init(&attr);
        if (error != 0) {
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_init() != 0");
        }

        /* set threads to be detached, so they dont need to be joined */
        error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (error != 0) {
                err_quit(__FILE__, __LINE__, __func__, "pthread_attr_setdetachstate() != 0");
        }

        memset((char *) &serv_addr, '\0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);

        error = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        if (error < 0) {
                err_quit(__FILE__, __LINE__, __func__, "bind() < 0");
        }

        listen(sockfd, 5);
        clilen = sizeof(cli_addr);

        // ignore sigpipe singla on write, since i cant catch it inside a thread
        signal(SIGPIPE, SIG_IGN);

        while (1) {
                data = create_data_store();
                data->socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), 16);
                data->port = ntohs(cli_addr.sin_port);

                error = pthread_create(&thread, &attr, &handle_request, data);
                if (error != 0) {
                        err_quit(__FILE__, __LINE__, __func__, "pthread_create() != 0");
                }
        }

        close(sockfd);
}
