#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "helper.h"
#include "config.h"

// BUFFSIZE_WRITE bytes are written to socket
#define BUFFSIZE_WRITE 8192

struct data_store*
create_data_store(void)
{
        struct data_store *data;

        data = err_malloc(sizeof(struct data_store));

        data->port = -1;
        data->socket = -1;
        data->url[0] = '\0';
        data->head = err_malloc(sizeof(char));
        data->head[0] = '\0';
        data->body = err_malloc(sizeof(char));
        data->body[0] = '\0';
        data->body_length = 0;
        data->req_type = TEXT;
        data->body_type = PLAIN;
        data->color = NULL;

        return data;
}

void
free_data_store(struct data_store *data)
{
        if (data == NULL) {
                return;
        }
        if (data->color != NULL) {
                (*data->color) = 0;
        }
        if (data->head != NULL) {
                free(data->head);
        }
        if (data->body != NULL) {
                free(data->body);
        }
        free(data);

        return;
}

int
send_text(int socket, char* text, size_t length)
{
        ssize_t write_res;
        int     sending;
        size_t  sent;

        write_res = 0;
        sent = 0;
        sending = 1;

        while (sending) {
                write_res = write(socket, text + sent, length - sent);
                if (write_res == -1) {
                        return WRITE_CLOSED;
                } else if (write_res == 0) {
                        return ZERO_WRITTEN;
                }
                sent = sent + (size_t)write_res;
                if (sent != length) {
                        continue;
                }
                sending = 0;
        }

        return OK;
}

int
send_file(struct data_store *data)
{
        ssize_t write_res;
        int     sending;
        size_t  read;
        size_t  sent;
        size_t  written;
        size_t  last_written;
        char    buffer[BUFFSIZE_WRITE];
        FILE    *f;
        time_t  last_time;
        time_t  current_time;
        char    message_buffer[256];
        enum err_status ret_status;

        f = fopen(data->body, "rb");
        if (f == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "fopen() retuned NULL");
        }

        write_res = 0;
        last_time = time(NULL);
        sending = 1;
        written = 0;
        last_written = 0;
        ret_status = OK;

        while (sending) {
                read = fread(buffer, 1, BUFFSIZE_WRITE, f);
                if (read < BUFFSIZE_WRITE) {
                        sending = 0;
                }
                sent = 0;
                while (sent < read) {
                        write_res = write(data->socket, buffer + sent, read - sent);
                        if (write_res == -1) {
                                sending = 0;
                                ret_status = WRITE_CLOSED;
                                break;
                        } else if (write_res == 0) {
                                sending = 0;
                                ret_status = ZERO_WRITTEN;
                                break;
                        }
                        sent = sent + (size_t)write_res;
                }
                written += sent;
                current_time = time(NULL);
                if ((current_time - last_time) > 1) {
                        sprintf(message_buffer, "%-20s size: %12lub written: %12lub remaining: %12lub %12lub/s %3lu%%",
                                             data->body + strlen(ROOT_DIR),
                                             data->body_length,
                                             written,
                                             data->body_length - written,
                                             (written - last_written) / (!sending ? 1 : (size_t)(current_time - last_time)),
                                             written * 100 / data->body_length);
                        print_info(data, "requested", message_buffer);
                        last_time = current_time;
                        last_written = written;
                }
        }
        fclose(f);

        return ret_status;
}

void
print_info(struct data_store *data, char *type, char *message)
{
        if (data->color == NULL) {
                printf("[%15s:%-5d - %3d]: %-10s - %s\n",
                                data->ip,
                                data->port,
                                data->socket,
                                type,
                                message);
        } else {
                printf("\x1b[3%dm[%15s:%-5d - %3d]: %-10s - %s\x1b[39;49m\n",
                                (*data->color),
                                data->ip,
                                data->port,
                                data->socket,
                                type,
                                message);
        }
}

char*
concat(char* dst, const char* src)
{
        dst = err_realloc(dst, strlen(dst) + strlen(src) + 1);
        strncat(dst, src, strlen(src));

        return dst;
}

/**
 * checks if given string is a directory, if its a file 0 is returned
 */
int
is_directory(const char *path)
{
        struct stat s;

        if (stat(path, &s) == 0) {
                if (s.st_mode & S_IFDIR) {
                        return 1;
                } else if (s.st_mode & S_IFREG) {
                        return 0;
                } else {
                        err_quit(__FILE__, __LINE__, __func__, "stat() has no file nor a directory");
                }
        } else {
                err_quit(__FILE__, __LINE__, __func__, "stat() retuned -1");
        }

        return 0;
}

int
starts_with(const char *line, const char *prefix)
{
        if (line == NULL || prefix == NULL) {
                return 0;
        }
        while (*prefix) {
                if (*prefix++ != *line++) {
                        return 0;
                }
        }

        return 1;
}

void*
file_to_buf(const char *file, size_t *length)
{
        FILE *fptr;
        char *buf;

        fptr = fopen(file, "rb");
        if (!fptr) {
                return NULL;
        }
        fseek(fptr, 0, SEEK_END);
        *length = (size_t)ftell(fptr);
        buf = (void*)err_malloc(*length + 1);
        fseek(fptr, 0, SEEK_SET);
        fread(buf, (*length), 1, fptr);
        fclose(fptr);
        buf[*length] = '\0';

        return buf;
}

void
*err_malloc(size_t size)
{
        void *tmp;

        tmp = malloc(size);
        if (tmp == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "could not malloc");
        }

        return tmp;
}

void
*err_realloc(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
        if (ptr == NULL) {
                err_quit(__FILE__, __LINE__, __func__, "could not realloc");
        }

        return ptr;
}

void
err_quit(const char *file, const int line, const char *function, const char *msg)
{
        fprintf(stderr, "%s:%d:%s: error: ", file, line, function);
        perror(msg);
        exit(1);
}

char*
get_content_encoding(char* type)
{
        if (type == NULL || !strcmp(type, "")) {
                return "application/octet-stream";
        } else if (!strcmp(type, "txt")) {
                return "text/plain";
        } else if (!strcmp(type, "htm") || !strcmp(type, "html") || !strcmp(type, "shtml")) {
                return "text/html";
        } else if (!strcmp(type, "jpeg") || !strcmp(type, "jpg") || !strcmp(type, "jpe")) {
                return "image/jpeg";
        } else if (!strcmp(type, "png")) {
                return "image/png";
        } else if (!strcmp(type, "gif")) {
                return "image/gif";
        } else if (!strcmp(type, "tar")) {
                return "application/x-tar";
        } else if (!strcmp(type, "gz")) {
                return "application/gzip";
        } else if (!strcmp(type, "zip")) {
                return "application/zip";
        } else if (!strcmp(type, "mpeg") || !strcmp(type, "mpg") || !strcmp(type, "mpe")) {
                return "video/mpeg";
        } else if (!strcmp(type, "dwg")) {
                return "application/acad";
        } else if (!strcmp(type, "asd") || !strcmp(type, "asn")) {
                return "application/astound";
        } else if (!strcmp(type, "tsp")) {
                return "application/dsptype";
        } else if (!strcmp(type, "dxf")) {
                return "application/dxf";
        } else if (!strcmp(type, "spl")) {
                return "application/futuresplash";
        } else if (!strcmp(type, "ptlk")) {
                return "application/listenup";
        } else if (!strcmp(type, "hqx")) {
                return "application/mac-binhex40";
        } else if (!strcmp(type, "mbd")) {
                return "application/mbedlet";
        } else if (!strcmp(type, "mif")) {
                return "application/mif";
        } else if (!strcmp(type, "xls") || !strcmp(type, "xla")) {
                return "application/msexcel";
        } else if (!strcmp(type, "hlp") || !strcmp(type, "chm")) {
                return "application/mshelp";
        } else if (!strcmp(type, "ppt") || !strcmp(type, "ppz") || !strcmp(type, "pps") || !strcmp(type, "pot")) {
                return "application/mspowerpoint";
        } else if (!strcmp(type, "doc") || !strcmp(type, "dot")) {
                return "application/msword";
        } else if (!strcmp(type, "bin") || !strcmp(type, "exe") || !strcmp(type, "com") || !strcmp(type, "dll") || !strcmp(type, "class")) {
                return "application/octet-stream";
        } else if (!strcmp(type, "oda")) {
                return "application/oda";
        } else if (!strcmp(type, "pdf")) {
                return "application/pdf";
        } else if (!strcmp(type, "ai") || !strcmp(type, "eps") || !strcmp(type, "ps")) {
                return "application/postscript";
        } else if (!strcmp(type, "rtc")) {
                return "application/rtc";
        } else if (!strcmp(type, "rtf")) {
                return "application/rtf";
        } else if (!strcmp(type, "smp")) {
                return "application/studiom";
        } else if (!strcmp(type, "tbk")) {
                return "application/toolbook";
        } else if (!strcmp(type, "vmd")) {
                return "application/vocaltec-media-desc";
        } else if (!strcmp(type, "vmf")) {
                return "application/vocaltec-media-file";
        } else if (!strcmp(type, "htm") || !strcmp(type, "html") || !strcmp(type, "shtml") || !strcmp(type, "xhtml")) {
                return "application/xhtml+xml";
        } else if (!strcmp(type, "xml")) {
                return "application/xml";
        } else if (!strcmp(type, "bcpio")) {
                return "application/x-bcpio";
        } else if (!strcmp(type, "z")) {
                return "application/x-compress";
        } else if (!strcmp(type, "cpio")) {
                return "application/x-cpio";
        } else if (!strcmp(type, "csh")) {
                return "application/x-csh";
        } else if (!strcmp(type, "dcr") || !strcmp(type, "dir") || !strcmp(type, "dxr")) {
                return "application/x-director";
        } else if (!strcmp(type, "dvi")) {
                return "application/x-dvi";
        } else if (!strcmp(type, "evy")) {
                return "application/x-envoy";
        } else if (!strcmp(type, "gtar")) {
                return "application/x-gtar";
        } else if (!strcmp(type, "hdf")) {
                return "application/x-hdf";
        } else if (!strcmp(type, "php") || !strcmp(type, "phtml")) {
                return "application/x-httpd-php";
        } else if (!strcmp(type, "js")) {
                return "application/x-javascript";
        } else if (!strcmp(type, "latex")) {
                return "application/x-latex";
        } else if (!strcmp(type, "bin")) {
                return "application/x-macbinary";
        } else if (!strcmp(type, "mif")) {
                return "application/x-mif";
        } else if (!strcmp(type, "nc") || !strcmp(type, "cdf")) {
                return "application/x-netcdf";
        } else if (!strcmp(type, "nsc")) {
                return "application/x-nschat";
        } else if (!strcmp(type, "sh")) {
                return "application/x-sh";
        } else if (!strcmp(type, "shar")) {
                return "application/x-shar";
        } else if (!strcmp(type, "swf") || !strcmp(type, "cab")) {
                return "application/x-shockwave-flash";
        } else if (!strcmp(type, "spr") || !strcmp(type, "sprite")) {
                return "application/x-sprite";
        } else if (!strcmp(type, "sit")) {
                return "application/x-stuffit";
        } else if (!strcmp(type, "sca")) {
                return "application/x-supercard";
        } else if (!strcmp(type, "sv4cpio")) {
                return "application/x-sv4cpio";
        } else if (!strcmp(type, "sv4crc")) {
                return "application/x-sv4crc";
        } else if (!strcmp(type, "tcl")) {
                return "application/x-tcl";
        } else if (!strcmp(type, "tex")) {
                return "application/x-tex";
        } else if (!strcmp(type, "texinfo") || !strcmp(type, "texi")) {
                return "application/x-texinfo";
        } else if (!strcmp(type, "t") || !strcmp(type, "tr") || !strcmp(type, "roff")) {
                return "application/x-troff";
        } else if (!strcmp(type, "man") || !strcmp(type, "troff")) {
                return "application/x-troff-man";
        } else if (!strcmp(type, "me") || !strcmp(type, "troff")) {
                return "application/x-troff-me";
        } else if (!strcmp(type, "ms") || !strcmp(type, "troff")) {
                return "application/x-troff-ms";
        } else if (!strcmp(type, "ustar")) {
                return "application/x-ustar";
        } else if (!strcmp(type, "src")) {
                return "application/x-wais-source";
        } else if (!strcmp(type, "au") || !strcmp(type, "snd")) {
                return "audio/basic";
        } else if (!strcmp(type, "es")) {
                return "audio/echospeech";
        } else if (!strcmp(type, "tsi")) {
                return "audio/tsplayer";
        } else if (!strcmp(type, "vox")) {
                return "audio/voxware";
        } else if (!strcmp(type, "aif") || !strcmp(type, "aiff") || !strcmp(type, "aifc")) {
                return "audio/x-aiff";
        } else if (!strcmp(type, "dus") || !strcmp(type, "cht")) {
                return "audio/x-dspeeh";
        } else if (!strcmp(type, "mid") || !strcmp(type, "midi")) {
                return "audio/x-midi";
        } else if (!strcmp(type, "mp2")) {
                return "audio/x-mpeg";
        } else if (!strcmp(type, "ram") || !strcmp(type, "ra")) {
                return "audio/x-pn-realaudio";
        } else if (!strcmp(type, "rpm")) {
                return "audio/x-pn-realaudio-plugin";
        } else if (!strcmp(type, "stream")) {
                return "audio/x-qt-stream";
        } else if (!strcmp(type, "wav")) {
                return "audio/x-wav";
        } else if (!strcmp(type, "dwf")) {
                return "drawing/x-dwf";
        } else if (!strcmp(type, "cod")) {
                return "image/cis-cod";
        } else if (!strcmp(type, "ras")) {
                return "image/cmu-raster";
        } else if (!strcmp(type, "fif")) {
                return "image/fif";
        } else if (!strcmp(type, "ief")) {
                return "image/ief";
        } else if (!strcmp(type, "tiff") || !strcmp(type, "tif")) {
                return "image/tiff";
        } else if (!strcmp(type, "mcf")) {
                return "image/vasa";
        } else if (!strcmp(type, "wbmp")) {
                return "image/vnd.wap.wbmp";
        } else if (!strcmp(type, "fh4") || !strcmp(type, "fh5") || !strcmp(type, "fhc")) {
                return "image/x-freehand";
        } else if (!strcmp(type, "ico")) {
                return "image/x-icon";
        } else if (!strcmp(type, "pnm")) {
                return "image/x-portable-anymap";
        } else if (!strcmp(type, "pbm")) {
                return "image/x-portable-bitmap";
        } else if (!strcmp(type, "pgm")) {
                return "image/x-portable-graymap";
        } else if (!strcmp(type, "ppm")) {
                return "image/x-portable-pixmap";
        } else if (!strcmp(type, "rgb")) {
                return "image/x-rgb";
        } else if (!strcmp(type, "xwd")) {
                return "image/x-windowdump";
        } else if (!strcmp(type, "xbm")) {
                return "image/x-xbitmap";
        } else if (!strcmp(type, "xpm")) {
                return "image/x-xpixmap";
        } else if (!strcmp(type, "wrl")) {
                return "model/vrml";
        } else if (!strcmp(type, "csv")) {
                return "text/comma-separated-values";
        } else if (!strcmp(type, "css")) {
                return "text/css";
        } else if (!strcmp(type, "js")) {
                return "text/javascript";
        } else if (!strcmp(type, "rtx")) {
                return "text/richtext";
        } else if (!strcmp(type, "rtf")) {
                return "text/rtf";
        } else if (!strcmp(type, "tsv")) {
                return "text/tab-separated-values";
        } else if (!strcmp(type, "wml")) {
                return "text/vnd.wap.wml";
        } else if (!strcmp(type, "wmlc")) {
                return "application/vnd.wap.wmlc";
        } else if (!strcmp(type, "wmls")) {
                return "text/vnd.wap.wmlscript";
        } else if (!strcmp(type, "wmlsc")) {
                return "application/vnd.wap.wmlscriptc";
        } else if (!strcmp(type, "xml")) {
                return "text/xml";
        } else if (!strcmp(type, "etx")) {
                return "text/x-setext";
        } else if (!strcmp(type, "sgm") || !strcmp(type, "sgml")) {
                return "text/x-sgml";
        } else if (!strcmp(type, "talk") || !strcmp(type, "spc")) {
                return "text/x-speech";
        } else if (!strcmp(type, "qt") || !strcmp(type, "mov")) {
                return "video/quicktime";
        } else if (!strcmp(type, "viv") || !strcmp(type, "vivo")) {
                return "video/vnd.vivo";
        } else if (!strcmp(type, "avi")) {
                return "video/x-msvideo";
        } else if (!strcmp(type, "movie")) {
                return "video/x-sgi-movie";
        } else if (!strcmp(type, "vts") || !strcmp(type, "vtts")) {
                return "workbook/formulaone";
        } else if (!strcmp(type, "3dmf") || !strcmp(type, "3dm") || !strcmp(type, "qd3d") || !strcmp(type, "qd3")) {
                return "x-world/x-3dmf";
        } else if (!strcmp(type, "wrl")) {
                return "x-world/x-vrml";
        } else {
                return "text/plain";
        }
}
