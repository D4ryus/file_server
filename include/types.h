#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

enum response_type {
	FILE_200,
	DIR_200,
	TXT_403,
	TXT_404,
	FILE_206
};

/*
 * intern error enum, used as return value from functions
 */
enum err_status {
	STAT_OK = 0,
	WRITE_CLOSED,
	ZERO_WRITTEN,
	CLOSED_CON,
	EMPTY_MESSAGE,
	INV_REQ_TYPE,
	INV_GET,
	INV_POST,
	CON_LENGTH_MISSING,
	BOUNDARY_MISSING,
	FILESIZE_ZERO,
	WRONG_BOUNDRY,
	HTTP_HEAD_LINE_EXT,
	FILE_HEAD_LINE_EXT,
	POST_NO_FILENAME,
	NO_FREE_SPOT,
	FILE_ERROR,
	NO_CONTENT_DISP,
	FILENAME_ERR,
	CONTENT_LENGTH_EXT,
	POST_DISABLED,
	HEADER_LINES_EXT,
	INV_CONTENT_TYPE,
	INV_RANGE,
	INV_POST_PATH,
	INV_CONNECTION,
	INV_HOST,
	IP_BLOCKED
};

/*
 * per request a data store
 */
struct client_info {
	/* will be set by main thread */
	char ip[16]; /* ip from client */
	int port; /* port from client */
	int sock; /* socket descriptor */
};

#endif
