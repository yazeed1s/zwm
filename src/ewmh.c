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

#include "ewmh.h"
#include "helper.h"
#include "state.h"
#include "xcb_util.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

static xcb_ewmh_conn_t *
ewmh_init(xcb_conn_t *conn);
static int
ewmh_set_supporting(xcb_window_t win, xcb_ewmh_conn_t *ewmh);
static int
ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd);
static size_t
get_active_clients_size(desktop_t **d, const int n);
static void
populate_client_array(node_t *root, xcb_window_t *arr, size_t *index);
static ewmh_window_type_t
determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom);

bool
ewmh_has(ewmh_state_t s, ewmh_state_t f)
{
	return (s & f) != 0;
}

static xcb_ewmh_conn_t *
ewmh_init(xcb_conn_t *conn)
{
	if (conn == 0x00) {
		_LOG_(ERROR, "connection is NULL");
		return NULL;
	}

	xcb_ewmh_conn_t *ewmh = calloc(1, sizeof(xcb_ewmh_conn_t));
	if (ewmh == NULL) {
		_LOG_(ERROR, "cannot calloc ewmh");
		return NULL;
	}

	xcb_intern_atom_cookie_t *c = xcb_ewmh_init_atoms(conn, ewmh);
	if (c == 0x00) {
		_LOG_(ERROR, "cannot init intern atom");
		return NULL;
	}

	const uint8_t res = xcb_ewmh_init_atoms_replies(ewmh, c, NULL);
	if (res != 1) {
		_LOG_(ERROR, "cannot init intern atom");
		return NULL;
	}
	return ewmh;
}

static int
ewmh_set_supporting(xcb_window_t win, xcb_ewmh_conn_t *ewmh)
{
	pid_t		 wm_pid = getpid();
	xcb_cookie_t supporting_cookie_root =
		xcb_ewmh_set_supporting_wm_check_checked(ewmh, wm->root_window, win);
	xcb_cookie_t supporting_cookie =
		xcb_ewmh_set_supporting_wm_check_checked(ewmh, win, win);
	xcb_cookie_t name_cookie =
		xcb_ewmh_set_wm_name_checked(ewmh, win, strlen(WM_NAME), WM_NAME);
	xcb_cookie_t pid_cookie =
		xcb_ewmh_set_wm_pid_checked(ewmh, win, (uint32_t)wm_pid);

	xcb_error_t *err;
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie_root))) {
		_LOG_(ERROR, "error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie))) {
		_LOG_(ERROR, "error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, name_cookie))) {
		_LOG_(ERROR, "error setting WM name: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, pid_cookie))) {
		_LOG_(ERROR, "error setting WM PID: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd)
{
	xcb_cookie_t c =
		xcb_ewmh_set_number_of_desktops_checked(ewmh, screen_nbr, nd);
	xcb_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

int
ewmh_update_desktop_names(void)
{
	char		 names[MAXLEN];
	uint32_t	 names_len = 0;
	unsigned int offset	   = 0;
	memset(names, 0, sizeof(names));

	for (int n = 0; n < prim_monitor->n_of_desktops; n++) {
		desktop_t *d  = prim_monitor->desktops[n];
		size_t	   nn = sizeof(names);
		for (int j = 0; d->name[j] != '\0' && (offset + j) < nn; j++) {
			names[offset + j] = d->name[j];
		}
		offset += strlen(d->name);
		if (offset < sizeof(names)) {
			names[offset++] = '\0';
		}
	}

	names_len	   = offset - 1;
	xcb_cookie_t c = xcb_ewmh_set_desktop_names_checked(
		wm->ewmh, wm->screen_nbr, names_len, names);
	xcb_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting names of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static size_t
get_active_clients_size(desktop_t **d, const int n)
{
	if (!d)
		return -1;
	size_t t = 0;
	for (int i = 0; i < n; ++i) {
		if (!d[i])
			continue;
		t += d[i]->n_count;
	}
	return t;
}

static void
populate_client_array(node_t *root, xcb_window_t *arr, size_t *index)
{
	if (root == NULL)
		return;

	if (root->client && root->client->window != XCB_NONE) {
		arr[*index] = root->client->window;
		(*index)++;
	}

	populate_client_array(root->first_child, arr, index);
	populate_client_array(root->second_child, arr, index);
}

void
ewmh_update_client_list(void)
{
	monitor_t *m	= head_monitor;
	size_t	   size = 0;
	while (m) {
		if (!m->desktops)
			continue;
		size += get_active_clients_size(m->desktops, m->n_of_desktops);
		m = m->next;
	}
	if (size == 0) {
		xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, 0, NULL);
		return;
	}
	xcb_window_t *active_clients =
		(xcb_window_t *)malloc((size + 1) * sizeof(xcb_window_t));
	if (active_clients == NULL) {
		return;
	}
	size_t	   index = 0;
	monitor_t *curr	 = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; ++i) {
			node_t *root = curr->desktops[i]->tree;
			populate_client_array(root, active_clients, &index);
		}
		curr = curr->next;
	}
	xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, size, active_clients);
	_FREE_(active_clients);
}

int
ewmh_update_current_desktop(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t i)
{
	xcb_cookie_t c = xcb_ewmh_set_current_desktop_checked(ewmh, screen_nbr, i);
	xcb_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

void
ewmh_update_desktop_viewport(void)
{
	uint32_t   count = 0;
	monitor_t *curr	 = head_monitor;
	while (curr) {
		count += curr->n_of_desktops;
		curr = curr->next;
	}
	if (count == 0) {
		xcb_ewmh_set_desktop_viewport(wm->ewmh, wm->screen_nbr, 0, NULL);
		return;
	}
	xcb_ewmh_coordinates_t coords[count];
	uint16_t			   desktop = 0;
	curr						   = head_monitor;
	while (curr) {
		for (int j = 0; j < curr->n_of_desktops; j++) {
			coords[desktop++] =
				(xcb_ewmh_coordinates_t){curr->rectangle.x, curr->rectangle.y};
		}
		curr = curr->next;
	}
	xcb_ewmh_set_desktop_viewport(wm->ewmh, wm->screen_nbr, desktop, coords);
}

int
ewmh_update_number_of_desktops(void)
{
	uint32_t desktops_count = 0;
	desktops_count			= prim_monitor->n_of_desktops;
	return ewmh_set_number_of_desktops(
		wm->ewmh, wm->screen_nbr, desktops_count);
}

bool
setup_ewmh(void)
{
	wm->ewmh = ewmh_init(wm->connection);
	if (wm->ewmh == NULL) {
		return false;
	}

	xcb_atom_t	 net_atoms[] = {wm->ewmh->_NET_SUPPORTED,
								wm->ewmh->_NET_SUPPORTING_WM_CHECK,
								wm->ewmh->_NET_DESKTOP_NAMES,
								wm->ewmh->_NET_DESKTOP_VIEWPORT,
								wm->ewmh->_NET_NUMBER_OF_DESKTOPS,
								wm->ewmh->_NET_CURRENT_DESKTOP,
								wm->ewmh->_NET_CLIENT_LIST,
								wm->ewmh->_NET_ACTIVE_WINDOW,
								wm->ewmh->_NET_WM_NAME,
								wm->ewmh->_NET_CLOSE_WINDOW,
								wm->ewmh->_NET_WM_STRUT_PARTIAL,
								wm->ewmh->_NET_WM_DESKTOP,
								wm->ewmh->_NET_WM_STATE,
								wm->ewmh->_NET_WM_STATE_FULLSCREEN,
								wm->ewmh->_NET_WM_STATE_BELOW,
								wm->ewmh->_NET_WM_STATE_ABOVE,
								wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION,
								wm->ewmh->_NET_WM_WINDOW_TYPE,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DOCK,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DESKTOP,
								wm->ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DIALOG,
								wm->ewmh->_NET_WM_WINDOW_TYPE_SPLASH,
								wm->ewmh->_NET_WM_WINDOW_TYPE_UTILITY,
								wm->ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR};

	xcb_cookie_t c			 = xcb_ewmh_set_supported_checked(
		wm->ewmh, wm->screen_nbr, LEN(net_atoms), net_atoms);
	xcb_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting supported ewmh masks: %d", err->error_code);
		_FREE_(err);
		return false;
	}

	if (ewmh_set_supporting(wm->root_window, wm->ewmh) != 0) {
		return false;
	}

	if (ewmh_update_number_of_desktops() != 0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	if (ewmh_update_current_desktop(
			wm->ewmh, wm->screen_nbr, (uint32_t)curr_monitor->desk->id) != 0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	ewmh_update_desktop_viewport();

	return true;
}

ewmh_state_t
get_net_wm_state_mask(xcb_window_t win)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return EWMH_STATE_NONE;

	ewmh_state_t			   mask = EWMH_STATE_NONE;
	xcb_get_property_cookie_t  ck	= xcb_ewmh_get_wm_state(wm->ewmh, win);
	xcb_ewmh_get_atoms_reply_t rep;
	if (!xcb_ewmh_get_wm_state_reply(wm->ewmh, ck, &rep, NULL)) {
		return EWMH_STATE_NONE;
	}

	for (unsigned i = 0; i < rep.atoms_len; ++i) {
		xcb_atom_t a = rep.atoms[i];
		if (a == wm->ewmh->_NET_WM_STATE_ABOVE)
			mask |= EWMH_STATE_ABOVE;
		else if (a == wm->ewmh->_NET_WM_STATE_BELOW)
			mask |= EWMH_STATE_BELOW;
		else if (a == wm->ewmh->_NET_WM_STATE_FULLSCREEN)
			mask |= EWMH_STATE_FULLSCREEN;
		else if (a == wm->ewmh->_NET_WM_STATE_MODAL)
			mask |= EWMH_STATE_MODAL;
		else if (a == wm->ewmh->_NET_WM_STATE_HIDDEN)
			mask |= EWMH_STATE_HIDDEN;
		else if (a == wm->ewmh->_NET_WM_STATE_STICKY)
			mask |= EWMH_STATE_STICKY;
		else if (a == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
			mask |= EWMH_STATE_DEMANDS_ATTN;
	}

	xcb_ewmh_get_atoms_reply_wipe(&rep);
	return mask;
}

ewmh_state_t
ewmh_flag_for_atom(xcb_atom_t atom)
{
	if (atom == wm->ewmh->_NET_WM_STATE_ABOVE)
		return EWMH_STATE_ABOVE;
	if (atom == wm->ewmh->_NET_WM_STATE_BELOW)
		return EWMH_STATE_BELOW;
	if (atom == wm->ewmh->_NET_WM_STATE_FULLSCREEN)
		return EWMH_STATE_FULLSCREEN;
	if (atom == wm->ewmh->_NET_WM_STATE_MODAL)
		return EWMH_STATE_MODAL;
	if (atom == wm->ewmh->_NET_WM_STATE_HIDDEN)
		return EWMH_STATE_HIDDEN;
	if (atom == wm->ewmh->_NET_WM_STATE_STICKY)
		return EWMH_STATE_STICKY;
	if (atom == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
		return EWMH_STATE_DEMANDS_ATTN;
	return EWMH_STATE_NONE;
}

void
update_client_ewmh_state(client_t *c, ewmh_state_t flag, bool set)
{
	if (!c || flag == EWMH_STATE_NONE)
		return;
	if (set)
		c->ewmh_state |= flag;
	else
		c->ewmh_state &= ~flag;
}

void
remove_property(xcb_connection_t *con,
				xcb_window_t	  win,
				xcb_atom_t		  prop,
				xcb_atom_t		  atom)
{
	xcb_grab_server(con);
	xcb_get_property_cookie_t c = xcb_get_property(
		con, false, win, prop, XCB_GET_PROPERTY_TYPE_ANY, 0, 4096);
	xcb_get_property_reply_t *reply = xcb_get_property_reply(con, c, NULL);
	if (reply == NULL || xcb_get_property_value_length(reply) == 0)
		goto release_grab;
	const xcb_atom_t *atoms = xcb_get_property_value(reply);
	if (atoms == NULL) {
		goto release_grab;
	}

	{
		int		  num = 0;
		const int curr_size =
			xcb_get_property_value_length(reply) / (reply->format / 8);
		xcb_atom_t values[curr_size];
		memset(values, 0, sizeof(values));
		for (int i = 0; i < curr_size; i++) {
			if (atoms[i] != atom)
				values[num++] = atoms[i];
		}
		xcb_change_property(con,
							XCB_PROP_MODE_REPLACE,
							win,
							prop,
							XCB_ATOM_ATOM,
							32,
							num,
							values);
	}

release_grab:
	if (reply)
		_FREE_(reply);
	xcb_ungrab_server(con);
}

int
update_net_wm_state_atom(xcb_window_t win, xcb_atom_t atom, bool set)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return -1;

	const ewmh_state_t flag = ewmh_flag_for_atom(atom);
	const ewmh_state_t mask = get_net_wm_state_mask(win);

	if (set) {
		if (flag != EWMH_STATE_NONE && (mask & flag))
			return 0;
		xcb_atom_t	 values[] = {atom};
		xcb_cookie_t c	 = xcb_change_property_checked(wm->connection,
													   XCB_PROP_MODE_APPEND,
													   win,
													   wm->ewmh->_NET_WM_STATE,
													   XCB_ATOM_ATOM,
													   32,
													   1,
													   values);
		xcb_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR,
				  "cannot append _NET_WM_STATE for %d: error code %d",
				  win,
				  err->error_code);
			_FREE_(err);
			return -1;
		}
		return 0;
	}

	remove_property(wm->connection, win, wm->ewmh->_NET_WM_STATE, atom);
	return 0;
}

void
fill_icccm_ewmh(client_t *c)
{
	xcb_window_t			  transient = XCB_NONE;
	xcb_get_property_cookie_t cv =
		xcb_icccm_get_wm_transient_for(wm->connection, c->window);
	xcb_icccm_get_wm_transient_for_reply(wm->connection, cv, &transient, NULL);

	c->transient_for = transient;

	xcb_get_window_attributes_cookie_t atc =
		xcb_get_window_attributes(wm->connection, c->window);
	xcb_get_window_attributes_reply_t *atr =
		xcb_get_window_attributes_reply(wm->connection, atc, NULL);
	c->override_redirect = (atr && atr->override_redirect);
	free(atr);

	c->ewmh_type  = window_type(c->window);
	c->ewmh_state = get_net_wm_state_mask(c->window);
}

int
set_active_window_name(xcb_window_t win)
{
	xcb_cookie_t aw_cookie =
		xcb_ewmh_set_active_window_checked(wm->ewmh, wm->screen_nbr, win);
	xcb_error_t *err = xcb_request_check(wm->connection, aw_cookie);

	if (err) {
		_LOG_(ERROR, "cannot setting active window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

int
set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state)
{
	const long	 data[] = {state, XCB_NONE};
	xcb_atom_t	 t		= get_atom("WM_STATE", wm->connection);
	xcb_cookie_t c		= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, t, t, 32, 2, data);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in changing property window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

int
update_net_wm_desktop(xcb_window_t win, uint32_t desktop)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return -1;
	xcb_cookie_t c	 = xcb_ewmh_set_wm_desktop_checked(wm->ewmh, win, desktop);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot set _NET_WM_DESKTOP for %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static ewmh_window_type_t
determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom)
{
	if (atom == ewmh->_NET_WM_WINDOW_TYPE_NORMAL) {
		return WINDOW_TYPE_NORMAL;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
		return WINDOW_TYPE_DOCK;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
		return WINDOW_TYPE_DESKTOP;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR ||
			   atom == ewmh->_NET_WM_WINDOW_TYPE_MENU) {
		return WINDOW_TYPE_TOOLBAR_MENU;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
		return WINDOW_TYPE_UTILITY;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
		return WINDOW_TYPE_SPLASH;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DIALOG) {
		return WINDOW_TYPE_DIALOG;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION) {
		return WINDOW_TYPE_NOTIFICATION;
	}
	return WINDOW_TYPE_NORMAL;
}

ewmh_window_type_t
window_type(xcb_window_t win)
{
	xcb_ewmh_get_atoms_reply_t w_type;
	xcb_get_property_cookie_t  c = xcb_ewmh_get_wm_window_type(wm->ewmh, win);

	const uint8_t			   r =
		xcb_ewmh_get_wm_window_type_reply(wm->ewmh, c, &w_type, NULL);

	if (r != 1) {
		return WINDOW_TYPE_UNKNOWN;
	}

	ewmh_window_type_t type = WINDOW_TYPE_NORMAL;
	for (unsigned int i = 0; i < w_type.atoms_len; ++i) {
		type = determine_window_type(wm->ewmh, w_type.atoms[i]);
		if (type != WINDOW_TYPE_NORMAL) {
			break;
		}
	}

	xcb_ewmh_get_atoms_reply_wipe(&w_type);
	return type;
}
