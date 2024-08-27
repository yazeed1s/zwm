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

#ifndef ZWM_ZWM_H
#define ZWM_ZWM_H

#include "type.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

// clang-format off
extern config_t 		  conf;
extern wm_t 			  *wm;
extern xcb_window_t 	  focused_win;
extern monitor_t 		  *head_monitor;
extern monitor_t		  *prim_monitor;
extern monitor_t 		  *cur_monitor;
extern bool 			  using_xrandr;
extern bool 		      using_xinerama;
extern uint8_t 			  randr_base;

xcb_window_t get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win);
void 		 raise_window(xcb_window_t win);
void 		 lower_window(xcb_window_t win);
void         window_grab_buttons(xcb_window_t win);
void 		 window_above(xcb_window_t, xcb_window_t);
void 		 window_below(xcb_window_t, xcb_window_t);
void         grab_pointer(xcb_window_t, bool);
void         ungrab_pointer(void);
int          set_visibility(xcb_window_t win, bool is_visible);
int          exec_process(arg_t *arg);
int 	     layout_handler(arg_t *arg);
int    	     cycle_win_wrapper(arg_t *arg);
int  		 set_fullscreen_wrapper();
int 		 flip_node_wrapper();
int          reload_config_wrapper();
int          get_focused_desktop_idx();
int 		 switch_desktop_wrapper(arg_t *arg);
int 		 gap_handler(arg_t *arg);
int 		 cycle_desktop_wrapper(arg_t *arg);
int 	     close_or_kill_wrapper();
int 		 traverse_stack_wrapper(arg_t *arg);
int 		 shift_floating_window(arg_t *arg);
int          tile(node_t *node);
int 		 set_focus(node_t *n, bool flag);
int 		 swap_node_wrapper();
int          change_state(arg_t *arg);
char        *win_name(xcb_window_t);
#endif // ZWM_ZWM_H
