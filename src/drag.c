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
#include "cursor.h"

#include <stdbool.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include "helper.h"
#include "state.h"
#include "tree.h"
#include "layout.h"
#include "type.h"
#include "xcb_util.h"

drag_state_t ds = {0};
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
		if (root->client->window != ds.window) {
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

	ds.window			 = win;
	ds.src_node			 = n;
	ds.start_x			 = x;
	ds.start_y			 = y;
	ds.cur_x			 = x;
	ds.cur_y			 = y;
	ds.active			 = true;
	ds.kbd_mode			 = kbd;
	ds.last_target		 = NULL;
	ds.preview_active	 = false;

	/* save the original state in case we need
	 * to revert on cancel or error */
	ds.original_desktop	 = curr_monitor->desk;
	ds.original_rect	 = n->rectangle;

	/* pop the window to the top layer so it doesn't get covered.
	 * dragged windows are always on top */
	const uint32_t val[] = {XCB_STACK_MODE_ABOVE};
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
	if (!ds.active)
		return 0;

	ds.cur_x	   = x;
	ds.cur_y	   = y;

	/* figure out which partition is under the cursor */
	node_t *root   = curr_monitor->desk->tree;
	node_t *target = find_leaf_at_point(root, x, y);

	if (!target || target == ds.src_node) {
		if (ds.last_target) {
			preview_clear();
			ds.last_target = NULL;
		}
	} else if (target != ds.last_target) {
		preview_clear();
		preview_apply(target);
		ds.last_target = ds.preview_active ? target : NULL;
	}

	/* center the window on the cursor */
	int16_t new_x = x - (ds.original_rect.width / 2);
	int16_t new_y = y - (ds.original_rect.height / 2);
	move_window(ds.window, new_x, new_y);

	return 0;
}

/* ends the drag session, committing changes */
int
drag_end(int16_t x, int16_t y)
{
	if (!ds.active)
		return 0;

	node_t *root   = curr_monitor->desk->tree;
	node_t *target = find_leaf_at_point(root, x, y);

	preview_clear();
	ds.last_target = NULL;

	if (!target || target == ds.src_node) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	if (!unlink_node(ds.src_node, curr_monitor->desk)) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	insert_node(target, ds.src_node, curr_monitor->desk->layout);
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree(curr_monitor->desk->tree);

cleanup:
	ungrab_pointer();
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/
	ds.active		  = false;
	ds.preview_active = false;

	xcb_flush(wm->connection);

	_LOG_(INFO, "drag ended");
	return 0;
}

/* cancel the drag and put everything back how it was */
int
drag_cancel(void)
{
	if (!ds.active)
		return 0;

	_LOG_(INFO, "drag cancelled");

	preview_clear();
	ds.last_target = NULL;

	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree_nomap(curr_monitor->desk->tree);

	ungrab_pointer();
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/

	ds.active		  = false;
	ds.preview_active = false;

	xcb_flush(wm->connection);

	return 0;
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

	node_t *ps	   = find_node_by_window_id(pr, ds.window);
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

	ds.preview_active = true;
}

static void
preview_clear(void)
{
	if (!ds.preview_active)
		return;

	preview_restore_layout();
	ds.preview_active = false;
}
