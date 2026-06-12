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

#include "xcb_util.h"
#include "helper.h"
#include "state.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

uint64_t
get_time_millis(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

static int16_t
get_cursor_axis(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return -1;
	}

	int16_t x = p_reply->root_x;
	_FREE_(p_reply);

	return x;
}

static void
log_children(xcb_conn_t *conn, xcb_window_t root_window)
{
	xcb_query_tree_cookie_t tree_cookie = xcb_query_tree(conn, root_window);
	xcb_query_tree_reply_t *tree_reply =
		xcb_query_tree_reply(conn, tree_cookie, NULL);
	if (tree_reply == NULL) {
		_LOG_(ERROR, "failed to query tree reply");
		return;
	}

	_LOG_(DEBUG, "children of root window:");
	xcb_window_t *children	   = xcb_query_tree_children(tree_reply);
	const int	  num_children = xcb_query_tree_children_length(tree_reply);
	for (int i = 0; i < num_children; ++i) {
		xcb_icccm_get_text_property_reply_t t_reply;
		xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(conn, children[i]);
		uint8_t wr = xcb_icccm_get_wm_name_reply(conn, cn, &t_reply, NULL);
		if (wr == 1) {
			_LOG_(DEBUG, "child %d: %s", i + 1, t_reply.name);
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		} else {
			_LOG_(DEBUG, "failed to get window name for child %d", i + 1);
		}
	}

	_FREE_(tree_reply);
}

#define WINDOW_X (XCB_CONFIG_WINDOW_X)
#define WINDOW_Y (XCB_CONFIG_WINDOW_Y)
#define WINDOW_W (XCB_CONFIG_WINDOW_WIDTH)
#define WINDOW_H (XCB_CONFIG_WINDOW_HEIGHT)
#define MOVE	 (WINDOW_X | WINDOW_Y)
#define RESIZE	 (WINDOW_W | WINDOW_H)

/* caller must free */
char *
win_name(xcb_window_t win)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr != 1)
		return NULL;

	char *str = (char *)malloc(t_reply.name_len + 1);
	if (str == NULL)
		return NULL;

	strncpy(str, (char *)t_reply.name, t_reply.name_len);
	str[t_reply.name_len] = '\0';
	xcb_icccm_get_text_property_reply_wipe(&t_reply);

	return str;
}

int
check_window_map_state(xcb_window_t win, win_map_state_t state)
{
	xcb_get_window_attributes_cookie_t attr_cookie =
		xcb_get_window_attributes(wm->connection, win);
	xcb_get_window_attributes_reply_t *attr =
		xcb_get_window_attributes_reply(wm->connection, attr_cookie, NULL);

	if (attr == NULL) {
		return 0;
	}

	if (state == WIN_MAP_STATE_ANY) {
		_FREE_(attr);
		return 1;
	}

	int matched = (attr->map_state == (uint8_t)state);
	_FREE_(attr);
	return matched;
}

void
window_above(xcb_window_t win1, xcb_window_t win2)
{
	if (win1 == wm->root_window || win2 == XCB_NONE)
		return;

	if (win2 == wm->root_window)
		return;

	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_ABOVE};
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

void
window_below(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_BELOW};
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

void
lower_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_BELOW};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

void
raise_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_ABOVE};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

xcb_get_geometry_reply_t *
get_geometry(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(conn, win);
	xcb_error_t				 *err;
	xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(conn, gc, &err);
	if (err) {
		_LOG_(ERROR,
			  "error getting geometry for window %u: %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return NULL;
	}

	if (gr == NULL) {
		_LOG_(ERROR, "failed to get geometry for window %u", win);
		return NULL;
	}
	return gr;
}

int
resize_window(xcb_window_t win, uint16_t width, uint16_t height)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t values[] = {width, height};
	xcb_cookie_t   cookie =
		xcb_configure_window_checked(wm->connection, win, RESIZE, values);

	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error resizing window (ID %u): %s",
			  win,
			  strerror(err->error_code));
		_FREE_(err);
		return -1;
	}

	return 0;
}

int
move_window(xcb_window_t win, int16_t x, int16_t y)
{
	if (win == 0 || win == XCB_NONE) {
		return 0;
	}

	const uint32_t values[] = {x, y};
	xcb_cookie_t   cookie =
		xcb_configure_window_checked(wm->connection, win, MOVE, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR, "error moving window (ID %u): %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

int
send_configure_notify(xcb_window_t win, rectangle_t r, uint16_t bw)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	xcb_configure_notify_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.response_type	  = XCB_CONFIGURE_NOTIFY;
	ev.event			  = win;
	ev.window			  = win;
	ev.above_sibling	  = XCB_NONE;
	ev.x				  = r.x;
	ev.y				  = r.y;
	ev.width			  = r.width;
	ev.height			  = r.height;
	ev.border_width	  = bw;
	ev.override_redirect = false;

	xcb_cookie_t c = xcb_send_event_checked(wm->connection,
											false,
											win,
											XCB_EVENT_MASK_STRUCTURE_NOTIFY,
											(const char *)&ev);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "error sending configure notify for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

int
apply_window_geometry(xcb_window_t win, rectangle_t r, uint16_t bw)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t values[] = {r.x, r.y, r.width, r.height};
	xcb_cookie_t   cookie	= xcb_configure_window_checked(
		  wm->connection, win, MOVE | RESIZE, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error configuring window (ID %u): %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return send_configure_notify(win, r, bw);
}

int
change_border_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		bcolor,
				   uint32_t		bwidth,
				   bool			stack)
{

	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t stack_	   = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;

	if (change_window_attr(conn, win, bpx_width, &bcolor) != 0) {
		return -1;
	}

	if (configure_window(conn, win, b_width, &bwidth) != 0) {
		return -1;
	}

	if (stack) {
		const uint16_t arg[1] = {XCB_STACK_MODE_ABOVE};
		if (configure_window(conn, win, stack_, arg) != 0) {
			return -1;
		}

		if (set_input_focus(conn, input, win, XCB_CURRENT_TIME) != 0) {
			return -1;
		}
	}

	xcb_flush(conn);
	return 0;
}

int
change_window_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		attr,
				   const void  *val)
{
	xcb_cookie_t attr_cookie =
		xcb_change_window_attributes_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, attr_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to change window attributes: error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

int
configure_window(xcb_conn_t *conn, xcb_window_t win, uint16_t attr, const void *val)
{
	xcb_cookie_t config_cookie =
		xcb_configure_window_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, config_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to configure window : error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

int
set_input_focus(xcb_conn_t	  *conn,
				uint8_t		   revert_to,
				xcb_window_t   win,
				xcb_timestamp_t time)
{
	/* if window is viewable before attempting to set focus */
	if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
		return -1;
	}

	xcb_cookie_t focus_cookie =
		xcb_set_input_focus_checked(conn, revert_to, win, time);
	xcb_error_t *err = xcb_request_check(conn, focus_cookie);
	if (err) {
		char *n = win_name(win);
		_LOG_(ERROR,
			  "failed to set input focus for win %d name %s : error code %d "
			  "error message %s",
			  win,
			  n,
			  err->error_code,
			  strerror(err->error_code));
		_FREE_(err);
		_FREE_(n);
		return -1;
	}
	return 0;
}

xcb_window_t
get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return XCB_NONE;
	}

	xcb_window_t x = p_reply->child;
	_FREE_(p_reply);

	return x;
}

void
grab_pointer(xcb_window_t win, bool wants_events)
{
	xcb_grab_pointer_reply_t *reply;
	xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(wm->connection,
														wants_events,
														win,
														XCB_NONE,
														XCB_GRAB_MODE_SYNC,
														XCB_GRAB_MODE_ASYNC,
														XCB_NONE,
														XCB_NONE,
														XCB_CURRENT_TIME);
	if ((reply = xcb_grab_pointer_reply(wm->connection, cookie, NULL))) {
		if (reply->status != XCB_GRAB_STATUS_SUCCESS)
			_LOG_(WARNING, "cannot grab the pointer");
	}
	_FREE_(reply);
}

void
ungrab_pointer(void)
{
	xcb_ungrab_pointer(wm->connection, XCB_CURRENT_TIME);
}

xcb_atom_t
get_atom(char *atom_name, xcb_conn_t *conn)
{
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_atom_t				 atom;
	xcb_intern_atom_reply_t *rep;

	atom_cookie =
		xcb_intern_atom(conn, 0, (uint16_t)strlen(atom_name), atom_name);
	rep = xcb_intern_atom_reply(conn, atom_cookie, NULL);
	if (NULL != rep) {
		atom = rep->atom;
		_FREE_(rep);
		return atom;
	}
	return 0;
}

bool
window_exists(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_tree_cookie_t c		   = xcb_query_tree(conn, win);
	xcb_query_tree_reply_t *tree_reply = xcb_query_tree_reply(conn, c, NULL);

	if (tree_reply == NULL) {
		return false;
	}

	_FREE_(tree_reply);
	return true;
}
