#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

struct config {
	char        *root_dir;
	uint16_t    port;
	uint8_t     verbosity;
	char        ip[16];
	char        upload_ip[16];
	int         color;
	const float update_timeout;
	char        *log_file;
	FILE        *log_file_d;
};

extern struct config CONF;

#endif
