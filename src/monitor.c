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

#include "monitor.h"
#include "desktop.h"
#include "helper.h"
#include "layout.h"
#include "queue.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xinerama.h>

static monitor_t *
init_monitor(void);
static void
add_monitor(monitor_t **head, monitor_t *m);
static void
unlink_monitor(monitor_t **head, monitor_t *m);
static void
log_monitors(void);
static monitor_t *
get_monitor_by_randr_id(xcb_randr_output_t id);
static monitor_t *
get_monitor_by_root_id(xcb_window_t id);
static int
get_connected_monitor_count_xinerama(void);
static int
get_connected_monitor_count_xrandr(void);
static int
get_connected_monitor_count(bool xrandr, bool xinerama);
static bool
setup_monitors_via_xrandr(void);
static bool
setup_monitors_via_xinerama(void);
static bool
handle_added_monitor(xcb_randr_get_output_info_reply_t *info,
					 xcb_randr_output_t					id);
static void
destroy_monitor(monitor_t *m);
static bool
is_monitor_layout_changed(xcb_randr_get_output_info_reply_t *info,
						  rectangle_t						*r,
						  rectangle_t						*r_out);
static bool
merge_monitors(monitor_t *om, monitor_t *nm);
static bool
is_disconnected(monitor_t *m, monitor_t *dl);
static void
update_monitors(uint32_t *changes);
static bool
ranges_overlap(int32_t a_start, int32_t a_end, int32_t b_start, int32_t b_end);

static monitor_t *
init_monitor(void)
{
	monitor_t *m = (monitor_t *)malloc(sizeof(monitor_t));
	if (m == 0x00)
		return NULL;
	m->id		= 0;
	m->randr_id = XCB_NONE;
	snprintf(m->name, sizeof(m->name), "%s", MONITOR_NAME);
	m->root		   = XCB_NONE;
	m->rectangle   = (rectangle_t){0};
	m->padding	   = (padding_t){0};
	m->is_focused  = false;
	m->is_occupied = false;
	m->is_wired	   = false;
	m->next		   = NULL;
	m->desktops	   = NULL;
	m->desk		   = NULL;
	m->mru_counter = 1;
	return m;
}

void
add_strut_window(xcb_window_t win)
{
	if (is_strut_window(win))
		return;

	strut_win_node_t *node =
		(strut_win_node_t *)malloc(sizeof(strut_win_node_t));
	if (node == NULL)
		return;
	node->win	  = win;
	node->next	  = strut_windows;
	strut_windows = node;
}

bool
remove_strut_window(xcb_window_t win)
{
	strut_win_node_t *curr = strut_windows;
	strut_win_node_t *prev = NULL;

	while (curr) {
		if (curr->win == win) {
			if (prev) {
				prev->next = curr->next;
			} else {
				strut_windows = curr->next;
			}
#ifdef _DEBUG_
			char *name = win_name(win);
			_LOG_(DEBUG,
				  "[STRUT] removed strut window: %s 0x%x",
				  name ? name : "<unknown>",
				  win);
			_FREE_(name);
#endif
			_FREE_(curr);
			return true;
		}
		prev = curr;
		curr = curr->next;
	}
	return false;
}

bool
is_strut_window(xcb_window_t win)
{
	for (strut_win_node_t *curr = strut_windows; curr; curr = curr->next) {
		if (curr->win == win)
			return true;
	}
	return false;
}

void
cleanup_strut_windows(void)
{
	strut_win_node_t *curr = strut_windows;
	while (curr) {
		strut_win_node_t *next = curr->next;
		_FREE_(curr);
		curr = next;
	}
	strut_windows = NULL;
}

int
handle_unmanaged_strut_window(xcb_window_t win)
{
	if (!wm || !wm->connection || win == XCB_NONE)
		return -1;

	if (!ignore_ewmh_struts)
		recalculate_all_struts();
	xcb_map_window(wm->connection, win);
	return 0;
}

static bool
ranges_overlap(int32_t a_start, int32_t a_end, int32_t b_start, int32_t b_end)
{
	if (a_start > a_end) {
		int32_t tmp = a_start;
		a_start		= a_end;
		a_end		= tmp;
	}
	if (b_start > b_end) {
		int32_t tmp = b_start;
		b_start		= b_end;
		b_end		= tmp;
	}
	return a_start <= b_end && a_end >= b_start;
}

bool
ewmh_handle_struts(xcb_window_t win)
{
	if (!wm || !wm->ewmh || !wm->screen || win == XCB_NONE)
		return false;

	xcb_get_property_cookie_t ck = xcb_ewmh_get_wm_strut_partial(wm->ewmh, win);
	xcb_ewmh_wm_strut_partial_t strut;
	if (!xcb_ewmh_get_wm_strut_partial_reply(wm->ewmh, ck, &strut, NULL))
		return false;

	int32_t screen_w = wm->screen->width_in_pixels;
	int32_t screen_h = wm->screen->height_in_pixels;

	if (strut.left >= (uint32_t)screen_w || strut.right >= (uint32_t)screen_w ||
		strut.top >= (uint32_t)screen_h || strut.bottom >= (uint32_t)screen_h) {
		return false;
	}

	add_strut_window(win);

	bool changed = false;

	for (monitor_t *m = head_monitor; m; m = m->next) {
		int32_t mx1 = m->rectangle.x;
		int32_t my1 = m->rectangle.y;
		int32_t mx2 = m->rectangle.x + m->rectangle.width;
		int32_t my2 = m->rectangle.y + m->rectangle.height;

		if (strut.left > 0 && ranges_overlap((int32_t)strut.left_start_y,
											 (int32_t)strut.left_end_y,
											 my1,
											 my2)) {
			int32_t dx = (int32_t)strut.left - mx1;
			if (dx > 0) {
				if (dx > INT16_MAX)
					dx = INT16_MAX;
				int16_t prev	= m->padding.left;
				m->padding.left = MAX((int16_t)dx, m->padding.left);
				if (m->padding.left != prev)
					changed = true;
			}
		}

		if (strut.right > 0 && ranges_overlap((int32_t)strut.right_start_y,
											  (int32_t)strut.right_end_y,
											  my1,
											  my2)) {
			int32_t dx = mx2 - (screen_w - (int32_t)strut.right);
			if (dx > 0) {
				if (dx > INT16_MAX)
					dx = INT16_MAX;
				int16_t prev	 = m->padding.right;
				m->padding.right = MAX((int16_t)dx, m->padding.right);
				if (m->padding.right != prev)
					changed = true;
			}
		}

		if (strut.top > 0 && ranges_overlap((int32_t)strut.top_start_x,
											(int32_t)strut.top_end_x,
											mx1,
											mx2)) {
			int32_t dy = (int32_t)strut.top - my1;
			if (dy > 0) {
				if (dy > INT16_MAX)
					dy = INT16_MAX;
				int16_t prev   = m->padding.top;
				m->padding.top = MAX((int16_t)dy, m->padding.top);
				if (m->padding.top != prev)
					changed = true;
			}
		}

		if (strut.bottom > 0 && ranges_overlap((int32_t)strut.bottom_start_x,
											   (int32_t)strut.bottom_end_x,
											   mx1,
											   mx2)) {
			int32_t dy = my2 - (screen_h - (int32_t)strut.bottom);
			if (dy > 0) {
				if (dy > INT16_MAX)
					dy = INT16_MAX;
				int16_t prev	  = m->padding.bottom;
				m->padding.bottom = MAX((int16_t)dy, m->padding.bottom);
				if (m->padding.bottom != prev)
					changed = true;
			}
		}
	}

	return changed;
}

rectangle_t
get_usable_area(monitor_t *m)
{
	rectangle_t r = {0};
	if (!m)
		return r;

	int32_t x = m->rectangle.x + m->padding.left;
	int32_t y = m->rectangle.y + m->padding.top;
	int32_t w =
		(int32_t)m->rectangle.width - m->padding.left - m->padding.right;
	int32_t h =
		(int32_t)m->rectangle.height - m->padding.top - m->padding.bottom;

	if (w < 0)
		w = 0;
	if (h < 0)
		h = 0;

	r.x		 = (int16_t)x;
	r.y		 = (int16_t)y;
	r.width	 = (uint16_t)w;
	r.height = (uint16_t)h;

	return r;
}

void
recalculate_all_struts(void)
{
	if (!wm || !wm->connection || !wm->ewmh)
		return;

	for (monitor_t *m = head_monitor; m; m = m->next)
		m->padding = (padding_t){0};

	xcb_query_tree_cookie_t ck =
		xcb_query_tree(wm->connection, wm->root_window);
	xcb_query_tree_reply_t *rep =
		xcb_query_tree_reply(wm->connection, ck, NULL);
	if (rep == NULL) {
		_LOG_(ERROR, "failed to query root window tree");
		return;
	}

	int			  len	   = xcb_query_tree_children_length(rep);
	xcb_window_t *children = xcb_query_tree_children(rep);

	for (int i = 0; i < len; ++i) ewmh_handle_struts(children[i]);

	free(rep);

	arrange_trees();
	render_trees();
}

static void
add_monitor(monitor_t **head, monitor_t *m)
{
	if (*head == NULL) {
		*head = m;
		return;
	}
	monitor_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = m;
}

static void
unlink_monitor(monitor_t **head, monitor_t *m)
{
	if (!head || !*head || !m) {
		return;
	}

	monitor_t *curr = *head;
	monitor_t *prev = NULL;

	while (curr) {
		if (curr == m) {
			if (prev == NULL) {
				*head = curr->next;
			} else {
				prev->next = curr->next;
			}
			curr->next = NULL;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

static void
log_monitors(void)
{
	if (!head_monitor) {
		_LOG_(INFO, "monitors list is empty");
		return;
	}
	monitor_t *curr = head_monitor;
	while (curr) {
		_LOG_(INFO,
			  "found monitor %s:%d, rectangle {.x = %d, .y = %d, .w = %d, .h = "
			  "%d}",
			  curr->name,
			  curr->randr_id,
			  curr->rectangle.x,
			  curr->rectangle.y,
			  curr->rectangle.width,
			  curr->rectangle.height);
		curr = curr->next;
	}
}

static monitor_t *
get_monitor_by_randr_id(xcb_randr_output_t id)
{
	monitor_t *current = head_monitor;
	while (current) {
		if (current->randr_id == id) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

static monitor_t *
get_monitor_by_root_id(xcb_window_t id)
{
	monitor_t *current = head_monitor;
	while (current) {
		if (current->root == id) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

monitor_t *
get_focused_monitor(void)
{
	xcb_query_pointer_cookie_t pointer_cookie =
		xcb_query_pointer(wm->connection, wm->root_window);
	xcb_query_pointer_reply_t *pointer_reply =
		xcb_query_pointer_reply(wm->connection, pointer_cookie, NULL);

	if (pointer_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer");
		return NULL;
	}

	int		   pointer_x = pointer_reply->root_x;
	int		   pointer_y = pointer_reply->root_y;

	monitor_t *current	 = head_monitor;
	while (current) {
		if (pointer_x >= current->rectangle.x &&
			pointer_x < (current->rectangle.x + current->rectangle.width) &&
			pointer_y >= current->rectangle.y &&
			pointer_y < (current->rectangle.y + current->rectangle.height)) {
			_FREE_(pointer_reply);
			return current;
		}
		current = current->next;
	}

	_FREE_(pointer_reply);
	return NULL;
}

static int
get_connected_monitor_count_xinerama(void)
{
	xcb_xinerama_query_screens_cookie_t c =
		xcb_xinerama_query_screens(wm->connection);
	xcb_xinerama_query_screens_reply_t *xquery =
		xcb_xinerama_query_screens_reply(wm->connection, c, NULL);
	int len = xcb_xinerama_query_screens_screen_info_length(xquery);
	_FREE_(xquery);
	return len;
}

static int
get_connected_monitor_count_xrandr(void)
{
	xcb_randr_get_screen_resources_current_cookie_t c =
		xcb_randr_get_screen_resources_current(wm->connection, wm->root_window);
	xcb_randr_get_screen_resources_current_reply_t *sres =
		xcb_randr_get_screen_resources_current_reply(wm->connection, c, NULL);
	if (sres == NULL) {
		_LOG_(ERROR, "failed to get screen resources");
		return -1;
	}
	int len = xcb_randr_get_screen_resources_current_outputs_length(sres);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(sres);
	int monitor_count = 0;
	for (int i = 0; i < len; i++) {
		xcb_randr_get_output_info_cookie_t info_c = xcb_randr_get_output_info(
			wm->connection, outputs[i], XCB_CURRENT_TIME);
		xcb_randr_get_output_info_reply_t *info =
			xcb_randr_get_output_info_reply(wm->connection, info_c, NULL);
		if (info) {
			if (info->connection == XCB_RANDR_CONNECTION_CONNECTED) {
				monitor_count++;
			}
			_FREE_(info);
		}
	}
	_FREE_(sres);
	return monitor_count;
}

static int
get_connected_monitor_count(bool xrandr, bool xinerama)
{
	int n = 0;
	if (xrandr == true && xinerama == false) {
		n = get_connected_monitor_count_xrandr();
	} else if (xrandr == false && xinerama == true) {
		n = get_connected_monitor_count_xinerama();
	} else if (xrandr == true && xinerama == true) {
		_LOG_(WARNING, "huh?...");
	} else {
		n = 1;
	}
	return n;
}

static bool
setup_monitors_via_xrandr(void)
{
	xcb_connection_t							   *conn = wm->connection;
	xcb_window_t									root = wm->root_window;
	xcb_randr_get_screen_resources_current_cookie_t sc =
		xcb_randr_get_screen_resources_current(conn, root);
	xcb_randr_get_screen_resources_current_reply_t *sr =
		xcb_randr_get_screen_resources_current_reply(conn, sc, NULL);
	if (sr == NULL) {
		_LOG_(ERROR, "failed to query screen resources");
		return false;
	}
	const xcb_timestamp_t time = sr->config_timestamp;
	const int len = xcb_randr_get_screen_resources_current_outputs_length(sr);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(sr);
	xcb_randr_get_output_info_cookie_t oc[len];
	for (int i = 0; i < len; i++) {
		oc[i] = xcb_randr_get_output_info(conn, outputs[i], time);
	}
	int monitors = 0;
	for (int i = 0; i < len; i++) {
		xcb_randr_get_output_info_reply_t *info;
		if ((info = xcb_randr_get_output_info_reply(conn, oc[i], NULL)) ==
			NULL) {
			_LOG_(INFO, "could not query output info... skipping this output");
			continue;
		}
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			_LOG_(INFO, "output is disconnected... skipping this output");
			_FREE_(info);
			continue;
		}
		if (info->crtc == XCB_NONE) {
			_LOG_(INFO, "output crtc is empty... skipping this output");
			_FREE_(info);
			continue;
		}
		xcb_randr_get_crtc_info_cookie_t ic;
		xcb_randr_get_crtc_info_reply_t *crtc;
		ic = xcb_randr_get_crtc_info(conn, info->crtc, time);
		if ((crtc = xcb_randr_get_crtc_info_reply(conn, ic, NULL)) == NULL) {
			_LOG_(INFO,
				  "could not get CRTC (0x%08x)... skipping output",
				  info->crtc);
			_FREE_(info);
			continue;
		}
		char	  *name		= (char *)xcb_randr_get_output_info_name(info);
		size_t	   name_len = xcb_randr_get_output_info_name_length(info);
		monitor_t *m		= init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			_FREE_(info);
			_FREE_(crtc);
			_FREE_(sr);
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), "%.*s", (int)name_len, name);
		m->rectangle   = (rectangle_t){.x	   = crtc->x,
									   .y	   = crtc->y,
									   .width  = crtc->width,
									   .height = crtc->height};
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = outputs[i];
		m->next		   = NULL;
		m->desktops	   = NULL;
		add_monitor(&head_monitor, m);
		_LOG_(INFO,
			  "monitor name = %.*s:%d, out %d Monitor rectangle = x = %d, y = "
			  "%d, w = %d, h = %d",
			  (int)name_len,
			  name,
			  m->randr_id,
			  outputs[i],
			  crtc->x,
			  crtc->y,
			  crtc->width,
			  crtc->height);
		monitors++;
		_FREE_(crtc);
		_FREE_(info);
	}
	_FREE_(sr);
	_LOG_(INFO, "%d connected monitors", monitors);
	return true;
}

static bool
setup_monitors_via_xinerama(void)
{
	xcb_xinerama_query_screens_cookie_t query_screens_c =
		xcb_xinerama_query_screens(wm->connection);
	xcb_xinerama_query_screens_reply_t *query_screens_r =
		xcb_xinerama_query_screens_reply(wm->connection, query_screens_c, NULL);
	if (query_screens_r == NULL) {
		_LOG_(ERROR, "failed to query Xinerama screens");
		return false;
	}
	xcb_xinerama_screen_info_t *xinerama_screen_i =
		xcb_xinerama_query_screens_screen_info(query_screens_r);
	int n = xcb_xinerama_query_screens_screen_info_length(query_screens_r);
	for (int i = 0; i < n; i++) {
		xcb_xinerama_screen_info_t info = xinerama_screen_i[i];
		rectangle_t				   r =
			(rectangle_t){info.x_org, info.y_org, info.width, info.height};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			_FREE_(query_screens_r);
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), "Xinerama %d", i);
		m->rectangle   = r;
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = 0;
		add_monitor(&head_monitor, m);
	}

	_FREE_(query_screens_r);
	return true;
}

void
free_monitors(void)
{
	monitor_t *current = head_monitor;
	while (current) {
		monitor_t *next = current->next;
		for (int j = 0; j < current->n_of_desktops; j++) {
			if (current->desktops[j]) {
				if (current->desktops[j]->tree) {
					free_tree(current->desktops[j]->tree);
					current->desktops[j]->tree = NULL;
				}
				_FREE_(current->desktops[j]);
			}
		}
		_FREE_(current->desktops);
		_FREE_(current);
		current = next;
	}
	head_monitor = NULL;
}

int
get_monitors_count(void)
{
	monitor_t *curr = head_monitor;
	int		   n	= 0;
	while (curr) {
		n++;
		curr = curr->next;
	}
	return n;
}

bool
setup_monitors(void)
{
	bool							   use_global_screen = false;
	const xcb_query_extension_reply_t *query_xr			 = NULL;
	const xcb_query_extension_reply_t *query_x			 = NULL;

	query_xr = xcb_get_extension_data(wm->connection, &xcb_randr_id);
	query_x	 = xcb_get_extension_data(wm->connection, &xcb_xinerama_id);

	if (query_xr->present) {
		using_xrandr = true;
		randr_base	 = query_xr->first_event;
		xcb_randr_select_input(wm->connection,
							   wm->root_window,
							   XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	} else if (query_x->present) {
		bool							xinerama_is_active = false;
		xcb_xinerama_is_active_cookie_t xc =
			xcb_xinerama_is_active(wm->connection);
		xcb_xinerama_is_active_reply_t *xis_active =
			xcb_xinerama_is_active_reply(wm->connection, xc, NULL);
		if (xis_active) {
			xinerama_is_active = xis_active->state;
			_FREE_(xis_active);
			using_xinerama = xinerama_is_active;
		}
	} else {
		using_xrandr = using_xinerama = false;
	}

	int n = get_connected_monitor_count(using_xrandr, using_xinerama);

	if (!using_xrandr && !using_xinerama && n == 1) {
		_LOG_(ERROR, "neither Xrandr nor Xinerama extensions are available");
		use_global_screen = true;
	}

	if (use_global_screen) {
		rectangle_t r = (rectangle_t){
			0, 0, wm->screen->width_in_pixels, wm->screen->height_in_pixels};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), ROOT_WINDOW);
		m->rectangle   = r;
		m->root		   = wm->root_window;
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = 0;
		m->desktops	   = NULL;
		add_monitor(&head_monitor, m);
		prim_monitor = curr_monitor = m;
		goto out;
	}

	bool setup_success = false;
	if (using_xrandr) {
		setup_success = setup_monitors_via_xrandr();
		if (setup_success) {
			_LOG_(INFO, "monitors successfully set up using Xrandr");
		}
	} else if (using_xinerama) {
		setup_success = setup_monitors_via_xinerama();
		if (setup_success) {
			_LOG_(INFO, "monitors successfully set up using Xinerama");
		}
	}

	if (!setup_success) {
		_LOG_(ERROR, "failed to set up monitors, defaulting to global screen");
		return false;
	}

	xcb_randr_get_output_primary_cookie_t ccc =
		xcb_randr_get_output_primary(wm->connection, wm->root_window);
	xcb_randr_get_output_primary_reply_t *primary_output_reply =
		xcb_randr_get_output_primary_reply(wm->connection, ccc, NULL);
	if (primary_output_reply) {
		monitor_t *mm = get_monitor_by_randr_id(primary_output_reply->output);
		if (mm) {
			mm->is_primary = true;
			prim_monitor = curr_monitor = mm;
		} else {
			prim_monitor = curr_monitor = head_monitor;
		}
	} else {
		prim_monitor = curr_monitor = head_monitor;
	}

	_LOG_(INFO,
		  "primary monitor %s:%d id %d, rect = x %d, y %d,width %d,height %d",
		  prim_monitor->name,
		  prim_monitor->randr_id,
		  prim_monitor->root,
		  prim_monitor->rectangle.x,
		  prim_monitor->rectangle.y,
		  prim_monitor->rectangle.width,
		  prim_monitor->rectangle.height);

	_FREE_(primary_output_reply);

out:
	multi_monitors = (get_monitors_count() > 1);
	_LOG_(INFO, "multi monitors = %s", multi_monitors ? "true" : "false");
	xcb_flush(wm->connection);
	return true;
}

static bool
handle_added_monitor(xcb_randr_get_output_info_reply_t *info,
					 xcb_randr_output_t					id)
{
	xcb_randr_get_crtc_info_cookie_t crtc_c =
		xcb_randr_get_crtc_info(wm->connection, info->crtc, XCB_CURRENT_TIME);
	xcb_randr_get_crtc_info_reply_t *crtc =
		xcb_randr_get_crtc_info_reply(wm->connection, crtc_c, NULL);
	if (!crtc) {
		_LOG_(ERROR, "failed to query crtc for %d", id);
		return false;
	}
	char	  *name		= (char *)xcb_randr_get_output_info_name(info);
	size_t	   name_len = xcb_randr_get_output_info_name_length(info);
	monitor_t *m		= init_monitor();
	if (!m) {
		_LOG_(ERROR, "failed to allocate single monitor for output %d", id);
		_FREE_(crtc);
		return false;
	}
	memset(m->name, 0, sizeof(m->name));
	snprintf(m->name, sizeof(m->name), "%.*s", (int)name_len, name);
	m->rectangle   = (rectangle_t){.x	   = crtc->x,
								   .y	   = crtc->y,
								   .width  = crtc->width,
								   .height = crtc->height};
	m->is_focused  = false;
	m->is_occupied = false;
	m->is_wired	   = false;
	m->randr_id	   = id;
	m->next		   = NULL;
	m->desktops	   = NULL;
	add_monitor(&head_monitor, m);
	_LOG_(INFO,
		  "monitor name = %.*s:%d, out %d Monitor rectangle = x = %d, y = %d, "
		  "w = %d, h = %d was ADDED",
		  (int)name_len,
		  name,
		  m->randr_id,
		  id,
		  crtc->x,
		  crtc->y,
		  crtc->width,
		  crtc->height);
	_FREE_(crtc);
	return true;
}

static void
destroy_monitor(monitor_t *m)
{
	if (!m) {
		_LOG_(ERROR, "attempted to destroy a NULL monitor.");
		return;
	}
	unlink_monitor(&head_monitor, m);
	assert(!get_monitor_by_randr_id(m->randr_id));

	_LOG_(INFO, "destroying monitor %s", m->name);
	for (int i = 0; i < m->n_of_desktops; i++) {
		desktop_t *desktop = m->desktops[i];
		if (!desktop) {
			continue;
		}
		if (desktop->tree) {
			free_tree(desktop->tree);
			desktop->tree = NULL;
		}
		_FREE_(desktop);
	}
	_FREE_(m->desktops);
	_FREE_(m);
	_LOG_(INFO, "monitor was destroyed.");
}

static bool
is_monitor_layout_changed(xcb_randr_get_output_info_reply_t *info,
						  rectangle_t						*r,
						  rectangle_t						*r_out)
{
	xcb_randr_get_crtc_info_cookie_t crtc_c =
		xcb_randr_get_crtc_info(wm->connection, info->crtc, XCB_CURRENT_TIME);
	xcb_randr_get_crtc_info_reply_t *crtc =
		xcb_randr_get_crtc_info_reply(wm->connection, crtc_c, NULL);
	if (!crtc) {
		_LOG_(ERROR, "failed to query crtc for");
		return false;
	}
	*r_out = (rectangle_t){.x	   = crtc->x,
						   .y	   = crtc->y,
						   .width  = crtc->width,
						   .height = crtc->height};
	_FREE_(crtc);
	return (r->x != r_out->x || r->y != r_out->y || r->width != r_out->width ||
			r->height != r_out->height);
}

static bool
merge_monitors(monitor_t *om, monitor_t *nm)
{
	assert(om->n_of_desktops == nm->n_of_desktops);

	for (int i = 0; i < om->n_of_desktops; i++) {
		desktop_t *od = om->desktops[i];
		desktop_t *nd = nm->desktops[i];

		if (!od->tree) {
			continue;
		}

		queue_t *q = create_queue();
		if (!q)
			return false;

		enqueue(q, od->tree);
		while (!is_queue_empty(q)) {
			node_t *node = dequeue(q);
			if (!IS_INTERNAL(node) && node->client) {
				if (!unlink_node(node, od)) {
					_LOG_(ERROR, "failed to unlink node.... abort");
					_FREE_(q);
					return false;
				}

				if (!transfer_node(node, nd)) {
					_LOG_(ERROR, "failed to transfer node... abort");
					_FREE_(q);
					return false;
				}
				node->client->mru_seq = get_next_mru_seq(nm);
			}

			if (node->first_child) {
				enqueue(q, node->first_child);
			}
			if (node->second_child) {
				enqueue(q, node->second_child);
			}
		}
		free_queue(q);
		assert(!od->tree);
		arrange_tree(nd->tree, nd->layout);
	}
	return true;
}

static bool
is_disconnected(monitor_t *m, monitor_t *dl)
{
	for (monitor_t *d = dl; d; d = d->next) {
		if (d == m)
			return true;
	}
	return false;
}

static void
update_monitors(uint32_t *changes)
{
	monitor_t									   *dl		  = NULL;
	xcb_connection_t							   *conn	  = wm->connection;
	xcb_randr_get_screen_resources_current_cookie_t rc		  = {0};
	xcb_randr_get_screen_resources_current_reply_t *resources = NULL;

	rc		  = xcb_randr_get_screen_resources_current(conn, wm->root_window);
	resources = xcb_randr_get_screen_resources_current_reply(conn, rc, NULL);

	if (!resources) {
		_LOG_(ERROR, "failed to get screen resources");
		return;
	}

	int len = xcb_randr_get_screen_resources_current_outputs_length(resources);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(resources);
	int								   monitor_count = 0;
	xcb_randr_get_output_info_cookie_t ic			 = {0};
	xcb_randr_get_output_info_reply_t *info			 = NULL;
	for (int i = 0; i < len; i++) {
		ic	 = xcb_randr_get_output_info(conn, outputs[i], XCB_CURRENT_TIME);
		info = xcb_randr_get_output_info_reply(conn, ic, NULL);
		if (!info)
			continue;
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				_FREE_(info);
				continue;
			}
			exist->next = dl;
			dl			= exist;
		}
		if (info->crtc == XCB_NONE) {
			_FREE_(info);
			continue;
		}
		if (info->connection == XCB_RANDR_CONNECTION_CONNECTED) {
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				if (!handle_added_monitor(info, outputs[i])) {
					_LOG_(ERROR, "failed to add new output %d", outputs[i]);
					_FREE_(info);
					continue;
				}
				monitor_count++;
				*changes &= ~_NONE;
				*changes |= CONNECTED;
			} else {
				rectangle_t r = {0};
				if (is_monitor_layout_changed(info, &exist->rectangle, &r)) {
					exist->rectangle = r;
					*changes &= ~_NONE;
					*changes |= LAYOUT;
				}
			}
		}
		_FREE_(info);
	}
	_FREE_(resources);
	if (dl) {
		monitor_t *m = NULL;
		if (prim_monitor && !is_disconnected(prim_monitor, dl)) {
			m = prim_monitor;
		} else {
			for (monitor_t *cur = head_monitor; cur; cur = cur->next) {
				if (!is_disconnected(cur, dl)) {
					m = cur;
					break;
				}
			}
		}

		if (!m) {
			_LOG_(ERROR, "no monitor found to merge with");
			return;
		}
		while (dl) {
			monitor_t *r = dl;
			dl			 = dl->next;
			_LOG_(INFO, "merging desktops from %s to %s", r->name, m->name);
			if (!merge_monitors(r, m)) {
				_LOG_(ERROR, "failed to merge desktops from %s", r->name);
				continue;
			}
			destroy_monitor(r);
		}
		*changes &= ~_NONE;
		*changes |= DISCONNECTED;
	}
	_LOG_(INFO, "%d newly connected monitor", monitor_count);
}

void
handle_monitor_changes(void)
{
	if (using_xinerama) {
		return;
	}

	uint32_t m_change = 0 | _NONE;
	bool	 render	  = false;
	update_monitors(&m_change);

	if (m_change & _NONE) {
		_LOG_(INFO, "no monitor changes was found");
		return;
	}
	if (m_change & CONNECTED) {
		_LOG_(INFO, "a monitor was connected");
		setup_desktops();
	} else if (m_change & DISCONNECTED) {
		_LOG_(INFO, "a monitor was disconnected");
		curr_monitor = prim_monitor = head_monitor;
		render						= true;
	} else if (m_change & LAYOUT) {
		_LOG_(INFO, "a monitor's layout was changed");
		render = true;
	}

	if (render) {
		if (!ignore_ewmh_struts) {
			recalculate_all_struts();
		} else {
			arrange_trees();
			render_trees();
		}
	}

	log_monitors();

	multi_monitors = (get_monitors_count() > 1);

	_LOG_(INFO,
		  "in update: multi monitors = %s",
		  multi_monitors ? "true" : "false");
}

monitor_t *
get_monitor_within_coordinate(int16_t x, int16_t y)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (x >= curr->rectangle.x &&
			x < (curr->rectangle.x + curr->rectangle.width) &&
			y >= curr->rectangle.y &&
			y < (curr->rectangle.y + curr->rectangle.height)) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}

monitor_t *
get_monitor_from_desktop(desktop_t *desktop)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int j = 0; j < curr->n_of_desktops; j++) {
			if (curr->desktops[j] == desktop) {
				return curr;
			}
		}
		curr = curr->next;
	}
	return NULL;
}

monitor_t *
get_monitor_by_window(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			node_t *node = find_node_by_window_id(curr->desktops[i]->tree, win);
			if (node) {
				return curr;
			}
		}
		curr = curr->next;
	}
	return NULL;
}

rectangle_t
calculate_monitor_area(const monitor_t *m)
{
	rectangle_t usable = get_usable_area((monitor_t *)m);
	return (rectangle_t){
		.x		= (int16_t)(usable.x + conf.window_gap),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(usable.width - 2 * conf.window_gap -
							 2 * conf.border_width),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap -
							 2 * conf.border_width)};
}

void
apply_monitor_layout_changes(monitor_t *m)
{
	for (int d = 0; d < m->n_of_desktops; ++d) {
		if (!m->desktops[d] || is_tree_empty(m->desktops[d]->tree))
			continue;

		layout_t layout = m->desktops[d]->layout;
		node_t	*tree	= m->desktops[d]->tree;

		if (layout == DEFAULT || layout == STACK || layout == GRID) {
			tree->rectangle = calculate_monitor_area(m);

			if (layout == DEFAULT)
				apply_default_layout(tree);
			else if (layout == STACK)
				apply_stack_layout(tree);
			else if (layout == GRID)
				apply_grid_layout(tree);

		} else if (layout == MASTER) {
			node_t *ms = find_master_node(tree);
			if (!ms && !(ms = find_any_leaf(tree)))
				return;

			ms->is_master			  = true;
			const double ratio		  = MASTER_RATIO;

			rectangle_t	 usable		  = get_usable_area(m);
			uint16_t	 master_width = (uint16_t)(usable.width * ratio);
			uint16_t	 r_width	  = (uint16_t)(usable.width * (1 - ratio));

			rectangle_t	 r1			  = {
				.x		= (int16_t)(usable.x + conf.window_gap),
				.y		= (int16_t)(usable.y + conf.window_gap),
				.width	= (uint16_t)(master_width - 2 * conf.window_gap),
				.height = (uint16_t)(usable.height - 2 * conf.window_gap),
			};
			rectangle_t r2 = {
				.x		= (int16_t)(usable.x + master_width),
				.y		= (int16_t)(usable.y + conf.window_gap),
				.width	= (uint16_t)(r_width - conf.window_gap),
				.height = (uint16_t)(usable.height - 2 * conf.window_gap),
			};
			ms->rectangle	= r1;
			tree->rectangle = r2;
			apply_master_layout(tree);
		}
	}
}

void
arrange_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		apply_monitor_layout_changes(curr);
		curr = curr->next;
	}
}