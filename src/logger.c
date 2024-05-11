#include "type.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <xcb/xcb.h>

#define YEAR_OFFSET 1900

void
log_message(log_level_t level, const char *format, ...)
{
	struct tm *ptr;
	time_t	   t;
	va_list	   args;
	char	   buf[100];
	t	= time(NULL);
	ptr = localtime(&t);
	strftime(buf, 100, "%F - %I:%M:%S %p", ptr);
	FILE *log_file = fopen("zwm.log", "a");
	va_start(args, format);
	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file for writing\n");
		va_end(args);
		return;
	}
	fprintf(log_file, "%s [", buf);
	switch (level) {
	case ERROR: fprintf(log_file, "ERROR"); break;
	case INFO: fprintf(log_file, "INFO"); break;
	case DEBUG: fprintf(log_file, "DEBUG"); break;
	default: break;
	}
	fprintf(log_file, "] ");
	vfprintf(log_file, format, args);
	fprintf(log_file, "\n");
	fclose(log_file);
	va_end(args);
	/* free(ptr); */
}

void
log_window_id(xcb_window_t window, const char *message)
{
	log_message(DEBUG, "%s: Window ID: %u", message, window);
}
