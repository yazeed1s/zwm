/*
 * BSD 2-Clause License
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	  1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	  2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "type.h"
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <xcb/xcb.h>

#define LOG_DIR		 "/.local/share/xorg"
#define LOG_FILE	 "zwm.log"
#define MAX_PATH_LEN 2 << 7

void
log_message(log_level_t level, const char *format, ...)
{
	static char full_path[MAX_PATH_LEN] = {0};
	static int	initialized				= false;

	if (!initialized) {
		const char *homedir;
		if ((homedir = getenv("HOME")) == NULL) {
			__uid_t		   id = getuid();
			struct passwd *pw = getpwuid(id);
			if (pw == NULL) {
				fprintf(stderr, "Failed to get home directory\n");
				return;
			}
			homedir = pw->pw_dir;
		}

		snprintf(full_path,
				 sizeof(full_path),
				 "%s%s/%s",
				 homedir,
				 LOG_DIR,
				 LOG_FILE);
		initialized = true;
	}

	struct tm *ptr;
	time_t	   t;
	va_list	   args;
	char	   buf[100];

	t	= time(NULL);
	ptr = localtime(&t);
	strftime(buf, sizeof(buf), "%F/%I:%M:%S %p", ptr);

	FILE *log_file = fopen(full_path, "a");
	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file for writing\n");
		return;
	}

	fprintf(log_file, "%s ", buf);
	switch (level) {
	case ERROR: fprintf(log_file, "[ERROR] "); break;
	case INFO: fprintf(log_file, "[INFO] "); break;
	case DEBUG: fprintf(log_file, "[DEBUG] "); break;
	default: break;
	}

	va_start(args, format);
	vfprintf(log_file, format, args);
	va_end(args);

	fprintf(log_file, "\n");
	fclose(log_file);
}

void
log_window_id(xcb_window_t window, const char *message)
{
	log_message(DEBUG, "%s: Window ID: %u", message, window);
}
