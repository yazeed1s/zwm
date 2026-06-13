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

#ifndef ZWM_XCB_UTIL_H
#define ZWM_XCB_UTIL_H

#include "type.h"
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xproto.h>

/* clang-format off */
int change_border_attr(xcb_conn_t *conn, xcb_window_t win, uint32_t bcolor, uint32_t bwidth, bool stack);
int change_window_attr(xcb_conn_t *conn, xcb_window_t win, uint32_t attr, const void *val);
int configure_window(xcb_conn_t *conn, xcb_window_t win, uint16_t attr, const void *val);
int set_input_focus(xcb_conn_t *conn, uint8_t revert_to, xcb_window_t win, xcb_timestamp_t time);
int resize_window(xcb_window_t win, uint16_t width, uint16_t height);
int move_window(xcb_window_t win, int16_t x, int16_t y);
int apply_window_geometry(xcb_window_t win, rectangle_t r, uint16_t bw);
int send_configure_notify(xcb_window_t win, rectangle_t r, uint16_t bw);
void raise_window(xcb_window_t win);
void lower_window(xcb_window_t win);
void window_above(xcb_window_t win1, xcb_window_t win2);
void window_below(xcb_window_t win1, xcb_window_t win2);
xcb_atom_t get_atom(char *atom_name, xcb_conn_t *conn);
xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_conn_t *conn);
bool window_exists(xcb_conn_t *conn, xcb_window_t win);
char *win_name(xcb_window_t win);
int check_window_map_state(xcb_window_t win, win_map_state_t state);
xcb_window_t get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win);
void grab_pointer(xcb_window_t win, bool wants_events);
void ungrab_pointer(void);
uint64_t get_time_millis(void);
/* clang-format on */

#endif /* ZWM_XCB_UTIL_H */
