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

#include "mouse.h"
#include "cursor.h"
#include "desktop.h"
#include "helper.h"
#include "state.h"
#include "tree.h"
#include "xcb_util.h"
#include "layout.h"
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

static node_t *
mouse_tree_root(node_t *n)
{
	if (!n)
		return NULL;
	while (n->parent)
		n = n->parent;
	return n;
}

void
window_grab_buttons(xcb_window_t win)
{
#define _GRAB_BUTTON_(button)                                                  \
	do {                                                                       \
		xcb_grab_button_checked(wm->connection,                                \
								false,                                         \
								win,                                           \
								XCB_EVENT_MASK_BUTTON_PRESS,                   \
								XCB_GRAB_MODE_ASYNC,                           \
								XCB_GRAB_MODE_ASYNC,                           \
								wm->root_window,                               \
								XCB_NONE,                                      \
								button,                                        \
								XCB_MOD_MASK_ANY);                             \
	} while (0)
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_1);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_2);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_3);
#undef _GRAB_BUTTON_
}

void
window_ungrab_buttons(xcb_window_t win)
{
	xcb_cookie_t cookie = xcb_ungrab_button_checked(
		wm->connection, XCB_BUTTON_INDEX_ANY, win, XCB_MOD_MASK_ANY);

	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in ungrab buttons for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return;
	}
}

void
ungrab_buttons_for_all(node_t *n)
{
	if (n == NULL)
		return;

	bool flag = n->node_type != INTERNAL_NODE && n->client;

	if (flag) {
		xcb_ungrab_button(wm->connection,
						  XCB_BUTTON_INDEX_ANY,
						  n->client->window,
						  XCB_MOD_MASK_ANY);
	}

	ungrab_buttons_for_all(n->first_child);
	ungrab_buttons_for_all(n->second_child);
}

bool
grab_pointer_for_mouse(cursor_t cursor_id)
{
	xcb_grab_pointer_reply_t *reply;
	xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(
		wm->connection,
		false,			 /* owner_events */
		wm->root_window, /* grab_window */
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
		XCB_GRAB_MODE_ASYNC, /* pointer_mode */
		XCB_GRAB_MODE_ASYNC, /* keyboard_mode */
		XCB_NONE,			 /* confine_to */
		get_cursor(cursor_id),
		XCB_CURRENT_TIME);

	reply = xcb_grab_pointer_reply(wm->connection, cookie, NULL);
	if (!reply) {
		return false;
	}
	bool ok = (reply->status == XCB_GRAB_STATUS_SUCCESS);
	_FREE_(reply);
	return ok;
}

void
clear_mouse_state(void)
{
	mouse_state = (mouse_state_t){0};
}

double
clamp_ratio(double ratio)
{
	const double min = 0.05;
	if (ratio < min) {
		return min;
	}
	if (ratio > (1.0 - min)) {
		return 1.0 - min;
	}
	return ratio;
}

uint8_t
detect_resize_edges(rectangle_t r, int16_t x, int16_t y)
{
	const int16_t edge	   = 10;
	uint8_t		  edges	   = 0;
	int16_t		  left_d   = (int16_t)(x - r.x);
	int16_t		  right_d  = (int16_t)((r.x + (int16_t)r.width) - x);
	int16_t		  top_d	   = (int16_t)(y - r.y);
	int16_t		  bottom_d = (int16_t)((r.y + (int16_t)r.height) - y);

	if (left_d >= 0 && left_d <= edge) {
		edges |= RESIZE_EDGE_LEFT;
	}
	if (right_d >= 0 && right_d <= edge) {
		edges |= RESIZE_EDGE_RIGHT;
	}
	if (top_d >= 0 && top_d <= edge) {
		edges |= RESIZE_EDGE_TOP;
	}
	if (bottom_d >= 0 && bottom_d <= edge) {
		edges |= RESIZE_EDGE_BOTTOM;
	}

	if ((edges & (RESIZE_EDGE_LEFT | RESIZE_EDGE_RIGHT)) ==
		(RESIZE_EDGE_LEFT | RESIZE_EDGE_RIGHT)) {
		edges &= (left_d <= right_d) ? ~RESIZE_EDGE_RIGHT : ~RESIZE_EDGE_LEFT;
	}
	if ((edges & (RESIZE_EDGE_TOP | RESIZE_EDGE_BOTTOM)) ==
		(RESIZE_EDGE_TOP | RESIZE_EDGE_BOTTOM)) {
		edges &= (top_d <= bottom_d) ? ~RESIZE_EDGE_BOTTOM : ~RESIZE_EDGE_TOP;
	}

	return edges;
}

bool
is_resize_band_hit(node_t	   *parent,
				   split_type_t split_type,
				   int16_t		x,
				   int16_t		y)
{
	if (!parent || !parent->first_child || !parent->second_child) {
		return false;
	}

	const int16_t edge = 8;
	if (split_type == HORIZONTAL_TYPE) {
		rectangle_t a	   = parent->first_child->rectangle;
		rectangle_t b	   = parent->second_child->rectangle;
		bool		a_left = (a.x <= b.x);
		int16_t left_edge = (int16_t)((a_left ? a.x + a.width : b.x + b.width));
		int16_t right_edge = (int16_t)(a_left ? b.x : a.x);
		int16_t min_x	   = (left_edge < right_edge) ? left_edge : right_edge;
		int16_t max_x	   = (left_edge > right_edge) ? left_edge : right_edge;
		return (x >= (min_x - edge) && x <= (max_x + edge));
	}
	if (split_type == VERTICAL_TYPE) {
		rectangle_t a	  = parent->first_child->rectangle;
		rectangle_t b	  = parent->second_child->rectangle;
		bool		a_top = (a.y <= b.y);
		int16_t top_edge = (int16_t)((a_top ? a.y + a.height : b.y + b.height));
		int16_t bottom_edge = (int16_t)(a_top ? b.y : a.y);
		int16_t min_y		= (top_edge < bottom_edge) ? top_edge : bottom_edge;
		int16_t max_y		= (top_edge > bottom_edge) ? top_edge : bottom_edge;
		return (y >= (min_y - edge) && y <= (max_y + edge));
	}

	return false;
}

bool
start_floating_move(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || !IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}

	mouse_state.op		   = MOUSE_OP_MOVE_FLOATING;
	mouse_state.node	   = n;
	mouse_state.window	   = n->client->window;
	mouse_state.start_x	   = x;
	mouse_state.start_y	   = y;
	mouse_state.start_rect = n->floating_rectangle;
	mouse_state.edges	   = 0;

	const uint32_t val[]   = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, n->client->window, XCB_CONFIG_WINDOW_STACK_MODE, val);

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

bool
start_floating_resize(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || !IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}

	uint8_t edges = detect_resize_edges(n->floating_rectangle, x, y);
	if (edges == 0) {
		return false;
	}

	mouse_state.op		   = MOUSE_OP_RESIZE_FLOATING;
	mouse_state.node	   = n;
	mouse_state.window	   = n->client->window;
	mouse_state.start_x	   = x;
	mouse_state.start_y	   = y;
	mouse_state.start_rect = n->floating_rectangle;
	mouse_state.edges	   = edges;

	const uint32_t val[]   = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, n->client->window, XCB_CONFIG_WINDOW_STACK_MODE, val);

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

bool
start_tiled_resize(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || n->parent == NULL || IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}
	if (curr_monitor->desk->layout == DECK ||
		curr_monitor->desk->layout == THREE_COL) {
		node_t *root = mouse_tree_root(n);
		if (!root)
			return false;

		const int16_t edge  = 10;
		const int16_t gap	= (int16_t)conf.window_gap;
		const int16_t bw	= (int16_t)conf.border_width;
		const double  ratio = (root->split_ratio <= 0.0 ||
							   root->split_ratio >= 1.0)
								  ? 0.5
								  : clamp_ratio(root->split_ratio);
		const int16_t mw	= (int16_t)(root->rectangle.width * ratio -
									 gap - 2 * bw);
		uint8_t		  hit	= RESIZE_EDGE_RIGHT;

		if (curr_monitor->desk->layout == THREE_COL) {
			const int16_t cw = mw;
			const int16_t side_total =
				(int16_t)(root->rectangle.width - cw - 2 * (gap + bw));
			const int16_t sw = (int16_t)(side_total / 2);
			const int16_t left_edge = (int16_t)(root->rectangle.x + sw);
			const int16_t right_edge =
				(int16_t)(left_edge + gap + bw + cw);
			if (x >= left_edge - edge && x <= left_edge + edge) {
				hit = RESIZE_EDGE_LEFT;
			} else if (x >= right_edge - edge && x <= right_edge + edge) {
				hit = RESIZE_EDGE_RIGHT;
			} else {
				return false;
			}
		} else {
			const int16_t master_edge = (int16_t)(root->rectangle.x + mw);
			const int16_t deck_edge =
				(int16_t)(root->rectangle.x + mw + gap + bw);
			if (!((x >= master_edge - edge && x <= master_edge + edge) ||
				  (x >= deck_edge - edge && x <= deck_edge + edge)))
				return false;
		}

		mouse_state.op			 = MOUSE_OP_RESIZE_TILED;
		mouse_state.node		 = n;
		mouse_state.parent		 = root;
		mouse_state.start_x		 = x;
		mouse_state.start_y		 = y;
		mouse_state.split_type	 = HORIZONTAL_TYPE;
		mouse_state.start_ratio	 = ratio;
		mouse_state.first_size	 = (int16_t)(root->rectangle.width * ratio);
		mouse_state.avail		 = root->rectangle.width;
		mouse_state.edges		 = hit;

		if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
			clear_mouse_state();
			return false;
		}

		return true;
	}
	if (curr_monitor->desk->layout != DEFAULT) {
		return false;
	}

	node_t *p = n->parent;
	node_t *s = (p->first_child == n) ? p->second_child : p->first_child;
	if (!s) {
		return false;
	}

	bool vs = (n->rectangle.x == s->rectangle.x);
	bool hs = (n->rectangle.y == s->rectangle.y);
	if (!vs && !hs) {
		return false;
	}

	split_type_t st = hs ? HORIZONTAL_TYPE : VERTICAL_TYPE;
	if (!is_resize_band_hit(p, st, x, y)) {
		return false;
	}

	const int16_t gap	= conf.window_gap - conf.border_width;
	const int16_t avail = (st == HORIZONTAL_TYPE) ? (p->rectangle.width - gap)
												  : (p->rectangle.height - gap);
	if (avail <= 0) {
		return false;
	}

	const int16_t first_size = (st == HORIZONTAL_TYPE)
								   ? p->first_child->rectangle.width
								   : p->first_child->rectangle.height;
	const double  ratio		 = (double)first_size / (double)avail;

	mouse_state.op			 = MOUSE_OP_RESIZE_TILED;
	mouse_state.node		 = n;
	mouse_state.parent		 = p;
	mouse_state.start_x		 = x;
	mouse_state.start_y		 = y;
	mouse_state.split_type	 = st;
	mouse_state.start_ratio	 = clamp_ratio(ratio);
	mouse_state.first_size	 = first_size;
	mouse_state.avail		 = avail;
	mouse_state.edges		 = 0;

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

void
handle_mouse_motion(int16_t x, int16_t y)
{
	if (mouse_state.op == MOUSE_OP_MOVE_FLOATING) {
		int16_t		dx = (int16_t)(x - mouse_state.start_x);
		int16_t		dy = (int16_t)(y - mouse_state.start_y);
		rectangle_t r  = mouse_state.start_rect;
		r.x			   = (int16_t)(r.x + dx);
		r.y			   = (int16_t)(r.y + dy);
		mouse_state.node->floating_rectangle = r;
		move_window(mouse_state.window, r.x, r.y);
		return;
	}

	if (mouse_state.op == MOUSE_OP_RESIZE_FLOATING) {
		const int32_t min_dim = 40;
		int32_t		  dx	  = (int32_t)(x - mouse_state.start_x);
		int32_t		  dy	  = (int32_t)(y - mouse_state.start_y);
		int32_t		  nx	  = mouse_state.start_rect.x;
		int32_t		  ny	  = mouse_state.start_rect.y;
		int32_t		  nw	  = mouse_state.start_rect.width;
		int32_t		  nh	  = mouse_state.start_rect.height;

		if (mouse_state.edges & RESIZE_EDGE_LEFT) {
			nx += dx;
			nw -= dx;
		}
		if (mouse_state.edges & RESIZE_EDGE_RIGHT) {
			nw += dx;
		}
		if (mouse_state.edges & RESIZE_EDGE_TOP) {
			ny += dy;
			nh -= dy;
		}
		if (mouse_state.edges & RESIZE_EDGE_BOTTOM) {
			nh += dy;
		}

		if (nw < min_dim) {
			if (mouse_state.edges & RESIZE_EDGE_LEFT) {
				nx = mouse_state.start_rect.x +
					 (mouse_state.start_rect.width - min_dim);
			}
			nw = min_dim;
		}
		if (nh < min_dim) {
			if (mouse_state.edges & RESIZE_EDGE_TOP) {
				ny = mouse_state.start_rect.y +
					 (mouse_state.start_rect.height - min_dim);
			}
			nh = min_dim;
		}

		rectangle_t r = {
			.x		= (int16_t)nx,
			.y		= (int16_t)ny,
			.width	= (uint16_t)nw,
			.height = (uint16_t)nh,
		};
		mouse_state.node->floating_rectangle = r;
		resize_window(mouse_state.window, r.width, r.height);
		move_window(mouse_state.window, r.x, r.y);
		return;
	}

	if (mouse_state.op == MOUSE_OP_RESIZE_TILED) {
		int32_t delta	  = (mouse_state.split_type == HORIZONTAL_TYPE)
								? (int32_t)(x - mouse_state.start_x)
								: (int32_t)(y - mouse_state.start_y);
		if (curr_monitor->desk->layout == THREE_COL &&
			(mouse_state.edges & RESIZE_EDGE_LEFT)) {
			delta = -delta;
		}
		int32_t new_first = mouse_state.first_size + delta;
		int32_t min_size  = 40;
		if (mouse_state.avail < min_size * 2) {
			min_size = mouse_state.avail / 2;
		}
		if (min_size < 1) {
			min_size = 1;
		}

		if (new_first < min_size) {
			new_first = min_size;
		}
		if (new_first > (mouse_state.avail - min_size)) {
			new_first = mouse_state.avail - min_size;
		}

		double ratio = (double)new_first / (double)mouse_state.avail;
		if (curr_monitor->desk->layout == DECK ||
			curr_monitor->desk->layout == THREE_COL) {
			mouse_state.parent->split_ratio = clamp_ratio(ratio);
			arrange_tree(mouse_state.parent, curr_monitor->desk->layout);
			render_desktop(curr_monitor->desk);
			return;
		}
		mouse_state.parent->split_type	= mouse_state.split_type;
		mouse_state.parent->split_ratio = clamp_ratio(ratio);
		resize_subtree(mouse_state.parent);
		render_tree_nomap(mouse_state.parent);
		return;
	}
}

void
finish_mouse_action(void)
{
	ungrab_pointer();
	clear_mouse_state();
	xcb_flush(wm->connection);
}

void
cancel_mouse_action(void)
{
	if (mouse_state.op == MOUSE_OP_MOVE_FLOATING ||
		mouse_state.op == MOUSE_OP_RESIZE_FLOATING) {
		if (mouse_state.node && mouse_state.node->client) {
			mouse_state.node->floating_rectangle = mouse_state.start_rect;
			resize_window(mouse_state.window,
						  mouse_state.start_rect.width,
						  mouse_state.start_rect.height);
			move_window(mouse_state.window,
						mouse_state.start_rect.x,
						mouse_state.start_rect.y);
		}
	} else if (mouse_state.op == MOUSE_OP_RESIZE_TILED) {
		if (mouse_state.parent) {
			mouse_state.parent->split_type	= mouse_state.split_type;
			mouse_state.parent->split_ratio = mouse_state.start_ratio;
			if (curr_monitor->desk->layout == DECK ||
				curr_monitor->desk->layout == THREE_COL) {
				arrange_tree(mouse_state.parent, curr_monitor->desk->layout);
				render_desktop(curr_monitor->desk);
			} else {
				resize_subtree(mouse_state.parent);
				render_tree_nomap(mouse_state.parent);
			}
		}
	}
	ungrab_pointer();
	clear_mouse_state();
	xcb_flush(wm->connection);
}
