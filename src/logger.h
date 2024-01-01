#ifndef ZWM_LOGGER_H
#define ZWM_LOGGER_H

#include "type.h"
void log_message(log_level_t level, const char *format, ...);
void log_window_id(xcb_window_t window, const char *message);
#endif // ZWM_LOGGER_H
