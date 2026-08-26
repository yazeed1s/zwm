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

#include "view.h"
#include "client.h"
#include "desktop.h"
#include "focus.h"
#include "helper.h"
#include "queue.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include <xcb/xcb.h>

leaf_visibility_t
_leaf_visibility_(desktop_t *d, node_t *leaf)
{
	/* normal layouts map tiled leaves.
	 * monocle/deck hide most tiled leaves, but floating is always shown. */
	if (!leaf || IS_INTERNAL(leaf) || !leaf->client)
		return LEAF_IGNORED;

	if (IS_FLOATING(leaf->client))
		return LEAF_VISIBLE_FLOATING;

	switch (d->layout) {
	case DEFAULT:
	case MASTER:
	case STACK:
	case GRID:
	case THREE_COL: return LEAF_VISIBLE_TILED;

	case MONOCLE: {
		/* one tiled node, picked from logical_focus */
		node_t *focus = d->logical_focus;
		if (!focus || IS_FLOATING(focus->client))
			focus = pick_desktop_focus(d);
		return (leaf == focus) ? LEAF_VISIBLE_TILED : LEAF_HIDDEN_TILED;
	}

	case DECK: {
		if (leaf->is_master)
			return LEAF_VISIBLE_TILED;
		/* master + one node from the deck side */
		node_t *focus = d->logical_focus;
		if (!focus || IS_FLOATING(focus->client) || focus->is_master)
			focus = pick_deck_focus(d);
		return (leaf == focus) ? LEAF_VISIBLE_TILED : LEAF_HIDDEN_TILED;
	}
	}

	return LEAF_VISIBLE_TILED;
}

/* BFS renderer */
static int
_render_tree_view_(desktop_t *d)
{
	if (!d || is_tree_empty(d->tree))
		return 0;

	queue_t *q = create_queue();
	if (!q)
		return -1;

	enqueue(q, d->tree);
	int rc = 0;

	while (q->front) {
		node_t *cur = dequeue(q);

		if (IS_INTERNAL(cur) || !cur->client) {
			if (cur->first_child)
				enqueue(q, cur->first_child);
			if (cur->second_child)
				enqueue(q, cur->second_child);
			continue;
		}

		if (IS_FULLSCREEN(cur->client)) {
			/* map before resize; some clients ignore the reverse order */
			if (set_desktop_visibility(cur->client->window, true) != 0) {
				rc = -1;
				break;
			}
			if (_handle_fullscreen_window(cur->client->window) != 0) {
				rc = -1;
				break;
			}
			continue;
		}

		leaf_visibility_t vis = _leaf_visibility_(d, cur);
		switch (vis) {
		case LEAF_VISIBLE_TILED:
		case LEAF_VISIBLE_FLOATING:
			if (set_desktop_visibility(cur->client->window, true) != 0) {
				rc = -1;
				goto done;
			}
			if (tile(cur) != 0) {
				rc = -1;
				goto done;
			}
			break;
		case LEAF_HIDDEN_TILED:
			if (set_desktop_visibility(cur->client->window, false) != 0) {
				rc = -1;
				goto done;
			}
			break;
		case LEAF_IGNORED: break;
		}
	}

done:
	free_queue(q);
	return rc;
}

void
_focus_node_(desktop_t *d, node_t *n)
{
	if (!d || !n)
		return;
	d->logical_focus = n;
	d->last_focused	 = n->client ? n->client->window : XCB_NONE;
	n->is_focused	 = true;
	update_focus(d, n);
}

int
_focus_input_(desktop_t *d, node_t *n)
{
	if (!d || !n || !n->client)
		return 0;
	/* win_focus sets border colour and X input focus; it does not raise */
	return win_focus(n->client->window, true);
}

node_t *
_pick_focus_(desktop_t *d)
{
	if (!d)
		return NULL;
	if (d->layout == DECK)
		return pick_deck_focus(d);
	return pick_desktop_focus(d);
}

int
_render_view_(desktop_t *d)
{
	if (!d)
		return 0;
	return _render_tree_view_(d);
}

void
_flush_view_(desktop_t *d)
{
	(void)d;
	restack();
	xcb_flush(wm->connection);
}
