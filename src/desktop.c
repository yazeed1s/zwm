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

#include "desktop.h"
#include "client.h"
#include "ewmh.h"
#include "focus.h"
#include "helper.h"
#include "monitor.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include "view.h"
#include "xcb_util.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

static void
update_focused_desktop(int id);
static node_t *
find_deck_stack_focus(node_t *root);

static void
remember_desktop_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return;

	node_t *n = get_focused_node(d->tree);
	if (n && n->client)
		d->last_focused = n->client->window;
}

node_t *
pick_desktop_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return NULL;

	node_t *n = get_focused_node(d->tree);
	if (n && n->client && IS_TILED(n->client))
		return n;

	if (d->last_focused != XCB_NONE &&
		window_exists(wm->connection, d->last_focused)) {
		n = find_node_by_window_id(d->tree, d->last_focused);
		if (n && n->client && IS_TILED(n->client))
			return n;
	}

	return find_any_leaf(d->tree);
}

node_t *
pick_deck_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return NULL;

	if (d->last_focused != XCB_NONE &&
		window_exists(wm->connection, d->last_focused)) {
		node_t *n = find_node_by_window_id(d->tree, d->last_focused);
		if (n && n->client && IS_TILED(n->client) && !n->is_master)
			return n;
	}

	node_t *n = get_focused_node(d->tree);
	if (n && n->client && IS_TILED(n->client) && !n->is_master)
		return n;

	return find_deck_stack_focus(d->tree);
}

static node_t *
find_deck_stack_focus(node_t *root)
{
	if (!root)
		return NULL;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client) &&
		!root->is_master)
		return root;

	node_t *n = find_deck_stack_focus(root->first_child);
	if (n)
		return n;
	return find_deck_stack_focus(root->second_child);
}

int
render_desktop(desktop_t *d)
{
	if (d == NULL || is_tree_empty(d->tree))
		return 0;

	return view_render_desktop(d);
}

void
render_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (!curr->desktops) {
			curr = curr->next;
			continue;
		}
		for (int i = 0; i < curr->n_of_desktops; i++) {
			if (is_tree_empty(curr->desktops[i]->tree))
				continue;
			if (curr->desktops[i]->is_focused)
				render_desktop(curr->desktops[i]);
			else /* keep X geometry correct for hidden desktops */
				render_tree_nomap(curr->desktops[i]->tree);
		}
		curr = curr->next;
	}
}

int
get_focused_desktop_idx(void)
{
	if (curr_monitor == NULL) {
		_LOG_(ERROR, "curr_monitor is null");
		return -1;
	}
	for (int i = curr_monitor->n_of_desktops; i--;) {
		if (curr_monitor->desktops[i]->is_focused) {
			return curr_monitor->desktops[i]->id;
		}
	}
	_LOG_(ERROR, "cannot find curr monitor focused desktop");
	return -1;
}

desktop_t *
get_focused_desktop(void)
{
	monitor_t *fm = get_focused_monitor();
	for (int i = fm->n_of_desktops; i--;) {
		if (fm->desktops[i] && fm->desktops[i]->is_focused) {
			return fm->desktops[i];
		}
	}

	return NULL;
}

/* useles after
 * https://github.com/yazeed1s/zwm/pull/36 */
static node_t *
get_foucsed_desktop_tree__(void)
{
	int i = get_focused_desktop_idx();
	assert(i >= 0 && i < conf.virtual_desktops);
	node_t *root = curr_monitor->desktops[i]->tree;
	return root;
}

desktop_t *
init_desktop(void)
{
	desktop_t *d = (desktop_t *)malloc(sizeof(desktop_t));
	if (d == 0x00)
		return NULL;
	d->id			 = 0;
	d->is_focused	 = false;
	d->n_count		 = 0;
	d->tree			 = NULL;
	d->last_focused	 = XCB_NONE;
	d->logical_focus = NULL;
	/*d->node	  = NULL;*/
	return d;
}

bool
setup_desktops(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		/* becaues this function is also called when monitors change, we need
		 * to skip old monitors in the list. */
		if (curr && curr->desktops) {
			_LOG_(INFO,
				  "monitor %s already has desktops... skipping",
				  curr->name);
			curr = curr->next;
			continue;
		}
		curr->n_of_desktops = conf.virtual_desktops;
		desktop_t **desktops =
			(desktop_t **)malloc(sizeof(desktop_t *) * curr->n_of_desktops);
		if (desktops == NULL) {
			_LOG_(ERROR, "failed to malloc desktops");
			return false;
		}
		curr->desktops = desktops;
		for (int j = 0; j < curr->n_of_desktops; j++) {
			desktop_t *d  = init_desktop();
			d->id		  = (uint16_t)j;
			d->is_focused = (j == 0);
			d->layout	  = DEFAULT;
			snprintf(d->name, sizeof(d->name), "%d", j + 1);
			curr->desktops[j] = d;
		}
		curr->desk = curr->desktops[0];
		_LOG_(INFO, "successfuly assigned desktops for monitor %s", curr->name);
		curr = curr->next;
	}
	return true;
}

static void
update_focused_desktop(int id)
{
	if (curr_monitor == NULL) {
		return;
	}
	for (int i = 0; i < curr_monitor->n_of_desktops; ++i) {
		if (curr_monitor->desktops[i]->id != id) {
			curr_monitor->desktops[i]->is_focused = false;
		} else {
			curr_monitor->desktops[i]->is_focused = true;
			curr_monitor->desk					  = curr_monitor->desktops[i];
		}
	}
}

int
switch_desktop(const int nd)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SWITCH_DESKTOP] ========== DESKTOP SWITCH START ==========");
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] switching from desktop %d to desktop %d",
		  curr_monitor->desk->id,
		  nd);
#endif
	if (nd > conf.virtual_desktops) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[SWITCH_DESKTOP] requested desktop %d > max %d, aborting",
			  nd,
			  conf.virtual_desktops);
#endif
		return 0;
	}

	desktop_t *old_desktop	  = curr_monitor->desk;
	node_t	  *tree_to_hide	  = old_desktop->tree;
	node_t	  *tree_to_show	  = curr_monitor->desktops[nd]->tree;
	desktop_t *target_desktop = curr_monitor->desktops[nd];

	if (curr_monitor->desk == curr_monitor->desktops[nd]) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG, "[SWITCH_DESKTOP] already on desktop %d, nothing to do", nd);
#endif
		return 0;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SWITCH_DESKTOP] updating focused desktop to %d", nd);
#endif
	remember_desktop_focus(old_desktop);
	update_focused_desktop(nd);

#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] calling hide_windows for desktop %d tree",
		  curr_monitor->desk->id);
#endif
	if (hide_windows(tree_to_hide) != 0) {
#ifdef _DEBUG__
		_LOG_(ERROR, "[SWITCH_DESKTOP] hide_windows failed for old desktop");
#endif
		return -1;
	}

	set_active_window_name(XCB_NONE);
	win_focus(focused_win, false);
	focused_win = XCB_NONE;

	if (!is_tree_empty(tree_to_show)) {
		/* pick logical focus for the target desktop, and honours
		 * restore_last_focus for all layouts except STACK, which never restores
		 * by position */
		node_t *focus = NULL;
		if (conf.restore_last_focus && target_desktop->layout != STACK) {
			xcb_window_t win = target_desktop->last_focused;
			if (win != XCB_NONE && window_exists(wm->connection, win)) {
				node_t *n = find_node_by_window_id(tree_to_show, win);
				/* only restore tiled nodes as logical focus */
				if (n && n->client && IS_TILED(n->client))
					focus = n;
			}
		}
		if (!focus)
			focus = view_pick_fallback_focus(target_desktop);

		if (focus) {
			view_set_logical_focus(target_desktop, focus);
			focus->client->mru_seq = get_next_mru_seq(curr_monitor);
#ifdef _DEBUG__
			_LOG_(DEBUG,
				  "[SWITCH_DESKTOP] logical focus -> win=%d on desktop %d",
				  focus->client->window,
				  nd);
#endif
		}

		/* render, maps/unmaps windows according to layout policy */
		if (view_render_desktop(target_desktop) != 0)
			return -1;

		/* apply X input focus after windows are mapped only */
		if (focus && focus->client) {
			if (view_apply_input_focus(target_desktop, focus) != 0)
				return -1;
			focused_win = focus->client->window;
			set_active_window_name(focused_win);
		}

		/* restack + flush x server */
		view_commit(target_desktop);
	}

#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", nd + 1);
	log_tree_nodes(tree_to_show);
	_LOG_(INFO, "old desktop %d nodes--------------", old_desktop->id + 1);
	log_tree_nodes(tree_to_hide);
#endif

	if (ewmh_update_current_desktop(wm->ewmh, wm->screen_nbr, nd) != 0) {
#ifdef _DEBUG__
		_LOG_(ERROR, "[SWITCH_DESKTOP] ewmh_update_current_desktop failed");
#endif
		return -1;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] ========== DESKTOP SWITCH COMPLETE ==========");
#endif
	return 0;
}

void
fill_root_rectangle(rectangle_t *r)
{
	rectangle_t usable = get_usable_area(curr_monitor);
	(*r).x			   = usable.x + conf.window_gap;
	(*r).y			   = usable.y + conf.window_gap;
	(*r).width	= usable.width - 2 * conf.window_gap - 2 * conf.border_width;
	(*r).height = usable.height - 2 * conf.window_gap - 2 * conf.border_width;
}

int
handle_net_desktop_change(uint32_t nd)
{
	if (!curr_monitor || nd >= (uint32_t)curr_monitor->n_of_desktops) {
		return -1;
	}
	return switch_desktop(nd);
}

int
handle_net_active_window(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(
		DEBUG,
		"[NET_ACTIVE_WINDOW] received _NET_ACTIVE_WINDOW for win=%d name='%s'",
		win,
		name ? name : "(null)");
	_FREE_(name);
#endif
	int d = find_desktop_by_window(win);
	if (d == -1) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG,
			"[NET_ACTIVE_WINDOW] window %d not found in any desktop, ignoring",
			win);
#endif
		return 0;
	}

	return switch_desktop(d);
}
