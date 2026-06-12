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

#ifndef ZWM_MOUSE_H
#define ZWM_MOUSE_H

#include "type.h"
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xproto.h>

/* clang-format off */
void clear_mouse_state(void);
bool grab_pointer_for_mouse(cursor_t cursor_id);
double clamp_ratio(double ratio);
uint8_t detect_resize_edges(rectangle_t r, int16_t x, int16_t y);
bool is_resize_band_hit(node_t *parent, split_type_t split_type, int16_t x, int16_t y);
bool start_floating_move(node_t *n, int16_t x, int16_t y);
bool start_floating_resize(node_t *n, int16_t x, int16_t y);
bool start_tiled_resize(node_t *n, int16_t x, int16_t y);
void handle_mouse_motion(int16_t x, int16_t y);
void finish_mouse_action(void);
void cancel_mouse_action(void);
void window_grab_buttons(xcb_window_t win);
void window_ungrab_buttons(xcb_window_t win);
void ungrab_buttons_for_all(node_t *n);
/* clang-format on */

#endif /* ZWM_MOUSE_H */
