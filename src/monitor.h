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

#ifndef ZWM_MONITOR_H
#define ZWM_MONITOR_H

#include "type.h"
#include <stdbool.h>
#include <xcb/randr.h>
#include <xcb/xproto.h>

/* clang-format off */
bool setup_monitors(void);
void free_monitors(void);
void handle_monitor_changes(void);
monitor_t *get_focused_monitor(void);
monitor_t *get_monitor_within_coordinate(int16_t x, int16_t y);
monitor_t *get_monitor_from_desktop(desktop_t *desktop);
monitor_t *get_monitor_by_window(xcb_window_t win);
int get_monitors_count(void);
int handle_unmanaged_strut_window(xcb_window_t win);
void add_strut_window(xcb_window_t win);
bool remove_strut_window(xcb_window_t win);
bool is_strut_window(xcb_window_t win);
void cleanup_strut_windows(void);
bool ewmh_handle_struts(xcb_window_t win);
rectangle_t get_usable_area(monitor_t *m);
void recalculate_all_struts(void);
void reapply_tracked_struts(void);
rectangle_t calculate_monitor_area(const monitor_t *m);
void apply_monitor_layout_changes(monitor_t *m);
void arrange_trees(void);
/* clang-format on */

#endif /* ZWM_MONITOR_H */
