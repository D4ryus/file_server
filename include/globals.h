#ifndef GLOBALS_H
#define GLOBALS_H

/*
 * default values, these get overwritten if specified on startup
 * ------------------------------------------------------------
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
 * default verbosity
 */
uint8_t VERBOSITY = 0;

/*
 * default allowed ip, * disables ip blocking.
 */
char IP[16] = "*";

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

/*
 * Http table config
 * -----------------
 */

/*
 * string which will be at the top of http table response
 */
const char *HTTP_TOP =
	"<!DOCTYPE html>"
	"<style>"
		"tbody tr:nth-child(odd) {"
			"background: #eeeeee;"
		"}"
		"a:hover {"
			"color: #229922;"
		"}"
	"</style>"
	"<html>"
		"<head>"
			"<title>file_server version 0.2</title>"
		"</head>"
		"<center>"
			"<h4>file_server version 0.2</h4>"
			"<body>";

/*
 * string which will be at the bottom of http table response
 */
const char *HTTP_BOT =
			"</body>"
		"<div align='right'>"
			"Powered by "
			"<a href='https://freedns.afraid.org/'>freedns.afraid.org</a>"
			" and "
			"<a href='http://www.vim.org/'>Vim</a>"
		"</div>"
		"</center>"
	"</html>";

/*
 * table head values which will be filed in at %s
 * given values are:
 * "Last modified" "Type" "Size" "Filename"
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]" "[filetype]" "[filesize]" "[directory]" "[filename]"
 * the buffer limit is defined in defines.h
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
 * plaintext message which will be sent on 403 - Forbidden
 */
const char *RESPONSE_403 = "403 - U better not go down this road!\r\n";

/*
 * plaintext message which will be sent on 404 - File not found
 */
const char *RESPONSE_404 = "404 - Watcha pulling here buddy?\r\n";

/*
 * plaintext message which will be sent on 405 - Method Not Allowed
 */
const char *RESPONSE_405 = "405 - ur Method is not allowed sir, if tried to "
			   "POST, ask for permission.\r\n";

/*
 * html response for http status 201 (file created),
 * needed for POST messages
 */
const char *RESPONSE_201=
"<html>"
	"<center>"
		"<h1>its done!</h1>"
		"<body>"
		"<a href='/'>back to Main page</a><br>"
		"<a href='#' onclick='history.back()'>"
			"load up another file"
		"</a>"
		"</body>"
	"</center>"
"</html>";

#endif
