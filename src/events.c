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

#include "events.h"
#include "actions.h"
#include "bindings.h"
#include "client.h"
#include "config_parser.h"
#include "desktop.h"
#include "drag.h"
#include "ewmh.h"
#include "focus.h"
#include "helper.h"
#include "monitor.h"
#include "mouse.h"
#include "stacking.h"
#include "state.h"
#include "tree.h"
#include "type.h"
#include "view.h"
#include "xcb_util.h"
#include <X11/keysym.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

/* clang-format off */
static int handle_map_request(const xcb_event_t *);
static int handle_unmap_notify(const xcb_event_t *);
static int handle_destroy_notify(const xcb_event_t *);
static int handle_client_message(const xcb_event_t *);
static int handle_configure_request(const xcb_event_t *);
static int handle_enter_notify(const xcb_event_t *);
static int handle_leave_notify(const xcb_event_t *);
static int handle_focus_in(const xcb_event_t *);
static int handle_property_notify(const xcb_event_t *);
static int handle_key_press(const xcb_event_t *);
static int handle_mapping_notify(const xcb_event_t *);
static int handle_button_press_event(const xcb_event_t *);
static int handle_motion_notify(const xcb_event_t *);
static int handle_button_release(const xcb_event_t *);

/* array of xcb events we need to handle -> {event, handler function} */
static const event_handler_entry_t _handlers_[] = {
	/* map request - is generated when a window wants to be mapped (displayed) on the screen */
    DEFINE_MAPPING(XCB_MAP_REQUEST, handle_map_request),
	/* unmap request - is generated when a window wants to be unmapped (removed) from the screen */
    DEFINE_MAPPING(XCB_UNMAP_NOTIFY, handle_unmap_notify),
	/* destroy notify - is generated when a window is killed */
    DEFINE_MAPPING(XCB_DESTROY_NOTIFY, handle_destroy_notify),
	/* client message (ewmh):
	 * These events are sent by other applications through ewmh protocol to zwm;
	 * I am only responding to requests where:
	 * 1- the state of the window is changed (below, above, or fullscreen only, rest is ignored)
	 * 		this generates a _NET_WM_STATE message
	 * 2- application wants to know where a window is located (_NET_ACTIVE_WINDOW),
	 * 		as result, zwm switches to the desktop containing that window.
	 * 3- application wants to close a window (issued by pagers)
	 * 		this generates a NET_CLOSE_WINDOW message
	 * 4- a desktop change was requested (usually through a status bar)
	 * 		this generates _NET_CURRENT_DESKTOP message
	 * 5- some application wants a window moved from one virtual desktop to another
	 * 		this generates _NET_WM_DESKTOP message
	 * other messages are ignored intentionally.*/
    DEFINE_MAPPING(XCB_CLIENT_MESSAGE, handle_client_message),
	/* configure request - this is used when a client wants to set or update its
	 * rectangle/positions or stacking mode.
	 * since zwm is a tiling wm, i am mostly ignoring this event even though it
	 * reveals important info for splash screens */
    DEFINE_MAPPING(XCB_CONFIGURE_REQUEST, handle_configure_request),
	/* enter notify - is generated when a cursor enters a window, as a result,
	 * i redirect the focus and do some book keeping for floating windows */
    DEFINE_MAPPING(XCB_ENTER_NOTIFY, handle_enter_notify),
	/* button press - is generated when a button is pressed, this event is handled
	 * when focus_follow_pointer is set to false (the focus is redirected as a result) */
    DEFINE_MAPPING(XCB_BUTTON_PRESS, handle_button_press_event),
    /* key press - is generated when a key is pressed, this event allows certain
     * actions to be performed when a key is pressed, and this is how
	 * keybinds take action */
    DEFINE_MAPPING(XCB_KEY_PRESS, handle_key_press),
    /* mapping notify - is generated when keyboard mapping is changed,
     * it only ungrab the re-grab the keys */
    DEFINE_MAPPING(XCB_MAPPING_NOTIFY, handle_mapping_notify),
   	/* will be implemented if needed */
    DEFINE_MAPPING(XCB_MOTION_NOTIFY, handle_motion_notify),
    DEFINE_MAPPING(XCB_BUTTON_RELEASE, handle_button_release),
    /* DEFINE_MAPPING(XCB_LEAVE_NOTIFY, handle_leave_notify), */
    /* DEFINE_MAPPING(XCB_KEY_RELEASE, handle_key_release), */
    /* DEFINE_MAPPING(XCB_FOCUS_IN, handle_focus_in), */
    /* DEFINE_MAPPING(XCB_FOCUS_OUT, handle_focus_out), */
    /* DEFINE_MAPPING(XCB_CONFIGURE_NOTIFY, handle_configure_notify), */
    DEFINE_MAPPING(XCB_PROPERTY_NOTIFY, handle_property_notify),
};
/* clang-format on */

static char *
xcb_event_to_string(uint8_t type)
{
	switch (type) {
	case XCB_MAP_REQUEST: return "XCB_MAP_REQUEST";
	case XCB_UNMAP_NOTIFY: return "XCB_UNMAP_NOTIFY";
	case XCB_DESTROY_NOTIFY: return "XCB_DESTROY_NOTIFY";
	case XCB_EXPOSE: return "XCB_EXPOSE";
	case XCB_CLIENT_MESSAGE: return "XCB_CLIENT_MESSAGE";
	case XCB_CONFIGURE_REQUEST: return "XCB_CONFIGURE_REQUEST";
	case XCB_CONFIGURE_NOTIFY: return "XCB_CONFIGURE_NOTIFY";
	case XCB_PROPERTY_NOTIFY: return "XCB_PROPERTY_NOTIFY";
	case XCB_ENTER_NOTIFY: return "XCB_ENTER_NOTIFY";
	case XCB_LEAVE_NOTIFY: return "XCB_LEAVE_NOTIFY";
	case XCB_MOTION_NOTIFY: return "XCB_MOTION_NOTIFY";
	case XCB_BUTTON_PRESS: return "XCB_BUTTON_PRESS";
	case XCB_BUTTON_RELEASE: return "XCB_BUTTON_RELEASE";
	case XCB_KEY_PRESS: return "XCB_KEY_PRESS";
	case XCB_KEY_RELEASE: return "XCB_KEY_RELEASE";
	case XCB_FOCUS_IN: return "XCB_FOCUS_IN";
	case XCB_FOCUS_OUT: return "XCB_FOCUS_OUT";
	case XCB_MAPPING_NOTIFY: return "XCB_MAPPING_NOTIFY";
	default: return "UNKNOWN_EVENT";
	}
}

static int
handle_event(xcb_event_t *event)
{
	uint8_t event_type = event->response_type & ~0x80;

	/* xinerima is ignored here */
	if (using_xrandr &&
		event_type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		_LOG_(INFO, "monitor update was requested");
		handle_monitor_changes();
		return 0;
	}

#if 0
	switch (event_type) {
#define _EVENT_HANDLER_(type, handler)                                         \
	case type: return handler((void *)event);
		_EVENT_HANDLER_(XCB_MAP_REQUEST, handle_map_request);
		_EVENT_HANDLER_(XCB_UNMAP_NOTIFY, handle_unmap_notify);
		_EVENT_HANDLER_(XCB_DESTROY_NOTIFY, handle_destroy_notify);
		_EVENT_HANDLER_(XCB_CLIENT_MESSAGE, handle_client_message);
		_EVENT_HANDLER_(XCB_CONFIGURE_REQUEST, handle_configure_request);
		_EVENT_HANDLER_(XCB_ENTER_NOTIFY, handle_enter_notify);
		_EVENT_HANDLER_(XCB_BUTTON_PRESS, handle_button_press_event);
		_EVENT_HANDLER_(XCB_KEY_PRESS, handle_key_press);
		_EVENT_HANDLER_(XCB_MAPPING_NOTIFY, handle_mapping_notify);
#undef _EVENT_HANDLER_
	}
#endif

	size_t n = sizeof(_handlers_) / sizeof(_handlers_[0]);
	for (size_t i = 0; i < n; i++) {
		if (_handlers_[i].type == event_type) {
			return _handlers_[i].handle(event);
		}
	}

	return 0;
}

/* the main loop that listens to redirected x events */
void
event_loop(wm_t *w)
{
	xcb_event_t *event;
	while (!should_shutdown && (event = xcb_wait_for_event(w->connection))) {
		if (event->response_type == 0) {
			_FREE_(event);
			continue;
		}
		if (handle_event(event) != 0) {
			uint8_t type = event->response_type & ~0x80;
			char   *es	 = xcb_event_to_string(type);
			_LOG_(ERROR, "error processing event: %s ", es);
		}
		_FREE_(event);
	}
}

static int
handle_map_request(const xcb_event_t *event)
{
	xcb_map_request_event_t *ev			= (xcb_map_request_event_t *)event;
	xcb_window_t			 win		= ev->window;
	bool					 is_visible = true;
	int						 ret		= 0;

	if (multi_monitors) {
		monitor_t *mm = get_focused_monitor();
		if (mm && mm != curr_monitor) {
			curr_monitor = mm;
		}
	}

	if (!should_manage(win, wm->connection)) {
		_LOG_(INFO, "win %d, shouldn't be managed.. ignoring request", win);
		return 0;
	}

	/* check if the window already exists in ANY desktop to avoid duplication */
	if (client_exist_in_desktops(win)) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[MAP_REQUEST] win %d already exists in a desktop, ignoring ",
			  win);
#endif
		return 0;
	}

	rule_t *rule = get_window_rule(win);

	if (rule) {
		if (rule->desktop_id != -1) {
			int target = rule->desktop_id - 1;
			if (curr_monitor->desk->id != target) {
				/* Insert into the target desktop, but do not map/focus now */
				ret = insert_into_desktop(
					rule->desktop_id, win, rule->state == TILED);
				if (ret != 0)
					goto manage_failed;
				desktop_t *target_desk = curr_monitor->desktops[target];
				render_tree_nomap(target_desk->tree);
				is_visible = false;
				goto out;
			}
			/* else: current desktop, fall through to normal logic below */
		}
		/* for both cases, if a state is specified, use it */
		if (rule->state == FLOATING) {
			ret = handle_floating_window_request(win, curr_monitor->desk);
			if (ret != 0)
				goto manage_failed;
			goto out;
		} else if (rule->state == TILED) {
			ret = handle_tiled_window_request(win, curr_monitor->desk);
			if (ret != 0)
				goto manage_failed;
			goto out;
		}
	}

	ewmh_window_type_t wint = window_type(win);
	if (wint != WINDOW_TYPE_DOCK && wint != WINDOW_TYPE_DESKTOP &&
		wint != WINDOW_TYPE_NOTIFICATION && apply_floating_hints(win) != -1) {
		ret = handle_floating_window_request(win, curr_monitor->desk);
		if (ret != 0)
			goto manage_failed;
		goto out;
	}

	/* notification windows are short-lived, they dont deserve entering the
	 * tree. Thus, we just map them as is, and they go away on their own shortly
	 * after */
	/*if (wint == WINDOW_TYPE_NOTIFICATION) {
		map_floating(win);
		return 0;
	}*/

	switch (wint) {
	case WINDOW_TYPE_DESKTOP:
		/* fallthrough */
	case WINDOW_TYPE_DOCK:
	case WINDOW_TYPE_NOTIFICATION: return handle_unmanaged_strut_window(win);
	case WINDOW_TYPE_UNKNOWN:
	case WINDOW_TYPE_NORMAL:
		ret = handle_tiled_window_request(win, curr_monitor->desk);
		break;
	case WINDOW_TYPE_TOOLBAR_MENU:
	case WINDOW_TYPE_UTILITY:
	case WINDOW_TYPE_SPLASH:
	case WINDOW_TYPE_DIALOG:
		ret = handle_floating_window_request(win, curr_monitor->desk);
		break;
	default: break;
	}
	if (ret != 0)
		goto manage_failed;
out:
	if (is_visible && conf.focus_follow_spawn &&
		curr_monitor->desk->layout != STACK) {
		node_t *f = NULL;
		if ((f = find_node_by_window_id(curr_monitor->desk->tree, win)) ==
			NULL) {
			_LOG_(DEBUG, "cannot find window %d, in tree", win);
			xcb_flush(wm->connection);
			return 0;
		}
		set_focus(f, true);
		set_active_window_name(f->client->window);
		focused_win = f->client->window;
		update_focus(curr_monitor->desk, f);
	}
	set_window_state(win,
					 is_visible ? XCB_ICCCM_WM_STATE_NORMAL
								: XCB_ICCCM_WM_STATE_ICONIC);
	ewmh_update_client_list();
	xcb_flush(wm->connection);

	return 0;

manage_failed:
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_enter_notify(const xcb_event_t *event)
{
	xcb_enter_notify_event_t *ev  = (xcb_enter_notify_event_t *)event;
	xcb_window_t			  win = ev->event;
	uint64_t				  now = get_time_millis();
	/* suppress enter-notify events caused by window mapping floods */
	if (now < suppress_enter_until_time || now - last_desk_switch_time < 250) {
#ifdef _DEBUG__
		_LOG_(DEBUG, "ignoring enter notify: map-flood cooldown in effect");
#endif
		return 0;
	}

	monitor_t *mm = get_focused_monitor();
	if (mm && mm != curr_monitor) {
		curr_monitor = mm;
	}

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved enter notify for %d, name %s ", win, name);
	_FREE_(name);
#endif
	/* ignore events with 1- non-normal modes. Those are because a grab
	 * activated/deactivated. 2- events with detail "inferior".  This detail
	 * means that the cursor was previously inside of a child window and now
	 * left that child window, it happens with broswer menues, code edeitor
	 * completion menues etc */
	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	/* if (win == focused_win) {
		return 0;
	} */

	node_t *root = curr_monitor->desk->tree;
	if (!root) {
		return -1;
	}
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n && n->client) ? n->client : NULL;

	if (client == NULL || n == NULL) {
		return 0;
	}

	if (win == wm->root_window) {
		return 0;
	}

	if (!conf.focus_follow_pointer) {
		if (has_floating_window(root)) {
			restack();
		}
		if (IS_FULLSCREEN(n->client)) {
			if (fullscreen_focus(n->client->window)) {
				_LOG_(ERROR, "cannot update win attributes");
				return -1;
			}
		}
		/* set_cursor(CURSOR_NOT_ALLOWED); */
		return 0;
	}

	if (n->client->window == focused_win) {
		return 0;
	}

	layout_t lay = curr_monitor->desk->layout;

	/* DECK master hover, do X input focus only,
	  never reshuffles deck-stack order
	 */
	if (lay == DECK && n->is_master && IS_TILED(n->client)) {
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR,
				  "cannot focus deck master %d (enter)",
				  n->client->window);
			return -1;
		}
		focused_win = n->client->window;
		set_active_window_name(win);
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
		_flush_view_(curr_monitor->desk);
		return 0;
	}

	const int r = set_active_window_name(win);
	if (r != 0) {
		return 0;
	}

	if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return -1;
		}
		focused_win		   = n->client->window;
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
		_flush_view_(curr_monitor->desk);
		return 0;
	}

	if (IS_FLOATING(n->client)) {
		if (focused_win != XCB_NONE)
			win_focus(focused_win, false);
		if (_focus_input_(curr_monitor->desk, n) != 0) {
			_LOG_(ERROR, "cannot focus window %d (enter)", n->client->window);
			return -1;
		}
		n->is_focused = true;
	} else {
		/* iff tiled, set logical focus + rerender
		   so MONOCLE/DECK show this window
		 */
		_focus_node_(curr_monitor->desk, n);
		_render_view_(curr_monitor->desk);
		_focus_input_(curr_monitor->desk, n);
	}

	focused_win = n->client->window;
	if (n && n->client)
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
	_flush_view_(curr_monitor->desk);
	return 0;
}

static int
handle_leave_notify(const xcb_event_t *event)
{
	if (!conf.focus_follow_pointer) {
		return 0;
	}
	xcb_leave_notify_event_t *ev  = (xcb_leave_notify_event_t *)event;
	xcb_window_t			  win = ev->event;

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved leave notify for %d, name %s ", win, name);
	_FREE_(name);
#endif

	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	if (curr_monitor->desk->layout == STACK) {
		return 0;
	}

	node_t		*root		   = curr_monitor->desk->tree;
	xcb_window_t active_window = XCB_NONE;
	node_t		*n			   = find_node_by_window_id(root, win);
	client_t	*client		   = (n && n->client) ? n->client : NULL;
	if (client == NULL) {
		return 0;
	}

	xcb_get_property_cookie_t c =
		xcb_ewmh_get_active_window(wm->ewmh, wm->screen_nbr);

	xcb_ewmh_get_active_window_reply(wm->ewmh, c, &active_window, NULL);
	if (active_window != client->window) {
		return 0;
	}

	if (set_focus(n, false) != 0) {
		_LOG_(ERROR,
			  "failed to change border attr for window %d",
			  client->window);
		return -1;
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_property_notify(const xcb_event_t *event)
{
	const xcb_property_notify_event_t *ev =
		(const xcb_property_notify_event_t *)event;
	if (wm && wm->ewmh && ev->atom == wm->ewmh->_NET_WM_STRUT_PARTIAL) {
		/* bar/panel resized or changed reserved space*/
		if (!ignore_ewmh_struts)
			recalculate_all_struts();
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_focus_in(const xcb_event_t *event)
{
	xcb_focus_in_event_t *ev  = (xcb_focus_in_event_t *)event;
	xcb_window_t		  win = ev->event;
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved focus in for %d, name %s ", win, name);
	_FREE_(name);
#endif
	if (ev->mode == XCB_NOTIFY_MODE_GRAB ||
		ev->mode == XCB_NOTIFY_MODE_UNGRAB ||
		ev->detail == XCB_NOTIFY_DETAIL_POINTER ||
		ev->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT ||
		ev->detail == XCB_NOTIFY_DETAIL_NONE) {
		return 0;
	}

	node_t *n = NULL;
	if ((n = get_focused_node(curr_monitor->desk->tree)) == NULL) {
		return -1;
	}

	if (n->client->window == win)
		return 0;

	set_focus(n, true);
	xcb_flush(wm->connection);
	return 0;
}

static inline window_state_action_t
convert_state_action(uint32_t action)
{
	switch (action) {
	case XCB_EWMH_WM_STATE_REMOVE: return STATE_ACTION_REMOVE;
	case XCB_EWMH_WM_STATE_ADD: return STATE_ACTION_ADD;
	case XCB_EWMH_WM_STATE_TOGGLE: return STATE_ACTION_TOGGLE;
	default: return STATE_ACTION_INVALID;
	}
}

static inline client_message_type_t
get_client_message_type(xcb_atom_t type, xcb_ewmh_conn_t *ewmh)
{
	if (type == ewmh->_NET_CURRENT_DESKTOP)
		return CLIENT_MESSAGE_CURRENT_DESKTOP;
	if (type == ewmh->_NET_WM_STATE)
		return CLIENT_MESSAGE_WINDOW_STATE;
	if (type == ewmh->_NET_ACTIVE_WINDOW)
		return CLIENT_MESSAGE_ACTIVE_WINDOW;
	if (type == ewmh->_NET_WM_DESKTOP)
		return CLIENT_MESSAGE_WINDOW_DESKTOP;
	if (type == ewmh->_NET_CLOSE_WINDOW)
		return CLIENT_MESSAGE_CLOSE_WINDOW;

	return CLIENT_MESSAGE_UNSUPPORTED;
}

static window_state_type_t
get_window_state_type(uint32_t state, xcb_ewmh_conn_t *ewmh)
{
	if (state == ewmh->_NET_WM_STATE_FULLSCREEN)
		return STATE_FULLSCREEN;
	if (state == ewmh->_NET_WM_STATE_BELOW)
		return STATE_BELOW;
	if (state == ewmh->_NET_WM_STATE_ABOVE)
		return STATE_ABOVE;
	if (state == ewmh->_NET_WM_STATE_HIDDEN)
		return STATE_HIDDEN;
	if (state == ewmh->_NET_WM_STATE_STICKY)
		return STATE_STICKY;
	if (state == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
		return STATE_DEMANDS_ATTENTION;

	return STATE_UNSUPPORTED;
}

static int
handle_fullscreen_state(node_t				 *node,
						window_state_action_t action,
						const char			 *name)
{
	if (!node || !node->client || !name) {
		return -1;
	}

	xcb_window_t		w	  = node->client->window;
	window_state_type_t state = STATE_FULLSCREEN;
#define _LOG_WM_STATE_ACTION_(action, state, w, name)                          \
	_LOG_(INFO, "received %s %s for window %d:%s", #action, #state, w, name)

	switch (action) {
	case STATE_ACTION_ADD: {
		_LOG_WM_STATE_ACTION_(ADD, FULLSCREEN, w, name);
		return set_fullscreen(node, true);
	}
	case STATE_ACTION_REMOVE: {
		_LOG_WM_STATE_ACTION_(REMOVE, FULLSCREEN, w, name);
		return set_fullscreen(node, false);
	}
	case STATE_ACTION_TOGGLE: {
		bool is_fullscreen = (node->client->state == FULLSCREEN);
		_LOG_WM_STATE_ACTION_(TOGGLE, FULLSCREEN, w, name);
		return set_fullscreen(node, !is_fullscreen);
	}
	default: return 0;
	}
}

static int
handle_net_wm_state(xcb_window_t win_event, uint32_t action, uint32_t state)
{
	if (state == 0) {
		return 0;
	}

	node_t *node = find_node_global(win_event);
	if (!node) {
		return 0;
	}

	xcb_window_t win  = node->client->window;
	char		*name = win_name(win);
	if (!name) {
		return -1;
	}

	int					  result	   = 0;
	window_state_type_t	  type		   = get_window_state_type(state, wm->ewmh);
	window_state_action_t state_action = convert_state_action(action);
	ewmh_state_t		  flag		   = ewmh_flag_for_atom(state);
	bool				  set_state	   = false;

#define _LOG_WM_STATE_(state, w, name)                                         \
	_LOG_(INFO, "received %s for window %d:%s", #state, w, name)

	if (flag != EWMH_STATE_NONE && state_action != STATE_ACTION_INVALID) {
		if (state_action == STATE_ACTION_ADD) {
			set_state = true;
		} else if (state_action == STATE_ACTION_REMOVE) {
			set_state = false;
		} else if (state_action == STATE_ACTION_TOGGLE) {
			set_state = !ewmh_has(node->client->ewmh_state, flag);
		}
		update_net_wm_state_atom(win, state, set_state);
		update_client_ewmh_state(node->client, flag, set_state);
	}

	switch (type) {
	case STATE_FULLSCREEN:
		_LOG_WM_STATE_(FULLSCREEN, win, name);
		result = handle_fullscreen_state(node, state_action, name);
		break;
	case STATE_BELOW: {
		_LOG_WM_STATE_(BELOW, win, name);
		break;
	}
	case STATE_ABOVE: {
		_LOG_WM_STATE_(ABOVE, win, name);
		break;
	}
	case STATE_HIDDEN: {
		_LOG_WM_STATE_(HIDDEN, win, name);
		break;
	}
	case STATE_STICKY: {
		_LOG_WM_STATE_(STICKY, win, name);
		break;
	}
	case STATE_DEMANDS_ATTENTION: {
		_LOG_WM_STATE_(DEMANDS_ATTENTION, win, name);
		break;
	}
	default: break;
	}
	if (flag != EWMH_STATE_NONE) {
		restack();
	}

	_FREE_(name);
	return result;
}

static int
handle_client_message(const xcb_event_t *event)
{
	xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;

	if (ev->format != 32) {
		return 0;
	}

	xcb_window_t		  win	 = ev->window;
	client_message_type_t type	 = get_client_message_type(ev->type, wm->ewmh);
	char				 *name	 = win_name(win);
	int					  result = 0;

#define _LOG_CLIENT_MESSAGE_(type, w, name)                                    \
	_LOG_(INFO, "received %s for window %d:%s", #type, w, name)

	switch (type) {
	case CLIENT_MESSAGE_CURRENT_DESKTOP: {
		_LOG_CLIENT_MESSAGE_(CURRENT_DESKTOP, win, name);
		/* if a pager wants to switch to another virtual desktop, it MUST send
		 * a _NET_CURRENT_DESKTOP client message to the root window */
		result = handle_net_desktop_change(ev->data.data32[0]);
		break;
	}
	case CLIENT_MESSAGE_WINDOW_STATE: {
		_LOG_CLIENT_MESSAGE_(WM_STATE, win, name);
		size_t n = sizeof(ev->data.data32) / sizeof(ev->data.data32[0]);
		for (size_t i = 0; i < n - 1; i++) {
			uint32_t state = ev->data.data32[i + 1];
			result = handle_net_wm_state(win, ev->data.data32[0], state);
		}
		break;
	}
	case CLIENT_MESSAGE_ACTIVE_WINDOW: {
		/*  if a client wants to activate another window, it MUST send a
		 * _NET_ACTIVE_WINDOW client message to the root window. I should just
		 * switch to the desktop where the window is located */
		_LOG_CLIENT_MESSAGE_(ACTIVE_WINDOW, win, name);
		result = handle_net_active_window(win);
		break;
	}
	case CLIENT_MESSAGE_WINDOW_DESKTOP: {
		/* this is a request to move window from one destkop to another */
		_LOG_CLIENT_MESSAGE_(WM_DESKTOP, win, name);
		uint32_t i = ev->data.data32[0];
		/* if a rule pins this window to a specific desktop, don't let the
		 * app override placement via _NET_WM_DESKTOP (like Firefox restoring
		 * its last-used desktop on launch). */
		rule_t	*r = get_window_rule(win);
		if (r && r->desktop_id != -1) {
			break;
		}
		result = handle_net_wm_desktop(win, i);
		break;
	}
	case CLIENT_MESSAGE_CLOSE_WINDOW: {
		/* pagers wanting to close a window MUST send a _NET_CLOSE_WINDOW client
		 * message request to the root window */
		_LOG_CLIENT_MESSAGE_(CLOSE_WINDOW, win, name);
		close_or_kill(win);
		break;
	}
	default: break;
	}
	_LOG_CLIENT_MESSAGE_(UNKNOWN_EVENT, win, name);
	_FREE_(name);
	xcb_flush(wm->connection);
	return result;
}

static int
handle_unmap_notify(const xcb_event_t *event)
{
	xcb_unmap_notify_event_t *ev  = (xcb_unmap_notify_event_t *)event;
	xcb_window_t			  win = ev->window;
#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved unmap notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	bool is_managed = client_exist(curr_monitor->desk->tree, win) ||
					  client_exist_in_desktops(win);
	if (!is_managed) {
		if (!ignore_ewmh_struts && remove_strut_window(win)) {
			reapply_tracked_struts();
		}
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		_FREE_(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (unmap)", win);
		return -1;
	}
	ewmh_update_client_list();
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_configure_request(const xcb_event_t *event)
{
	xcb_configure_request_event_t *ev  = (xcb_configure_request_event_t *)event;
	xcb_window_t				   win = ev->window;

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, ev->window);
	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	char name[256];
	if (wr == 1) {
		snprintf(name, sizeof(name), "%s", t_reply.name);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "window %d  name %s wants to be at %dx%d with %dx%d",
		  win,
		  name,
		  ev->x,
		  ev->y,
		  ev->width,
		  ev->height);
#endif
	node_t *node = find_node_global(win);
	if (!node) {
		uint16_t mask = 0;
		uint32_t values[7];
		memset(values, 0, sizeof(values));
		uint16_t i = 0;
		if (ev->value_mask & XCB_CONFIG_WINDOW_X) {
			mask |= XCB_CONFIG_WINDOW_X;
			values[i++] = (uint32_t)ev->x;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_Y) {
			mask |= XCB_CONFIG_WINDOW_Y;
			values[i++] = (uint32_t)ev->y;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_WIDTH;
			values[i++] = ev->width;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
			mask |= XCB_CONFIG_WINDOW_HEIGHT;
			values[i++] = ev->height;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
			values[i++] = ev->border_width;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
			mask |= XCB_CONFIG_WINDOW_SIBLING;
			values[i++] = ev->sibling;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
			mask |= XCB_CONFIG_WINDOW_STACK_MODE;
			values[i++] = ev->stack_mode;
		}

		xcb_configure_window(wm->connection, win, mask, values);
	} else {
		/* reflect actual geometry (floating/fullscreen) in ConfigureNotify. */
		rectangle_t r = node->rectangle;
		if (node->client && IS_FLOATING(node->client)) {
			r = node->floating_rectangle;
		} else if (node->client && IS_FULLSCREEN(node->client)) {
			monitor_t *m = get_monitor_by_window(node->client->window);
			if (m) {
				r = m->rectangle;
			} else if (curr_monitor) {
				r = curr_monitor->rectangle;
			}
		}
		xcb_configure_notify_event_t evt;
		memset(&evt, 0, sizeof(evt));
		unsigned int bw		  = (node->client && IS_FULLSCREEN(node->client))
									? 0
									: conf.border_width;
		evt.response_type	  = XCB_CONFIGURE_NOTIFY;
		evt.event			  = win;
		evt.window			  = win;
		evt.above_sibling	  = XCB_NONE;
		evt.x				  = r.x;
		evt.y				  = r.y;
		evt.width			  = r.width;
		evt.height			  = r.height;
		evt.border_width	  = bw;
		evt.override_redirect = false;
		xcb_send_event(wm->connection,
					   false,
					   win,
					   XCB_EVENT_MASK_STRUCTURE_NOTIFY,
					   (const char *)&evt);
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_destroy_notify(const xcb_event_t *event)
{
	xcb_destroy_notify_event_t *ev	= (xcb_destroy_notify_event_t *)event;
	xcb_window_t				win = ev->window;
#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved destroy notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	bool is_managed = client_exist(curr_monitor->desk->tree, win) ||
					  client_exist_in_desktops(win);
	if (!is_managed) {
		if (!ignore_ewmh_struts && remove_strut_window(win)) {
			reapply_tracked_struts();
		}
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		_FREE_(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (destroy)", win);
		return -1;
	}
	ewmh_update_client_list();
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_key_press(const xcb_event_t *event)
{
	xcb_key_press_event_t *ev			 = (xcb_key_press_event_t *)event;
	uint16_t			   cleaned_state = normalize_mods(ev->state);

	if (ds.active || ms.op != MOUSE_OP_NONE) {
		xcb_keysym_t k = get_keysym(ev->detail, wm->connection);
		if (ds.active && k == XK_Escape) {
			drag_cancel();
			return 0;
		}
		if (ms.op != MOUSE_OP_NONE && k == XK_Escape) {
			cancel_mouse_action();
			return 0;
		}
	}

	if (key_head) {
		conf_key_t *current = key_head;
		while (current) {
			if (cleaned_state == normalize_mods((uint16_t)current->mod)) {
				/* Keybind dispatch uses physical keycodes so shortcuts keep
				 * working under non-US XKB layouts. */
				if (current->keycode == ev->detail) {
					arg_t	 *a	  = current->arg;
					const int ret = current->execute(a);
					if (ret != 0) {
						_LOG_(ERROR, "error while executing function_ptr(..)");
					}
					break;
				}
			}
			current = current->next;
		}
		return 0;
	}

	for (size_t i = _keys_len; i--;) {
		if (cleaned_state == normalize_mods((uint16_t)_keys_[i].mod)) {
			if (_keys_[i].keycode == ev->detail) {
				arg_t	 *a	  = _keys_[i].arg;
				const int ret = _keys_[i].execute(a);
				if (ret != 0) {
					_LOG_(ERROR, "error while executing function_ptr(..)");
				}
				break;
			}
		}
	}
	return 0;
}

static int
handle_mapping_notify(const xcb_event_t *event)
{
	xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t *)event;

	if (ev->request != XCB_MAPPING_KEYBOARD &&
		ev->request != XCB_MAPPING_MODIFIER) {
		return 0;
	}

	if (is_kgrabbed) {
		ungrab_keys(wm->connection, wm->root_window);
		is_kgrabbed = !is_kgrabbed;
	}

	if (0 != grab_keys(wm->connection, wm->root_window)) {
		_LOG_(ERROR, "cannot grab keys");
		return -1;
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_button_press_event(const xcb_event_t *event)
{
	xcb_button_press_event_t *ev  = (xcb_button_press_event_t *)event;
	xcb_window_t			  win = ev->event;
	if (win == wm->root_window && ev->child != XCB_NONE) {
		win = ev->child;
	}

	_LOG_(INFO,
		  "=== BUTTON PRESS: detail=%d, state=0x%x, SUPER=0x%x ===",
		  ev->detail,
		  ev->state,
		  SUPER);
	_LOG_(INFO,
		  "    win=%d, root_x=%d, root_y=%d, detail==BTN1? %d, state&SUPER? %d",
		  win,
		  ev->root_x,
		  ev->root_y,
		  ev->detail == XCB_BUTTON_INDEX_1,
		  (ev->state & SUPER) != 0);

	if (ev->detail == XCB_BUTTON_INDEX_1 && (ev->state & SUPER)) {
		_LOG_(INFO, ">>> SUPER+Button1 match, checking drag conditions...");
		if (!window_exists(wm->connection, win))
			goto normal_handling;
		node_t *root = curr_monitor->desk->tree;
		node_t *n	 = find_node_by_window_id(root, win);
		if (!n) {
			_LOG_(INFO, "    node not found in tree");
			goto normal_handling;
		}
		if (!n->client) {
			_LOG_(INFO, "    node has no client");
			goto normal_handling;
		}
		_LOG_(INFO, "    client state=%d (TILED=%d)", n->client->state, TILED);
		if (IS_TILED(n->client)) {
			_LOG_(INFO, "    >>> calling drag_start()...");
			if (drag_start(win, ev->root_x, ev->root_y, false) == 0) {
				_LOG_(INFO, "    >>> drag started..");
			}
		} else if (IS_FLOATING(n->client)) {
			_LOG_(INFO, "    >>> calling start_floating_move()...");
			start_floating_move(n, ev->root_x, ev->root_y);
		}
	}

	if (ev->detail == XCB_BUTTON_INDEX_3 && (ev->state & SUPER)) {
		if (!window_exists(wm->connection, win))
			goto normal_handling;
		node_t *root = curr_monitor->desk->tree;
		node_t *n	 = find_node_by_window_id(root, win);
		if (!n || !n->client) {
			goto normal_handling;
		}
		if (IS_FLOATING(n->client)) {
			start_floating_resize(n, ev->root_x, ev->root_y);
		} else if (IS_TILED(n->client)) {
			start_tiled_resize(n, ev->root_x, ev->root_y);
		}
	}

normal_handling:
	if (conf.focus_follow_pointer) {
		return 0;
	}
#ifdef _DEBUG__
	char *name = win_name(ev->event);
	_LOG_(DEBUG,
		  "RCIEVED BUTTON PRESS EVENT window %d, window name %s",
		  ev->event,
		  name);
	_FREE_(name);
#endif

	if (!window_exists(wm->connection, win)) {
		return 0;
	}
	node_t	 *root	 = curr_monitor->desk->tree;
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n && n->client) ? n->client : NULL;

	if (client == NULL) {
		return -1;
	}

	if (win == wm->root_window) {
		return 0;
	}

	window_ungrab_buttons(client->window);

	const int r = set_active_window_name(win);
	if (r != 0) {
		return 0;
	}

	if (IS_FLOATING(n->client)) {
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR, "cannot focus window %d (enter)", n->client->window);
			return -1;
		}
		n->is_focused = true;
	} else if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return -1;
		}
	} else {
		if (curr_monitor->desk->layout == STACK) {
			if (win_focus(n->client->window, true) != 0) {
				_LOG_(
					ERROR, "cannot focus window %d (enter)", n->client->window);
				return -1;
			}
			n->is_focused = true;
		} else {
			if (set_focus(n, true) != 0) {
				_LOG_(ERROR, "cannot focus node (enter)");
				return -1;
			}
		}
	}

	focused_win = n->client->window;
	update_focus(curr_monitor->desk, n);

	if (has_floating_window(root)) {
		restack();
	}

	xcb_allow_events(wm->connection, XCB_ALLOW_SYNC_POINTER, ev->time);
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_motion_notify(const xcb_event_t *event)
{
	xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
#ifdef _DEBUG__
	_LOG_(INFO,
		  "recevied motion notify on root %dx%d event %dx%d",
		  ev->root_x,
		  ev->root_y,
		  ev->event_x,
		  ev->event_y);
#endif

	if (ms.op != MOUSE_OP_NONE) {
		handle_mouse_motion(ev->root_x, ev->root_y);
		return 0;
	}
	if (ds.active) {
		_LOG_(INFO, "MOTION NOTIFY: x=%d, y=%d", ev->root_x, ev->root_y);
		drag_move(ev->root_x, ev->root_y);
		return 0;
	}

	if (ev->child != XCB_NONE) {
		return 0;
	}
	int16_t	   rx = ev->root_x;
	int16_t	   ry = ev->root_y;
	monitor_t *m  = get_monitor_within_coordinate(rx, ry);
	if (!m) {
		return 0;
	}
	if (curr_monitor && curr_monitor != m) {
		curr_monitor = m;
	}
	return 0;
}

static int
handle_button_release(const xcb_event_t *event)
{
	xcb_button_release_event_t *ev = (xcb_button_release_event_t *)event;

	if (ms.op != MOUSE_OP_NONE) {
		finish_mouse_action();
		return 0;
	}
	if (ds.active) {
		drag_end(ev->root_x, ev->root_y);
		return 0;
	}

	return 0;
}
