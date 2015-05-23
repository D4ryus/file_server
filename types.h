#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/*
 * enums
 */
enum message_type {CONNECTED, SENT, ERROR, TRANSFER, POST};

enum request_type {PLAIN, HTTP};

enum response_type {FILE_200, DIR_200, TXT_403, TXT_404};

/*
 * intern error enum, used as return value from functions
 */
enum err_status {
	STAT_OK            =   0,
	WRITE_CLOSED       =  -1,
	ZERO_WRITTEN       =  -2,
	CLOSED_CON         =  -3,
	EMPTY_MESSAGE      =  -4,
	INV_REQ_TYPE       =  -5,
	INV_GET            =  -6,
	INV_POST           =  -7,
	CON_LENGTH_MISSING =  -8,
	BOUNDARY_MISSING   =  -9,
	REFERER_MISSING    = -10,
	FILESIZE_ZERO      = -11,
	WRONG_BOUNDRY      = -12,
	LINE_LIMIT_EXT     = -13
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
