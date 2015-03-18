#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "helper.h"
#include "handle_request.h"

int main(int, const char**);

int
main(int argc, const char *argv[])
{
        int sockfd;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        int portno = 8283;

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

        while (1) {
                int client = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                printf("Accpepted - [IP: %s, Connected on PORT: %i]\n",
                                inet_ntoa(cli_addr.sin_addr),
                                ntohs(cli_addr.sin_port));
                handle_request(client);
                close(client);
        }

        close(sockfd);
}
