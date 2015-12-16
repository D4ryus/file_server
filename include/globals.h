#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

extern char *ROOT_DIR;
extern uint16_t PORT;
extern uint8_t VERBOSITY;
extern char IP[16];
extern char UPLOAD_IP[16];
extern int COLOR;
extern const size_t UPDATE_TIMEOUT;
extern char *LOG_FILE;
extern FILE *LOG_FILE_D;
extern const char *HTTP_TOP;
extern const char *HTTP_UPLOAD;
extern const char *HTTP_BOT;
extern const char *TABLE_PLAIN[3];
extern const char *TABLE_HTML[3];
extern const char *RESPONSE_403;
extern const char *RESPONSE_404;
extern const char *RESPONSE_405;
extern const char *RESPONSE_201;

#endif
