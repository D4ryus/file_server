#ifndef CONFIG_H
#define CONFIG_H

/*
 * default values
 * --------------
 */

/*
 * shared directory if none specified
 */
char *ROOT_DIR = ".";

/*
 * port if none specified
 */
uint16_t PORT = 8283;

/*
 * port if none specified
 */
int VERBOSITY = 0;

/*
 * enable coloring
 */
int COLOR = 0;

/*
 * status update timeout
 */
const size_t UPDATE_TIMEOUT = 1;

/*
 * log file, if != NULL it will be used.
 */
char *LOG_FILE = NULL;

#ifdef NCURSES
/*
 * ncurses logging window height (rows)
 */
const int LOGGING_WINDOW_HEIGTH = 10;
#endif

/*
 * Http table config
 * -----------------
 */

/*
 * string which will be at the top of http table response
 */
const char *HTTP_TOP =
	"<!DOCTYPE html>"
	"<html>"
		"<head>"
			"<link href='http://fonts.googleapis.com/css?family=Iceland'"
				"rel='stylesheet'"
				"type='text/css'>"
			"<meta http-equiv='content-type'"
				 "content='text/html';"
				 "charset=UTF-8'/>"
		"</head>"
		"<center>"
			"<h4>file_server version 0.2</h4>"
			"<body>";

/*
 * string which will be at the bottom of http table response
 */
const char *HTTP_BOT =
			"</body>"
		"</center>"
	"</html>";

/*
 * the TABLE_BUFFER_SIZE is the size of the buffer where the table contents
 * will be filled in with snprintf(). so if table is getting bigger change value
 * accordingly.
 */
const size_t TABLE_BUFFER_SIZE = 512;

/*
 * table head values which will be filed in at %s
 * given values are:
 * "Last modified" "Type" "Size" "Filename"
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]" "[filetype]" "[filesize]" "[directory]" "[filename]"
 */
const char *TABLE_PLAIN[3] = {
	/* table head */
	"%-19s %-11s %-6s %s\n"
	"-----------------------------------------------\n",
	/* table body */
	"%19s %11s %6s %s/%s\n",
	/* table end */
	""};

const char *TABLE_HTML[3] = {
	/* table head */
	"<style>"
		"tbody tr:nth-child(odd) {"
			"background: #eee;"
		"}"
	"</style>"
	"<table>"
		"<thead>"
			"<tr>"
				"<th align='left'>%s</th>"
				"<th align='left'>%s</th>"
				"<th align='left'>%s</th>"
				"<th align='left'>%s</th>"
			"</tr>"
		"</thead>"
		"<tbody>",
	/* table body */
			"<tr>"
				"<td align='center'>%s</td>"
				"<td align='center'>%s</td>"
				"<td align='right'>%6s</td>"
				"<td align='left'><a href=\"%s/%s\">%5$s</a></td>"
			"</tr>",
	/* table end */
		"</tbody>"
	"</table>"};

/*
 * plaintext message which will be sent on 404 - File not found
 */
const char *RESPONSE_404 = "404 - Watcha pulling here buddy?\r\n";

/*
 * plaintext message which will be sent on 403 - Forbidden
 */
const char *RESPONSE_403 = "403 - U better not go down this road!\r\n";

/*
 * on new connection BUFFSIZE_READ -1 bytes are read from the socket,
 * everything extending that limit will be thrown away.
 */
const size_t BUFFSIZE_READ = 2048;

/*
 * if a file is transferd BUFFSIZE_WRITE describes the buffersize of
 * bytes read and then written to the socket.
 */
const size_t BUFFSIZE_WRITE = 8192;

#endif
