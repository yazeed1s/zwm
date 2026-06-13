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

#ifndef ZWM_EWMH_H
#define ZWM_EWMH_H

#include "type.h"
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

/* clang-format off */
bool ewmh_has(ewmh_state_t s, ewmh_state_t f);
bool setup_ewmh(void);
int ewmh_update_current_desktop(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t i);
int ewmh_update_number_of_desktops(void);
int ewmh_update_desktop_names(void);
void ewmh_update_desktop_viewport(void);
void ewmh_update_client_list(void);
int set_active_window_name(xcb_window_t win);
int set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state);
int update_net_wm_desktop(xcb_window_t win, uint32_t desktop);
ewmh_state_t get_net_wm_state_mask(xcb_window_t win);
ewmh_state_t ewmh_flag_for_atom(xcb_atom_t atom);
int update_net_wm_state_atom(xcb_window_t win, xcb_atom_t atom, bool set);
void update_client_ewmh_state(client_t *c, ewmh_state_t flag, bool set);
void fill_icccm_ewmh(client_t *c);
ewmh_window_type_t window_type(xcb_window_t win);
void remove_property(xcb_connection_t *con, xcb_window_t win, xcb_atom_t prop, xcb_atom_t atom);
/* clang-format on */

#endif /* ZWM_EWMH_H */
