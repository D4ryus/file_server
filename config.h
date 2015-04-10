#ifndef CONFIG_H
#define CONFIG_H

/**
 * default values
 * --------------
 */

/**
 * shared directory if none specified
 */
char *ROOT_DIR = ".";

/**
 * port if none specified
 */
int PORT = 8283;

/**
 * Http table config
 * -----------------
 */

/**
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
                                 "content='text/html;"
                                 "charset=UTF-8'/>"
                "</head>"
                "<body>";
/**
 * string which will be at the bottom of http table response
 */
const char *HTTP_BOT =
                "</body>"
        "</html>";

/**
 * plaintext message which will be sent on 404 - File not found
 */
const char *RESPONSE_404 = "404 - Watcha pulling here buddy?\r\n";

/**
 * plaintext message which will be sent on 403 - Forbidden
 */
const char *RESPONSE_403 = "403 - U better not go down this road!\r\n";

/**
 * the TABLE_BUFFER_SIZE is the size of the buffer where the table contents
 * will be filled in with sprintf(). so if table is getting bigger change value
 * accordingly.
 */
const int TABLE_BUFFER_SIZE = 512;

/**
 * table head values which will be filed in at %s
 * given values are:
 * "Last modified" "Type" "Size" "Filename"
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]" "[filetype]" "[filesize]" "[directory]" "[filename]" "[filename]"
 */
const char *TABLE_PLAIN[3] = {
        /* table head */
        "%-20s %-17s %-12s %s\n",
        /* table body */
        "%20s %17s %12li %s/%s\n",
        /* table end */
        ""};

const char *TABLE_HTML[3] = {
        /* table head */
        "<style>"
                "tbody tr:nth-child(odd) {"
                        "background: #eee;"
                "}"
        "</style>"
        "<table style size='100%'>"
                "<tbody>"
                "<thead>"
                        "<tr>"
                                "<th align='left'>%s</th>"
                                "<th align='left'>%s</th>"
                                "<th align='left'>%s</th>"
                                "<th align='left'>%s</th>"
                        "</tr>"
                "</thead>",
        /* table body */
                "<tr>"
                        "<td align='center'>%s</td>"
                        "<td align='center'>%s</td>"
                        "<td align='right'>%12li</td>"
                        "<td align='left'><a href='%s/%s'>%s</a></td>"
                "</tr>",
        /* table end */
                "</tbody>"
        "</table>"};

/**
 * on new connection BUFFSIZE_READ -1 bytes are read from the socket,
 * everything extending that limit will be thrown away.
 */
const size_t BUFFSIZE_READ  = 2048;

/**
 * if i file is transferd BUFFSIZE_WRITE describes the buffersize of
 * bytes read and then written to the socket.
 */
const size_t BUFFSIZE_WRITE = 8192;

#endif
