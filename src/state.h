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

#ifndef ZWM_STATE_H
#define ZWM_STATE_H

#include "type.h"
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xproto.h>

/* clang-format off */
extern wm_t                 *wm;
extern monitor_t            *prim_monitor;
extern monitor_t            *curr_monitor;
extern monitor_t            *head_monitor;
extern strut_win_node_t     *strut_windows;
extern xcb_cursor_context_t *cursor_ctx;
extern xcb_window_t          focused_win;
extern xcb_window_t          meta_window;
extern bool                  is_kgrabbed;
extern bool                  using_xrandr;
extern bool                  multi_monitors;
extern bool                  using_xinerama;
extern bool                  ignore_ewmh_struts;
extern config_t              conf;
extern volatile sig_atomic_t should_shutdown;
extern uint8_t               randr_base;
extern uint64_t              last_desk_switch_time;
extern xcb_cursor_t          cursors[CURSOR_MAX];
extern mouse_state_t         mouse_state;
/* clang-format on */

#endif /* ZWM_STATE_H */
