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
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "focus.h"
#include "helper.h"
#include "mouse.h"
#include "state.h"
#include "xcb_util.h"
#include <xcb/xcb.h>

int
fullscreen_focus(xcb_window_t win)
{
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor	   = 0;
	uint32_t bwidth	   = 0;

	/* if window is viewable before attempting focus */
	if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
		return 0;
	}

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) != 0) {
		_LOG_(ERROR, "cannot set input focus");
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

int
win_focus(xcb_window_t win, bool set_focus)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[WIN_FOCUS] win=%d name='%s' set_focus=%s",
		  win,
		  name ? name : "(null)",
		  set_focus ? "TRUE" : "FALSE");
	_FREE_(name);
#endif
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor =
		set_focus ? conf.active_border_color : conf.normal_border_color;
	uint32_t bwidth = conf.border_width;

	/* check if window is viewable before attempting focus operations */
	if (set_focus) {
		if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
			return 0;
		}
	}

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_focus) {
		if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) !=
			0) {
			_LOG_(ERROR, "cannot set input focus");
			return -1;
		}
	}

	xcb_flush(wm->connection);
	return 0;
}

int
set_focus(node_t *n, bool flag)
{
#ifdef _DEBUG__
	char *name = (n && n->client) ? win_name(n->client->window) : NULL;
	_LOG_(DEBUG,
		  "[SET_FOCUS] set_focus called: win=%d name='%s' flag=%s state=%s",
		  (n && n->client) ? n->client->window : 0,
		  name ? name : "(null)",
		  flag ? "TRUE" : "FALSE",
		  (n && n->client && IS_FLOATING(n->client)) ? "FLOATING" : "TILED");
	_FREE_(name);
#endif
	n->is_focused = flag;

	/* skip focus attempt if trying to set focus on unmapped window */
	if (flag) {
		if (!check_window_map_state(n->client->window,
									WIN_MAP_STATE_VIEWABLE)) {
			return 0; /* Not an error just skip focusing */
		}
	}

	if (win_focus(n->client->window, flag) != 0) {
		_LOG_(ERROR, "cannot set focus");
		return -1;
	}

	return 0;
}

void
update_grabbed_window(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client;
	if (flag && root != n) {
		set_focus(root, false);
		window_grab_buttons(root->client->window);
	}

	update_grabbed_window(root->first_child, n);
	update_grabbed_window(root->second_child, n);
}
