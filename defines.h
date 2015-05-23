#ifndef DEFINES_H
#define DEFINES_H

#define HTTP_HEADER_LINE_LIMIT 2048

/*
 * the TABLE_BUFFER_SIZE is the size of the buffer where the table contents
 * will be filled in with snprintf(). so if table is getting bigger change value
 * accordingly.
 */
#define TABLE_BUFFER_SIZE 1024

/*
 * if a file is transferd BUFFSIZE_WRITE describes the buffersize of
 * bytes read and then written to the socket.
 */
#define BUFFSIZE_WRITE 8192

#endif
