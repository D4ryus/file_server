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
        pthread_t thread;
        int sockfd;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        int portno = 8283;
        struct thread_info *info;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
                quit("ERROR: socket()");
        }

        int on = 1;
        if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on))) {
                quit("ERROR: setsockopt() SO_REUSEADDR");
        }

        memset((char *) &serv_addr, '\0', sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                quit("ERROR: bind()");
        }

        listen(sockfd, 5);
        clilen = sizeof(cli_addr);

        // ignore sigpipe singla on write, since i cant catch it inside the threads
        signal(SIGPIPE, SIG_IGN);
        while (1) {
                info = malloc(sizeof(struct thread_info));
                if (info == NULL) {
                        mem_error("main()", "info", sizeof(struct thread_info));
                }
                info->socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                strncpy(info->ip, inet_ntoa(cli_addr.sin_addr), 16);
                info->port = ntohs(cli_addr.sin_port);

                if (pthread_create(&thread, NULL, &handle_request, info) != 0) {
                        quit("wasn't able to create Thread!");
                }
        }

        close(sockfd);
}
