//
// Created by yaz on 12/31/23.
//

#include "type.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

void log_message(log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    FILE *log_file = fopen("zwm.log", "a");
    if (log_file == NULL) {
        fprintf(stderr, "Failed to open log file for writing\n");
        va_end(args);
        return;
    }
    fprintf(log_file, "[");
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
}

void log_window_id(xcb_window_t window, const char *message) {
    log_message(DEBUG, "%s: Window ID: %u", message, window);
}