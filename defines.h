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
 * if a file is transferd (get) BUFFSIZE_WRITE describes the buffersize of
 * bytes read from file and then written to the socket.
 */
#define BUFFSIZE_WRITE 8192

/*
 * if a file is transferd (post) BUFFSIZE_READ describes the buffersize of
 * bytes read from socket and then written to the file.
 */
#define BUFFSIZE_READ 8192

#endif
