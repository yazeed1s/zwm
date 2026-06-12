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

#ifndef ZWM_CLIENT_H
#define ZWM_CLIENT_H

#include "type.h"
#include <stdbool.h>
#include <xcb/xproto.h>

/* clang-format off */
client_t *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn);
void get_class_name(xcb_window_t win, char **out);
void get_wm_name(xcb_window_t win, char **out);
bool should_manage(xcb_window_t win, xcb_conn_t *conn);
int apply_floating_hints(xcb_window_t win);
bool should_ignore_hints(xcb_window_t win, const char *name);
bool is_transient(xcb_window_t win);
bool supports_protocol(xcb_window_t win, xcb_atom_t atom, xcb_conn_t *conn);
bool client_exist_in_desktops(xcb_window_t win);
void find_window_in_desktops(desktop_t **curr_desktop, node_t **curr_node, xcb_window_t win, bool *found);
node_t *find_node_global(xcb_window_t win);
int find_desktop_by_window(xcb_window_t win);
int handle_first_window(client_t *client, desktop_t *d);
int handle_subsequent_window(client_t *client, desktop_t *d);
int handle_floating_window(client_t *client, desktop_t *d);
int insert_into_desktop(int idx, xcb_window_t win, bool is_tiled);
int handle_tiled_window_request(xcb_window_t win, desktop_t *d);
int handle_floating_window_request(xcb_window_t win, desktop_t *d);
int kill_window(xcb_window_t win);
int close_or_kill(xcb_window_t win);
void map_floating(xcb_window_t x);
int display_client(rectangle_t r, xcb_window_t win);
int handle_net_wm_desktop(xcb_window_t win, uint32_t index);
int set_visibility(xcb_window_t win, bool is_visible);
int set_desktop_visibility(xcb_window_t win, bool is_visible);
/* clang-format on */

#endif /* ZWM_CLIENT_H */
