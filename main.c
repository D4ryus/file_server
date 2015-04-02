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
                quit("ERROR: socket()");
        }

        on = 1;
        error = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
        if (error == -1) {
                quit("ERROR: setsockopt() SO_REUSEADDR");
        }

        error = pthread_attr_init(&attr);
        if (error != 0) {
                quit("ERROR: pthread_attr_init()");
        }

        /* set threads to be detached, so they dont need to be joined */
        error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (error != 0) {
                quit("ERROR: pthread_attr_setdetachstate()");
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

        // ignore sigpipe singla on write, since i cant catch it inside a thread
        signal(SIGPIPE, SIG_IGN);

        while (1) {
                data = create_data_store();
                if (data == NULL) {
                        mem_error("main()", "data", sizeof(struct data_store));
                }
                data->socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                strncpy(data->ip, inet_ntoa(cli_addr.sin_addr), 16);
                data->port = ntohs(cli_addr.sin_port);

                error = pthread_create(&thread, &attr, &handle_request, data);
                if (error != 0) {
                        quit("ERROR: pthread_create() could not create Thread!");
                }
        }

        /* error = pthread_attr_destory(&attr); */
        /* if (error != 0) { */
        /*         quit("ERROR: pthread_attr_destroy()"); */
        /* } */

        close(sockfd);
}
