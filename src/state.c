/*
 * BSD 2-Clause License
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	1. Redistributions of source code must retain the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer.
 *
 *	2. Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "state.h"

wm_t				 *wm			 = NULL;
monitor_t			 *prim_monitor	 = NULL;
monitor_t			 *curr_monitor	 = NULL;
monitor_t			 *head_monitor	 = NULL;
strut_win_node_t	 *strut_windows	 = NULL;
xcb_cursor_context_t *cursor_ctx	 = NULL;
xcb_window_t		  focused_win	 = XCB_NONE;
xcb_window_t		  meta_window	 = XCB_NONE;
bool				  is_kgrabbed	 = false;
bool				  using_xrandr	 = false;
bool				  multi_monitors = false;
bool				  using_xinerama = false;
bool ignore_ewmh_struts = false; /* this is hardcoded for now, plan on making it
									configurable via IPC or config file */
config_t			  conf					= {0};
volatile sig_atomic_t should_shutdown		= 0;
uint8_t				  randr_base			= 0;
uint64_t			  last_desk_switch_time = 0;
xcb_cursor_t		  cursors[CURSOR_MAX];
mouse_state_t		  mouse_state = {0};
