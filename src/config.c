#include "config.h"

/*
 * default values, these get overwritten if specified on startup
 * -------------------------------------------------------------
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
	 * default allowed ip, x disables ip blocking (allows all).
	 */
	.ip = "x",

	/*
	 * default upload IP, - disables upload (allows none)
	 */
	.upload_ip = "-",

	/*
	 * enable coloring
	 */
	.color = 0,

	/*
	 * log file, if != NULL it will be used.
	 */
	.log_file = NULL,

	/*
	 * LOG_FILE descriptor
	 */
	.log_file_d = NULL,
};
