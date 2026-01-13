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
/* clang-format off */
static void apply_preview_layout(node_t *root);
static void preview_restore_layout(void);
static void preview_apply(node_t *target);
static void preview_clear(void);
/* clang-format on */

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

/* starts the drag session */
int
drag_start(xcb_window_t win, int16_t x, int16_t y, bool kbd)
{
	node_t *root = curr_monitor->desk->tree;
	node_t *n	 = find_node_by_window_id(root, win);

	if (!n || !n->client) {
		_LOG_(WARNING, "cannot drag: window not found");
		return -1;
	}

	/* we only care about dragging tiled windows here. floating windows
	 * are handled elsewhere, since they behave differently... */
	if (IS_FLOATING(n->client) || IS_FULLSCREEN(n->client)) {
		_LOG_(WARNING, "cannot drag floating or fullscreen windows");
		return -1;
	}

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

	/* save the original state in case we need
	 * to revert on cancel or error */
	drag_state.original_desktop = curr_monitor->desk;
	drag_state.original_rect	= n->rectangle;

	/* pop the window to the top layer so it doesn't get covered.
	 * dragged windows are always on top */
	const uint32_t val[]		= {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, win, XCB_CONFIG_WINDOW_STACK_MODE, val);

	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_MOVE)});*/

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

	drag_move(x, y);

	xcb_flush(wm->connection);
	_LOG_(INFO, "drag started for window %d (LIVE PREVIEW)", win);
	return 0;
}

/* handles cursor movement while dragging */
int
drag_move(int16_t x, int16_t y)
{
	if (!drag_state.active)
		return 0;

	drag_state.cur_x = x;
	drag_state.cur_y = y;

	/* figure out which partition is under the cursor */
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

	/* center the window on the cursor */
	int16_t new_x = x - (drag_state.original_rect.width / 2);
	int16_t new_y = y - (drag_state.original_rect.height / 2);
	move_window(drag_state.window, new_x, new_y);

	return 0;
}

/* ends the drag session, committing changes */
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
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/
	drag_state.active		  = false;
	drag_state.preview_active = false;

	xcb_flush(wm->connection);

	_LOG_(INFO, "drag ended");
	return 0;
}

/* cancel the drag and put everything back how it was */
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
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/

	drag_state.active		  = false;
	drag_state.preview_active = false;

	xcb_flush(wm->connection);

	return 0;
}

/* wrapper to start dragging via keyboard shortcut */
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

	/* assume we want to drag from the center of the focused window */
	int16_t cx = n->rectangle.x + n->rectangle.width / 2;
	int16_t cy = n->rectangle.y + n->rectangle.height / 2;

	/* warp the mouse to the center so the drag feels as smoth as possible */
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
preview_apply(node_t *t)
{
	if (!t || !t->client)
		return;

	node_t *r = curr_monitor->desk->tree;
	if (!r)
		return;

	node_t *pr = clone_tree(r, NULL);
	if (!pr)
		return;

	desktop_t desk = {0};
	desk.tree	   = pr;
	desk.layout	   = curr_monitor->desk->layout;

	node_t *ps	   = find_node_by_window_id(pr, drag_state.window);
	node_t *pt	   = find_node_by_window_id(pr, t->client->window);
	if (!ps || !pt || ps == pt) {
		free_tree(pr);
		return;
	}

	if (!unlink_node(ps, &desk)) {
		free_tree(desk.tree);
		return;
	}

	insert_node(pt, ps, desk.layout);
	arrange_tree(desk.tree, desk.layout);
	apply_preview_layout(desk.tree);
	free_tree(desk.tree);

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
