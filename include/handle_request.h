#ifndef HANDLE_REQUEST_H
#define HANDLE_REQUEST_H

/*
 * data struct passed from main thread to handle_request thread
 */
struct client_info {
	/* will be set by main thread */
	char ip[16]; /* ip from client */
	int port; /* port from client */
	int sock; /* socket descriptor */
};


void *handle_request(void *);

#endif
