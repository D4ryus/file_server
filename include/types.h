#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/*
 * message verbosity
 */
enum message_type {
	ERROR = 1,
	CONNECTED = 2,
	FINISHED = 2
};

enum response_type {
	FILE_200,
	DIR_200,
	TXT_403,
	TXT_404,
	FILE_206
};

enum transfer_type {
	UPLOAD,
	DOWNLOAD,
	PARTIAL
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
	INV_HOST
};

/*
 * per request a data store is generated and then during execution filled
 */
struct client_info {
	/* will be set by main thread */
	char     ip[16];		/* ip from client */
	int      port;			/* port from client */
	int      sock;			/* socket descriptor */
	/* set by handle request thread */
	uint64_t size;			/* file size */
	uint64_t written;		/* written data */
	uint64_t last_written;		/* will be updated by print thread */
	char     *requested_path;	/* requested path */
	enum     transfer_type type;    /* PLAIN or HTTP */
};

/*
 * Linked list containing all client_info structs which should be printed
 * as status information, see messages.[ch]
 */
struct status_list_node {
	struct client_info *data;
	struct status_list_node *next;
	uint8_t remove_me;
};

#endif
