#include "globals.h"

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
 * default upload IP, - disables upload
 */
char UPLOAD_IP[16] = "-";

/*
 * enable coloring
 */
int COLOR = 0;

/*
 * ncurses status and redraw timeout
 */
const float UPDATE_TIMEOUT = 0.2f;

/*
 * log file, if != NULL it will be used.
 */
char *LOG_FILE = NULL;

/*
 * LOG_FILE descriptor
 */
FILE *LOG_FILE_D = NULL;

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
		"html, body, tbody {"
			"background-color: #303030;"
			"color: #888888;"
		"}"
		"th, h3, h2, a:hover {"
			"color: #ffffff;"
		"}"
		"a {"
			"color: #44aaff;"
			"text-decoration: none;"
		"}"
		"table.download {"
			"margin-top: -18px;"
		"}"
		"form.upload {"
			"margin-top: -8px;"
		"}"
		"h3.download, h3.upload {"
			"left: 0;"
			"border-bottom: 1px solid #ffffff;"
			"width: 100%;"
			"padding: 0.2em;"
		"}"
	"</style>"
	"<html>"
		"<head>"
			"<title>file_server version 0.3</title>"
		"</head>"
		"<center>"
		"<table><tbody><tr><td>"
			"<center>"
				"<h2>file_server version 0.3</h2>"
			"</center>"
			"<div align='left'>"
				"<h3 class='download'>Download</h3>"
			"</div>"
		"</td></tr><tr><td>"
			"<body>";

/*
 * if current ip is allowed to upload this will be displayed b4 HTTP_BOT,
 * %s will be replaced with upload directory
 */
const char *HTTP_UPLOAD = "<h3 class='upload'>Upload</h3></a>"
			"<form class='upload' action='%s'"
				     "method='post'"
				     "enctype='multipart/form-data'>"
				"<input type='file' name='file[]' multiple='true'>"
				"<button type='submit'>Upload</button>"
			"</form>";

/*
 * string which will be at the bottom of http table response
 */
const char *HTTP_BOT =
			"</body>"
		"</td></tr></tbody></table>"
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
 * "Last modified" "Size" "Filename"
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]" "[filesize]" "[directory]" "[filename]"
 * the buffer limit is defined in defines.h
 */
const char *TABLE_PLAIN[3] = {
	/* table head */
	"%-19s %-6s %s\n"
	"-----------------------------------\n",
	/* table body */
	"%19s %6s %s/%s%s\n",
	/* table end */
	""};

const char *TABLE_HTML[3] = {
	/* table head */
	"<table class='download'>"
		"<thead>"
			"<tr>"
				"<th align='left'>%s</th>"
				"<th align='left'>%s</th>"
				"<th align='left'>%s</th>"
			"</tr>"
		"</thead>"
		"<tbody>",
	/* table body */
			"<tr>"
				"<td align='center'>%s</td>"
				"<td align='right'>%s</td>"
				"<td align='left'><a href=\"%s/%s\">%4$s%s</a></td>"
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
"<!DOCTYPE html>"
	"<style>"
		"html, body {"
			"background-color: #303030;"
			"color: #888888;"
		"}"
		"h1, a:hover {"
			"color: #ffffff;"
		"}"
		"a {"
			"color: #44aaff;"
			"text-decoration: none;"
		"}"
	"</style>"
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
