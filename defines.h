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
 * if a file is transferd (post) BUFFSIZE_READ describes the buffersize of
 * bytes read from socket and then written to the file.
 */
#define BUFFSIZE_READ 8388608

/*
 * size of several message buffer, for example the one used to print on stdout.
 */
#define MSG_BUFFER_SIZE 256

#endif