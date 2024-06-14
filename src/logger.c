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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>

#define YEAR_OFFSET 1900
#define LOG_PATH	"~/.local/share/xorg/zwm.log"

void
log_message(log_level_t level, const char *format, ...)
{
	struct tm *ptr;
	time_t	   t;
	va_list	   args;
	char	   buf[100];
	memset(buf, 0, sizeof(buf));
	t	= time(NULL);
	ptr = localtime(&t);
	strftime(buf, 100, "%F/%I:%M:%S %p", ptr);
	FILE *log_file = fopen(LOG_PATH, "a");
	va_start(args, format);
	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file for writing\n");
		va_end(args);
		return;
	}
	fprintf(log_file, "%s ", buf);
	switch (level) {
	case ERROR: fprintf(log_file, "[ERROR] "); break;
	case INFO: fprintf(log_file, "[INFO] "); break;
	case DEBUG: fprintf(log_file, "[DEBUG] "); break;
	default: break;
	}
	vfprintf(log_file, format, args);
	fprintf(log_file, "\n");
	fclose(log_file);
	va_end(args);
}

void
log_window_id(xcb_window_t window, const char *message)
{
	log_message(DEBUG, "%s: Window ID: %u", message, window);
}
