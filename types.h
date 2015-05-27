#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/*
 * enums
 */
enum message_type {
	CONNECTED = 0,
	SENT,
	ERROR,
	TRANSFER,
	POST
};

enum request_type {PLAIN, HTTP};

enum response_type {FILE_200, DIR_200, TXT_403, TXT_404};

/*
 * intern error enum, used as return value from functions
 */
enum err_status {
	STAT_OK =  0,
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
	CONTENT_LENGTH_EXT
};

/*
 * per request a data store is generated and then during execution filled
 */
struct client_info {
	char     ip[16];		/* ip from client */
	int      port;			/* port from client */
	int      socket;		/* socket descriptor */
	char     *requested_path;	/* requested path */
	uint64_t size;			/* file size */
	uint64_t written;		/* written data */
	uint64_t last_written;		/* will be updated by print thread */
};

/*
 * file information
 */
struct file {
	char *name;
	char type[11];
	char time[20];
	off_t size;
};

/*
 * contains multiple file structs
 */
struct dir {
	int length;
	char *name;
	struct file *files[];
};

/*
 * Linked list containing all client_info structs which should be printed
 * as status information, see messages.[ch]
 */
struct status_list_node {
	uint8_t remove_me;
	struct client_info *data;
	struct status_list_node *next;
};

#endif
