#ifndef TYPES_H
#define TYPES_H

/**
 * enums
 */
enum message_type { ACCEPTED, SENT, ERROR, TRANSFER_STATUS };

enum request_type { PLAIN, HTTP };

enum body_type { DATA, TEXT, ERR_404 = 404, ERR_403 = 403};

enum err_status {
        STAT_OK       =  0,
        WRITE_CLOSED  = -1,
        ZERO_WRITTEN  = -2,
        READ_CLOSED   = -3,
        EMPTY_MESSAGE = -4,
        INV_GET       = -5
};

/**
 * file information
 */
struct file {
        char  *name;
        char  type[17];
        char  time[20];
        off_t size;
};

/**
 * contains multiple file structs
 */
struct dir {
        int  length;
        char *name;
        struct file *files[];
};

/**
 * per request a data store is generated and then during execution filled
 */
struct data_store {
        char   ip[16];                /* ip from client */
        int    port;                  /* port from client */
        int    socket;                /* socket descriptor */
        char   *url;                  /* requested file */
        char   *head;                 /* response header */
        char   *body;                 /* response body, if file filename */
        size_t body_length;           /* length of response body / filesize */
        enum  request_type req_type;  /* requested type */
        enum  body_type    body_type; /* type of body */
        size_t written;               /* written data */
        size_t last_written;          /* will be updated by status print thread */
};

struct status_list_node {
        struct data_store *data;
        struct status_list_node *next;
};

/**
 * mallocs a new data_store and sets initial values
 */
struct data_store *create_data_store(void);

/**
 * savely frees the given datastore from memory
 */
void free_data_store(struct data_store*);

#endif
