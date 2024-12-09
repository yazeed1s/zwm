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

#include "zwm.h"
#include "config_parser.h"
#include "helper.h"
#include "queue.h"
#include "tree.h"
#include "type.h"
#include <X11/keysym.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xinerama.h>
#include <xcb/xproto.h>

#define WINDOW_X		(XCB_CONFIG_WINDOW_X)
#define WINDOW_Y		(XCB_CONFIG_WINDOW_Y)
#define WINDOW_W		(XCB_CONFIG_WINDOW_WIDTH)
#define WINDOW_H		(XCB_CONFIG_WINDOW_HEIGHT)
#define S_NOTIFY		(XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY)
#define S_REDIRECT		(XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define MOVE_RESIZE		(WINDOW_X | WINDOW_Y | WINDOW_W | WINDOW_H)
#define MOVE			(WINDOW_X | WINDOW_Y)
#define RESIZE			(WINDOW_W | WINDOW_H)
#define SUBSTRUCTURE	(S_NOTIFY | S_REDIRECT)

#define PROPERTY_CHANGE (XCB_EVENT_MASK_PROPERTY_CHANGE)
#define FOCUS_CHANGE	(XCB_EVENT_MASK_FOCUS_CHANGE)
#define ENTER_WINDOW	(XCB_EVENT_MASK_ENTER_WINDOW)
#define LEAVE_WINDOW	(XCB_EVENT_MASK_LEAVE_WINDOW)
#define BUTTON_PRESS	(XCB_EVENT_MASK_BUTTON_PRESS)
#define BUTTON_RELEASE	(XCB_EVENT_MASK_BUTTON_RELEASE)
#define POINTER_MOTION	(XCB_EVENT_MASK_POINTER_MOTION)

#define ALT_MASK		(XCB_MOD_MASK_1)
#define SUPER_MASK		(XCB_MOD_MASK_4)
#define SHIFT_MASK		(XCB_MOD_MASK_SHIFT)
#define CTRL_MASK		(XCB_MOD_MASK_CONTROL)
#define CLICK_TO_FOCUS	(XCB_BUTTON_INDEX_1)

#define CLIENT_EVENT_MASK                                                      \
	(PROPERTY_CHANGE | FOCUS_CHANGE | ENTER_WINDOW | LEAVE_WINDOW)

#define ROOT_EVENT_MASK                                                        \
	(SUBSTRUCTURE | BUTTON_PRESS | FOCUS_CHANGE | POINTER_MOTION | ENTER_WINDOW)

#define NUMBER_OF_DESKTOPS 7
#define WM_NAME			   "zwm"
#define WM_CLASS_NAME	   "null"
#define WM_INSTANCE_NAME   "null"

wm_t				 *wm			 = NULL;
config_t			  conf			 = {0};
xcb_window_t		  focused_win	 = XCB_NONE;
xcb_window_t		  meta_window	 = XCB_NONE;
bool				  is_kgrabbed	 = false;
monitor_t			 *prim_monitor	 = NULL;
monitor_t			 *curr_monitor	 = NULL;
monitor_t			 *head_monitor	 = NULL;
bool				  using_xrandr	 = false;
bool				  multi_monitors = false;
bool				  using_xinerama = false;
uint8_t				  randr_base	 = 0;
xcb_cursor_context_t *cursor_ctx;
xcb_cursor_t		  cursors[CURSOR_MAX];

/* clang-format off */

/* keys_[] is used as a fallback in case of an
 * error while loading the keys from the config file */

/* see X11/keysymdef.h */
static const _key__t _keys_[] = {
    DEFINE_KEY(SUPER_MASK,          XK_w,       close_or_kill_wrapper,     NULL),
    DEFINE_KEY(SUPER_MASK,          XK_Return,  exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"alacritty"}})),
    DEFINE_KEY(SUPER_MASK,          XK_space,   exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"dmenu_run"}})),
    DEFINE_KEY(SUPER_MASK,          XK_p,       exec_process,              &((arg_t){.argc = 3, .cmd = (char *[]){"rofi", "-show", "drun"}})),
    DEFINE_KEY(SUPER_MASK,          XK_1,       switch_desktop_wrapper,    &((arg_t){.idx = 0})),
    DEFINE_KEY(SUPER_MASK,          XK_2,       switch_desktop_wrapper,    &((arg_t){.idx = 1})),
    DEFINE_KEY(SUPER_MASK,          XK_3,       switch_desktop_wrapper,    &((arg_t){.idx = 2})),
    DEFINE_KEY(SUPER_MASK,          XK_4,       switch_desktop_wrapper,    &((arg_t){.idx = 3})),
    DEFINE_KEY(SUPER_MASK,          XK_5,       switch_desktop_wrapper,    &((arg_t){.idx = 4})),
    DEFINE_KEY(SUPER_MASK,          XK_6,       switch_desktop_wrapper,    &((arg_t){.idx = 5})),
    DEFINE_KEY(SUPER_MASK,          XK_7,       switch_desktop_wrapper,    &((arg_t){.idx = 6})),
    DEFINE_KEY(SUPER_MASK,          XK_Left,    cycle_win_wrapper,         &((arg_t){.d = LEFT})),
    DEFINE_KEY(SUPER_MASK,          XK_Right,   cycle_win_wrapper,         &((arg_t){.d = RIGHT})),
    DEFINE_KEY(SUPER_MASK,          XK_Up,      cycle_win_wrapper,         &((arg_t){.d = UP})),
    DEFINE_KEY(SUPER_MASK,          XK_Down,    cycle_win_wrapper,         &((arg_t){.d = DOWN})),
    DEFINE_KEY(SUPER_MASK,          XK_l,       horizontal_resize_wrapper, &((arg_t){.r = GROW})),
    DEFINE_KEY(SUPER_MASK,          XK_h,       horizontal_resize_wrapper, &((arg_t){.r = SHRINK})),
    DEFINE_KEY(SUPER_MASK,          XK_f,       set_fullscreen_wrapper,    NULL),
    DEFINE_KEY(SUPER_MASK,          XK_s,       swap_node_wrapper,         NULL),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_1,    transfer_node_wrapper,     &((arg_t){.idx = 0})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_2,    transfer_node_wrapper,     &((arg_t){.idx = 1})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_3,    transfer_node_wrapper,     &((arg_t){.idx = 2})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_4,    transfer_node_wrapper,     &((arg_t){.idx = 3})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_5,    transfer_node_wrapper,     &((arg_t){.idx = 4})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_6,    transfer_node_wrapper,     &((arg_t){.idx = 5})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_7,    transfer_node_wrapper,     &((arg_t){.idx = 6})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_m,    layout_handler,            &((arg_t){.t = MASTER})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_d,    layout_handler,            &((arg_t){.t = DEFAULT})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_s,    layout_handler,            &((arg_t){.t = STACK})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_k,    traverse_stack_wrapper,    &((arg_t){.d = UP})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_j,    traverse_stack_wrapper,    &((arg_t){.d = DOWN})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_f,    flip_node_wrapper,         NULL),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_r,    reload_config_wrapper,     NULL),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_Left, cycle_desktop_wrapper,     &((arg_t){.d = LEFT})),
    DEFINE_KEY(SUPER_MASK | SHIFT_MASK, XK_Right,cycle_desktop_wrapper,     &((arg_t){.d = RIGHT})),
    DEFINE_KEY(SHIFT_MASK,          XK_Left,    shift_floating_window,     &((arg_t){.d = LEFT})),
    DEFINE_KEY(SHIFT_MASK,          XK_Right,   shift_floating_window,     &((arg_t){.d = RIGHT})),
    DEFINE_KEY(SHIFT_MASK,          XK_Up,      shift_floating_window,     &((arg_t){.d = UP})),
    DEFINE_KEY(SHIFT_MASK,          XK_Down,    shift_floating_window,     &((arg_t){.d = DOWN})),
    DEFINE_KEY(SUPER_MASK,          XK_i,       gap_handler,               &((arg_t){.r = GROW})),
    DEFINE_KEY(SUPER_MASK,          XK_d,       gap_handler,               &((arg_t){.r = SHRINK})),
    DEFINE_KEY(SHIFT_MASK,          XK_f,       change_state,              &((arg_t){.s = FLOATING})),
    DEFINE_KEY(SHIFT_MASK,          XK_t,       change_state,              &((arg_t){.s = TILED})),
};

static const uint32_t _buttons_[] = {
	XCB_BUTTON_INDEX_1, XCB_BUTTON_INDEX_2, XCB_BUTTON_INDEX_3};

static monitor_t *get_focused_monitor();
static int set_fullscreen(node_t *, bool);
static int change_border_attr(xcb_conn_t *, xcb_window_t, uint32_t, uint32_t, bool);
static int resize_window(xcb_window_t, uint16_t x, uint16_t y);
static int move_window(xcb_window_t, int16_t x, int16_t y);
static int win_focus(xcb_window_t, bool);
static void update_grabbed_window(node_t *, node_t *);
static void ungrab_keys(xcb_conn_t *, xcb_window_t);
static void arrange_trees(void);
static int grab_keys(xcb_conn_t *, xcb_window_t);
static desktop_t *init_desktop();
static int ewmh_update_current_desktop(xcb_ewmh_conn_t *, int, uint32_t);
static int ewmh_update_number_of_desktops(void);
static int set_active_window_name(xcb_window_t);
static int change_border_attr(xcb_conn_t *, xcb_window_t, uint32_t, uint32_t, bool);
static int change_window_attr(xcb_conn_t *, xcb_window_t, uint32_t, const void *);
static int configure_window(xcb_conn_t *, xcb_window_t, uint16_t, const void *);
static int set_input_focus(xcb_conn_t *, uint8_t, xcb_window_t, xcb_timestamp_t);
static xcb_atom_t get_atom(char *, xcb_conn_t *);
static bool window_exists(xcb_conn_t *, xcb_window_t);
static int close_or_kill(xcb_window_t);
static int switch_desktop(const int);
static int handle_tiled_window_request(xcb_window_t, desktop_t *);
static int handle_floating_window_request(xcb_window_t, desktop_t *);
static int handle_bar_request(xcb_window_t, desktop_t *);
static int show_window(xcb_window_t win);
static int hide_window(xcb_window_t win);
static xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_conn_t *conn);
static bool setup_desktops(void);
static node_t *get_foucsed_desktop_tree(void);
static int handle_map_request(const xcb_event_t *);
static int handle_unmap_notify(const xcb_event_t *);
static int handle_destroy_notify(const xcb_event_t *);
static int handle_client_message(const xcb_event_t *);
static int handle_configure_request(const xcb_event_t *);
static int handle_enter_notify(const xcb_event_t *);
static int handle_button_press_event(const xcb_event_t *);
static int handle_key_press(const xcb_event_t *);
static int handle_mapping_notify(const xcb_event_t *);
static int handle_leave_notify(const xcb_event_t *);
static int handle_motion_notify(const xcb_event_t *);

/* array of xcb events we need to handle -> {event, handler function} */
static const event_handler_entry_t _handlers_[] = {
	/* core window management events */
	/* map request - is generated when a window wants to be mapped (displayed) on the screen */
    DEFINE_MAPPING(XCB_MAP_REQUEST, handle_map_request),
	/* unmap request - is generated when a window wants to be unmapped (removed) from the screen */
    DEFINE_MAPPING(XCB_UNMAP_NOTIFY, handle_unmap_notify),
	/* destroy notify - is generated when a window is killed */
    DEFINE_MAPPING(XCB_DESTROY_NOTIFY, handle_destroy_notify),
    /* communication and configuration events */
	/* client message (ewmh):
	 * These events are sent by other applications through ewmh protocol to zwm;
	 * I am only responding to requestes where:
	 * 1- the state of the window is changed (below, above, or fullscreen only, rest is ignored)
	 * 		this generates a _NET_WM_STATE message
	 * 2- an application wants to know where a window is located (_NET_ACTIVE_WINDOW),
	 * 		as result, zwm switches to the desktop containing that window.
	 * 3- application wants to be closed (when a user clicks the close button at the corner)
	 * 		this generates a NET_CLOSE_WINDOW message
	 * 4- a desktop change was requested (usually through a status bar)
	 * 		this generates _NET_CURRENT_DESKTOP message
	 * other messages like are ignored intentionally.*/
    DEFINE_MAPPING(XCB_CLIENT_MESSAGE, handle_client_message),
	/* configure request - this is used when a client wants to set or update its
	 * rectangle/positions or stacking mode.
	 * since zwm is a tiling wm, i am mostly ignoring this event even though it
	 * reveals important info for splash screens */
    DEFINE_MAPPING(XCB_CONFIGURE_REQUEST, handle_configure_request),
    /* input and interaction events */
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
    /* key press - is generated when keyboard mapping is changed,
    * it only ungrab the re-grab the keys */
    DEFINE_MAPPING(XCB_MAPPING_NOTIFY, handle_mapping_notify),
   	/* will be implemented if needed */
    /* DEFINE_MAPPING(XCB_MOTION_NOTIFY, handle_motion_notify),*/
    /* DEFINE_MAPPING(XCB_LEAVE_NOTIFY, handle_leave_notify), */
    /* DEFINE_MAPPING(XCB_BUTTON_RELEASE, handle_button_release), */
    /* DEFINE_MAPPING(XCB_KEY_RELEASE, handle_key_release), */
    /* DEFINE_MAPPING(XCB_FOCUS_IN, handle_focus_in), */
    /* DEFINE_MAPPING(XCB_FOCUS_OUT, handle_focus_out), */
    /* DEFINE_MAPPING(XCB_CONFIGURE_NOTIFY, handle_configure_notify), */
    /* DEFINE_MAPPING(XCB_PROPERTY_NOTIFY, handle_property_notify), */
};
/* clang-format on */

static void
load_cursors(void)
{
	if (xcb_cursor_context_new(wm->connection, wm->screen, &cursor_ctx) < 0) {
		_LOG_(ERROR, "failed to allocate xcursor context");
		return;
	}
/* _LOAD_CURSOR_ is reserved by some other lib */
#define __LOAD__CURSOR__(cursor, name)                                         \
	do {                                                                       \
		cursors[cursor] = xcb_cursor_load_cursor(cursor_ctx, name);            \
	} while (0)
	__LOAD__CURSOR__(CURSOR_POINTER, "left_ptr");
	__LOAD__CURSOR__(CURSOR_WATCH, "watch");
	__LOAD__CURSOR__(CURSOR_MOVE, "fleur");
	__LOAD__CURSOR__(CURSOR_XTERM, "xterm");
	__LOAD__CURSOR__(CURSOR_NOT_ALLOWED, "not-allowed");
	__LOAD__CURSOR__(CURSOR_HAND2, "hand2");
#undef __LOAD__CURSOR__
}

static xcb_cursor_t
get_cursor(cursor_t c)
{
	assert(c < CURSOR_MAX);
	return cursors[c];
}

static void
set_cursor(int cursor_id)
{
	xcb_cursor_t c		  = get_cursor(cursor_id);
	uint32_t	 values[] = {c};
	xcb_cookie_t cookie	  = xcb_change_window_attributes_checked(
		  wm->connection, wm->root_window, XCB_CW_CURSOR, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR, "Error setting cursor on root window %d", err->error_code);
		_FREE_(err);
	}
	xcb_flush(wm->connection);
}

/* caller must free */
char *
win_name(xcb_window_t win)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr != 1)
		return NULL;

	char *str = (char *)malloc(t_reply.name_len + 1);
	if (str == NULL)
		return NULL;

	strncpy(str, (char *)t_reply.name, t_reply.name_len);
	str[t_reply.name_len] = '\0';
	xcb_icccm_get_text_property_reply_wipe(&t_reply);

	return str;
}

int
layout_handler(arg_t *arg)
{
	int i = get_focused_desktop_idx();
	if (i == -1) {
		_LOG_(ERROR, "Cannot get focused desktop");
		return -1;
	}

	desktop_t *d = curr_monitor->desktops[i];
	if (arg->t == STACK && d->n_count < 2)
		return 0;

	apply_layout(d, arg->t);
	return render_tree(d->tree);
}

static node_t *
get_foucsed_desktop_tree(void)
{
	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return NULL;

	node_t *root = curr_monitor->desktops[idx]->tree;
	if (root == NULL)
		return NULL;

	return root;
}

static void
render_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (!curr->desktops) {
			curr = curr->next;
			continue;
		}
		for (int i = 0; i < curr->n_of_desktops; i++) {
			if (!curr->desktops[i]->is_focused)
				continue;
			if (is_tree_empty(curr->desktops[i]->tree))
				continue;
			render_tree(curr->desktops[i]->tree);
		}
		curr = curr->next;
	}
}

/* changes the state of a window between tiled and floating. */
int
change_state(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == XCB_NONE)
		return -1;

	node_t *root = get_foucsed_desktop_tree();
	if (root == NULL)
		return -1;

	node_t *n = find_node_by_window_id(root, w);

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
		int h  = g->height / 2;
		int wi = g->width / 2;
		int x  = curr_monitor->rectangle.x +
				(curr_monitor->rectangle.width / 2) - (wi / 2);
		int y = curr_monitor->rectangle.y +
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

	return render_tree(root);
}

static xcb_ewmh_conn_t *
ewmh_init(xcb_conn_t *conn)
{
	if (conn == 0x00) {
		_LOG_(ERROR, "Connection is NULL");
		return NULL;
	}

	xcb_ewmh_conn_t *ewmh = calloc(1, sizeof(xcb_ewmh_conn_t));
	if (ewmh == NULL) {
		_LOG_(ERROR, "Cannot calloc ewmh");
		return NULL;
	}

	xcb_intern_atom_cookie_t *c = xcb_ewmh_init_atoms(conn, ewmh);
	if (c == 0x00) {
		_LOG_(ERROR, "Cannot init intern atom");
		return NULL;
	}

	const uint8_t res = xcb_ewmh_init_atoms_replies(ewmh, c, NULL);
	if (res != 1) {
		_LOG_(ERROR, "Cannot init intern atom");
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
		_LOG_(ERROR, "Error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie))) {
		_LOG_(ERROR, "Error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, name_cookie))) {
		fprintf(stderr, "Error setting WM name: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, pid_cookie))) {
		fprintf(stderr, "Error setting WM PID: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
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

static desktop_t *
get_focused_desktop(void)
{
	monitor_t *focused_monitor = get_focused_monitor();
	for (int i = focused_monitor->n_of_desktops; i--;) {
		if (focused_monitor->desktops[i] &&
			focused_monitor->desktops[i]->is_focused) {
			return focused_monitor->desktops[i];
		}
	}

	return NULL;
}

static int
ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd)
{
	xcb_cookie_t cookie =
		xcb_ewmh_set_number_of_desktops_checked(ewmh, screen_nbr, nd);
	xcb_error_t *err = xcb_request_check(ewmh->connection, cookie);
	if (err) {
		_LOG_(ERROR, "Error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
ewmh_update_desktop_names(void)
{
	char		 names[MAXLEN];
	uint32_t	 names_len = 0;
	unsigned int offset	   = 0;
	memset(names, 0, sizeof(names));

	for (int n = 0; n < prim_monitor->n_of_desktops; n++) {
		desktop_t *d = prim_monitor->desktops[n];
		for (int j = 0; d->name[j] != '\0' && (offset + j) < sizeof(names);
			 j++) {
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
		_LOG_(ERROR, "Error setting names of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int16_t
modfield_from_keysym(xcb_keysym_t keysym)
{
	uint16_t						  modfield = 0;
	xcb_keycode_t					 *keycodes = NULL, *mod_keycodes = NULL;
	xcb_get_modifier_mapping_reply_t *reply = NULL;
	xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(wm->connection);

	if ((keycodes = xcb_key_symbols_get_keycode(symbols, keysym)) == NULL ||
		(reply = xcb_get_modifier_mapping_reply(
			 wm->connection, xcb_get_modifier_mapping(wm->connection), NULL)) ==
			NULL ||
		reply->keycodes_per_modifier < 1 ||
		(mod_keycodes = xcb_get_modifier_mapping_keycodes(reply)) == NULL) {
		goto end;
	}

	unsigned int num_mod = xcb_get_modifier_mapping_keycodes_length(reply) /
						   reply->keycodes_per_modifier;
	for (unsigned int i = 0; i < num_mod; i++) {
		for (unsigned int j = 0; j < reply->keycodes_per_modifier; j++) {
			xcb_keycode_t mk =
				mod_keycodes[i * reply->keycodes_per_modifier + j];
			if (mk == XCB_NO_SYMBOL) {
				continue;
			}
			for (xcb_keycode_t *k = keycodes; *k != XCB_NO_SYMBOL; k++) {
				if (*k == mk) {
					modfield |= (1 << i);
				}
			}
		}
	}

end:
	xcb_key_symbols_free(symbols);
	_FREE_(keycodes);
	_FREE_(reply);
	return modfield;
}

static void
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
		const int current_size =
			xcb_get_property_value_length(reply) / (reply->format / 8);
		xcb_atom_t values[current_size];
		memset(values, 0, sizeof(values));
		for (int i = 0; i < current_size; i++) {
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

/* stack win1 above win2 */
void
window_above(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_ABOVE};
	xcb_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

/* stack win1 below win2 */
void
window_below(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_BELOW};
	xcb_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

void
lower_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_BELOW};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

void
raise_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_ABOVE};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

int
swap_node_wrapper()
{
	if (curr_monitor == NULL) {
		_LOG_(ERROR, "Failed to swap node, current monitor is NULL");
		return -1;
	}

	node_t *root = get_foucsed_desktop_tree();
	if (root == NULL)
		return -1;

	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = get_focused_node(root);
	if (n == NULL)
		return -1;

	if (swap_node(n) != 0)
		return -1;

	return render_tree(root);
}

/* transfer_node_wrapper - handles transferring a node between desktops.
 *
 * This function moves the focused window (or the one under the cursor)
 * from the currently active desktop to another desktop.
 *
 * How it works:
 * 1. Determines the focused desktop and retrieves the focused node.
 * 2. If the window is already on the target desktop, it logs a message and
 * exits.
 * 3. Hides the window visually before unlinking it from the source desktop.
 * 4. Calls `transfer_node` to handle the actual node relocation.
 * 5. Updates node counts for both desktops and re-renders the source desktop.
 *
 * Handles:
 * - Rearranging the source and target desktop trees to keep things consistent.
 *
 * Notes:
 * - This wrapper is focused on coordinating the high-level flow,
 *   it leaves layout-specific handling to the `transfer_node` function.
 */
int
transfer_node_wrapper(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window)
		return 0;

	const int i = arg->idx;
	int		  d = get_focused_desktop_idx();
	if (d == -1)
		return d;

	if (d == i) {
		_LOG_(INFO, "switch node to curr desktop... abort");
		return 0;
	}

	node_t *root = curr_monitor->desktops[d]->tree;
	if (is_tree_empty(root)) {
		return 0;
	}

	node_t *node = get_focused_node(root);
	if (!node) {
		_LOG_(ERROR, "focused node is null");
		return 0;
	}

	desktop_t *nd = curr_monitor->desktops[i];
	desktop_t *od = curr_monitor->desktops[d];
#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", i + 1);
	log_tree_nodes(nd->tree);
	_LOG_(INFO, "old desktop %d nodes--------------", d + 1);
	log_tree_nodes(od->tree);
#endif
	if (set_visibility(node->client->window, false) != 0) {
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
	arrange_tree(nd->tree, nd->layout);
	if (nd->layout == STACK) {
		set_focus(node, true);
	}
	if (!is_tree_empty(od->tree)) {
		arrange_tree(od->tree, od->layout);
	}
	return render_tree(od->tree);
}

int
horizontal_resize_wrapper(arg_t *arg)
{
	const int i = get_focused_desktop_idx();
	if (i == -1)
		return -1;

	if (curr_monitor->desktops[i]->layout == STACK) {
		return 0;
	}

	node_t *root = curr_monitor->desktops[i]->tree;
	if (root == NULL)
		return -1;

	node_t *n = get_focused_node(root);
	if (n == NULL)
		return -1;
	/* todo: if node was flipped, reize up or down instead */
	grab_pointer(wm->root_window,
				 false); /* steal the pointer and prevent it from sending
						  * enter_notify events (which focuses the window
						  * being under cursor as the resize happens); */
	horizontal_resize(n, arg->r);
	render_tree(root);
	ungrab_pointer();
	return 0;
}

int
shift_floating_window(arg_t *arg)
{
	node_t *root = get_foucsed_desktop_tree();
	if (root == NULL)
		return -1;

	node_t *n = get_focused_node(root);
	if (n == NULL)
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const int16_t pxl			 = 10;
	int16_t		  new_x			 = n->floating_rectangle.x;
	int16_t		  new_y			 = n->floating_rectangle.y;
	int16_t		  monitor_x		 = curr_monitor->rectangle.x;
	int16_t		  monitor_y		 = curr_monitor->rectangle.y;
	int16_t		  monitor_width	 = curr_monitor->rectangle.width;
	int16_t		  monitor_height = curr_monitor->rectangle.height;

	switch (arg->d) {
	case LEFT:
		new_x -= pxl;
		if (new_x < monitor_x) {
			return 0;
		}
		break;
	case RIGHT:
		new_x += pxl;
		if (new_x + n->floating_rectangle.width > monitor_x + monitor_width) {
			return 0;
		}
		break;
	case UP:
		new_y -= pxl;
		if (new_y < monitor_y) {
			return 0;
		}
		break;
	case DOWN:
		new_y += pxl;
		if (new_y + n->floating_rectangle.height > monitor_y + monitor_height) {
			return 0;
		}
		break;
	case NONE: return 0;
	}

	grab_pointer(wm->root_window, false);
	if (move_window(n->client->window, new_x, new_y) != 0) {
		return -1;
	}

	n->floating_rectangle.x = new_x;
	n->floating_rectangle.y = new_y;
	ungrab_pointer();
	return 0;
}

int
set_fullscreen_wrapper()
{
	node_t *root = get_foucsed_desktop_tree();
	if (root == NULL)
		return -1;

	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = find_node_by_window_id(root, w);
	if (n == NULL) {
		_LOG_(ERROR, "cannot find focused node");
		return -1;
	}

	n->client->state == FULLSCREEN ? set_fullscreen(n, false)
								   : set_fullscreen(n, true);
	return 0;
}

static int
set_fullscreen(node_t *n, bool flag)
{
	if (n == NULL)
		return -1;

	rectangle_t r = {0};
	if (flag) {
		long data[]		 = {wm->ewmh->_NET_WM_STATE_FULLSCREEN};
		r.x				 = curr_monitor->rectangle.x;
		r.y				 = curr_monitor->rectangle.y;
		r.width			 = curr_monitor->rectangle.width;
		r.height		 = curr_monitor->rectangle.height;
		n->client->state = FULLSCREEN;
		if (change_border_attr(wm->connection,
							   n->client->window,
							   conf.normal_border_color,
							   0,
							   false) != 0) {
			return -1;
		}
		if (resize_window(n->client->window, r.width, r.height) != 0 ||
			move_window(n->client->window, r.x, r.y) != 0) {
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
			_LOG_(ERROR, "Error changing window property: %d", err->error_code);
			_FREE_(err);
			return -1;
		}
		goto out;
	}

	r				 = n->rectangle;
	n->client->state = TILED;
	if (resize_window(n->client->window, r.width, r.height) != 0 ||
		move_window(n->client->window, r.x, r.y) != 0) {
		return -1;
	}
	remove_property(wm->connection,
					n->client->window,
					wm->ewmh->_NET_WM_STATE,
					wm->ewmh->_NET_WM_STATE_FULLSCREEN);
	if (change_border_attr(wm->connection,
						   n->client->window,
						   conf.normal_border_color,
						   conf.border_width,
						   true) != 0) {
		return -1;
	}
out:
	xcb_flush(wm->connection);
	return 0;
}

static int
change_colors(node_t *root)
{
	if (root == NULL)
		return 0;

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

/* TODO: rewrite this shit */
static void
apply_monitor_layout_changes(monitor_t *m)
{
	for (int d = 0; d < m->n_of_desktops; ++d) {
		if (!m->desktops[d])
			continue;
		if (is_tree_empty(m->desktops[d]->tree))
			continue;
		layout_t l	  = m->desktops[d]->layout;
		node_t	*tree = m->desktops[d]->tree;
		if (l == DEFAULT) {
			rectangle_t	   r = {0};
			uint16_t	   w = m->rectangle.width;
			uint16_t	   h = m->rectangle.height;
			const uint16_t x = m->rectangle.x;
			const uint16_t y = m->rectangle.y;
			if (wm->bar && m == prim_monitor) {
				r.x		 = x + conf.window_gap;
				r.y		 = y + wm->bar->rectangle.height + conf.window_gap;
				r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
				r.height = h - wm->bar->rectangle.height - 2 * conf.window_gap -
						   2 * conf.border_width;
			} else {
				r.x		 = x + conf.window_gap;
				r.y		 = y + conf.window_gap;
				r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
				r.height = h - 2 * conf.window_gap - 2 * conf.border_width;
			}
			tree->rectangle = r;
			apply_default_layout(tree);
		} else if (l == MASTER) {
			node_t		  *ms			= find_master_node(tree);
			const double   ratio		= 0.70;
			uint16_t	   w			= m->rectangle.width;
			uint16_t	   h			= m->rectangle.height;
			const uint16_t x			= m->rectangle.x;
			const uint16_t y			= m->rectangle.y;
			uint16_t	   master_width = w * ratio;
			uint16_t	   r_width		= (uint16_t)(w * (1 - ratio));
			if (ms == NULL) {
				ms = find_any_leaf(tree);
				if (ms == NULL) {
					return;
				}
			}
			ms->is_master = true;
			uint16_t bar_height =
				wm->bar == NULL ? 0 : wm->bar->rectangle.height;
			rectangle_t r1 = {
				.x		= x + conf.window_gap,
				.y		= (int16_t)(y + bar_height + conf.window_gap),
				.width	= (uint16_t)(master_width - 2 * conf.window_gap),
				.height = (uint16_t)(h - 2 * conf.window_gap - bar_height),
			};
			rectangle_t r2 = {
				.x		= (x + master_width),
				.y		= (int16_t)(y + bar_height + conf.window_gap),
				.width	= (uint16_t)(r_width - (1 * conf.window_gap)),
				.height = (uint16_t)(h - 2 * conf.window_gap - bar_height),
			};
			ms->rectangle	= r1;
			tree->rectangle = r2;
			apply_master_layout(tree);
		} else if (l == STACK) {
			rectangle_t	   r = {0};
			uint16_t	   w = m->rectangle.width;
			uint16_t	   h = m->rectangle.height;
			const uint16_t x = m->rectangle.x;
			const uint16_t y = m->rectangle.y;
			if (wm->bar && m == prim_monitor) {
				r.x		 = x + conf.window_gap;
				r.y		 = y + wm->bar->rectangle.height + conf.window_gap;
				r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
				r.height = h - wm->bar->rectangle.height - 2 * conf.window_gap -
						   2 * conf.border_width;
			} else {
				r.x		 = x + conf.window_gap;
				r.y		 = y + conf.window_gap;
				r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
				r.height = h - 2 * conf.window_gap - 2 * conf.border_width;
			}
			tree->rectangle = r;
			apply_stack_layout(tree);
		} else if (l == GRID) {
			// todo
		}
	}
}

static void
arrange_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		apply_monitor_layout_changes(curr);
		curr = curr->next;
	}
}

/* TODO: rewrite this ugly shit */
int
reload_config_wrapper()
{
	uint16_t prev_border_width		  = conf.border_width;
	uint16_t prev_window_gap		  = conf.window_gap;
	uint32_t prev_active_border_color = conf.active_border_color;
	uint32_t prev_normal_border_color = conf.normal_border_color;
	int		 prev_virtual_desktops	  = conf.virtual_desktops;

	memset(&conf, 0, sizeof(config_t));

	ungrab_keys(wm->connection, wm->root_window);
	is_kgrabbed = false;
	free_keys();
	free_rules();
	assert(key_head == NULL && rule_head == NULL);

	if (reload_config(&conf) != 0) {
		_LOG_(ERROR, "Error while reloading config -> using default macros");

		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		if (0 != grab_keys(wm->connection, wm->root_window)) {
			_LOG_(ERROR, "cannot grab keys after reload");
			return -1;
		}
		return 0;
	}

	bool color_changed =
		(prev_normal_border_color != conf.normal_border_color) ||
		(prev_active_border_color != conf.active_border_color);
	bool layout_changed = (conf.window_gap != prev_window_gap) ||
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
		_LOG_(INFO, "Reloading desktop changes is not implemented yet");
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
					d->id		  = (uint8_t)j;
					d->is_focused = false;
					d->layout	  = DEFAULT;
					snprintf(d->name, sizeof(d->name), "%d", j + 1);
					current_monitor->desktops[j] = d;
				}
				current_monitor = current_monitor->next;
			}
		} else if (conf.virtual_desktops < prev_virtual_desktops) {
			int		   idx			   = get_focused_desktop_idx();
			monitor_t *current_monitor = head_monitor;
			while (current_monitor) {
				for (int j = conf.virtual_desktops; j < prev_virtual_desktops;
					 j++) {
					if (idx == current_monitor->desktops[j]->id) {
						switch_desktop_wrapper(&(arg_t){.idx = idx--});
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

		const int di = get_focused_desktop_idx();
		if (di == -1) {
			return false;
		}

		if (ewmh_update_current_desktop(
				wm->ewmh, wm->screen_nbr, (uint32_t)di) != 0) {
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
	render_tree(curr_monitor->desktops[get_focused_desktop_idx()]->tree);
	xcb_flush(wm->connection);
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

	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return -1;

	render_tree(curr_monitor->desktops[idx]->tree);
	xcb_flush(wm->connection);

	return 0;
}

int
flip_node_wrapper()
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	node_t *tree = get_foucsed_desktop_tree();
	if (!tree)
		return -1;
	node_t *node = get_focused_node(tree);
	if (node == NULL)
		return -1;

	flip_node(node);
	return render_tree(tree);
}

int
cycle_win_wrapper(arg_t *arg)
{
	direction_t d	 = arg->d;
	node_t	   *root = get_foucsed_desktop_tree();
	if (!root) {
		return 0;
	}
	node_t *f = get_focused_node(root);
	if (!f) {
		_LOG_(INFO, "cannot find focused window");
		xcb_window_t w =
			get_window_under_cursor(wm->connection, wm->root_window);
		f = find_node_by_window_id(root, w);
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
	set_focus(next, true);
	set_active_window_name(next->client->window);
	update_focus(root, next);

	return 0;
}

int
traverse_stack_wrapper(arg_t *arg)
{
	direction_t	 d = arg->d;
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	node_t *root = get_foucsed_desktop_tree();
	if (!root)
		return -1;

	node_t *node = get_focused_node(root);
	node_t *n	 = d == UP ? next_node(node) : prev_node(node);

	if (n == NULL) {
		return -1;
	}

	set_focus(n, true);
	if (has_floating_window(root))
		restack();

	return 0;
}

static size_t
get_active_clients_size_prime()
{
	size_t	   t	= 0;
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; ++i) {
			t += curr->desktops[i]->n_count;
		}
		curr = curr->next;
	}
	return t;
}

static size_t
get_active_clients_size(desktop_t **d, const int n)
{
	size_t t = 0;
	for (int i = 0; i < n; ++i) {
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

static void
ewmh_update_client_list(void)
{
	size_t size = get_active_clients_size(prim_monitor->desktops,
										  prim_monitor->n_of_desktops);
	if (size == 0) {
		xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, 0, NULL);
		_LOG_(ERROR, "unable to get clients size");
		return;
	}
	/* TODO handle stacking client list --
	 * xcb_ewmh_set_client_list_stacking(...)
	 * xcb_window_t active_clients[size+1]; */
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

static int
ewmh_update_current_desktop(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t i)
{
	xcb_cookie_t c = xcb_ewmh_set_current_desktop_checked(ewmh, screen_nbr, i);
	xcb_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "Error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static xcb_get_geometry_reply_t *
get_geometry(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(conn, win);
	xcb_error_t				 *err;
	xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(conn, gc, &err);
	if (err) {
		_LOG_(ERROR,
			  "Error getting geometry for window %u: %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return NULL;
	}

	if (gr == NULL) {
		_LOG_(ERROR, "Failed to get geometry for window %u", win);
		return NULL;
	}
	return gr;
}

static client_t *
create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn)
{
	client_t *c = (client_t *)malloc(sizeof(client_t));
	if (c == 0x00)
		return NULL;

	c->window				= win;
	c->type					= wtype;
	c->border_width			= (uint32_t)-1;
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {CLIENT_EVENT_MASK};
	xcb_cookie_t   cookie =
		xcb_change_window_attributes_checked(conn, c->window, mask, values);
	xcb_error_t *err = xcb_request_check(conn, cookie);
	if (err) {
		_LOG_(ERROR,
			  "Error setting window attributes for client %u: %d",
			  c->window,
			  err->error_code);
		_FREE_(err);
		_FREE_(c);
		exit(EXIT_FAILURE);
	}

	if (change_border_attr(wm->connection,
						   win,
						   conf.normal_border_color,
						   conf.border_width,
						   false) != 0) {
		_LOG_(ERROR, "Failed to change border attr for window %d", win);
		_FREE_(c);
		return NULL;
	}

	return c;
}

static desktop_t *
init_desktop(void)
{
	desktop_t *d = (desktop_t *)malloc(sizeof(desktop_t));
	if (d == 0x00)
		return NULL;
	d->id		  = 0;
	d->is_focused = false;
	d->n_count	  = 0;
	d->tree		  = NULL;
	return d;
}

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
	m->is_focused  = false;
	m->is_occupied = false;
	m->is_wired	   = false;
	m->next		   = NULL;
	return m;
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
			  "found monitor %s:%d, rectangle {.x = %d, .y = %d, .w = %d, "
			  ".h = "
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

/* init_wm - initializes the window manager by setting up the necessary
 * X connection, retrieving screen information, and creating the required
 * windows */
static wm_t *
init_wm(void)
{
	int i, default_screen;
	wm = (wm_t *)malloc(sizeof(wm_t));
	if (wm == NULL) {
		_LOG_(ERROR, "Failed to malloc for window manager");
		return NULL;
	}

	wm->connection = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(wm->connection) > 0) {
		_LOG_(ERROR, "Error: Unable to open X connection");
		_FREE_(wm);
		return NULL;
	}

	const xcb_setup_t	 *setup = xcb_get_setup(wm->connection);
	xcb_screen_iterator_t iter	= xcb_setup_roots_iterator(setup);
	for (i = 0; i < default_screen; ++i) {
		xcb_screen_next(&iter);
	}
	wm->screen				= iter.data;
	wm->root_window			= iter.data->root;
	wm->screen_nbr			= default_screen;
	wm->split_type			= DYNAMIC_TYPE;
	wm->bar					= NULL;
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {ROOT_EVENT_MASK};

	/* register events */
	xcb_cookie_t   cookie	= xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "Error registering for substructure redirection "
			  "events on window "
			  "%u: %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(wm);
		_FREE_(err);
		return NULL;
	}

	meta_window				 = xcb_generate_id(wm->connection);
	xcb_connection_t *dpy	 = wm->connection;
	uint8_t			  depth	 = XCB_COPY_FROM_PARENT;
	xcb_window_t	  mw	 = meta_window;
	xcb_window_t	  rw	 = wm->root_window;
	uint32_t		  m		 = XCB_NONE;
	xcb_visualid_t	  visual = XCB_COPY_FROM_PARENT;
	uint16_t class			 = XCB_WINDOW_CLASS_INPUT_ONLY;

	xcb_create_window(
		dpy, depth, mw, rw, -1, -1, 1, 1, 0, class, visual, m, NULL);
	xcb_icccm_set_wm_class(dpy, mw, sizeof(WM_NAME), WM_NAME);
	return wm;
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

static monitor_t *
get_focused_monitor()
{
	xcb_query_pointer_cookie_t pointer_cookie =
		xcb_query_pointer(wm->connection, wm->root_window);
	xcb_query_pointer_reply_t *pointer_reply =
		xcb_query_pointer_reply(wm->connection, pointer_cookie, NULL);

	if (pointer_reply == NULL) {
		_LOG_(ERROR, "Failed to query pointer");
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
get_connected_monitor_count_xinerama()
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
get_connected_monitor_count_xrandr()
{
	xcb_randr_get_screen_resources_current_cookie_t c =
		xcb_randr_get_screen_resources_current(wm->connection, wm->root_window);
	xcb_randr_get_screen_resources_current_reply_t *sres =
		xcb_randr_get_screen_resources_current_reply(wm->connection, c, NULL);
	if (sres == NULL) {
		fprintf(stderr, "Failed to get screen resources");
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

/* setup_monitors_via_xrandr - initializes monitors using the Xrandr extension
 * by querying the screen resources and output information */
static bool
setup_monitors_via_xrandr()
{
	xcb_connection_t							   *conn = wm->connection;
	xcb_window_t									root = wm->root_window;
	/* get screen resources (primary output, crtcs, outputs, modes, etc) */
	xcb_randr_get_screen_resources_current_cookie_t sc =
		xcb_randr_get_screen_resources_current(conn, root);
	xcb_randr_get_screen_resources_current_reply_t *sr =
		xcb_randr_get_screen_resources_current_reply(conn, sc, NULL);
	if (sr == NULL) {
		_LOG_(ERROR, "failed to query screen resources");
		return false;
	}
	const xcb_timestamp_t time = sr->config_timestamp;
	/* an output is eDP, HDMI-* VGA-*, etc. (a physical video outputs) */
	const int len = xcb_randr_get_screen_resources_current_outputs_length(sr);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(sr);
	xcb_randr_get_output_info_cookie_t oc[len];
	/* loop through all outputs available for this X screen and get their info
	 */
	for (int i = 0; i < len; i++) {
		oc[i] = xcb_randr_get_output_info(conn, outputs[i], time);
	}
	int monitors = 0;
	/* loop through all outputs and uses the connected ones */
	for (int i = 0; i < len; i++) {
		/* request information for each output */
		xcb_randr_get_output_info_reply_t *info;
		if ((info = xcb_randr_get_output_info_reply(conn, oc[i], NULL)) ==
			NULL) {
			_LOG_(INFO, "could not query output info... skipping this output");
			continue;
		}
		/* skip if this ouput isn't connected */
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			_LOG_(INFO, "output is disconnected... skipping this output");
			_FREE_(info);
			continue;
		}
		/* skip if this ouput has no crtc */
		if (info->crtc == XCB_NONE) {
			_LOG_(INFO, "output crtc is empty... skipping this output");
			_FREE_(info);
			continue;
		}
		/* if we rached here, then this output is connected and has a valid
		 * crtc. */
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
			  "Monitor name = %.*s:%d, out %d Monitor "
			  "rectangle = x = "
			  "%d, y = %d, w = %d, h = %d",
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
setup_monitors_via_xinerama()
{
	xcb_xinerama_query_screens_cookie_t query_screens_c =
		xcb_xinerama_query_screens(wm->connection);
	xcb_xinerama_query_screens_reply_t *query_screens_r =
		xcb_xinerama_query_screens_reply(wm->connection, query_screens_c, NULL);
	xcb_xinerama_screen_info_t *xinerama_screen_i =
		xcb_xinerama_query_screens_screen_info(query_screens_r);
	if (query_screens_r == NULL) {
		_LOG_(ERROR, "Failed to query Xinerama screens");
		return false;
	}
	int n = xcb_xinerama_query_screens_screen_info_length(query_screens_r);
	for (int i = 0; i < n; i++) {
		xcb_xinerama_screen_info_t info = xinerama_screen_i[i];
		rectangle_t				   r =
			(rectangle_t){info.x_org, info.y_org, info.width, info.height};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "Failed to allocate single monitor");
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

static void
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

static void
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

static int
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

/* setup_monitors - called when we first establishe a
 * connection to the X server and need the initial information to setup
 * monitors. It checks for the availability of Xrandr
 * or Xinerama extensions and configures the monitors accordingly.
 */
static bool
setup_monitors(void)
{
	/* if we should use a single global screen, rarely happens, or never */
	bool							   use_global_screen = false;
	/* query for the Xrandr extension */
	const xcb_query_extension_reply_t *query_xr			 = NULL;
	/* query for the Xinerama extension */
	const xcb_query_extension_reply_t *query_x			 = NULL;

	query_xr = xcb_get_extension_data(wm->connection, &xcb_randr_id);
	query_x	 = xcb_get_extension_data(wm->connection, &xcb_xinerama_id);

	/* if xrandr is available, we use it for managing monitors */
	if (query_xr->present) {
		/* set the flag for xrandr */
		using_xrandr = true;
		/* get the base event number for xrandr */
		randr_base	 = query_xr->first_event;
		/* listen for screen change notifications from xrandr */
		xcb_randr_select_input(wm->connection,
							   wm->root_window,
							   XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	} else if (query_x->present) {
		/* if xinerama is available but xrandr is not, use xinerama */
		bool							xinerama_is_active = false;
		/* check if xinerama is active */
		xcb_xinerama_is_active_cookie_t xc =
			xcb_xinerama_is_active(wm->connection);
		xcb_xinerama_is_active_reply_t *xis_active =
			xcb_xinerama_is_active_reply(wm->connection, xc, NULL);
		if (xis_active) {
			xinerama_is_active = xis_active->state;
			_FREE_(xis_active);
			/* enable xinerama based on its state */
			using_xinerama = xinerama_is_active;
		}
	} else {
		/* if neither xrandr nor xinerama is available, set both
		 * flags to false.
		 * This should NEVER happen, but in case it does, we use
		 * the global screen as a monitor. */
		using_xrandr = using_xinerama = false;
	}

	/* get the number of connected monitors based on the available extension */
	int n = get_connected_monitor_count(using_xrandr, using_xinerama);

	/* if neither extension is available and only one "monitor" is connected,
	 * default to global screen */
	if (!using_xrandr && !using_xinerama && n == 1) {
		_LOG_(ERROR, "Neither Xrandr nor Xinerama extensions are available");
		use_global_screen = true;
	}

	/* if we are using a global screen, set up a single monitor */
	if (use_global_screen) {
		rectangle_t r = (rectangle_t){
			0, 0, wm->screen->width_in_pixels, wm->screen->height_in_pixels};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "Failed to allocate single monitor");
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
		add_monitor(&head_monitor, m);
		prim_monitor = curr_monitor = m;
		goto out; /* skip to the end of the function */
	}

	bool setup_success = false;
	/* if using xrandr and not xinerama, set up monitors via xrandr */
	if (using_xrandr) {
		setup_success = setup_monitors_via_xrandr();
		if (setup_success) {
			_LOG_(INFO, "Monitors successfully set up using Xrandr");
		}
	} else if (using_xinerama) {
		/* if using xinerama and not xrandr, set up monitors via xinerama */
		setup_success = setup_monitors_via_xinerama();
		if (setup_success) {
			_LOG_(INFO, "Monitors successfully set up using Xinerama");
		}
	}

	/* if setup fails, log an error and return false */
	if (!setup_success) {
		_LOG_(ERROR, "failed to set up monitors, defaulting to global screen");
		return false;
	}

/* set monitors roots */
#if 0
	monitor_t *curr = head_monitor;
	while (curr) {
		uint32_t values[] = {ENTER_WINDOW | POINTER_MOTION};
		curr->root		  = xcb_generate_id(wm->connection);
		xcb_create_window(wm->connection,
						  XCB_COPY_FROM_PARENT,
						  curr->root,
						  wm->root_window,
						  curr->rectangle.x,
						  curr->rectangle.y,
						  curr->rectangle.width,
						  curr->rectangle.height,
						  0,
						  XCB_WINDOW_CLASS_INPUT_ONLY,
						  XCB_COPY_FROM_PARENT,
						  XCB_CW_EVENT_MASK,
						  values);
		_LOG_(INFO,
			  "succseffuly created root %d for monitor %s",
			  curr->root,
			  curr->name);
		show_window(curr->root);
		lower_window(curr->root);
		xcb_icccm_set_wm_class(
			wm->connection, curr->root, sizeof(ROOT_WINDOW), ROOT_WINDOW);
		xcb_icccm_set_wm_name(wm->connection,
							  curr->root,
							  XCB_ATOM_STRING,
							  8,
							  strlen(curr->name),
							  curr->name);
	}
#endif
	/* get the primary monitor output using xrandr, this will fail if xinerma is
	 * used, but it is fine we make head_mon our primary if that happens */
	xcb_randr_get_output_primary_cookie_t ccc =
		xcb_randr_get_output_primary(wm->connection, wm->root_window);
	xcb_randr_get_output_primary_reply_t *primary_output_reply =
		xcb_randr_get_output_primary_reply(wm->connection, ccc, NULL);
	if (primary_output_reply) {
		monitor_t *mm = get_monitor_by_randr_id(primary_output_reply->output);
		if (!mm) {
			mm->is_primary = true;
			prim_monitor = curr_monitor = mm;
		} else {
			prim_monitor = curr_monitor = head_monitor;
		}
	} else {
		prim_monitor = curr_monitor = head_monitor;
	}

	_LOG_(INFO,
		  "primary monitor %s:%d id %d, rect = x %d, y %d,width "
		  "%d,height %d",
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

bool
handle_added_monitor(xcb_randr_get_output_info_reply_t *info,
					 xcb_randr_output_t					id)
{
	/* each CRT controller has a rectangle(x,y,w,h) we are interested in.*/
	xcb_randr_get_crtc_info_cookie_t crtc_c =
		xcb_randr_get_crtc_info(wm->connection, info->crtc, XCB_CURRENT_TIME);
	xcb_randr_get_crtc_info_reply_t *crtc =
		xcb_randr_get_crtc_info_reply(wm->connection, crtc_c, NULL);
	if (!crtc) {
		_LOG_(ERROR, "failed to query crtc for %d", id);
		return false;
	}
	/* give me the ouput name, like HDMI-* or eDP etc*/
	char	  *name		= (char *)xcb_randr_get_output_info_name(info);
	size_t	   name_len = xcb_randr_get_output_info_name_length(info);
	/* build up the monitor using the info we gathered so far */
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
		  "monitor name = %.*s:%d, out %d Monitor "
		  "rectangle = x = "
		  "%d, y = %d, w = %d, h = %d was ADDED",
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

void
destroy_monitor(monitor_t *m)
{
	if (!m) {
		_LOG_(ERROR, "attempted to destroy a NULL monitor.");
		return;
	}

	_LOG_(INFO, "removing m from linked list");
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

/* merge_monitors - is called when monitor *m* was disconnected.
 *
 * Instead of losing what was in *m* , this function transfers the
 * windows from *m* to any other avaialble monitor.
 * Note: it transfers windows from desktop[i] to the same desktop[i] in the
 * target monitor */
static bool
merge_monitors(monitor_t *om, monitor_t *nm)
{
	assert(om->n_of_desktops == nm->n_of_desktops);

	for (int i = 0; i < om->n_of_desktops; i++) {
		desktop_t *od = om->desktops[i];
		desktop_t *nd = nm->desktops[i];

		/* skip if the old desktop has no tree - nothing to transfer */
		if (!od->tree) {
			continue;
		}

		queue_t *q = create_queue();
		if (!q)
			return false;

		enqueue(q, od->tree);
		while (!is_queue_empty(q)) {
			node_t *node = dequeue(q);
			/* we only want to transfer leaf nodes with clients.
			 * internal nodes (containers) stay where they are */
			if (!IS_INTERNAL(node) && node->client) {
				/* try to unlink the node. If unlink fails, abort */
				if (!unlink_node(node, od)) {
					_LOG_(ERROR, "failed to unlink node.... abort");
					_FREE_(q);
					return false;
				}

				/* try to transfer the node. If transfer fails, abort */
				if (!transfer_node(node, nd)) {
					_LOG_(ERROR, "Failed to transfer node... abort");
					_FREE_(q);
					return false;
				}
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
		/* rearrange the trees - apply visual changes */
		arrange_tree(nd->tree, nd->layout);
	}
	return true;
}

/* update_monitors - queries current outputs, and checks if anything was changed
 * since we last queried the outputs in setup_monitor().
 *
 * It asks xrandr for a list of connected monitors/outputs and
 * checks for changes like new monitors being connected, existing ones being
 * disconnected, or changes in the layout (resolution, position, etc.).
 * It then updates the monitor list accordingly and handles adding, removing, or
 * updating layouts. */
static void
update_monitors(uint32_t *changes)
{
	monitor_t		 *dl   = NULL; /* a list of monitors to remove*/
	xcb_connection_t *conn = wm->connection;
	xcb_randr_get_screen_resources_current_cookie_t rc		  = {0};
	xcb_randr_get_screen_resources_current_reply_t *resources = NULL;

	/* get screen resources (primary output, crtcs, outputs, modes, etc) */
	rc		  = xcb_randr_get_screen_resources_current(conn, wm->root_window);
	resources = xcb_randr_get_screen_resources_current_reply(conn, rc, NULL);

	if (!resources) {
		_LOG_(ERROR, "failed to get screen resources");
		return;
	}

	/* an output is eDP, HDMI-* VGA-*, etc. (a physical video outputs) */
	int len = xcb_randr_get_screen_resources_current_outputs_length(resources);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(resources);
	int								   monitor_count = 0;
	xcb_randr_get_output_info_cookie_t ic			 = {0};
	xcb_randr_get_output_info_reply_t *info			 = NULL;
	/* loop through all outputs available for this X screen */
	for (int i = 0; i < len; i++) {
		/* request information for each output */
		ic	 = xcb_randr_get_output_info(conn, outputs[i], XCB_CURRENT_TIME);
		info = xcb_randr_get_output_info_reply(conn, ic, NULL);
		if (!info)
			continue;
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			/* this output might have been connnected before, if so, add it
			 * to the list so we can remove it later */
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				_FREE_(info);
				continue;
			}
			/* append to the list */
			exist->next = dl;
			dl			= exist;
		}
		if (info->crtc == XCB_NONE) {
			_FREE_(info);
			continue;
		}
		if (info->connection == XCB_RANDR_CONNECTION_CONNECTED) {
			/* if this output is connected;
			 * 1- it could be new.
			 * 2- it could be old but its resolution or position have
			 * changed.
			 */
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				/* this is a new monitor */
				if (!handle_added_monitor(info, outputs[i])) {
					_LOG_(ERROR, "failed to add new output %d", outputs[i]);
					_FREE_(info);
					continue;
				}
				monitor_count++;
				*changes &= ~_NONE;
				*changes |= CONNECTED;
			} else {
				/* this monitor exists, check whether or not its layout was
				 * changed */
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
	/* check for disconnected monitors */
	if (dl) {
		/* find the primary monitor to transfer desktops to */
		monitor_t *m = prim_monitor;
		if (!m) {
			_LOG_(ERROR, "no primary monitor found to merge with");
			return;
		}
		/* merge and destroy each disconnected monitor */
		while (dl) {
			monitor_t *r = dl;
			dl			 = dl->next;
			_LOG_(INFO, "merging desktops from %s to %s", r->name, m->name);
			/* merge desktops */
			if (!merge_monitors(r, m)) {
				_LOG_(ERROR, "failed to merge desktops from %s", r->name);
				continue;
			}
			/* destroy the disconnected monitor */
			destroy_monitor(r);
		}
		*changes &= ~_NONE;
		*changes |= DISCONNECTED;
	}
	_LOG_(INFO, "%d newly connected monitor", monitor_count);
}

/* TODO: the api for this is ugly, figure out a better way to do it */
/* handle_monitor_changes - handles RandR screen change events,
 * that is when the user changes the screen configuration */
static void
handle_monitor_changes(void)
{
	/* there are 3 scenarios we're interested in
	 * 1- a new monitor is connected, we need to add it to the monitor
	 * list and assign desktops to it.
	 * 2- an exisitng monitor was
	 * disconnected, we need to merge its desktops with the primary monitor
	 * then remove it and free its desktops.
	 * 3- a resolution or oreintation was changed for an existing
	 * monitor, we need to recaluclate the rectangle for it and resize its
	 * trees
	 */

	if (using_xinerama) {
		return;
	}

	uint32_t m_change = 0 | _NONE; /* flags for post processing */
	bool	 render	  = false;
	update_monitors(&m_change);

	if (m_change & _NONE) {
		_LOG_(INFO, "no monitor changes was found");
		return;
	}
	/* post processsing */
	if (m_change & CONNECTED) {
		_LOG_(INFO, "a monitor was connected");
		/* a new monitor was added, we need to assign desktops to it */
		setup_desktops();
	} else if (m_change & DISCONNECTED) {
		_LOG_(INFO, "a monitor was disconnected");
		/* a monitor was disconnected, we need to render and re-arrange the
		 * trees */
		curr_monitor = prim_monitor = head_monitor;
		render						= true;
	} else if (m_change & LAYOUT) {
		_LOG_(INFO, "a monitor's layout was changed");
		/* layout was changed, we need to adopt its new rectangle, render and
		 * re-arrange the trees */
		render = true;
	}

	if (render) {
		arrange_trees();
		render_trees();
	}

	log_monitors();

	multi_monitors = (get_monitors_count() > 1);

	_LOG_(INFO,
		  "in update: multi monitors = %s",
		  multi_monitors ? "true" : "false");
	/* TODO: update ewmh */
}

static monitor_t *
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

static monitor_t *
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

static bool
setup_desktops(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
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
			d->id		  = (uint8_t)j;
			d->is_focused = (j == 0);
			d->layout	  = DEFAULT;
			snprintf(d->name, sizeof(d->name), "%d", j + 1);
			curr->desktops[j] = d;
		}
		_LOG_(INFO, "successfuly assigned desktops for monitor %s", curr->name);
		curr = curr->next;
	}
	return true;
}

static int
ewmh_update_number_of_desktops(void)
{
	uint32_t desktops_count = 0;
	desktops_count			= prim_monitor->n_of_desktops;
	xcb_cookie_t cookie		= xcb_ewmh_set_number_of_desktops_checked(
		wm->ewmh, wm->screen_nbr, desktops_count);
	xcb_error_t *err = xcb_request_check(wm->ewmh->connection, cookie);
	if (err) {
		_LOG_(ERROR, "Error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static bool
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
								wm->ewmh->_NET_WM_STATE_HIDDEN,
								wm->ewmh->_NET_WM_STATE_FULLSCREEN,
								wm->ewmh->_NET_WM_STATE_BELOW,
								wm->ewmh->_NET_WM_STATE_ABOVE,
								wm->ewmh->_NET_WM_STATE_STICKY,
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

	const int di = get_focused_desktop_idx();
	if (di == -1) {
		return false;
	}

	if (ewmh_update_current_desktop(wm->ewmh, wm->screen_nbr, (uint32_t)di) !=
		0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	ewmh_update_desktop_viewport();

	return true;
}

static bool
setup_wm(void)
{
	if (wm == NULL)
		return false;

	if (!setup_monitors()) {
		_LOG_(ERROR, "error while setting up monitors");
		return false;
	}

	if (!setup_desktops()) {
		_LOG_(ERROR, "error while setting up desktops");
		return false;
	}

	if (!setup_ewmh()) {
		_LOG_(ERROR, "error while setting up ewmh");
		return false;
	}
	/* load_cursors(); */
	/* set_cursor(CURSOR_POINTER); */
	/* init_pointer(); */

	return true;
}

static int
resize_window(xcb_window_t win, uint16_t width, uint16_t height)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t values[] = {width, height};
	xcb_cookie_t   cookie =
		xcb_configure_window_checked(wm->connection, win, RESIZE, values);

	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error resizing window (ID %u): %s",
			  win,
			  strerror(err->error_code));
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
move_window(xcb_window_t win, int16_t x, int16_t y)
{
	if (win == 0 || win == XCB_NONE) {
		return 0;
	}

	const uint32_t values[] = {x, y};
	xcb_cookie_t   cookie =
		xcb_configure_window_checked(wm->connection, win, MOVE, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR, "error moving window (ID %u): %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
fullscreen_focus(xcb_window_t win)
{
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor	   = 0;
	uint32_t bwidth	   = 0;

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) != 0) {
		_LOG_(ERROR, "cannot set input focus");
		return -1;
	}

	raise_window(win);

	xcb_flush(wm->connection);
	return 0;
}

static int
win_focus(xcb_window_t win, bool set_focus)
{
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor =
		set_focus ? conf.active_border_color : conf.normal_border_color;
	uint32_t bwidth = conf.border_width;

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_focus) {
		if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) !=
			0) {
			_LOG_(ERROR, "cannot set input focus");
			return -1;
		}
	}

	xcb_flush(wm->connection);
	return 0;
}

static void
hide_bar(xcb_window_t win)
{
	xcb_cookie_t cookie = xcb_unmap_window(wm->connection, win);
	xcb_error_t *err	= xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in unmapping window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return;
	}
	_FREE_(wm->bar);
	arrange_trees();
}

/* TODO: rewrite this */
static int
change_border_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		bcolor,
				   uint32_t		bwidth,
				   bool			stack)
{

	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t stack_	   = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;

	if (change_window_attr(conn, win, bpx_width, &bcolor) != 0) {
		return -1;
	}

	if (configure_window(conn, win, b_width, &bwidth) != 0) {
		return -1;
	}

	if (stack) {
		const uint16_t arg[1] = {XCB_STACK_MODE_ABOVE};
		if (configure_window(conn, win, stack_, arg) != 0) {
			return -1;
		}

		if (set_input_focus(conn, input, win, XCB_CURRENT_TIME) != 0) {
			return -1;
		}
	}

	xcb_flush(conn);
	return 0;
}

static int
change_window_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		attr,
				   const void  *val)
{
	xcb_cookie_t attr_cookie =
		xcb_change_window_attributes_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, attr_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to change window attributes: error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
configure_window(xcb_conn_t	 *conn,
				 xcb_window_t win,
				 uint16_t	  attr,
				 const void	 *val)
{
	xcb_cookie_t config_cookie =
		xcb_configure_window_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, config_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to configure window : error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
set_input_focus(xcb_conn_t	   *conn,
				uint8_t			revert_to,
				xcb_window_t	win,
				xcb_timestamp_t time)
{
	xcb_cookie_t focus_cookie =
		xcb_set_input_focus_checked(conn, revert_to, win, time);
	xcb_error_t *err = xcb_request_check(conn, focus_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to set input focus : error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

int
tile(node_t *node)
{
	if (node == NULL || node->client == NULL) {
		return -1;
	}

	const uint16_t width  = IS_FLOATING(node->client)
								? node->floating_rectangle.width
								: node->rectangle.width;
	const uint16_t height = IS_FLOATING(node->client)
								? node->floating_rectangle.height
								: node->rectangle.height;
	const int16_t  x = IS_FLOATING(node->client) ? node->floating_rectangle.x
												 : node->rectangle.x;
	const int16_t  y = IS_FLOATING(node->client) ? node->floating_rectangle.y
												 : node->rectangle.y;

	if (resize_window(node->client->window, width, height) != 0 ||
		move_window(node->client->window, x, y) != 0) {
		return -1;
	}

	xcb_cookie_t cookie =
		xcb_map_window_checked(wm->connection, node->client->window);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in mapping window %d: error code %d",
			  node->client->window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

static bool
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

static int
display_client(rectangle_t r, xcb_window_t win)
{
	uint16_t width	= r.width;
	uint16_t height = r.height;
	int16_t	 x		= r.x;
	int16_t	 y		= r.y;

	if (resize_window(win, width, height) != 0 || move_window(win, x, y) != 0) {
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

static int16_t
get_cursor_axis(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return -1;
	}

	int16_t x = p_reply->root_x;
	_FREE_(p_reply);

	return x;
}

xcb_window_t
get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return XCB_NONE;
	}

	xcb_window_t x = p_reply->child;
	_FREE_(p_reply);

	return x;
}

static xcb_keycode_t *
get_keycode(xcb_keysym_t keysym, xcb_conn_t *conn)
{
	xcb_key_symbols_t *keysyms = NULL;
	xcb_keycode_t	  *keycode = NULL;

	if ((keysyms = xcb_key_symbols_alloc(conn)) == NULL) {
		xcb_key_symbols_free(keysyms);
		return NULL;
	}

	keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);

	return keycode;
}

static xcb_keysym_t
get_keysym(xcb_keycode_t keycode, xcb_connection_t *conn)
{
	xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);
	xcb_keysym_t	   keysym  = 0;

	if (keysyms) {
		keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
		xcb_key_symbols_free(keysyms);
	}

	return keysym;
}

void
window_grab_buttons(xcb_window_t win)
{
#define _GRAB_BUTTON_(button)                                                  \
	do {                                                                       \
		xcb_grab_button_checked(wm->connection,                                \
								false,                                         \
								win,                                           \
								XCB_EVENT_MASK_BUTTON_PRESS,                   \
								XCB_GRAB_MODE_ASYNC,                           \
								XCB_GRAB_MODE_ASYNC,                           \
								wm->root_window,                               \
								XCB_NONE,                                      \
								button,                                        \
								XCB_MOD_MASK_ANY);                             \
	} while (0)
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_1);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_2);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_3);
#undef _GRAB_BUTTON_
}

static void
window_ungrab_buttons(xcb_window_t win)
{
	xcb_cookie_t cookie = xcb_ungrab_button_checked(
		wm->connection, XCB_BUTTON_INDEX_ANY, win, XCB_MOD_MASK_ANY);

	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in ungrab buttons for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return;
	}
}

static void
ungrab_buttons_for_all(node_t *n)
{
	if (n == NULL)
		return;

	bool flag = n->node_type != INTERNAL_NODE && n->client;

	if (flag) {
		xcb_ungrab_button(wm->connection,
						  XCB_BUTTON_INDEX_ANY,
						  n->client->window,
						  XCB_MOD_MASK_ANY);
	}

	ungrab_buttons_for_all(n->first_child);
	ungrab_buttons_for_all(n->second_child);
}

void
grab_pointer(xcb_window_t win, bool wants_events)
{
	xcb_grab_pointer_reply_t *reply;
	xcb_grab_pointer_cookie_t cookie = xcb_grab_pointer(wm->connection,
														wants_events,
														win,
														XCB_NONE,
														XCB_GRAB_MODE_SYNC,
														XCB_GRAB_MODE_ASYNC,
														XCB_NONE,
														XCB_NONE,
														XCB_CURRENT_TIME);
	if ((reply = xcb_grab_pointer_reply(wm->connection, cookie, NULL))) {
		if (reply->status != XCB_GRAB_STATUS_SUCCESS)
			_LOG_(WARNING, "cannot grab the pointer");
	}
	_FREE_(reply);
}

void
ungrab_pointer(void)
{
	xcb_ungrab_pointer(wm->connection, XCB_CURRENT_TIME);
}

static int
grab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return -1;
	}

	if (key_head) {
		conf_key_t *current = key_head;
		while (current) {
			xcb_keycode_t *key = get_keycode(current->keysym, conn);
			if (key == NULL)
				return -1;
			xcb_cookie_t cookie = xcb_grab_key_checked(conn,
													   1,
													   win,
													   (uint16_t)current->mod,
													   *key,
													   XCB_GRAB_MODE_ASYNC,
													   XCB_GRAB_MODE_ASYNC);
			_FREE_(key);
			xcb_error_t *err = xcb_request_check(conn, cookie);
			if (err) {
				_LOG_(ERROR, "error grabbing key %d", err->error_code);
				_FREE_(err);
				return -1;
			}
			current = current->next;
		}
		is_kgrabbed = true;
		return 0;
	}

	_LOG_(INFO, "----grabbing default keys------");
	const size_t n = sizeof(_keys_) / sizeof(_keys_[0]);

	for (size_t i = n; i--;) {
		xcb_keycode_t *key = get_keycode(_keys_[i].keysym, conn);
		if (key == NULL)
			return -1;
		xcb_cookie_t cookie = xcb_grab_key_checked(conn,
												   1,
												   win,
												   (uint16_t)_keys_[i].mod,
												   *key,
												   XCB_GRAB_MODE_ASYNC,
												   XCB_GRAB_MODE_ASYNC);
		_FREE_(key);
		xcb_error_t *err = xcb_request_check(conn, cookie);
		if (err) {
			_LOG_(ERROR, "error grabbing key %d", err->error_code);
			_FREE_(err);
			return -1;
		}
	}
	is_kgrabbed = true;
	return 0;
}

static xcb_atom_t
get_atom(char *atom_name, xcb_conn_t *conn)
{
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_atom_t				 atom;
	xcb_intern_atom_reply_t *rep;

	atom_cookie =
		xcb_intern_atom(conn, 0, (uint16_t)strlen(atom_name), atom_name);
	rep = xcb_intern_atom_reply(conn, atom_cookie, NULL);
	if (NULL != rep) {
		atom = rep->atom;
		_FREE_(rep);
		return atom;
	}
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
close_or_kill_wrapper()
{
	xcb_window_t win = get_window_under_cursor(wm->connection, wm->root_window);
	if (!window_exists(wm->connection, win))
		return 0;
	return close_or_kill(win);
}

static int
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

static void
ungrab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return;
	}

	const xcb_keycode_t modifier = (xcb_keycode_t)XCB_MOD_MASK_ANY;
	xcb_cookie_t		cookie =
		xcb_ungrab_key_checked(conn, XCB_GRAB_ANY, win, modifier);
	xcb_error_t *err = xcb_request_check(conn, cookie);
	if (err) {
		_LOG_(ERROR, "error ungrabbing keys: %d", err->error_code);
		_FREE_(err);
	}
}

static void
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
	resize_window(x, rc.width, rc.height);
	move_window(x, rc.x, rc.y);
	xcb_map_window(wm->connection, x);
}

static void
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

static bool
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

static int
kill_window(xcb_window_t win)
{
	if (win == XCB_NONE) {
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

	int curi = get_focused_desktop_idx();
	if (curi == -1) {
		_LOG_(ERROR, "cannot find focused desktop");
		return curi;
	}

	desktop_t *d			   = curr_monitor->desktops[curi];
	node_t	  *n			   = find_node_by_window_id(d->tree, win);
	client_t  *c			   = (n) ? n->client : NULL;
	bool	   another_desktop = false;
	if (c == NULL) {
		/* window isn't in current desktop */
		find_window_in_desktops(&d, &n, win, &another_desktop);
		c = (n) ? n->client : NULL;
		if (c == NULL) {
			_LOG_(ERROR, "cannot find client with window %d", win);
			return -1;
		}
	}

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

	delete_node(n, d);
	ewmh_update_client_list();

	if (is_tree_empty(d->tree)) {
		set_active_window_name(XCB_NONE);
	}

	if (!another_desktop) {
		if (render_tree(d->tree) != 0) {
			_LOG_(ERROR, "cannot render tree");
			return -1;
		}
	}

	return 0;
}

int
set_visibility(xcb_window_t win, bool is_visible)
{
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

	ret = is_visible ? show_window(win) : hide_window(win);
	if (ret == -1) {
		_LOG_(
			ERROR, "cannot set visibilty to %s", is_visible ? "true" : "false");
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
	return 0;
}

static int
show_window(xcb_window_t win)
{
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
	return 0;
}

static int
hide_window(xcb_window_t win)
{
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
		}
	}
}

int
set_focus(node_t *n, bool flag)
{
	n->is_focused = flag;
	if (win_focus(n->client->window, flag) != 0) {
		_LOG_(ERROR, "cannot set focus");
		return -1;
	}

	if (flag)
		raise_window(n->client->window);

	return 0;
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
	node_t *tree = curr_monitor->desktops[arg->idx]->tree;
	return render_tree(tree);
}

static int
switch_desktop(const int nd)
{
	if (nd > conf.virtual_desktops) {
		return 0;
	}

	int current = get_focused_desktop_idx();
	if (current == -1)
		return current;
	if (nd == current)
		return 0;

	update_focused_desktop(nd);

	if (show_windows(curr_monitor->desktops[nd]->tree) != 0) {
		return -1;
	}

	if (hide_windows(curr_monitor->desktops[current]->tree) != 0) {
		return -1;
	}

	set_active_window_name(XCB_NONE);
	win_focus(focused_win, false);
	focused_win = XCB_NONE;

#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", nd + 1);
	log_tree_nodes(curr_monitor->desktops[nd]->tree);
	_LOG_(INFO, "old desktop %d nodes--------------", current + 1);
	log_tree_nodes(curr_monitor->desktops[current]->tree);
#endif

	if (ewmh_update_current_desktop(wm->ewmh, wm->screen_nbr, nd) != 0) {
		return -1;
	}

	xcb_flush(wm->connection);

	return 0;
}

void
fill_root_rectangle(rectangle_t *r)
{
	const uint16_t w = curr_monitor->rectangle.width;
	const uint16_t h = curr_monitor->rectangle.height;
	const uint16_t x = curr_monitor->rectangle.x;
	const uint16_t y = curr_monitor->rectangle.y;
	if (wm->bar && curr_monitor == prim_monitor) {
		(*r).x		= x + conf.window_gap;
		(*r).y		= y + wm->bar->rectangle.height + conf.window_gap;
		(*r).width	= w - 2 * conf.window_gap - 2 * conf.border_width;
		(*r).height = h - wm->bar->rectangle.height - 2 * conf.window_gap -
					  2 * conf.border_width;
	} else {
		(*r).x		= x + conf.window_gap;
		(*r).y		= y + conf.window_gap;
		(*r).width	= w - 2 * conf.window_gap - 2 * conf.border_width;
		(*r).height = h - 2 * conf.window_gap - 2 * conf.border_width;
	}
}

static void
fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r)
{
	int x = curr_monitor->rectangle.x + (curr_monitor->rectangle.width / 2) -
			(geometry->width / 2);
	int y = curr_monitor->rectangle.y + (curr_monitor->rectangle.height / 2) -
			(geometry->height / 2);
	(*r).x		= x;
	(*r).y		= y;
	(*r).width	= geometry->width;
	(*r).height = geometry->height;
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
	return render_tree(curr_monitor->desktops[next]->tree);
}

static int
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

static int
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

static bool
should_manage(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_window_attributes_cookie_t attr_cookie;
	xcb_get_window_attributes_reply_t *attr_reply;

	attr_cookie = xcb_get_window_attributes(conn, win);
	attr_reply	= xcb_get_window_attributes_reply(conn, attr_cookie, NULL);

	if (attr_reply == NULL) {
		return true;
	}

	bool manage = !attr_reply->override_redirect;
	_FREE_(attr_reply);
	return manage;
}

static int
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

static int
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

static ewmh_window_type_t
determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom)
{
	if (atom == ewmh->_NET_WM_WINDOW_TYPE_NORMAL) {
		/* WINDOW_TYPE_NORMAL indicates a normal, top-level window.
		 * This is the default window type for standard application windows.*/
		return WINDOW_TYPE_NORMAL;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
		/* WINDOW_TYPE_DOCK indicates a dock or panel feature.
		 * Typically, a Window Manager would keep such windows on top of all
		 * other windows. Examples include system trays, taskbars, or desktop
		 * panels. */
		return WINDOW_TYPE_DOCK;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR ||
			   atom == ewmh->_NET_WM_WINDOW_TYPE_MENU) {
		/* WINDOW_TYPE_TOOLBAR_MENU represents toolbar and pinnable menu
		 * windows. These are toolbars and menus "torn off" from the main
		 * application. Windows of this type may set the WM_TRANSIENT_FOR hint
		 * indicating the main application window.*/
		return WINDOW_TYPE_TOOLBAR_MENU;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
		/* WINDOW_TYPE_UTILITY indicates a small persistent utility window.
		 * Examples include palettes or toolboxes that:
		 * - Are distinct from toolbars (not torn off from main app)
		 * - Differ from dialogs (not transient)
		 * - Users typically keep open while working
		 * May set WM_TRANSIENT_FOR hint to main application window.*/
		return WINDOW_TYPE_UTILITY;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
		/* WINDOW_TYPE_SPLASH represents a splash screen
		 * displayed as an application is starting up.
		 * Typically shown briefly during application initialization. */
		return WINDOW_TYPE_SPLASH;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DIALOG) {
		/* WINDOW_TYPE_DIALOG indicates a dialog window.
		 * If NETWM_WINDOW_TYPE is not set, windows with
		 * WM_TRANSIENT_FOR hint MUST be treated as this type.
		 * Temporary windows for user interaction, input, or notifications. */
		return WINDOW_TYPE_DIALOG;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION) {
		/* WINDOW_TYPE_NOTIFICATION represents a notification window.
		 * Examples include informative bubbles like:
		 * "Your laptop is running out of power"
		 * Typically used on override-redirect windows. */
		return WINDOW_TYPE_NOTIFICATION;
	}
	return WINDOW_TYPE_NORMAL;
}

static ewmh_window_type_t
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

static bool
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

static bool
window_exists(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_tree_cookie_t c		   = xcb_query_tree(conn, win);
	xcb_query_tree_reply_t *tree_reply = xcb_query_tree_reply(conn, c, NULL);

	if (tree_reply == NULL) {
		return false;
	}

	_FREE_(tree_reply);
	return true;
}

static bool
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

static int
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
	set_focus(d->tree, true);

	ewmh_update_client_list();
	return tile(d->tree);
}

static int
handle_subsequent_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling tiled window %s id %d", name, client->window);
	_FREE_(name);
#endif

	xcb_window_t wi = get_window_under_cursor(wm->connection, wm->root_window);
	node_t		*n	= NULL;

	if (wm->bar && wi == wm->bar->window) {
		n = find_any_leaf(d->tree);
	} else {
		n = get_focused_node(d->tree);
		if (n == NULL || n->client == NULL) {
			_LOG_(ERROR, "cannot find focused node");
			return 0;
		}
	}

	if (IS_FLOATING(n->client) && !IS_ROOT(n)) {
		_LOG_(INFO, "node under cursor is floating %d", wi);
		n = find_any_leaf(d->tree);
		if (n == NULL) {
			_LOG_(ERROR, "ret here");
			return 0;
		}
	}

	if (IS_FULLSCREEN(n->client)) {
		set_fullscreen(n, false);
	}

	if (n == NULL || n->client == NULL) {
		_LOG_(ERROR, "cannot find node with window id %d", wi);
		return -1;
	}

	if (client == NULL) {
		_LOG_(ERROR, "client is null");
		return -1;
	}

	node_t *new_node = create_node(client);
	if (new_node == NULL) {
		_LOG_(ERROR, "new node is null");
		return -1;
	}

	insert_node(n, new_node, d->layout);
	d->n_count += 1;
	if (d->layout == STACK) {
		set_focus(new_node, true);
	}

	ewmh_update_client_list();
	return render_tree(d->tree);
}

static int
handle_floating_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling floating window %s id %d", name, client->window);
	_FREE_(name);
#endif

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
		set_focus(d->tree, true);
		return tile(d->tree);
	} else {
		xcb_window_t wi =
			get_window_under_cursor(wm->connection, wm->root_window);
		if (wi == wm->root_window || wi == 0) {
			_FREE_(client);
			return 0;
		}
		node_t *n = find_node_by_window_id(d->tree, wi);
		n		  = n == NULL ? find_any_leaf(d->tree) : n;
		if (n == NULL || n->client == NULL) {
			_FREE_(client);
			_LOG_(ERROR, "cannot find node with window id %d", wi);
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
		d->n_count += 1;
		ewmh_update_client_list();
		return render_tree(d->tree);
	}
}

static int
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
	if (is_tree_empty(d->tree)) {
		rectangle_t	   r = {0};
		const uint16_t w = curr_monitor->rectangle.width;
		const uint16_t h = curr_monitor->rectangle.height;
		const uint16_t x = curr_monitor->rectangle.x;
		const uint16_t y = curr_monitor->rectangle.y;
		if (wm->bar && curr_monitor == prim_monitor) {
			r.x		 = x + conf.window_gap;
			r.y		 = y + wm->bar->rectangle.height + conf.window_gap;
			r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
			r.height = h - wm->bar->rectangle.height - 2 * conf.window_gap -
					   2 * conf.border_width;
		} else {
			r.x		 = x + conf.window_gap;
			r.y		 = y + conf.window_gap;
			r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
			r.height = h - 2 * conf.window_gap - 2 * conf.border_width;
		}

		if (client == NULL) {
			_LOG_(ERROR, "client is null");
			return -1;
		}

		d->tree			   = init_root();
		d->tree->client	   = client;
		d->tree->rectangle = r;
		d->n_count += 1;
		ewmh_update_client_list();
	} else {
		node_t *n = NULL;
		n		  = find_any_leaf(d->tree);
		if (n == NULL || n->client == NULL) {
			char *name = win_name(win);
			_LOG_(INFO, "cannot find win  %s:%d", win);
			_FREE_(name);
			return 0;
		}
		if (n->client->state == FLOATING) {
			return 0;
		}
		if (n->client->state == FULLSCREEN) {
			set_fullscreen(n, false);
		}
		node_t *new_node = create_node(client);
		if (new_node == NULL) {
			_LOG_(ERROR, "new node is null");
			return -1;
		}
		if (new_node->client->state == FLOATING) {
			xcb_get_geometry_reply_t *g =
				get_geometry(client->window, wm->connection);
			if (g == NULL) {
				_LOG_(ERROR, "cannot get %d geometry", client->window);
				return -1;
			}
			int x = (curr_monitor->rectangle.width / 2) - (g->width / 2);
			int y = (curr_monitor->rectangle.height / 2) - (g->height / 2);
			rectangle_t rc = {
				.x = x, .y = y, .width = g->width, .height = g->height};
			new_node->rectangle = new_node->floating_rectangle = rc;
			_FREE_(g);
		}
		insert_node(n, new_node, d->layout);
		d->n_count += 1;
		if (d->layout == STACK) {
			set_focus(new_node, true);
		}
		ewmh_update_client_list();
	}
	return 0;
}

static int
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

	if (is_tree_empty(d->tree)) {
		return handle_first_window(client, d);
	}

	return handle_subsequent_window(client, d);
}

static int
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

	return handle_floating_window(client, d);
}

static int
handle_bar_request(xcb_window_t win, desktop_t *d)
{
	if (wm->bar) {
		return 0;
	}

	wm->bar = (bar_t *)malloc(sizeof(bar_t));
	if (wm->bar == NULL) {
		return -1;
	}

	wm->bar->window				= win;
	xcb_get_geometry_reply_t *g = get_geometry(win, wm->connection);
	if (g == NULL) {
		_LOG_(ERROR, "cannot get %d geometry", wm->bar->window);
		return -1;
	}

	wm->bar->rectangle = (rectangle_t){
		.height = g->height, .width = g->width, .x = g->x, .y = g->y};
	_FREE_(g);

	arrange_trees();
	if (display_client(wm->bar->rectangle, wm->bar->window) != 0) {
		return -1;
	}

	return render_tree(d->tree);
}

static int
handle_map_request(const xcb_event_t *event)
{
	xcb_map_request_event_t *ev	 = (xcb_map_request_event_t *)event;
	xcb_window_t			 win = ev->window;

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

	int idx = get_focused_desktop_idx();
	if (idx == -1) {
		_LOG_(ERROR, "cannot get focused desktop idx");
		return idx;
	}

	/* check if the window already exists in the tree to avoid duplication */
	if (find_node_by_window_id(curr_monitor->desktops[idx]->tree, win) !=
		NULL) {
		return 0;
	}

	desktop_t *d	= curr_monitor->desktops[idx];
	rule_t	  *rule = get_window_rule(win);

	if (rule) {
		if (rule->desktop_id != -1) {
			return insert_into_desktop(
				rule->desktop_id, win, rule->state == TILED);
		}
		if (rule->state == FLOATING) {
			return handle_floating_window_request(win, d);
		} else if (rule->state == TILED) {
			return handle_tiled_window_request(win, d);
		}
	}

	ewmh_window_type_t wint = window_type(win);
	if ((apply_floating_hints(win) != -1 && wint != WINDOW_TYPE_DOCK)) {
		return handle_floating_window_request(win, d);
	}
	if (wint == WINDOW_TYPE_NOTIFICATION) {
		map_floating(win);
		return 0;
	}

	switch (wint) {
	case WINDOW_TYPE_UNKNOWN:
	case WINDOW_TYPE_NORMAL: return handle_tiled_window_request(win, d);
	case WINDOW_TYPE_DOCK: return handle_bar_request(win, d);
	case WINDOW_TYPE_TOOLBAR_MENU:
	case WINDOW_TYPE_UTILITY:
	case WINDOW_TYPE_SPLASH:
	case WINDOW_TYPE_DIALOG: return handle_floating_window_request(win, d);
	default: return 0;
	}
}

static int
handle_enter_notify(const xcb_event_t *event)
{
	xcb_enter_notify_event_t *ev  = (xcb_enter_notify_event_t *)event;
	xcb_window_t			  win = ev->event;

	if (multi_monitors) {
		monitor_t *mm = get_focused_monitor();
		if (mm && mm != curr_monitor) {
			curr_monitor = mm;
		}
	}
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved enter notify for %d, name %s ", win, name);
	_FREE_(name);
#endif
	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (wm->bar && win == wm->bar->window) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	const int curd = get_focused_desktop_idx();
	if (curd == -1)
		return curd;

	node_t *root = curr_monitor->desktops[curd]->tree;
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
		if (curr_monitor->desktops[curd]->layout == STACK) {
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
	update_focus(root, n);

	if (has_floating_window(root)) {
		restack();
	}

	xcb_flush(wm->connection);
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

	if (wm->bar && win == wm->bar->window) {
		return 0;
	}

	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	const int curd = get_focused_desktop_idx();
	if (curd == -1)
		return -1;
	if (curr_monitor->desktops[curd]->layout == STACK) {
		return 0;
	}

	node_t		*root		   = curr_monitor->desktops[curd]->tree;
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

	return 0;
}

static int
handle_key_press(const xcb_event_t *event)
{
	xcb_key_press_event_t *ev			 = (xcb_key_press_event_t *)event;
	uint16_t			   cleaned_state = (ev->state & ~(XCB_MOD_MASK_LOCK));
	xcb_keysym_t		   k = get_keysym(ev->detail, wm->connection);

	if (key_head) {
		conf_key_t *current = key_head;
		while (current) {
			if (cleaned_state == (current->mod & ~(XCB_MOD_MASK_LOCK))) {
				if (current->keysym == k) {
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

	size_t n = sizeof(_keys_) / sizeof(_keys_[0]);
	for (size_t i = n; i--;) {
		if (cleaned_state == (_keys_[i].mod & ~(XCB_MOD_MASK_LOCK))) {
			if (_keys_[i].keysym == k) {
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
handle_state(node_t		 *n,
			 xcb_atom_t	  state,
			 xcb_atom_t	  state_,
			 unsigned int action)
{
	if (n == NULL)
		return -1;

	char		*name = win_name(n->client->window);
	xcb_window_t w	  = n->client->window;

	if (state == wm->ewmh->_NET_WM_STATE_FULLSCREEN ||
		state_ == wm->ewmh->_NET_WM_STATE_FULLSCREEN) {
		_LOG_(INFO, "STATE_FULLSCREEN received for win %d:%s", w, name);
		if (action == XCB_EWMH_WM_STATE_ADD) {
			_FREE_(name);
			return set_fullscreen(n, true);
		} else if (action == XCB_EWMH_WM_STATE_REMOVE) {
			/* if (n->client->state == FULLSCREEN) { */
			_FREE_(name);
			return set_fullscreen(n, false);
			/* } */
		} else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
			uint32_t mode = (n->client->state == FULLSCREEN)
								? XCB_EWMH_WM_STATE_REMOVE
								: XCB_EWMH_WM_STATE_ADD;
			_FREE_(name);
			return set_fullscreen(n, mode == XCB_EWMH_WM_STATE_ADD);
		}
	} else if (state == wm->ewmh->_NET_WM_STATE_BELOW) {
		_LOG_(INFO, "STATE_BELOW received for win %d:%s", w, name);
		if (curr_monitor->desktops[get_focused_desktop_idx()]->layout !=
			STACK) {
			lower_window(w);
		}
	} else if (state == wm->ewmh->_NET_WM_STATE_ABOVE) {
		_LOG_(INFO, "STATE_ABOVE received for win %d:%s", w, name);
		if (curr_monitor->desktops[get_focused_desktop_idx()]->layout !=
			STACK) {
			raise_window(w);
		}
	} else if (state == wm->ewmh->_NET_WM_STATE_HIDDEN) {
		_LOG_(INFO, "STATE_HIDDEN received for win %d:%s", w, name);
	} else if (state == wm->ewmh->_NET_WM_STATE_STICKY) {
		_LOG_(INFO, "STATE_STICKY received for win %d:%s", w, name);
	} else if (state == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {
		_LOG_(INFO, "STATE_DEMANDS_ATTENTION received for win %d:%s", w, name);
	}
	_FREE_(name);
	return 0;
}

static int
handle_client_message(const xcb_event_t *event)
{
	xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;
#ifdef _DEBUG__
	char *name = win_name(ev->window);
	_LOG_(DEBUG, "recieved client message for %d, name %s ", ev->window, name);
	_FREE_(name);
#endif
	if (ev->format != 32) {
		return 0;
	}

	int d = get_focused_desktop_idx();
	if (d == -1)
		return d;

	node_t *root = curr_monitor->desktops[d]->tree;
	/* reciever fonctions will perform the null check for n */
	node_t *n	 = find_node_by_window_id(root, ev->window);
#ifdef _DEBUG__
	_LOG_(DEBUG, "received data32 for win %d:", ev->window);
	for (ulong i = 0; i < LEN(ev->data.data32); i++) {
		_LOG_(DEBUG, "data32[%d]: %u", i, ev->data.data32[i]);
	}
#endif
	char *s = win_name(ev->window);
	if (ev->type == wm->ewmh->_NET_CURRENT_DESKTOP) {
		uint32_t nd = ev->data.data32[0];
		_LOG_(INFO, "recieved desktop change to %d", nd);
		if (nd > wm->ewmh->_NET_NUMBER_OF_DESKTOPS - 1) {
			return -1;
		}
		if (switch_desktop(nd) != 0) {
			_FREE_(s);
			return -1;
		}
	} else if (ev->type == wm->ewmh->_NET_WM_STATE) {
		_LOG_(INFO, "NET_WM_STATE for %d name %s", ev->window, s);
		handle_state(
			n, ev->data.data32[1], ev->data.data32[2], ev->data.data32[0]);
	} else if (ev->type == wm->ewmh->_NET_ACTIVE_WINDOW) {
		_LOG_(INFO, "_NET_ACTIVE_WINDOW for %d name %s", ev->window, s);
		int di = find_desktop_by_window(ev->window);
		if (di == -1)
			goto out;

		if (switch_desktop(di) != 0) {
			_FREE_(s);
			return -1;
		}
	} else if (ev->type == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {
		_LOG_(INFO, "WM_STATE_DEMANDS_ATTENTION for %d name %s", ev->window, s);
	} else if (ev->type == wm->ewmh->_NET_WM_STATE_STICKY) {
		_LOG_(INFO, "NET_WM_STATE_STICKY for %d name %s", ev->window, s);
	} else if (ev->type == wm->ewmh->_NET_WM_DESKTOP) {
		_LOG_(INFO, "NET_WM_DESKTOP for %d name %s", ev->window, s);
	} else if (ev->type == wm->ewmh->_NET_CLOSE_WINDOW) {
		_LOG_(INFO, "NET_CLOSE_WINDOW for %d name %s", ev->window, s);
		close_or_kill(ev->window);
	}
out:
	_FREE_(s);
	return 0;
}

static int
handle_unmap_notify(const xcb_event_t *event)
{
	xcb_unmap_notify_event_t *ev  = (xcb_unmap_notify_event_t *)event;
	xcb_window_t			  win = ev->window;
	int						  idx = get_focused_desktop_idx();
	if (idx == -1)
		return -1;

#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved unmap notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	node_t *root = curr_monitor->desktops[idx]->tree;
	if (root == NULL)
		return 0;

	if (wm->bar && wm->bar->window == win) {
		hide_bar(win);
		render_tree(root);
		return 0;
	}

	if (!client_exist(root, win) && !client_exist_in_desktops(win)) {
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
	const int d = get_focused_desktop_idx();
	if (d == -1) {
		return d;
	}

	node_t *n		   = curr_monitor->desktops[d]->tree;
	bool	is_managed = client_exist(n, win);
	if (!is_managed) {
		uint16_t mask = 0;
		uint32_t values[7];
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
		const node_t *node = find_node_by_window_id(n, ev->window);
		if (node == NULL) {
			_LOG_(ERROR,
				  "config request -> cannot find node with win id %d",
				  ev->window);
			return 0;
		}
		/* TODO: deal with *node */
	}
	return 0;
}

static int
handle_destroy_notify(const xcb_event_t *event)
{
	xcb_destroy_notify_event_t *ev	= (xcb_destroy_notify_event_t *)event;
	xcb_window_t				win = ev->window;
	int							idx = get_focused_desktop_idx();
	if (idx == -1)
		return -1;

#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved destroy notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	node_t *root = curr_monitor->desktops[idx]->tree;
	if (root == NULL)
		return 0;

	if (wm->bar && wm->bar->window == win) {
		hide_bar(win);
		render_tree(root);
		return 0;
	}

	if (!client_exist(root, win) && !client_exist_in_desktops(win)) {
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

	return 0;
}

static void
update_grabbed_window(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client;
	if (flag && root != n) {
		set_focus(root, false);
		window_grab_buttons(root->client->window);
	}

	update_grabbed_window(root->first_child, n);
	update_grabbed_window(root->second_child, n);
}

static int
handle_button_press_event(const xcb_event_t *event)
{
	if (conf.focus_follow_pointer) {
		return 0;
	}
	xcb_button_press_event_t *ev = (xcb_button_press_event_t *)event;
#ifdef _DEBUG__
	char *name = win_name(ev->event);
	_LOG_(DEBUG,
		  "RCIEVED BUTTON PRESS EVENT window %d, window name %s",
		  ev->event,
		  name);
	_FREE_(name);
#endif
	xcb_window_t win = ev->event;

	if (wm->bar && win == wm->bar->window) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	const int curd = get_focused_desktop_idx();
	if (curd == -1)
		return -1;

	node_t	 *root	 = curr_monitor->desktops[curd]->tree;
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n && n->client) ? n->client : NULL;

	if (client == NULL) {
		return -1;
	}

	if (win == wm->root_window) {
		return 0;
	}

	// window_ungrab_buttons(client->window);

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
		if (curr_monitor->desktops[curd]->layout == STACK) {
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
	update_focus(root, n);

	if (has_floating_window(root)) {
		restack();
	}

	xcb_allow_events(wm->connection, XCB_ALLOW_SYNC_POINTER, ev->time);
	/* set_cursor(CURSOR_POINTER); */
	xcb_allow_events(wm->connection, XCB_ALLOW_REPLAY_POINTER, ev->time);
	xcb_flush(wm->connection);
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
	/* skip events where the pointer was over a child window, we are only
	 * interested in events on the root window. */
	if (ev->child != XCB_NONE) {
		return 0;
	}
	int16_t	   rx = ev->root_x;
	int16_t	   ry = ev->root_y;
	/* find out if this crosses monitor boundary */
	monitor_t *m  = get_monitor_within_coordinate(rx, ry);
	if (!m) {
		return 0;
	}
	if (curr_monitor && curr_monitor != m) {
		curr_monitor = m;
	}
	return 0;
}

static void
log_children(xcb_conn_t *conn, xcb_window_t root_window)
{
	xcb_query_tree_cookie_t tree_cookie = xcb_query_tree(conn, root_window);
	xcb_query_tree_reply_t *tree_reply =
		xcb_query_tree_reply(conn, tree_cookie, NULL);
	if (tree_reply == NULL) {
		_LOG_(ERROR, "failed to query tree reply");
		return;
	}

	_LOG_(DEBUG, "children of root window:");
	xcb_window_t *children	   = xcb_query_tree_children(tree_reply);
	const int	  num_children = xcb_query_tree_children_length(tree_reply);
	for (int i = 0; i < num_children; ++i) {
		xcb_icccm_get_text_property_reply_t t_reply;
		xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(conn, children[i]);
		uint8_t wr = xcb_icccm_get_wm_name_reply(conn, cn, &t_reply, NULL);
		if (wr == 1) {
			_LOG_(DEBUG, "child %d: %s", i + 1, t_reply.name);
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		} else {
			_LOG_(DEBUG, "failed to get window name for child %d", i + 1);
		}
	}

	_FREE_(tree_reply);
}

static void
parse_args(int argc, char **argv)
{
	char *c = NULL;
	if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-run") == 0) {
		if (argc >= 2) {
			c = argv[2];
		} else {
			_LOG_(ERROR, "missing argument after -r/--run");
		}
	}
	exec_process(&((arg_t){.argc = 1, .cmd = (char *[]){c}}));
}

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

	if (using_xrandr &&
		event_type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		_LOG_(INFO, "monitor update was requested");
		handle_monitor_changes();
		return 0;
	}

	size_t n = sizeof(_handlers_) / sizeof(_handlers_[0]);
	for (size_t i = 0; i < n; i++) {
		if (_handlers_[i].type == event_type) {
			return _handlers_[i].handle(event);
		}
	}

	return 0;
}

static void
event_loop(wm_t *w)
{
	xcb_event_t *event;
	while ((event = xcb_wait_for_event(w->connection))) {
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

static void
cleanup(int sig)
{
	xcb_disconnect(wm->connection);
	xcb_ewmh_connection_wipe(wm->ewmh);
	free_keys();
	free_rules();
	free_monitors(); /* frees desktops and trees as well */
	_FREE_(wm);
	_LOG_(INFO, "ZWM exits with signal number %d", sig);
	/* uncommenting the following line *exit(sig)* prevents the os
	 * from generating a core dump file when zwm crashes */
	/* exit(sig); */
}

int
main(int argc, char **argv)
{

	/* if loading the config file went sideways, we use the default values,
	 * and default keys */
	if (load_config(&conf) != 0) {
		_LOG_(ERROR, "error while loading config -> using default macros");
		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
	}

	wm = init_wm();
	if (wm == 0x00) {
		_LOG_(ERROR, "failed to initialize window manager");
		exit(EXIT_FAILURE);
	}

	if (!setup_wm()) {
		_LOG_(ERROR, "failed to setup window manager");
		exit(EXIT_FAILURE);
	}

	if (argc >= 2) {
		parse_args(argc, argv);
	}

	/* do not wait for mapping event. Grab the keys as soon as zwm starts */
	if (grab_keys(wm->connection, wm->root_window) != 0) {
		_LOG_(ERROR, "cannot grab keys");
	}

	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGSEGV, cleanup);
	signal(SIGABRT, cleanup);

	event_loop(wm);
	cleanup(0);

	return 0;
}
