#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* config struct, these value may change if specified as args */
struct config {
	char     *root_dir;
	uint16_t port;
	uint8_t  verbosity;
	char     ip[16];
	char     upload_ip[16];
	int      color;
	char     *log_file;
	FILE     *log_file_d;
};

extern struct config CONF;

/*
 * version string
 */
#define VERSION "0.4"

/*
 * ncurses status timeout
 */
#define UPDATE_TIMEOUT 0.2f

/*
 * maximum characters per header field
 */
#define HTTP_HEADER_LINE_LIMIT 4096

/*
 * maximum lines per header, maximum 256 (uint8_t)
 */
#define HTTP_HEADER_LINES_MAX 128

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
#define BUFFSIZE_READ 8192

/*
 * size of several message buffer, for example the one used to print on stdout.
 */
#define MSG_BUFFER_SIZE 256

/*
 * length of displayed name (+ path) on status window (ncurses)
 */
#define NAME_LENGTH 64

#endif
