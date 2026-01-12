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

#include "drag.h"

#include <stdbool.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include "helper.h"
#include "tree.h"
#include "type.h"
#include "zwm.h"

drag_state_t drag_state = {0};

static node_t *
clone_tree(node_t *root, node_t *parent);
static void
apply_preview_layout(node_t *root);
static void
preview_restore_layout(void);
static void
preview_apply(node_t *target);
static void
preview_clear(void);

/* find_leaf_at_point - map cursor position to BSP leaf node */
node_t *
find_leaf_at_point(node_t *root, int16_t x, int16_t y)
{
	if (root == NULL)
		return NULL;

	/* if external node, check if point is inside */
	if (IS_EXTERNAL(root)) {
		rectangle_t r = root->rectangle;
		if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) {
			/* Skip floating clients so they don't become drop targets. */
			if (root->client && IS_FLOATING(root->client))
				return NULL;
			return root;
		}
		return NULL;
	}

	/* internal node - recurse */
	if (root->first_child) {
		node_t *f = find_leaf_at_point(root->first_child, x, y);
		if (f)
			return f;
	}
	if (root->second_child) {
		node_t *s = find_leaf_at_point(root->second_child, x, y);
		if (s)
			return s;
	}

	return NULL;
}

static node_t *
clone_tree(node_t *root, node_t *parent)
{
	if (!root)
		return NULL;

	node_t *n = (node_t *)calloc(1, sizeof(node_t));
	if (!n)
		return NULL;

	n->parent			  = parent;
	n->node_type		  = root->node_type;
	n->is_focused		  = root->is_focused;
	n->is_master		  = root->is_master;
	n->split_type		  = root->split_type;
	n->split_ratio		  = root->split_ratio;
	n->rectangle		  = root->rectangle;
	n->floating_rectangle = root->floating_rectangle;

	if (root->client) {
		client_t *c = (client_t *)malloc(sizeof(client_t));
		if (!c) {
			_FREE_(n);
			return NULL;
		}
		*c		  = *root->client;
		n->client = c;
	}

	if (root->first_child) {
		n->first_child = clone_tree(root->first_child, n);
		if (!n->first_child) {
			free_tree(n);
			return NULL;
		}
	}
	if (root->second_child) {
		n->second_child = clone_tree(root->second_child, n);
		if (!n->second_child) {
			free_tree(n);
			return NULL;
		}
	}

	return n;
}

static void
apply_preview_layout(node_t *root)
{
	if (!root)
		return;

	if (!IS_INTERNAL(root) && root->client) {
		if (IS_FULLSCREEN(root->client))
			return;
		if (root->client->window != drag_state.window) {
			const rectangle_t r = IS_FLOATING(root->client)
									  ? root->floating_rectangle
									  : root->rectangle;
			resize_window(root->client->window, r.width, r.height);
			move_window(root->client->window, r.x, r.y);
		}
		return;
	}

	apply_preview_layout(root->first_child);
	apply_preview_layout(root->second_child);
}

/* drag_start - initialize drag session */
int
drag_start(xcb_window_t win, int16_t x, int16_t y, bool kbd)
{
	node_t *root = curr_monitor->desk->tree;
	node_t *n	 = find_node_by_window_id(root, win);

	if (!n || !n->client) {
		_LOG_(WARNING, "cannot drag: window not found");
		return -1;
	}

	/* only drag tiled windows */
	if (IS_FLOATING(n->client) || IS_FULLSCREEN(n->client)) {
		_LOG_(WARNING, "cannot drag floating or fullscreen windows");
		return -1;
	}

	/* Initialize Drag State */
	drag_state.window			= win;
	drag_state.src_node			= n;
	drag_state.start_x			= x;
	drag_state.start_y			= y;
	drag_state.cur_x			= x;
	drag_state.cur_y			= y;
	drag_state.active			= true;
	drag_state.kbd_mode			= kbd;
	drag_state.last_target		= NULL;
	drag_state.preview_active	= false;

	/* Save Restore Info */
	drag_state.original_desktop = curr_monitor->desk;
	drag_state.original_rect	= n->rectangle;

	/* Raise the window so it stays above during drag */
	const uint32_t val[]		= {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, win, XCB_CONFIG_WINDOW_STACK_MODE, val);

	/* change cursor */
	// xcb_change_window_attributes(wm->connection,
	// 							 wm->root_window,
	// 							 XCB_CW_CURSOR,
	// 							 (uint32_t[]){get_cursor(CURSOR_MOVE)});

	/* Active Pointer Grab */
	xcb_grab_pointer_cookie_t cookie =
		xcb_grab_pointer(wm->connection,
						 false,			  /* owner_events */
						 wm->root_window, /* grab_window */
						 XCB_EVENT_MASK_BUTTON_RELEASE |
							 XCB_EVENT_MASK_POINTER_MOTION, /* event_mask */
						 XCB_GRAB_MODE_ASYNC,				/* pointer_mode */
						 XCB_GRAB_MODE_ASYNC,				/* keyboard_mode */
						 XCB_NONE,							/* confine_to */
						 get_cursor(CURSOR_MOVE),			/* cursor */
						 XCB_CURRENT_TIME);

	xcb_grab_pointer_reply_t *reply =
		xcb_grab_pointer_reply(wm->connection, cookie, NULL);
	if (reply)
		free(reply);

	/* Initial move to position window and preview */
	drag_move(x, y);

	xcb_flush(wm->connection);
	_LOG_(INFO, "drag started for window %d (LIVE PREVIEW)", win);
	return 0;
}

/* drag_move - handle cursor movement during drag */
int
drag_move(int16_t x, int16_t y)
{
	if (!drag_state.active)
		return 0;

	drag_state.cur_x = x;
	drag_state.cur_y = y;

	/* Find target partition under cursor */
	node_t *root	 = curr_monitor->desk->tree;
	node_t *target	 = find_leaf_at_point(root, x, y);

	if (!target || target == drag_state.src_node) {
		if (drag_state.last_target) {
			preview_clear();
			drag_state.last_target = NULL;
		}
	} else if (target != drag_state.last_target) {
		preview_clear();
		preview_apply(target);
		drag_state.last_target = drag_state.preview_active ? target : NULL;
	}

	/* Move the dragged window to cursor (centered) */
	int16_t new_x = x - (drag_state.original_rect.width / 2);
	int16_t new_y = y - (drag_state.original_rect.height / 2);
	move_window(drag_state.window, new_x, new_y);

	return 0;
}

/* drag_end - commit or cancel drag */
int
drag_end(int16_t x, int16_t y)
{
	if (!drag_state.active)
		return 0;

	node_t *root   = curr_monitor->desk->tree;
	node_t *target = find_leaf_at_point(root, x, y);

	preview_clear();
	drag_state.last_target = NULL;

	if (!target || target == drag_state.src_node) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	if (!unlink_node(drag_state.src_node, curr_monitor->desk)) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	insert_node(target, drag_state.src_node, curr_monitor->desk->layout);
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree(curr_monitor->desk->tree);

cleanup:
	ungrab_pointer();
	// xcb_change_window_attributes(wm->connection,
	// 							 wm->root_window,
	// 							 XCB_CW_CURSOR,
	// 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});
	drag_state.active		  = false;
	drag_state.preview_active = false;

	xcb_flush(wm->connection);

	_LOG_(INFO, "drag ended");
	return 0;
}

/* drag_cancel - abort drag, restore original state */
int
drag_cancel(void)
{
	if (!drag_state.active)
		return 0;

	_LOG_(INFO, "drag cancelled");

	preview_clear();
	drag_state.last_target = NULL;

	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree_nomap(curr_monitor->desk->tree);

	ungrab_pointer();
	// xcb_change_window_attributes(wm->connection,
	// 							 wm->root_window,
	// 							 XCB_CW_CURSOR,
	// 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});

	drag_state.active		  = false;
	drag_state.preview_active = false;

	xcb_flush(wm->connection);

	return 0;
}

/* start_keyboard_drag_wrapper - initiate keyboard-driven drag */
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

	/* start drag from center of focused window */
	int16_t cx = n->rectangle.x + n->rectangle.width / 2;
	int16_t cy = n->rectangle.y + n->rectangle.height / 2;

	/* Move mouse pointer to center of window so drag starts smoothly */
	xcb_warp_pointer(
		wm->connection, XCB_NONE, wm->root_window, 0, 0, 0, 0, cx, cy);
	xcb_flush(wm->connection);

	return drag_start(n->client->window, cx, cy, true);
}

static void
preview_restore_layout(void)
{
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree_nomap(curr_monitor->desk->tree);
}

static void
preview_apply(node_t *target)
{
	if (!target || !target->client)
		return;

	node_t *root = curr_monitor->desk->tree;
	if (!root)
		return;

	node_t *preview_root = clone_tree(root, NULL);
	if (!preview_root)
		return;

	desktop_t preview_desk = {0};
	preview_desk.tree	   = preview_root;
	preview_desk.layout	   = curr_monitor->desk->layout;

	node_t *preview_src =
		find_node_by_window_id(preview_root, drag_state.window);
	node_t *preview_target =
		find_node_by_window_id(preview_root, target->client->window);
	if (!preview_src || !preview_target || preview_src == preview_target) {
		free_tree(preview_root);
		return;
	}

	if (!unlink_node(preview_src, &preview_desk)) {
		free_tree(preview_desk.tree);
		return;
	}

	insert_node(preview_target, preview_src, preview_desk.layout);
	arrange_tree(preview_desk.tree, preview_desk.layout);
	apply_preview_layout(preview_desk.tree);
	free_tree(preview_desk.tree);

	drag_state.preview_active = true;
}

static void
preview_clear(void)
{
	if (!drag_state.preview_active)
		return;

	preview_restore_layout();
	drag_state.preview_active = false;
}
