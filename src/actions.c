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

/*
 * actions.c implements all keybind callback functions
 *
 * Every function in this file can be bound to a key in the user's config
 * file or in the built-in fallback table in bindings.c.  They all share
 * the same callback contract:
 *
 *   int func(arg_t *arg);
 *
 * where arg_t (defined in type.h) carries whatever parameters the binding
 * needs:
 *
 *   typedef struct {
 *       char       **cmd;   // command + arguments, used by exec_process
 *       uint8_t      argc;  // argument count for cmd
 *       uint8_t      idx;   // desktop index (switch_desktop / transfer_node)
 *       resize_t     r;     // GROW or SHRINK
 *       resize_dir_t rd;    // HORIZONTAL_DIR or VERTICAL_DIR
 *       layout_t     t;     // DEFAULT, MASTER, STACK, or GRID
 *       traversal_t  tr;    // NEXT or PREV  (cycle_monitors)
 *       direction_t  d;     // LEFT, RIGHT, UP, DOWN, or NONE
 *       state_t      s;     // TILED, FLOATING, or FULLSCREEN
 *   } arg_t;
 *
 * All callbacks return 0 on success and -1 on error.
 */

#include "actions.h"
#include "bindings.h"
#include "client.h"
#include "config_parser.h"
#include "desktop.h"
#include "drag.h"
#include "ewmh.h"
#include "focus.h"
#include "helper.h"
#include "layout.h"
#include "monitor.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include "view.h"
#include "xcb_util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

static void
collect_deck_stack_leaves(node_t *root, node_t **buf, int *n, int cap)
{
	if (!root || *n >= cap)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client) &&
		!root->is_master) {
		buf[(*n)++] = root;
		return;
	}
	collect_deck_stack_leaves(root->first_child, buf, n, cap);
	collect_deck_stack_leaves(root->second_child, buf, n, cap);
}

static node_t *
next_deck_stack_node(node_t *root, direction_t d, xcb_window_t cursor_win)
{
	node_t *leaves[64];
	int		n = 0;
	collect_deck_stack_leaves(root, leaves, &n, 64);
	if (n == 0)
		return NULL;

	node_t *current = NULL;
	if (cursor_win != XCB_NONE && cursor_win != wm->root_window) {
		node_t *under_cursor = find_node_by_window_id(root, cursor_win);
		if (under_cursor && under_cursor->client && !under_cursor->is_master)
			current = under_cursor;
	}
	if (!current) {
		node_t *focused = get_focused_node(root);
		if (focused && !focused->is_master)
			current = focused;
	}
	if (!current)
		return (d == UP) ? leaves[0] : leaves[n - 1];

	for (int i = 0; i < n; i++) {
		if (leaves[i] == current) {
			return (d == UP) ? leaves[(i + 1) % n] : leaves[(i + n - 1) % n];
		}
	}

	return (d == UP) ? leaves[0] : leaves[n - 1];
}

int
layout_handler(arg_t *arg)
{
	desktop_t *d = curr_monitor->desk;
	if ((arg->t == STACK || arg->t == MONOCLE || arg->t == DECK) &&
		d->n_count < 2)
		return 0;

	suppress_enter_until_time = get_time_millis() + 250;
	apply_layout(d, arg->t);

	node_t *focus = view_pick_fallback_focus(d);
	if (focus)
		view_set_logical_focus(d, focus);
	int ret = view_render_desktop(d);
	if (ret == 0 && focus)
		ret = view_apply_input_focus(d, focus);
	view_commit(d);
	return ret;
}

int
change_state(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == XCB_NONE)
		return -1;

	node_t *n = find_node_by_window_id(curr_monitor->desk->tree, w);
	if (n == NULL)
		return -1;

	if (IS_ROOT(n))
		return 0;

	state_t state  = arg->s;
	node_t *parent = n->parent;
	if (state == TILED) {
		if (IS_TILED(n->client))
			return 0;
		n->client->state = TILED;
		if (n->rectangle.width >= n->rectangle.height) {
			parent->first_child->rectangle.x = parent->rectangle.x;
			parent->first_child->rectangle.y = parent->rectangle.y;
			parent->first_child->rectangle.width =
				(parent->rectangle.width -
				 (conf.window_gap - conf.border_width)) /
				2;
			parent->first_child->rectangle.height = parent->rectangle.height;

			parent->second_child->rectangle.x =
				(int16_t)(parent->rectangle.x +
						  parent->first_child->rectangle.width +
						  conf.window_gap + conf.border_width);
			parent->second_child->rectangle.y = parent->rectangle.y;
			parent->second_child->rectangle.width =
				parent->rectangle.width - parent->first_child->rectangle.width -
				conf.window_gap - conf.border_width;
			parent->second_child->rectangle.height = parent->rectangle.height;
		} else {
			parent->first_child->rectangle.x	 = parent->rectangle.x;
			parent->first_child->rectangle.y	 = parent->rectangle.y;
			parent->first_child->rectangle.width = parent->rectangle.width;
			parent->first_child->rectangle.height =
				(parent->rectangle.height -
				 (conf.window_gap - conf.border_width)) /
				2;

			parent->second_child->rectangle.x = parent->rectangle.x;
			parent->second_child->rectangle.y =
				(int16_t)(parent->rectangle.y +
						  parent->first_child->rectangle.height +
						  conf.window_gap + conf.border_width);
			parent->second_child->rectangle.width = parent->rectangle.width;
			parent->second_child->rectangle.height =
				parent->rectangle.height -
				parent->first_child->rectangle.height - conf.window_gap -
				conf.border_width;
		}
		if (IS_INTERNAL(parent->second_child)) {
			resize_subtree(parent->second_child);
		}
		if (IS_INTERNAL(parent->first_child)) {
			resize_subtree(parent->first_child);
		}
	} else if (state == FLOATING) {
		if (IS_FLOATING(n->client))
			return 0;
		xcb_get_geometry_reply_t *g =
			get_geometry(n->client->window, wm->connection);
		uint16_t	h		  = g->height / 2;
		uint16_t	wi		  = g->width / 2;
		int16_t		x		  = curr_monitor->rectangle.x +
								(curr_monitor->rectangle.width / 2) - (wi / 2);
		int16_t		y		  = curr_monitor->rectangle.y +
								(curr_monitor->rectangle.height / 2) - (h / 2);
		rectangle_t rc		  = {.x = x, .y = y, .width = wi, .height = h};
		n->floating_rectangle = rc;
		_FREE_(g);
		n->client->state = FLOATING;
		if (parent) {
			if (parent->first_child == n) {
				parent->second_child->rectangle = parent->rectangle;
				if (IS_INTERNAL(parent->second_child)) {
					resize_subtree(parent->second_child);
				}
			} else {
				parent->first_child->rectangle = parent->rectangle;
				if (IS_INTERNAL(parent->first_child)) {
					resize_subtree(parent->first_child);
				}
			}
		}
	}

	int ret = view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	return ret;
}

int
swap_node_wrapper(arg_t *arg)
{
	(void)arg;
	if (curr_monitor == NULL) {
		_LOG_(ERROR, "failed to swap node, current monitor is NULL");
		return -1;
	}

	layout_t lay = curr_monitor->desk->layout;
	if (lay != DEFAULT && lay != MASTER && lay != GRID && lay != THREE_COL)
		return 0;

	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (swap_node(n) != 0)
		return -1;

	int ret = view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	return ret;
}

int
dynamic_resize_wrapper(arg_t *arg)
{
	if (curr_monitor->desk->layout == STACK ||
		curr_monitor->desk->layout == MASTER ||
		curr_monitor->desk->layout == MONOCLE) {
		return 0;
	}

	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	/* todo: if node was flipped, reize up or down instead
	 * i think this is done already... as of 2026-01-14. confirmed?? */
	grab_pointer(wm->root_window,
				 false); /* steal the pointer and prevent it from sending
						  * enter_notify events (which focuses the window
						  * being under cursor as the resize happens); */
	dynamic_resize(n, arg->r);
	view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	ungrab_pointer();
	return 0;
}

int
set_fullscreen_wrapper(arg_t *arg)
{
	(void)arg;
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = find_node_by_window_id(curr_monitor->desk->tree, w);
	if (n == NULL) {
		_LOG_(ERROR, "cannot find focused node");
		return -1;
	}

	n->client->state == FULLSCREEN ? set_fullscreen(n, false)
								   : set_fullscreen(n, true);
	return 0;
}

int
set_fullscreen(node_t *n, bool flag)
{
	if (n == NULL || n->client == NULL)
		return -1;

	desktop_t *d	  = curr_monitor->desk;
	node_t	  *f	  = NULL;
	bool	   exists = false;
	find_window_in_desktops(&d, &f, n->client->window, &exists);
	if (!exists)
		d = curr_monitor->desk;

	monitor_t  *m			 = get_monitor_by_window(n->client->window);
	bool		_active		 = (m && m->desk == d);
	bool		should_focus = IS_TILED(n->client) || !flag;
	rectangle_t r			 = {0};
	if (flag) {
		if (!m) {
			m = curr_monitor;
		}
		long data[]		 = {wm->ewmh->_NET_WM_STATE_FULLSCREEN};
		r.x				 = m->rectangle.x;
		r.y				 = m->rectangle.y;
		r.width			 = m->rectangle.width;
		r.height		 = m->rectangle.height;
		n->client->state = FULLSCREEN;
		if (change_border_attr(wm->connection,
							   n->client->window,
							   conf.normal_border_color,
							   0,
							   false) != 0) {
			return -1;
		}
		if (apply_window_geometry(n->client->window, r, 0) != 0) {
			return -1;
		}
		xcb_cookie_t c	 = xcb_change_property_checked(wm->connection,
													   XCB_PROP_MODE_REPLACE,
													   n->client->window,
													   wm->ewmh->_NET_WM_STATE,
													   XCB_ATOM_ATOM,
													   32,
													   true,
													   data);
		xcb_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR, "error changing window property: %d", err->error_code);
			_FREE_(err);
			return -1;
		}
		update_client_ewmh_state(n->client, EWMH_STATE_FULLSCREEN, true);
		goto out;
	}

	r				 = n->rectangle;
	n->client->state = TILED;
	if (apply_window_geometry(n->client->window, r, conf.border_width) != 0) {
		return -1;
	}
	remove_property(wm->connection,
					n->client->window,
					wm->ewmh->_NET_WM_STATE,
					wm->ewmh->_NET_WM_STATE_FULLSCREEN);
	update_client_ewmh_state(n->client, EWMH_STATE_FULLSCREEN, false);
	if (change_border_attr(wm->connection,
						   n->client->window,
						   conf.normal_border_color,
						   conf.border_width,
						   true) != 0) {
		return -1;
	}
out:
	if (!_active) {
		if (set_desktop_visibility(n->client->window, false) != 0)
			return -1;
		restack();
		xcb_flush(wm->connection);
		return 0;
	}

	if (should_focus)
		view_set_logical_focus(d, n);
	if (view_render_desktop(d) != 0)
		return -1;

	if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window) != 0)
			return -1;
	} else if (view_apply_input_focus(d, n) != 0) {
		return -1;
	}
	focused_win		   = n->client->window;
	n->client->mru_seq = get_next_mru_seq(m ? m : curr_monitor);
	set_active_window_name(focused_win);
	view_commit(d);
	return 0;
}

static int
change_colors(node_t *root)
{
	if (root == NULL)
		return 0;
	/* win_foucs internally changes the border color of a window, if the focus
	 * param is set to true it applies the active_border_color, otherwise the
	 * normal_border_color is chosen */
	if (root->node_type != INTERNAL_NODE && root->client) {
		if (win_focus(root->client->window, root->is_focused) != 0) {
			_LOG_(ERROR, "cannot focus node");
			return -1;
		}
	}

	if (root->first_child)
		change_colors(root->first_child);
	if (root->second_child)
		change_colors(root->second_child);

	return 0;
}

int
reload_config_wrapper(arg_t *arg)
{
	(void)arg;
	/* store the old config values so i can compare them later with the new
	 * values to determine what needs to be done */
	uint16_t prev_border_width		  = conf.border_width;
	uint16_t prev_window_gap		  = conf.window_gap;
	uint32_t prev_active_border_color = conf.active_border_color;
	uint32_t prev_normal_border_color = conf.normal_border_color;
	int		 prev_virtual_desktops	  = conf.virtual_desktops;
	/* clear the config data structures */
	memset(&conf, 0, sizeof(config_t));

	ungrab_keys(wm->connection, wm->root_window);
	is_kgrabbed = false;
	free_keys();
	free_rules();
	assert(key_head == NULL && rule_head == NULL);

	if (reload_config(&conf) != 0) {
		_LOG_(ERROR, "error while reloading config -> using default macros");
		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		conf.focus_follow_spawn	  = FOCUS_FOLLOW_SPAWN;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
		conf.restore_last_focus	  = RESTORE_LAST_FOCUS;
		if (0 != grab_keys(wm->connection, wm->root_window)) {
			_LOG_(ERROR, "cannot grab keys after reload");
			return -1;
		}
		return 0;
	}

	bool color_changed =
		(prev_normal_border_color != conf.normal_border_color) ||
		(prev_active_border_color != conf.active_border_color);
	bool layout_changed	 = (conf.window_gap != prev_window_gap) ||
						   (conf.border_width != prev_border_width);
	bool desktop_changed = (prev_virtual_desktops != conf.virtual_desktops);

	if (color_changed) {
		monitor_t *current_monitor = head_monitor;
		while (current_monitor) {
			for (int j = 0; j < current_monitor->n_of_desktops; j++) {
				if (!is_tree_empty(current_monitor->desktops[j]->tree)) {
					if (change_colors(current_monitor->desktops[j]->tree) !=
						0) {
						_LOG_(ERROR,
							  "error while reloading config for "
							  "desktop %d",
							  current_monitor->desktops[j]->id);
					}
				}
			}
			current_monitor = current_monitor->next;
		}
	}

	if (layout_changed) {
		monitor_t *current_monitor = head_monitor;
		while (current_monitor) {
			apply_monitor_layout_changes(current_monitor);
			current_monitor = current_monitor->next;
		}
	}

	if (desktop_changed) {
		_LOG_(INFO, "reloading desktop changes is not implemented yet");
		if (conf.virtual_desktops > prev_virtual_desktops) {
			monitor_t *current_monitor = head_monitor;
			while (current_monitor) {
				current_monitor->n_of_desktops = conf.virtual_desktops;
				desktop_t **n				   = (desktop_t **)realloc(
					current_monitor->desktops,
					sizeof(desktop_t *) * current_monitor->n_of_desktops);
				if (n == NULL) {
					_LOG_(ERROR, "failed to realloc desktops");
					goto out;
				}
				current_monitor->desktops = n;
				for (int j = prev_virtual_desktops;
					 j < current_monitor->n_of_desktops;
					 j++) {
					desktop_t *d = init_desktop();
					if (d == NULL) {
						_LOG_(ERROR, "failed to initialize new desktop");
						goto out;
					}
					d->id		  = (uint16_t)j;
					d->is_focused = false;
					d->layout	  = DEFAULT;
					snprintf(d->name, sizeof(d->name), "%d", j + 1);
					current_monitor->desktops[j] = d;
				}
				current_monitor = current_monitor->next;
			}
		} else if (conf.virtual_desktops < prev_virtual_desktops) {
			monitor_t *current_monitor = head_monitor;
			while (current_monitor) {
				for (int j = conf.virtual_desktops; j < prev_virtual_desktops;
					 j++) {
					if (curr_monitor->desk->id ==
						current_monitor->desktops[j]->id) {
						switch_desktop_wrapper(&(arg_t){
							.idx = curr_monitor->desk->id == 0 ? 1 : 0});
					}
					if (current_monitor->desktops[j]) {
						if (!is_tree_empty(
								current_monitor->desktops[j]->tree)) {
							free_tree(current_monitor->desktops[j]->tree);
							current_monitor->desktops[j]->tree = NULL;
						}
						_FREE_(current_monitor->desktops[j]);
					}
				}
				current_monitor->n_of_desktops = conf.virtual_desktops;
				desktop_t **n				   = (desktop_t **)realloc(
					current_monitor->desktops,
					sizeof(desktop_t *) * current_monitor->n_of_desktops);
				if (n == NULL) {
					_LOG_(ERROR, "failed to realloc desktops");
					goto out;
				}
				current_monitor->desktops = n;
				current_monitor			  = current_monitor->next;
			}
		}

		if (ewmh_update_number_of_desktops() != 0) {
			return false;
		}

		if (ewmh_update_desktop_names() != 0) {
			return false;
		}

		if (ewmh_update_current_desktop(
				wm->ewmh, wm->screen_nbr, (uint32_t)curr_monitor->desk->id) !=
			0) {
			return false;
		}

		if (ewmh_update_desktop_names() != 0) {
			return false;
		}
	}

	if (0 != grab_keys(wm->connection, wm->root_window)) {
		_LOG_(ERROR, "cannot grab keys after reload");
		return -1;
	}

out:
	view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	return 0;
}

int
gap_handler(arg_t *arg)
{
	const int pxl = 5;
	if (arg->r == GROW) {
		conf.window_gap += pxl;
	} else {
		conf.window_gap =
			(conf.window_gap - pxl <= 0) ? 0 : conf.window_gap - pxl;
	}

	monitor_t *current_monitor = head_monitor;
	while (current_monitor) {
		apply_monitor_layout_changes(current_monitor);
		current_monitor = current_monitor->next;
	}
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	return 0;
}

int
flip_node_wrapper(arg_t *arg)
{
	(void)arg;
	layout_t lay = curr_monitor->desk->layout;
	if (lay != DEFAULT && lay != MASTER && lay != GRID && lay != THREE_COL)
		return 0;

	node_t *tree = curr_monitor->desk->tree;
	node_t *node = NULL;
	if (!(node = get_focused_node(tree)))
		return -1;

	flip_node(node);
	int ret = view_render_desktop(curr_monitor->desk);
	view_commit(curr_monitor->desk);
	return ret;
}

int
cycle_win_wrapper(arg_t *arg)
{
	direction_t d = arg->d;
	node_t	   *f = NULL;
	if (!(f = get_focused_node(curr_monitor->desk->tree))) {
		_LOG_(INFO, "cannot find focused window");
		xcb_window_t w =
			get_window_under_cursor(wm->connection, wm->root_window);
		f = find_node_by_window_id(curr_monitor->desk->tree, w);
	}
	node_t *next = cycle_win(f, d);
	if (next == NULL) {
		return 0;
	}
#ifdef _DEBUG__
	char *s = win_name(next->client->window);
	_LOG_(DEBUG, "found node %d name %s", next->client->window, s);
	_FREE_(s);
#endif
	view_set_logical_focus(curr_monitor->desk, next);
	set_active_window_name(next->client->window);
	next->client->mru_seq = get_next_mru_seq(curr_monitor);
	int ret				  = view_render_desktop(curr_monitor->desk);
	if (ret == 0)
		ret = view_apply_input_focus(curr_monitor->desk, next);
	view_commit(curr_monitor->desk);
	return ret;
}

void
move_mouse_to_monitor(monitor_t *m)
{
	if (!m)
		return;
	xcb_query_pointer_reply_t *ptr = xcb_query_pointer_reply(
		wm->connection,
		xcb_query_pointer(wm->connection, wm->root_window),
		NULL);

	if (ptr == NULL) {
		_LOG_(ERROR, "failed to query pointer");
		return;
	}

	int pointer_x = ptr->root_x;
	int pointer_y = ptr->root_y;

	int x		  = (m->rectangle.x + m->rectangle.width / 2) + 100;
	int y		  = (m->rectangle.y + m->rectangle.height / 2) + 100;

	xcb_warp_pointer(wm->connection,
					 wm->root_window,
					 wm->root_window,
					 pointer_x,
					 pointer_y,
					 curr_monitor->rectangle.width,
					 curr_monitor->rectangle.height,
					 x,
					 y);
	xcb_flush(wm->connection);
}

int
cycle_monitors(arg_t *arg)
{
	if (!multi_monitors)
		return 0;

	traversal_t dir	 = arg->tr;
	monitor_t  *curr = curr_monitor;
	monitor_t  *m	 = NULL;

	if (dir == NEXT) {
		m = curr->next;
		if (!m) {
			/* wrap around to the first monitor */
			m = head_monitor;
		}
	} else {
		monitor_t *head = head_monitor;
		monitor_t *prev = NULL;
		while (head && head->next) {
			if (head->next == curr) {
				m = head;
				break;
			}
			head = head->next;
		}

		/* wrap around to the last monitor */
		if (!m) {
			while (head && head->next) {
				head = head->next;
			}
			m = head;
		}
	}

	if (m && m != curr) {
		_LOG_(INFO, "switching monitor: '%s' -> '%s'", curr->name, m->name);
		/* when a mouse moves, we update the current monitor in the enter_notify
		 * handler */
		move_mouse_to_monitor(m);
		/*curr_monitor = m;*/
		return 0;
	}

	_LOG_(INFO, "no monitor change occurred");
	return 0;
}

int
traverse_stack_wrapper(arg_t *arg)
{
	direction_t	 d = arg->d;
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	layout_t lay  = curr_monitor->desk->layout;
	node_t	*node = get_focused_node(curr_monitor->desk->tree);
	if (!node && lay != DECK)
		return -1;

	node_t *n = (lay == DECK)
					? next_deck_stack_node(curr_monitor->desk->tree, d, w)
					: (d == UP ? next_node(node) : prev_node(node));

	if (n == NULL) {
		return -1;
	}

	view_set_logical_focus(curr_monitor->desk, n);
	if (n->client)
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
	if (view_render_desktop(curr_monitor->desk) != 0)
		return -1;
	if (view_apply_input_focus(curr_monitor->desk, n) != 0)
		return -1;
	view_commit(curr_monitor->desk);
	return 0;
}

int
exec_process(arg_t *arg)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("Fork failed");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		if (arg->argc == 1) {
			char *p		 = (char *)arg->cmd[0];
			char *args[] = {p, NULL};
			execvp(p, args);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		} else {
			const char *args[arg->argc + 1];
			for (int i = 0; i < arg->argc; i++) {
				args[i] = arg->cmd[i];
#ifdef _DEBUG__
				_LOG_(DEBUG, "args areee %s", args[i]);
#endif
			}
			args[arg->argc] = NULL;
			execvp(args[0], (char *const *)args);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

int
close_or_kill_wrapper(arg_t *arg)
{
	(void)arg;
	xcb_window_t win = get_window_under_cursor(wm->connection, wm->root_window);
	if (!window_exists(wm->connection, win))
		return 0;
	return close_or_kill(win);
}

int
transfer_node_wrapper(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window)
		return 0;
	const int i = arg->idx;

	if (curr_monitor->desk->id == i) {
		_LOG_(INFO, "switch node to curr desktop... abort");
		return 0;
	}

	if (is_tree_empty(curr_monitor->desk->tree)) {
		return 0;
	}

	node_t *node = NULL;
	if (!(node = get_focused_node(curr_monitor->desk->tree))) {
		_LOG_(ERROR, "focused node is null");
		return 0;
	}

	desktop_t *nd = curr_monitor->desktops[i];
	desktop_t *od = curr_monitor->desk;
#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", i + 1);
	log_tree_nodes(nd->tree);
	_LOG_(INFO, "old desktop %d nodes--------------", od->id + 1);
	log_tree_nodes(od->tree);
#endif
	if (set_desktop_visibility(node->client->window, false) != 0) {
		_LOG_(ERROR, "cannot hide window %d", node->client->window);
		return -1;
	}
	if (unlink_node(node, od)) {
		if (!transfer_node(node, nd)) {
			_LOG_(ERROR, "could not transfer node.. abort");
			return -1;
		}
	} else {
		_LOG_(ERROR, "could not unlink node.. abort");
		return -1;
	}

	od->n_count--;
	nd->n_count++;
	update_net_wm_desktop(node->client->window, nd->id);
	arrange_tree(nd->tree, nd->layout);
	if (nd->layout == STACK) {
		set_focus(node, true);
	}
	if (!is_tree_empty(od->tree)) {
		arrange_tree(od->tree, od->layout);
	}
	int ret = view_render_desktop(od);
	view_commit(od);
	return ret;
}

int
switch_desktop_wrapper(arg_t *arg)
{
	if (arg->idx > conf.virtual_desktops) {
		return 0;
	}
	if (switch_desktop(arg->idx) != 0) {
		return -1;
	}
	last_desk_switch_time = get_time_millis();
	return 0;
}

int
cycle_desktop_wrapper(arg_t *arg)
{
	int current = get_focused_desktop_idx();
	if (current == -1) {
		_LOG_(ERROR, "cnnot find current desktop");
		return -1;
	}

	int n_desktops = curr_monitor->n_of_desktops;
	int next = (current + (arg->d == RIGHT ? 1 : -1) + n_desktops) % n_desktops;

	switch_desktop(next);
	last_desk_switch_time = get_time_millis();
	return 0;
}

int
grow_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const uint16_t	   step		  = 10;
	const resize_dir_t resize_dir = arg->rd;
	const uint16_t	   max_dim	  = (resize_dir == HORIZONTAL_DIR)
										? curr_monitor->rectangle.width
										: curr_monitor->rectangle.height;
	uint16_t		  *dim		  = (resize_dir == HORIZONTAL_DIR)
										? &n->floating_rectangle.width
										: &n->floating_rectangle.height;
	int16_t *pos = (resize_dir == HORIZONTAL_DIR) ? &n->floating_rectangle.x
												  : &n->floating_rectangle.y;

	if (*dim + (step * 2) > max_dim)
		return 0;

	*dim += (step * 2);
	*pos -= step;

	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

int
shrink_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;
	const uint16_t	   step		  = 10;
	const resize_dir_t resize_dir = arg->rd;
	const uint16_t	   min_dim	  = step * 40;
	uint16_t		  *dim		  = (resize_dir == HORIZONTAL_DIR)
										? &n->floating_rectangle.width
										: &n->floating_rectangle.height;
	int16_t *pos = (resize_dir == HORIZONTAL_DIR) ? &n->floating_rectangle.x
												  : &n->floating_rectangle.y;
	if (*dim - (step * 2) < min_dim) {
		return 0;
	}
	*dim -= (step * 2);
	*pos += step;
	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

int
resize_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const uint16_t	   step			 = 10;
	const resize_t	   resize_type	 = arg->r;
	const resize_dir_t resize_dir	 = arg->rd;
	int16_t			   delta		 = (resize_type == GROW ? step : -step);
	uint16_t		  *dim_to_resize = (resize_dir == HORIZONTAL_DIR)
										   ? &n->floating_rectangle.width
										   : &n->floating_rectangle.height;
	int16_t			  *pos_to_adjust = (resize_dir == HORIZONTAL_DIR)
										   ? &n->floating_rectangle.x
										   : &n->floating_rectangle.y;
	*pos_to_adjust -= delta / 2;
	*dim_to_resize += delta;
	if (*dim_to_resize <= 0) {
		*dim_to_resize = step;
		*pos_to_adjust += delta / 2;
	}
	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

int
shift_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const int16_t	   step			= 10;
	rectangle_t		  *rect			= &n->floating_rectangle;
	const rectangle_t *monitor_rect = &curr_monitor->rectangle;
	const direction_t  dir			= arg->d;

	switch (dir) {
	case LEFT: rect->x -= step; break;
	case RIGHT: rect->x += step; break;
	case UP: rect->y -= step; break;
	case DOWN: rect->y += step; break;
	case NONE: return 0;
	}

	if (rect->x < monitor_rect->x) {
		rect->x = monitor_rect->x;
		return 0;
	}
	if (rect->x + rect->width > monitor_rect->x + monitor_rect->width) {
		rect->x = monitor_rect->x + monitor_rect->width - rect->width;
		return 0;
	}
	if (rect->y < monitor_rect->y) {
		rect->y = monitor_rect->y;
		return 0;
	}
	if (rect->y + rect->height > monitor_rect->y + monitor_rect->height) {
		rect->y = monitor_rect->y + monitor_rect->height - rect->height;
		return 0;
	}

	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(n->client->window, *rect, conf.border_width) !=
		0) {
		return -1;
	}

	ungrab_pointer();
	return 0;
}

int
start_keyboard_drag_wrapper(arg_t *arg)
{
	(void)arg;

	if (!curr_monitor || !curr_monitor->desk)
		return -1;

	node_t *root = curr_monitor->desk->tree;
	node_t *n	 = get_focused_node(root);

	if (!n || !n->client) {
		_LOG_(WARNING, "no focused window to drag");
		return -1;
	}

	int16_t cx = n->rectangle.x + n->rectangle.width / 2;
	int16_t cy = n->rectangle.y + n->rectangle.height / 2;

	xcb_warp_pointer(
		wm->connection, XCB_NONE, wm->root_window, 0, 0, 0, 0, cx, cy);
	xcb_flush(wm->connection);

	return drag_start(n->client->window, cx, cy, true);
}
