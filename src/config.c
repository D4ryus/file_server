#include "config.h"

/*
 * default values, these get overwritten if specified on startup
 * ------------------------------------------------------------
 */

struct config CONF = {
	/*
	 * shared directory if none specified
	 */
	.root_dir = ".",

	/*
	 * port if none specified
	 */
	.port = 8283,

	/*
	 * default verbosity
	 */
	.verbosity = 0,

	/*
	 * default allowed ip, * disables ip blocking.
	 */
	.ip = "*",

	/*
	 * default upload IP, - disables upload
	 */
	.upload_ip = "-",

	/*
	 * enable coloring
	 */
	.color = 0,

	/*
	 * ncurses status timeout
	 */
	.update_timeout = 0.2f,

	/*
	 * log file, if != NULL it will be used.
	 */
	.log_file = NULL,

	/*
	 * LOG_FILE descriptor
	 */
	.log_file_d = NULL,
};
