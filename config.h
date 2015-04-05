/**
 * table head values which will be filed in at %s
 * given values are:
 * "Last modified"
 * "Type"
 * "Size"
 * "Filename"
 */

/**
 * table body values which will be filed in at %s
 * the body part will be repeaded for each file found inside the directory
 * given values are:
 * "[last modified]"
 * "[filetype]"
 * "[filesize]"
 * "[directory]"
 * "[filename]"
 * "[filename]"
 */

/**
 * the TABLE_BUFFER_SIZE is the size of the buffer where the table contents
 * will be filled in with sprintf(). so if table is getting bigger change value
 * accordingly.
 */
const int TABLE_BUFFER_SIZE = 512;

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
