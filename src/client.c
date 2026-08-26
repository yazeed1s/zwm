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
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "client.h"
#include "actions.h"
#include "desktop.h"
#include "ewmh.h"
#include "focus.h"
#include "helper.h"
#include "layout.h"
#include "mouse.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include "view.h"
#include "xcb_util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

static int
show_window(xcb_window_t win, bool update_hidden_state);
static int
hide_window(xcb_window_t win, bool update_hidden_state);

static void
fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r);
static int
send_client_message(xcb_window_t win,
					xcb_atom_t	 property,
					xcb_atom_t	 value,
					xcb_conn_t	*conn);

void
get_class_name(xcb_window_t win, char **out)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		/* out should be freed after it's copied over in the caller function */
		*out = strdup(t_reply.class_name);
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
}

void
get_wm_name(xcb_window_t win, char **out)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	uint8_t					  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		/* out should be freed after it's copied over in the caller function */
		*out = strdup(t_reply.name);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}
}

client_t *
create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn)
{
	client_t *c = (client_t *)calloc(1, sizeof(client_t));
	if (c == 0x00)
		return NULL;

	/*memset(c->class_name, 0, sizeof(c->class_name));
	memset(c->wm_name, 0, sizeof(c->wm_name));
	char *wm_name	 = NULL;
	char *class_name = NULL;
	get_class_name(win, &class_name);
	get_wm_name(win, &wm_name);
	if (wm_name && class_name) {
		strncpy(c->class_name, class_name, sizeof(c->class_name) - 1);
		strncpy(c->wm_name, wm_name, sizeof(c->wm_name) - 1);
_LOG_(INFO, "created client wm_class %s", c->class_name);
_LOG_(INFO, "created client wm_name %s", c->wm_name);
		_FREE_(wm_name);
		_FREE_(class_name);
	}*/

	c->window				= win;
	c->type					= wtype;
	c->border_width			= (uint32_t)-1;
	/* props are not being utlized, will use them in the future */
	c->props.delete_window	= false;
	c->props.input_hint		= false;
	c->props.take_focus		= false;
	c->mru_seq				= 0;
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {CLIENT_EVENT_MASK};
	xcb_cookie_t   cookie =
		xcb_change_window_attributes_checked(conn, c->window, mask, values);
	xcb_error_t *err = xcb_request_check(conn, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error setting window attributes for client %u: %d",
			  c->window,
			  err->error_code);
		_FREE_(err);
		_FREE_(c);
		return NULL;
	}

	if (change_border_attr(wm->connection,
						   win,
						   conf.normal_border_color,
						   conf.border_width,
						   false) != 0) {
		_LOG_(ERROR, "failed to change border attr for window %d", win);
		_FREE_(c);
		return NULL;
	}

	return c;
}

bool
supports_protocol(xcb_window_t win, xcb_atom_t atom, xcb_conn_t *conn)
{
	xcb_get_property_cookie_t		   cookie = {0};
	xcb_icccm_get_wm_protocols_reply_t protocols;
	bool							   result = false;
	xcb_atom_t WM_PROTOCOLS					  = get_atom("WM_PROTOCOLS", conn);

	cookie = xcb_icccm_get_wm_protocols(conn, win, WM_PROTOCOLS);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1) {
		return false;
	}

	for (uint32_t i = 0; i < protocols.atoms_len; i++) {
		if (protocols.atoms[i] == atom) {
			result = true;
		}
	}

	xcb_icccm_get_wm_protocols_reply_wipe(&protocols);

	return result;
}

int
display_client(rectangle_t r, xcb_window_t win)
{
	if (apply_window_geometry(win, r, conf.border_width) != 0) {
		return -1;
	}

	xcb_cookie_t cookie = xcb_map_window_checked(wm->connection, win);
	xcb_error_t *err	= xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(
			ERROR, "in mapping window %d: error code %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

static int
send_client_message(xcb_window_t win,
					xcb_atom_t	 property,
					xcb_atom_t	 value,
					xcb_conn_t	*conn)
{
	xcb_client_message_event_t *e = calloc(32, 1);
	e->response_type			  = XCB_CLIENT_MESSAGE;
	e->window					  = win;
	e->type						  = property;
	e->format					  = 32;
	e->data.data32[0]			  = value;
	e->data.data32[1]			  = XCB_CURRENT_TIME;
	xcb_cookie_t c				  = xcb_send_event_checked(
		conn, false, win, XCB_EVENT_MASK_NO_EVENT, (char *)e);

	xcb_error_t *err = xcb_request_check(conn, c);
	if (err) {
		_LOG_(ERROR, "error sending event: %d", err->error_code);
		_FREE_(e);
		_FREE_(err);
		return -1;
	}

	xcb_flush(conn);
	_FREE_(e);
	return 0;
}

int
close_or_kill(xcb_window_t win)
{
	xcb_atom_t wm_delete = get_atom("WM_DELETE_WINDOW", wm->connection);
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);

	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (supports_protocol(win, wm_delete, wm->connection)) {
		if (wr == 1) {
#ifdef _DEBUG__
			_LOG_(DEBUG,
				  "window id = %d, reply name = %s: supports "
				  "WM_DELETE_WINDOW",
				  win,
				  t_reply.name);
#endif
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		}
		int ret = send_client_message(
			win, wm->ewmh->WM_PROTOCOLS, wm_delete, wm->connection);
		if (ret != 0) {
			_LOG_(ERROR, "failed to send client message");
			return -1;
		}
		return 0;
	}

	xcb_cookie_t c	 = xcb_kill_client_checked(wm->connection, win);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(
			ERROR, "error closing window: %d, error: %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

void
map_floating(xcb_window_t x)
{
	rectangle_t				  rc = {0};
	xcb_get_geometry_reply_t *g	 = get_geometry(x, wm->connection);
	if (g == NULL) {
		return;
	}

	rc.height = g->height;
	rc.width  = g->width;
	rc.x	  = g->x;
	rc.y	  = g->y;

	_FREE_(g);
	apply_window_geometry(x, rc, conf.border_width);
	xcb_map_window(wm->connection, x);
}

bool
client_exist_in_desktops(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; ++i) {
			if (!is_tree_empty(curr->desktops[i]->tree)) {
				if (client_exist(curr->desktops[i]->tree, win))
					return true;
			}
		}
		curr = curr->next;
	}
	return false;
}

void
find_window_in_desktops(desktop_t  **curr_desktop,
						node_t	   **curr_node,
						xcb_window_t win,
						bool		*found)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			desktop_t *d = curr->desktops[i];
			node_t	  *n = find_node_by_window_id(d->tree, win);
			if (n) {
				*curr_desktop = d;
				*curr_node	  = n;
				*found		  = true;
				_LOG_(DEBUG, "window %d found in desktop %d", win, i);
				return;
			}
		}
		curr = curr->next;
	}
	_LOG_(ERROR, "window %d not found in any desktop", win);
}

int
kill_window(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "[KILL_WINDOW] KILL WINDOW START ");
	_LOG_(DEBUG,
		  "[KILL_WINDOW] killing win=%d name='%s'",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	if (win == XCB_NONE) {
#ifdef _DEBUG__
		_LOG_(DEBUG, "[KILL_WINDOW] win is XCB_NONE, aborting");
#endif
		return -1;
	}

	if (win == wm->root_window) {
		_LOG_(INFO, "root window, returning %d", win);
		return 0;
	}

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);

	if (wr == 1) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG, "delete window id = %d, reply name = %s", win, t_reply.name);
#endif
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}

	desktop_t *d			   = curr_monitor->desk;
	node_t	  *n			   = find_node_by_window_id(d->tree, win);
	client_t  *c			   = (n) ? n->client : NULL;
	bool	   another_desktop = false;
	if (c == NULL) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[KILL_WINDOW] win %d not in current desktop %d, searching other "
			  "desktops",
			  win,
			  d->id);
#endif
		/* window isn't in current desktop */
		find_window_in_desktops(&d, &n, win, &another_desktop);
		c = (n) ? n->client : NULL;
		if (c == NULL) {
			_LOG_(ERROR, "cannot find client with window %d", win);
			return -1;
		}
#ifdef _DEBUG__
		_LOG_(DEBUG, "[KILL_WINDOW] found win %d in desktop %d", win, d->id);
#endif
	} else {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[KILL_WINDOW] found win %d in current desktop %d",
			  win,
			  d->id);
#endif
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] unmapping win=%d before deletion", c->window);
#endif
	xcb_cookie_t cookie = xcb_unmap_window(wm->connection, c->window);
	xcb_error_t *err	= xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR,
			  "error in unmapping window %d: error code %d",
			  c->window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] calling delete_node for win=%d", c->window);
#endif
	delete_node(n, d);
	ewmh_update_client_list();

	if (is_tree_empty(d->tree)) {
		d->logical_focus = NULL;
		d->last_focused	 = XCB_NONE;
		if (!another_desktop) {
			set_active_window_name(XCB_NONE);
			focused_win = XCB_NONE;
		}
	} else {
		/* pick fallback before rendering so MONOCLE/DECK never flicker to a
		 * hidden window between delete and the next render */
		node_t *nn = _pick_focus_(d);
		if (nn) {
			_focus_node_(d, nn);
			if (!another_desktop)
				nn->client->mru_seq = get_next_mru_seq(curr_monitor);
		}
	}

	if (!another_desktop) {
		if (_render_view_(d) != 0) {
			_LOG_(ERROR, "cannot render tree");
			return -1;
		}
		if (!is_tree_empty(d->tree) && d->logical_focus &&
			d->logical_focus->client) {
			_focus_input_(d, d->logical_focus);
			focused_win = d->logical_focus->client->window;
			set_active_window_name(focused_win);
		}
		_flush_view_(d);
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] kill_window complete for win=%d", win);
#endif
	return 0;
}

node_t *
find_node_global(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(
		DEBUG,
		"[FIND_NODE_GLOBAL] searching for win=%d name='%s' across all desktops",
		win,
		name ? name : "(null)");
	_FREE_(name);
#endif
	monitor_t *m = head_monitor;

	while (m) {
		for (int i = 0; i < m->n_of_desktops; i++) {
			desktop_t *d = m->desktops[i];
			if (!d || !d->tree)
				continue;
			node_t *n = find_node_by_window_id(d->tree, win);
			if (n) {
#ifdef _DEBUG__
				_LOG_(DEBUG,
					  "[FIND_NODE_GLOBAL] window %d found in monitor='%s' "
					  "desktop=%d",
					  win,
					  m->name,
					  d->id);
#endif
				return n;
			}
		}
		m = m->next;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[FIND_NODE_GLOBAL] window %d not found in ANY desktop", win);
#endif
	return NULL;
}

bool
should_manage(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_window_attributes_cookie_t attr_cookie;
	xcb_get_window_attributes_reply_t *attr_reply;

	attr_cookie		 = xcb_get_window_attributes(conn, win);
	xcb_error_t *err = NULL;
	attr_reply		 = xcb_get_window_attributes_reply(conn, attr_cookie, &err);

	if (err) {
		_LOG_(DEBUG,
			  "cannot get attributes for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return false;
	}
	if (attr_reply == NULL) {
		return false;
	}

	bool manage = !attr_reply->override_redirect;
	_FREE_(attr_reply);
	return manage;
}

int
apply_floating_hints(xcb_window_t win)
{
	xcb_get_property_cookie_t c =
		xcb_icccm_get_wm_normal_hints(wm->connection, win);
	xcb_size_hints_t size_hints;

	uint8_t			 r = xcb_icccm_get_wm_normal_hints_reply(
		wm->connection, c, &size_hints, NULL);
	if (1 == r) {
		/* if min-h == max-h && min-w == max-w, */
		/* then window should be floated */
		uint32_t size_mask =
			(XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE);
		int32_t miw = size_hints.min_width;
		int32_t mxw = size_hints.max_width;
		int32_t mih = size_hints.min_height;
		int32_t mxh = size_hints.max_height;

		if ((size_hints.flags & size_mask) && (miw == mxw) && (mih == mxh)) {
			return 0;
		}
	}
	return -1;
}

bool
should_ignore_hints(xcb_window_t win, const char *name)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		if (strcasecmp(t_reply.class_name, name) == 0) {
			xcb_icccm_get_wm_class_reply_wipe(&t_reply);
			return true;
		}
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
	return false;
}

bool
is_transient(xcb_window_t win)
{
	xcb_window_t			  transient = XCB_NONE;
	xcb_get_property_cookie_t c =
		xcb_icccm_get_wm_transient_for(wm->connection, win);
	const uint8_t r = xcb_icccm_get_wm_transient_for_reply(
		wm->connection, c, &transient, NULL);

	if (r != 1) {
		return false;
	}

	if (transient != XCB_NONE) {
		return true;
	}
	return false;
}

int
handle_first_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling first ever window %s id %d", name, client->window);
	_FREE_(name);
#endif
	rectangle_t r = {0};
	fill_root_rectangle(&r);

	if (client == NULL) {
		_LOG_(ERROR, "client is null");
		return -1;
	}

	d->tree			   = init_root();
	d->tree->client	   = client;
	d->tree->rectangle = r;
	d->n_count += 1;
	update_net_wm_desktop(client->window, d->id);
	set_focus(d->tree, true);
	/*d->node = d->tree;*/
	ewmh_update_client_list();
	client->mru_seq = get_next_mru_seq(curr_monitor);
	int ret			= tile(d->tree);
	restack();
	return ret;
}

int
handle_subsequent_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling tiled window %s id %d", name, client->window);
	_FREE_(name);
#endif
	xcb_window_t wi = get_window_under_cursor(wm->connection, wm->root_window);
	node_t		*n	= NULL;

	if (client == NULL) {
		_LOG_(ERROR, "client is null");
		return -1;
	}

	n = get_focused_node(d->tree);

	if (n == NULL || n->client == NULL) {
		_LOG_(ERROR, "cannot find node with window id");
		return -1;
	}

	if (IS_FLOATING(n->client) && !IS_ROOT(n)) {
		_LOG_(INFO, "node under cursor is floating %d", n->client->window);
		n = find_any_leaf(d->tree);
		if (n == NULL) {
			return 0;
		}
	}

	if (IS_FULLSCREEN(n->client)) {
		set_fullscreen(n, false);
	}

	node_t *new_node = create_node(client);
	if (new_node == NULL) {
		_LOG_(ERROR, "new node is null");
		return -1;
	}

	insert_node(n, new_node, d->layout);
	d->n_count += 1;
	update_net_wm_desktop(client->window, d->id);
	/*curr_monitor->desk->node = new_node;*/
	ewmh_update_client_list();
	client->mru_seq = get_next_mru_seq(curr_monitor);

	/* set logical focus before rendering so MONOCLE/DECK show the new window */
	_focus_node_(d, new_node);

	int ret = _render_view_(d);
	if (ret == 0)
		ret = _focus_input_(d, new_node);
	_flush_view_(d);
	return ret;
}

static void
fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r)
{
	int x  = curr_monitor->rectangle.x + (curr_monitor->rectangle.width / 2) -
			 (geometry->width / 2);
	int y  = curr_monitor->rectangle.y + (curr_monitor->rectangle.height / 2) -
			 (geometry->height / 2);
	(*r).x = x;
	(*r).y = y;
	(*r).width	= geometry->width;
	(*r).height = geometry->height;
}

static bool
client_is_modal_transient(const client_t *c)
{
	if (!c)
		return false;
	return c->ewmh_type == WINDOW_TYPE_DIALOG ||
		   ewmh_has(c->ewmh_state, EWMH_STATE_MODAL) ||
		   c->transient_for != XCB_NONE;
}

static int
focus_modal_transient(desktop_t *d, node_t *n)
{
	if (!d || !n || !n->client)
		return 0;

	if (focused_win != XCB_NONE)
		win_focus(focused_win, false);
	if (_focus_input_(d, n) != 0)
		return -1;

	n->is_focused	   = true;
	focused_win		   = n->client->window;
	n->client->mru_seq = get_next_mru_seq(curr_monitor);
	set_active_window_name(focused_win);
	return 0;
}

int
handle_floating_window(client_t *client, desktop_t *d)
{
	/* floating is not just "tile=false".
	 * dialogs/transients can take real focus, but hidden layouts still need
	 * their old tiled logical_focus or monocle/deck jumps around. */
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling floating window %s id %d", name, client->window);
	_FREE_(name);
#endif

	xcb_get_geometry_reply_t *g = NULL;
	if (is_tree_empty(d->tree)) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[HANDLE_FLOATING] tree is empty, creating root node for win=%d",
			  client->window);
#endif
		d->tree			= init_root();
		d->tree->client = client;
		g				= get_geometry(client->window, wm->connection);
		if (g == NULL) {
			_LOG_(ERROR, "cannot get %d geometry", client->window);
			return -1;
		}
		fill_floating_rectangle(g, &d->tree->floating_rectangle);
		fill_root_rectangle(&d->tree->rectangle);
		_FREE_(g);
		d->n_count += 1;
		update_net_wm_desktop(client->window, d->id);
		ewmh_update_client_list();
		set_focus(d->tree, true);
		client->mru_seq = get_next_mru_seq(curr_monitor);
		int ret			= tile(d->tree);
		if (client_is_modal_transient(client)) {
			focused_win = client->window;
			set_active_window_name(focused_win);
		}
		restack();
		return ret;
	} else {
		const bool modal_transient = client_is_modal_transient(client);
		node_t	  *n			   = NULL;

		if (client->transient_for != XCB_NONE)
			n = find_node_by_window_id(d->tree, client->transient_for);

		if (!n) {
			xcb_window_t wi =
				get_window_under_cursor(wm->connection, wm->root_window);
			if (wi != wm->root_window && wi != 0)
				n = find_node_by_window_id(d->tree, wi);
		}

		n = n == NULL ? find_any_leaf(d->tree) : n;
		if (n == NULL || n->client == NULL) {
			_FREE_(client);
			return -1;
		}

		node_t *new_node = create_node(client);
		if (new_node == NULL) {
			_FREE_(client);
			_LOG_(ERROR, "new node is null");
			return -1;
		}

		g = get_geometry(client->window, wm->connection);
		if (g == NULL) {
			_LOG_(ERROR, "cannot get %d geometry", client->window);
			return -1;
		}
		fill_floating_rectangle(g, &new_node->floating_rectangle);
		new_node->rectangle = new_node->floating_rectangle;
		_FREE_(g);
		insert_node(n, new_node, d->layout);
		if (d->logical_focus == n && n->first_child && n->first_child->client) {
			d->logical_focus = n->first_child;
			if (IS_TILED(n->first_child->client))
				d->last_focused = n->first_child->client->window;
		}
		d->n_count += 1;
		update_net_wm_desktop(client->window, d->id);
		ewmh_update_client_list();
		client->mru_seq = get_next_mru_seq(curr_monitor);
		/* modal/transient blocks parent. focus it for real, but do not make
		 * it the hidden layout tiled selection. */
		if (modal_transient) {
			int ret = _render_view_(d);
			if (ret == 0)
				ret = focus_modal_transient(d, new_node);
			_flush_view_(d);
			return ret;
		}
		/* floating spawn must not steal logical_focus in monocle/deck.
		 * normal layouts are fine, they dont hide tiled windows. */
		if (d->layout != MONOCLE && d->layout != DECK)
			_focus_node_(d, new_node);
		int ret = _render_view_(d);
		if (ret == 0)
			ret = _focus_input_(d, new_node);
		_flush_view_(d);
		return ret;
	}
}

int
insert_into_desktop(int idx, xcb_window_t win, bool is_tiled)
{
	desktop_t *d = curr_monitor->desktops[--idx];
	assert(d);
	if (find_node_by_window_id(d->tree, win)) {
		return 0;
	}

	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = is_tiled ? TILED : FLOATING;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	update_net_wm_desktop(client->window, d->id);
	set_window_state(client->window, XCB_ICCCM_WM_STATE_ICONIC);
	if (client->state == FLOATING) {
		xcb_get_geometry_reply_t *g = NULL;
		if (is_tree_empty(d->tree)) {
			d->tree			= init_root();
			d->tree->client = client;
			g				= get_geometry(client->window, wm->connection);
			if (g == NULL) {
				_LOG_(ERROR, "cannot get %d geometry", client->window);
				return -1;
			}
			fill_floating_rectangle(g, &d->tree->floating_rectangle);
			fill_root_rectangle(&d->tree->rectangle);
			_FREE_(g);
			d->n_count += 1;
			ewmh_update_client_list();
		} else {
			node_t *n = NULL;
			n		  = find_any_leaf(d->tree);
			if (n == NULL || n->client == NULL) {
				char *name = win_name(win);
				_LOG_(INFO, "cannot find win  %s:%d", name, win);
				_FREE_(name);
				return 0;
			}
			node_t *new_node = create_node(client);
			if (new_node == NULL) {
				_FREE_(client);
				_LOG_(ERROR, "new node is null");
				return -1;
			}
			g = get_geometry(client->window, wm->connection);
			if (g == NULL) {
				_LOG_(ERROR, "cannot get %d geometry", client->window);
				return -1;
			}
			fill_floating_rectangle(g, &new_node->floating_rectangle);
			new_node->rectangle = new_node->floating_rectangle;
			_FREE_(g);
			insert_node(n, new_node, d->layout);
			d->n_count += 1;
			ewmh_update_client_list();
		}
	} else {
		if (is_tree_empty(d->tree)) {
			rectangle_t r = {0};
			fill_root_rectangle(&r);
			d->tree			   = init_root();
			d->tree->client	   = client;
			d->tree->rectangle = r;
			d->n_count += 1;
			ewmh_update_client_list();
		} else {
			node_t *n = NULL;
			n		  = find_any_leaf(d->tree);
			if (n == NULL || n->client == NULL) {
				_LOG_(ERROR, "cannot find node with window id");
				return -1;
			}
			if (IS_FULLSCREEN(n->client)) {
				set_fullscreen(n, false);
			}
			if (n->client->state == FLOATING) {
				return 0;
			}
			node_t *new_node = create_node(client);
			if (new_node == NULL) {
				_FREE_(client);
				_LOG_(ERROR, "new node is null");
				return -1;
			}
			insert_node(n, new_node, d->layout);
			d->n_count += 1;
			if (d->layout == STACK) {
				set_focus(new_node, true);
			}
			ewmh_update_client_list();
		}
	}
	restack();
	return 0;
}

int
handle_tiled_window_request(xcb_window_t win, desktop_t *d)
{
	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = TILED;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	fill_icccm_ewmh(client);

	if (is_tree_empty(d->tree)) {
		return handle_first_window(client, d);
	}

	return handle_subsequent_window(client, d);
}

int
handle_floating_window_request(xcb_window_t win, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "window %s id %d is floating", name, win);
	_FREE_(name);
#endif
	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = FLOATING;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	fill_icccm_ewmh(client);
	return handle_floating_window(client, d);
}

int
find_desktop_by_window(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			desktop_t *d = curr->desktops[i];
			node_t	  *n = find_node_by_window_id(d->tree, win);
			if (n) {
				return d->id;
			}
		}
		curr = curr->next;
	}
	return -1;
}

int
handle_net_wm_desktop(xcb_window_t win, uint32_t index)
{
	if (index > wm->ewmh->_NET_NUMBER_OF_DESKTOPS - 1) {
		return 0;
	}
	desktop_t *td	 = curr_monitor->desktops[index];
	node_t	  *n	 = NULL;
	desktop_t *d	 = NULL;
	bool	   found = false;
	find_window_in_desktops(&d, &n, win, &found);
	if (!found) {
		return 0;
	}
	if (!n || !d) {
		_LOG_(ERROR, "desktop or node is null");
		return -1;
	}
	if (set_desktop_visibility(n->client->window, false) != 0) {
		_LOG_(ERROR, "cannot hide window %d", n->client->window);
		return -1;
	}
	if (unlink_node(n, d)) {
		if (!transfer_node(n, td)) {
			_LOG_(ERROR, "could not transfer node.. abort");
			return -1;
		}
	} else {
		_LOG_(ERROR, "could not unlink node.. abort");
		return -1;
	}

	d->n_count--;
	td->n_count++;
	update_net_wm_desktop(n->client->window, td->id);
	arrange_tree(td->tree, td->layout);
	if (td->layout == STACK) {
		set_focus(n, true);
	}
	if (!is_tree_empty(d->tree)) {
		arrange_tree(d->tree, d->layout);
	}

	bool render = curr_monitor->desk == d;
	return render ? _render_view_(d) : 0;
}

#if 0
static int
show_window(xcb_window_t win, node_t *n)
{
	if (win == XCB_NONE || n == NULL) {
_LOG_(ERROR, "show_window: invalid args (win=%d, node=%p)", win, n);
		return -1;
	}

	if (!n->client) {
_LOG_(ERROR, "show_window: node has no client for win=%d", win);
		return -1;
	}

	rectangle_t r =
		IS_FLOATING(n->client) ? n->floating_rectangle : n->rectangle;
	uint32_t	   vals[4] = {r.x, r.y, r.width, r.height};

	const uint16_t mask	   = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
						  XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

	_LOG_(DEBUG,
		  "showing window %d at (%d, %d) size (%d x %d)%s",
		  win,
		  r.x,
		  r.y,
		  r.width,
		  r.height,
		  IS_FLOATING(n->client) ? " [FLOATING]" : "");

	xcb_cookie_t ck =
		xcb_configure_window_checked(wm->connection, win, mask, vals);

	xcb_error_t *err = xcb_request_check(wm->connection, ck);
	if (err) {
		_LOG_(ERROR,
			  "show_window: failed to show window %d "
			  "pos (%d,%d) size (%d,%d) error=%d",
			  win,
			  r.x,
			  r.y,
			  r.width,
			  r.height,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}
#endif

static int
show_window(xcb_window_t win, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[SHOW_WINDOW] showing win=%d name='%s' (setting WM_STATE to NORMAL, "
		  "then mapping)",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	xcb_error_t		*err;
	xcb_cookie_t	 c;
	/* According to ewmh:
	 * Mapped windows should be placed in NormalState, according to
	 * the ICCCM */
	const long		 data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, wm_s, wm_s, 32, 2, data);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	if (update_hidden_state) {
		update_net_wm_state_atom(win, wm->ewmh->_NET_WM_STATE_HIDDEN, false);
		node_t *n = find_node_global(win);
		if (n && n->client) {
			update_client_ewmh_state(n->client, EWMH_STATE_HIDDEN, false);
		}
	}

	c	= xcb_map_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SHOW_WINDOW] successfully mapped win=%d", win);
#endif
	return 0;
}

#if 0
static int
hide_window(xcb_window_t win)
{
	if (win == XCB_NONE) {
_LOG_(ERROR, "invalid window (XCB_NONE)");
		return -1;
	}

	const int32_t  offscreen = -20000;
	const uint32_t values[2] = {offscreen, offscreen};
	const uint16_t mask		 = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;

_LOG_(DEBUG, "hiding window %d at (%d, %d)", win, offscreen, offscreen);

	xcb_cookie_t ck =
		xcb_configure_window_checked(wm->connection, win, mask, values);

	xcb_error_t *err = xcb_request_check(wm->connection, ck);
	if (err) {
		_LOG_(ERROR,
			  "failed to configure window %d offscreen (error=%d)",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}
#endif

static int
hide_window(xcb_window_t win, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[HIDE_WINDOW] hiding win=%d name='%s' (setting WM_STATE to ICONIC, "
		  "then unmapping)",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	xcb_error_t		*err;
	xcb_cookie_t	 c;
	/* According to ewmh:
	 * Unmapped windows should be placed in IconicState, according to
	 * the ICCCM. Windows which are actually iconified or minimized
	 * should have the _NET_WM_STATE_HIDDEN property set, to
	 * communicate to pagers that the window should not be represented
	 * as "onscreen."
	 **/
	const long		 data[] = {XCB_ICCCM_WM_STATE_ICONIC, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, wm_s, wm_s, 32, 2, data);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	if (update_hidden_state) {
		update_net_wm_state_atom(win, wm->ewmh->_NET_WM_STATE_HIDDEN, true);
		node_t *n = find_node_global(win);
		if (n && n->client) {
			update_client_ewmh_state(n->client, EWMH_STATE_HIDDEN, true);
		}
	}

	c	= xcb_unmap_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[HIDE_WINDOW] successfully unmapped win=%d", win);
#endif
	return 0;
}

static int
set_visibility_mode(xcb_window_t win, bool is_visible, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[VISIBILITY] set_visibility called: win=%d name='%s' is_visible=%s",
		  win,
		  name ? name : "(null)",
		  is_visible ? "TRUE" : "FALSE");
	_FREE_(name);
#endif
	/* zwm must NOT recieve events before mapping (showing) or unmapping
	 * (hiding) windows.
	 * otherwise, it will recieve unmap/map notify and handle it as it
	 * should, this results in deleting or spanning the window that is
	 * meant to be hidden or shown */
	const uint32_t _off[] = {ROOT_EVENT_MASK &
							 ~XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	const uint32_t _on[]  = {ROOT_EVENT_MASK};
	xcb_error_t	  *err;
	xcb_cookie_t   c;
	int			   ret = 0;

	/* stop zwm from recieving events */
	c				   = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _off);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	/* node_t *n = find_node_global(win); */
	/* if (!n) { */
	/* _LOG_(ERROR, "cannot fin win %d", win); */
	/* return -1; */
	/* } */
	/* ret = is_visible ? show_window(win, n) : hide_window(win); */
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[VISIBILITY] calling %s for win=%d",
		  is_visible ? "show_window" : "hide_window",
		  win);
#endif
	ret = is_visible ? show_window(win, update_hidden_state)
					 : hide_window(win, update_hidden_state);
	if (ret == -1) {
		_LOG_(
			ERROR, "cannot set visibilty to %s", is_visible ? "true" : "false");
#ifdef _DEBUG__
	} else {
		_LOG_(DEBUG,
			  "[VISIBILITY] successfully set visibility to %s for win=%d",
			  is_visible ? "true" : "false",
			  win);
#endif
	}

	/* subscribe for events again */
	c = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _on);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[VISIBILITY] set_visibility completed successfully for win=%d",
		  win);
#endif
	return 0;
}

int
set_visibility(xcb_window_t win, bool is_visible)
{
	return set_visibility_mode(win, is_visible, true);
}

int
set_desktop_visibility(xcb_window_t win, bool is_visible)
{
	return set_visibility_mode(win, is_visible, false);
}
