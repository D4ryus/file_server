#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

enum http_method {
	RESPONSE = 0,
	HEAD,
	GET,
	POST
};

enum http_status {
	_200_OK,
	_201_Created,
	_206_Partial_Content,
	_403_Forbidden,
	_404_Not_Found,
	_405_Method_Not_Allowed
};

/* sorted and match mime_string */
enum mime {
	APPLICATION_GZIP,
	APPLICATION_MSEXCEL,
	APPLICATION_MSPOWERPOINT,
	APPLICATION_MSWORD,
	APPLICATION_OCTET_STREAM,
	APPLICATION_PDF,
	APPLICATION_POSTSCRIPT,
	APPLICATION_X_COMPRESS,
	APPLICATION_X_CSH,
	APPLICATION_X_DVI,
	APPLICATION_XHTML_XML,
	APPLICATION_X_HTTPD_PHP,
	APPLICATION_X_LATEX,
	APPLICATION_X_SH,
	APPLICATION_X_SHOCKWAVE_FLASH,
	APPLICATION_X_TAR,
	APPLICATION_X_TCL,
	APPLICATION_X_TEX,
	APPLICATION_X_TROFF_MS,
	APPLICATION_ZIP,
	AUDIO_BASIC,
	AUDIO_MP4,
	AUDIO_MPEG,
	AUDIO_OGG,
	AUDIO_X_QT_STREAM,
	AUDIO_X_WAV,
	DRAWING_X_DWF,
	IMAGE_GIF,
	IMAGE_JPEG,
	IMAGE_PNG,
	IMAGE_TIFF,
	IMAGE_X_ICON,
	IMAGE_X_PORTABLE_ANYMAP,
	IMAGE_X_PORTABLE_BITMAP,
	IMAGE_X_PORTABLE_GRAYMAP,
	IMAGE_X_PORTABLE_PIXMAP,
	IMAGE_X_RGB,
	MULTIPART_FORM_DATA,
	TEXT_COMMA_SEPARATED_VALUES,
	TEXT_CSS,
	TEXT_HTML,
	TEXT_JAVASCRIPT,
	TEXT_PLAIN,
	TEXT_RICHTEXT,
	TEXT_RTF,
	TEXT_TAB_SEPARATED_VALUES,
	TEXT_XML,
	VIDEO_MP4,
	VIDEO_MPEG,
	VIDEO_OGG,
	VIDEO_QUICKTIME,
	VIDEO_WEBM,
	VIDEO_X_MSVIDEO,
	VIDEO_X_SGI_MOVIE
};

struct http_header {
	enum http_method method;  /* GET /magic.txt HTTP/1.1 */
	enum http_status status;  /* 200 201, 206, 403, 404, 405 */
	char *url;                /* /magic.txt */
	char *host;               /* Host: somehost.com */
	enum mime content_type;   /* Content-Type: text/plain*/
	char *boundary;           /* boundary=XXXX: */
	uint64_t content_length;  /* Content-Length: 123321 */
	struct {                  /* Content-Range: bytes=FROM-TO/SIZE */
		uint64_t from;
		uint64_t to;
		uint64_t size;
	} range;
	struct {
		/* these default to 0 */
		uint8_t range : 1; /* 1 on Content-Range is present */
		uint8_t keep_alive : 1; /* 1 on Connection: keep-alive */
		uint8_t http : 1; /* 1 on http request */
	} flags;
};

void init_http_header(struct http_header *data);
int parse_header(struct http_header *, int);
char *print_header(struct http_header *);
void delete_http_header(struct http_header *);
enum mime get_mime_type(const char *);

#endif
