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

#include "zwm.h"
#include "config_parser.h"
#include "helper.h"
#include "tree.h"
#include "type.h"
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#define XCB_MOVE_RESIZE                                                   \
	(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |                          \
	 XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define XCB_MOVE   (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)
#define XCB_RESIZE (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define SUBSTRUCTURE_REDIRECTION                                          \
	(XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |                                 \
	 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)
#define CLIENT_EVENT_MASK                                                 \
	(XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE |       \
	 XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW)
#define ROOT_EVENT_MASK                                                   \
	(SUBSTRUCTURE_REDIRECTION | XCB_EVENT_MASK_BUTTON_PRESS |             \
	 XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW)

#define NUMBER_OF_DESKTOPS 5
#define WM_NAME			   "zwm"
#define WM_CLASS_NAME	   "null"
#define WM_INSTANCE_NAME   "null"
#define ALT_MASK		   XCB_MOD_MASK_1
#define SUPER_MASK		   XCB_MOD_MASK_4
#define SHIFT_MASK		   XCB_MOD_MASK_SHIFT
#define CTRL_MASK		   XCB_MOD_MASK_CONTROL
#define CLICK_TO_FOCUS	   XCB_BUTTON_INDEX_1

wm_t				 *wm		  = NULL;
config_t			  conf		  = {0};
xcb_window_t		  focused_win = XCB_NONE;
uint16_t			  num_lock;
uint16_t			  caps_lock;
uint16_t			  scroll_lock;
int8_t				  click_to_focus;

// clang-format off
// X11/keysymdef.h
static const _key__t keys_[] = {
	{SUPER_MASK,              XK_w,      close_or_kill_wrapper,      NULL                            },
	{SUPER_MASK,              XK_Return,
    exec_process,             &((arg_t){.argc = 1, .cmd = (char *[]){("alacritty")}})          	     },
	{SUPER_MASK,              XK_space,
    exec_process,             &((arg_t){.argc = 1, .cmd = (char *[]){("dmenu_run")}})                },
	{SUPER_MASK,              XK_p,
    exec_process,             &((arg_t){.argc = 3, .cmd = (char *[]){"rofi", "-show", "drun"}})},
	{SUPER_MASK,              XK_1,      switch_desktop_wrapper,    &((arg_t){.idx = 0})             },
	{SUPER_MASK,              XK_2,      switch_desktop_wrapper,    &((arg_t){.idx = 1})             },
	{SUPER_MASK,              XK_3,      switch_desktop_wrapper,    &((arg_t){.idx = 2})             },
	{SUPER_MASK,              XK_4,      switch_desktop_wrapper,    &((arg_t){.idx = 3})             },
	{SUPER_MASK,              XK_5,      switch_desktop_wrapper,    &((arg_t){.idx = 4})             },
	{SUPER_MASK,              XK_6,      switch_desktop_wrapper,    &((arg_t){.idx = 5})             },
	{SUPER_MASK,              XK_7,      switch_desktop_wrapper,    &((arg_t){.idx = 6})             },
	{SUPER_MASK,              XK_Left,   cycle_win_wrapper,         &((arg_t){.d = LEFT})            },
	{SUPER_MASK,              XK_Right,  cycle_win_wrapper,         &((arg_t){.d = RIGHT})           },
	{SUPER_MASK,              XK_Up,     cycle_win_wrapper,         &((arg_t){.d = UP})              },
	{SUPER_MASK,              XK_Down,   cycle_win_wrapper,         &((arg_t){.d = DOWN})            },
	{SUPER_MASK,              XK_l,      horizontal_resize_wrapper, &((arg_t){.r = GROW})            },
	{SUPER_MASK,              XK_h,      horizontal_resize_wrapper, &((arg_t){.r = SHRINK})          },
	{SUPER_MASK,              XK_f,      set_fullscreen_wrapper,    NULL                             },
	{SUPER_MASK,              XK_s,      swap_node_wrapper,         NULL                             },
	{SUPER_MASK | SHIFT_MASK, XK_1,      transfer_node_wrapper,     &((arg_t){.idx = 0})             },
	{SUPER_MASK | SHIFT_MASK, XK_2,      transfer_node_wrapper,     &((arg_t){.idx = 1})             },
	{SUPER_MASK | SHIFT_MASK, XK_3,      transfer_node_wrapper,     &((arg_t){.idx = 2})             },
	{SUPER_MASK | SHIFT_MASK, XK_4,      transfer_node_wrapper,     &((arg_t){.idx = 3})             },
	{SUPER_MASK | SHIFT_MASK, XK_5,      transfer_node_wrapper,     &((arg_t){.idx = 4})             },
	{SUPER_MASK | SHIFT_MASK, XK_6,      transfer_node_wrapper,     &((arg_t){.idx = 5})             },
	{SUPER_MASK | SHIFT_MASK, XK_7,      transfer_node_wrapper,     &((arg_t){.idx = 6})             },
	{SUPER_MASK | SHIFT_MASK, XK_m,      layout_handler,            &((arg_t){.t = MASTER})          },
    {SUPER_MASK | SHIFT_MASK, XK_d,      layout_handler,            &((arg_t){.t = DEFAULT})         },
    {SUPER_MASK | SHIFT_MASK, XK_s,      layout_handler,            &((arg_t){.t = STACK})           },
	{SUPER_MASK | SHIFT_MASK, XK_k,      traverse_stack_wrapper,    &((arg_t){.d = UP})              },
    {SUPER_MASK | SHIFT_MASK, XK_j,      traverse_stack_wrapper,    &((arg_t){.d = DOWN})            },
    {SUPER_MASK | SHIFT_MASK, XK_f,      flip_node_wrapper,   		NULL            				 },
    {SUPER_MASK | SHIFT_MASK, XK_r,      reload_config_wrapper,   	NULL            				 },
	{SUPER_MASK | SHIFT_MASK, XK_Left, 	 cycle_desktop_wrapper, 	&((arg_t){.d = LEFT}) 			 },
	{SUPER_MASK | SHIFT_MASK, XK_Right,  cycle_desktop_wrapper, 	&((arg_t){.d = RIGHT}) 			 }
};
// clang-format on

static const uint32_t buttons_[] = {
	XCB_BUTTON_INDEX_1, XCB_BUTTON_INDEX_2, XCB_BUTTON_INDEX_3};

int
len_of_digits(long n)
{
	int count = 0;
	do {
		n /= 10;
		++count;
	} while (n != 0);
	return count;
}

int
check_xcb_error(xcb_conn_t		 *conn,
				xcb_void_cookie_t cookie,
				const char		 *err)
{
	xcb_generic_error_t *error = xcb_request_check(conn, cookie);
	if (error != NULL) {
		_LOG_(ERROR, "%s: error code %d\n", err, error->error_code);
		free(error);
		return -1;
	}
	return 0;
}

// caller must free
char *
win_name(xcb_window_t win)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		char *str = (char *)malloc(t_reply.name_len * sizeof(char));
		if (str == NULL)
			return NULL;
		strncpy(str, t_reply.name, t_reply.name_len);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
		return str;
	}
	return NULL;
}

int
layout_handler(arg_t *arg)
{
	int		   i = get_focused_desktop_idx();
	desktop_t *d = wm->desktops[i];
	if (arg->t == STACK && d->n_count < 2)
		return 0;
	apply_layout(d, arg->t);
	return render_tree(d->tree);
}

xcb_ewmh_connection_t *
ewmh_init(xcb_conn_t *conn)
{
	if (conn == 0x00) {
		_LOG_(ERROR, "Connection is NULL");
		return NULL;
	}

	xcb_ewmh_connection_t *ewmh = calloc(1, sizeof(xcb_ewmh_connection_t));
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

int
ewmh_set_supporting(xcb_window_t win, xcb_ewmh_connection_t *ewmh)
{
	pid_t			  wm_pid = getpid();
	xcb_void_cookie_t supporting_cookie =
		xcb_ewmh_set_supporting_wm_check_checked(ewmh, win, win);
	xcb_void_cookie_t name_cookie =
		xcb_ewmh_set_wm_name_checked(ewmh, win, strlen(WM_NAME), WM_NAME);
	xcb_void_cookie_t pid_cookie =
		xcb_ewmh_set_wm_pid_checked(ewmh, win, (uint32_t)wm_pid);

	xcb_generic_error_t *err;
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie))) {
		_LOG_(ERROR,
			  "Error setting supporting window: %d\n",
			  err->error_code);
		free(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, name_cookie))) {
		fprintf(stderr, "Error setting WM name: %d\n", err->error_code);
		free(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, pid_cookie))) {
		fprintf(stderr, "Error setting WM PID: %d\n", err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
get_focused_desktop_idx()
{
	for (int i = wm->n_of_desktops; i--;) {
		if (wm->desktops[i]->is_focused) {
			return wm->desktops[i]->id;
		}
	}
	return -1;
}

desktop_t *
get_focused_desktop()
{
	for (int i = 0; i < wm->n_of_desktops; ++i) {
		if (wm->desktops[i]->is_focused) {
			return wm->desktops[i];
		}
	}
	return NULL;
}

int
ewmh_set_number_of_desktops(xcb_ewmh_connection_t *ewmh,
							int					   screen_nbr,
							uint32_t			   nd)
{
	xcb_void_cookie_t cookie =
		xcb_ewmh_set_number_of_desktops_checked(ewmh, screen_nbr, nd);
	xcb_generic_error_t *err = xcb_request_check(ewmh->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "Error setting number of desktops: %d\n",
			  err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
ewmh_update_desktop_names()
{
	char		 names[MAXLEN];
	uint32_t	 names_len = 0;
	unsigned int offset	   = 0;
	memset(names, 0, sizeof(names));

	for (int i = 0; i < wm->n_of_desktops; i++) {
		desktop_t *d = wm->desktops[i];
		for (int j = 0; d->name[j] != '\0' && (offset + j) < sizeof(names);
			 j++) {
			names[offset + j] = d->name[j];
		}
		offset += strlen(d->name);
		if (offset < sizeof(names)) {
			names[offset++] = '\0';
		}
	}

	names_len			= offset - 1;
	xcb_void_cookie_t c = xcb_ewmh_set_desktop_names_checked(
		wm->ewmh, wm->screen_nbr, names_len, names);
	xcb_generic_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "Error setting names of desktops: %d\n",
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

int16_t
modfield_from_keysym(xcb_keysym_t keysym)
{
	uint16_t	   modfield = 0;
	xcb_keycode_t *keycodes = NULL, *mod_keycodes = NULL;
	xcb_get_modifier_mapping_reply_t *reply = NULL;
	xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(wm->connection);
	if (wm->root_window) {
	}
	if ((keycodes = xcb_key_symbols_get_keycode(symbols, keysym)) ==
			NULL ||
		(reply = xcb_get_modifier_mapping_reply(
			 wm->connection,
			 xcb_get_modifier_mapping(wm->connection),
			 NULL)) == NULL ||
		reply->keycodes_per_modifier < 1 ||
		(mod_keycodes = xcb_get_modifier_mapping_keycodes(reply)) ==
			NULL) {
		goto end;
	}

	unsigned int num_mod =
		xcb_get_modifier_mapping_keycodes_length(reply) /
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
	free(keycodes);
	free(reply);
	return modfield;
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
		free(reply);
	xcb_ungrab_server(con);
}

// Stack win1 above win2
void
window_above(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask =
		XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t		  values[] = {win2, XCB_STACK_MODE_ABOVE};
	xcb_void_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		free(err);
	}
}

// Stack win1 below win2
void
window_below(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask =
		XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t		  values[] = {win2, XCB_STACK_MODE_BELOW};
	xcb_void_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		free(err);
	}
}

void
lower_window(xcb_window_t win)
{
	uint32_t		  values[] = {XCB_STACK_MODE_BELOW};
	uint16_t		  mask	   = XCB_CONFIG_WINDOW_STACK_MODE;

	xcb_void_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in stacking window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
	}
}

void
raise_window(xcb_window_t win)
{
	uint32_t		  values[] = {XCB_STACK_MODE_ABOVE};
	uint16_t		  mask	   = XCB_CONFIG_WINDOW_STACK_MODE;

	xcb_void_cookie_t cookie =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in stacking window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
	}
}

int
swap_node_wrapper()
{
	if (wm->desktops[get_focused_desktop_idx()]->layout != DEFAULT)
		return 0;

	node_t *root = wm->desktops[get_focused_desktop_idx()]->tree;
	if (root == NULL)
		return -1;

	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);
	node_t *n = find_node_by_window_id(root, w);
	if (n == NULL)
		return -1;

	if (swap_node(n) != 0)
		return -1;

	return render_tree(root);
}

int
set_fullscreen_wrapper()
{
	const int i = get_focused_desktop_idx();
	if (i == -1)
		return i;

	node_t *root = wm->desktops[i]->tree;
	if (root == NULL)
		return -1;

	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);

	node_t *n = find_node_by_window_id(root, w);
	n->client->state == FULLSCREEN ? set_fullscreen(n, false)
								   : set_fullscreen(n, true);
	return 0;
}

int
set_fullscreen(node_t *n, bool flag)
{
	if (n == NULL)
		return -1;

	rectangle_t r = {0};

	if (flag) {
		long data[]		 = {wm->ewmh->_NET_WM_STATE_FULLSCREEN};
		r.x				 = 0;
		r.y				 = 0;
		r.width			 = wm->screen->width_in_pixels;
		r.height		 = wm->screen->height_in_pixels;
		n->client->state = FULLSCREEN;
		if (change_border_attr(wm->connection,
							   n->client->window,
							   NORMAL_BORDER_COLOR,
							   0,
							   false) != 0) {
			return -1;
		}
		if (resize_window(n->client->window, r.width, r.height) != 0 ||
			move_window(n->client->window, r.x, r.y) != 0) {
			return -1;
		}
		xcb_void_cookie_t c =
			xcb_change_property_checked(wm->connection,
										XCB_PROP_MODE_REPLACE,
										n->client->window,
										wm->ewmh->_NET_WM_STATE,
										XCB_ATOM_ATOM,
										32,
										true,
										data);
		xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR,
				  "Error changing window property: %d\n",
				  err->error_code);
			free(err);
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
	// xcb_void_cookie_t c =
	// 	xcb_delete_property_checked(wm->connection,
	// 								n->client->window,
	// 								wm->ewmh->_NET_WM_STATE_FULLSCREEN);
	// xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
	// if (err) {
	// 	_LOG_(ERROR,
	// 				"Error removing window property: %d\n",
	// 				err->error_code);
	// 	free(err);
	// 	return -1;
	// }
out:
	xcb_flush(wm->connection);
	return 0;
}

int
change_colors(node_t *root)
{
	if (root == NULL)
		return 0;

	if (root->node_type != INTERNAL_NODE && root->client != NULL) {
		if (win_focus(root->client->window, root->is_focused) != 0) {
			return -1;
		}
	}

	if (root->first_child != NULL)
		change_colors(root->first_child);
	if (root->second_child != NULL)
		change_colors(root->second_child);

	return 0;
}

int
reload_config_wrapper()
{
	uint16_t previous_border_width		  = conf.border_width;
	uint16_t previous_window_gap		  = conf.window_gap;
	uint32_t previous_active_border_color = conf.active_border_color;
	uint32_t previous_normal_border_color = conf.normal_border_color;
	int		 previous_virtual_desktops	  = conf.virtual_desktops;

	memset(&conf, 0, sizeof(config_t));

	if (reload_config(&conf) != 0) {
		_LOG_(ERROR,
			  "Error while reloading config -> using default macros");

		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		return 0;
	}

	bool color_changed =
		(previous_normal_border_color != conf.normal_border_color) ||
		(previous_active_border_color != conf.active_border_color);
	bool layout_changed = (conf.window_gap != previous_window_gap) ||
						  (conf.border_width != previous_border_width);

	if (color_changed) {
		for (int i = 0; i < wm->n_of_desktops; ++i) {
			if (!is_tree_empty(wm->desktops[i]->tree)) {
				if (change_colors(wm->desktops[i]->tree) != 0) {
					_LOG_(ERROR,
						  "error while reloading config for desktop %d",
						  wm->desktops[i]->id);
				}
			}
		}
	}

	if (layout_changed) {
		for (int i = 0; i < wm->n_of_desktops; ++i) {
			if (!is_tree_empty(wm->desktops[i]->tree)) {
				arrange_tree(wm->desktops[i]->tree,
							 wm->desktops[i]->layout);
			}
		}
	}

	render_tree(wm->desktops[get_focused_desktop_idx()]->tree);

	if (previous_virtual_desktops != conf.virtual_desktops) {
		_LOG_(INFO, "Reloading desktop changes is not implemented yet");
		// TODO: do the changes required for handling virtual desktops
		// reconfiguration
	}

	xcb_flush(wm->connection);

	return 0;
}

int
flip_node_wrapper()
{
	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	node_t *node = find_node_by_window_id(
		wm->desktops[get_focused_desktop_idx()]->tree, w);
	if (node == NULL)
		return -1;

	flip_node(node);
	return render_tree(wm->desktops[get_focused_desktop_idx()]->tree);
}

int
cycle_win_wrapper(arg_t *arg)
{
	direction_t d	 = arg->d;
	node_t	   *root = wm->desktops[get_focused_desktop_idx()]->tree;
	node_t	   *f	 = get_focused_node(root);

	if (root == NULL) {
		// tree is empty
		return 0;
	}

	if (f == NULL) {
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
	free(s);
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
	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	int i = get_focused_desktop_idx();
	if (i == -1)
		return i;

	node_t *root = wm->desktops[i]->tree;
	node_t *node = find_node_by_window_id(root, w);
	node_t *n	 = NULL;
	if (d == UP) {
		n = next_node(node);
		if (n == NULL) {
			return -1;
		}
	} else if (d == DOWN) {
		n = prev_node(node);
		if (n == NULL) {
			return -1;
		}
	}
	set_focus(n, true);
	wm->desktops[i]->top_w = n->client->window;
	return 0;
}

size_t
get_active_clients_size(desktop_t **d, const int n)
{
	size_t t = 0;
	for (int i = 0; i < n; ++i) {
		t += d[i]->n_count;
	}
	return t;
}

void
populate_client_array(node_t *root, xcb_window_t *arr, size_t *index)
{
	if (root == NULL)
		return;

	if (root->client != NULL && root->client->window != XCB_NONE) {
		arr[*index] = root->client->window;
		(*index)++;
	}

	populate_client_array(root->first_child, arr, index);
	populate_client_array(root->second_child, arr, index);
}

void
ewmh_update_client_list()
{
	size_t size = get_active_clients_size(wm->desktops, wm->n_of_desktops);
	if (size == 0) {
		xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, 0, NULL);
		return;
	}
	// xcb_window_t active_clients[size + 1];
	xcb_window_t *active_clients =
		(xcb_window_t *)malloc((size + 1) * sizeof(xcb_window_t));
	if (active_clients == NULL) {
		return;
	}
	size_t index = 0;
	for (int i = 0; i < wm->n_of_desktops; ++i) {
		node_t *root = wm->desktops[i]->tree;
		populate_client_array(root, active_clients, &index);
	}
	xcb_ewmh_set_client_list(
		wm->ewmh, wm->screen_nbr, size, active_clients);
	free(active_clients);
	active_clients = NULL;
}

int
ewmh_update_current_desktop(xcb_ewmh_connection_t *ewmh,
							int					   screen_nbr,
							uint32_t			   i)
{
	xcb_void_cookie_t c =
		xcb_ewmh_set_current_desktop_checked(ewmh, screen_nbr, i);
	xcb_generic_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "Error setting number of desktops: %d\n",
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

xcb_get_geometry_reply_t *
get_geometry(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(conn, win);
	xcb_generic_error_t		 *err;
	xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(conn, gc, &err);

	if (err) {
		_LOG_(ERROR,
			  "Error getting geometry for window %u: %d\n",
			  win,
			  err->error_code);
		free(err);
		return NULL;
	}

	if (gr == NULL) {
		_LOG_(ERROR, "Failed to get geometry for window %u\n", win);
		return NULL;
	}

	return gr;
}

client_t *
create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn)
{
	client_t *c = (client_t *)malloc(sizeof(client_t));
	if (c == 0x00)
		return NULL;

	// c->id					   = win;
	// c->is_managed			   = false;
	c->window				   = win;
	c->type					   = wtype;
	c->border_width			   = (uint32_t)-1;
	const uint32_t	  mask	   = XCB_CW_EVENT_MASK;
	const uint32_t	  values[] = {CLIENT_EVENT_MASK};
	xcb_void_cookie_t cookie   = xcb_change_window_attributes_checked(
		  conn, c->window, mask, values);
	xcb_generic_error_t *err = xcb_request_check(conn, cookie);

	if (err) {
		_LOG_(ERROR,
			  "Error setting window attributes for client %u: %d\n",
			  c->window,
			  err->error_code);
		free(err);
		exit(EXIT_FAILURE);
	}

	if (change_border_attr(wm->connection,
						   win,
						   conf.normal_border_color,
						   conf.border_width,
						   false) != 0) {
		_LOG_(ERROR, "Failed to change border attr for window %d\n", win);
		free(c);
		c = NULL;
		return NULL;
	}

	return c;
}

void
init_pointer(void)
{
	num_lock	   = modfield_from_keysym(0xff7f);
	caps_lock	   = modfield_from_keysym(0xffe5);
	scroll_lock	   = modfield_from_keysym(0xff14);
	click_to_focus = CLICK_TO_FOCUS;
	if (caps_lock == XCB_NO_SYMBOL) {
		caps_lock = XCB_MOD_MASK_LOCK;
	}
}

desktop_t *
init_desktop()
{
	desktop_t *d = (desktop_t *)malloc(sizeof(desktop_t));
	if (d == 0x00)
		return NULL;

	d->id		  = 0;
	d->is_focused = false;
	d->n_count	  = 0;
	d->tree		  = NULL;
	d->top_w	  = XCB_NONE;
	return d;
}

wm_t *
setup_wm()
{
	int i, default_screen;
	wm = (wm_t *)malloc(sizeof(wm_t));
	if (wm == NULL) {
		_LOG_(ERROR, "Failed to malloc for window manager\n");
		return NULL;
	}

	wm->connection = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(wm->connection) > 0) {
		_LOG_(ERROR, "Error: Unable to open X connection\n");
		free(wm);
		return NULL;
	}

	const xcb_setup_t	 *setup = xcb_get_setup(wm->connection);
	xcb_screen_iterator_t iter	= xcb_setup_roots_iterator(setup);
	for (i = 0; i < default_screen; ++i) {
		xcb_screen_next(&iter);
	}

	wm->screen				   = iter.data;
	wm->root_window			   = iter.data->root;
	wm->screen_nbr			   = default_screen;
	wm->desktops			   = NULL;
	wm->n_of_desktops		   = -1;
	wm->split_type			   = DYNAMIC_TYPE;
	wm->bar					   = NULL;
	const uint32_t	  mask	   = XCB_CW_EVENT_MASK;
	const uint32_t	  values[] = {ROOT_EVENT_MASK};

	xcb_void_cookie_t cookie   = xcb_change_window_attributes_checked(
		  wm->connection, wm->root_window, mask, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "Error registering for substructure redirection "
			  "events on window "
			  "%u: %d\n",
			  wm->root_window,
			  err->error_code);
		free(wm);
		free(err);
		return NULL;
	}

	return wm;
}

bool
setup_desktops(wm_t *w)
{
	desktop_t **desktops =
		(desktop_t **)malloc(sizeof(desktop_t *) * w->n_of_desktops);
	if (desktops == NULL) {
		_LOG_(ERROR, "Failed to malloc desktops\n");
		free(w);
		return false;
	}
	w->desktops = desktops;
	for (int i = 0; i < w->n_of_desktops; i++) {
		desktop_t *d  = init_desktop();
		d->id		  = (uint8_t)i;
		d->is_focused = i == 0 ? true : false;
		d->layout	  = DEFAULT;
		snprintf(d->name, sizeof(d->name), "%d", i + 1);
		w->desktops[i] = d;
	}

	if (ewmh_set_number_of_desktops(
			w->ewmh, w->screen_nbr, (uint32_t)w->n_of_desktops) != 0) {
		return false;
	}

	const int di = get_focused_desktop_idx();
	if (di == -1) {
		return false;
	}

	if (ewmh_update_current_desktop(
			w->ewmh, w->screen_nbr, (uint32_t)di) != 0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	return true;
}

bool
setup_ewmh()
{
	wm->ewmh = ewmh_init(wm->connection);
	if (wm->ewmh == NULL) {
		return false;
	}

	xcb_atom_t		  net_atoms[] = {wm->ewmh->_NET_SUPPORTED,
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

	xcb_void_cookie_t c			  = xcb_ewmh_set_supported_checked(
		  wm->ewmh, wm->screen_nbr, LEN(net_atoms), net_atoms);
	xcb_generic_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "Error setting number of desktops: %d\n",
			  err->error_code);
		free(err);
		return false;
	}

	if (ewmh_set_supporting(wm->root_window, wm->ewmh) != 0) {
		return false;
	}

	return true;
}

wm_t *
init_wm()
{
	wm = setup_wm();
	if (wm == NULL)
		return NULL;

	if (!setup_ewmh())
		return NULL;

	// init_pointer();

	return wm;
}

int
resize_window(xcb_window_t win, uint16_t width, uint16_t height)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t	  values[] = {width, height};

	xcb_void_cookie_t cookie   = xcb_configure_window_checked(
		  wm->connection, win, XCB_RESIZE, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR,
			  "Error resizing window (ID %u): %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
move_window(xcb_window_t win, int16_t x, int16_t y)
{
	if (win == 0 || win == XCB_NONE) {
		return 0;
	}

	const uint32_t	  values[] = {x, y};
	xcb_void_cookie_t cookie   = xcb_configure_window_checked(
		  wm->connection, win, XCB_MOVE, values);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR,
			  "Error moving window (ID %u): %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
fulllscreen_focus(xcb_window_t win)
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

	if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) !=
		0) {
		_LOG_(ERROR, "cannot set input focus");
		return -1;
	}

	raise_window(win);

	xcb_flush(wm->connection);
	return 0;
}

int
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
		if (set_input_focus(
				wm->connection, input, win, XCB_CURRENT_TIME) != 0) {
			_LOG_(ERROR, "cannot set input focus");
			return -1;
		}
	}

	xcb_flush(wm->connection);
	return 0;
}

// TODO: rewrite this
int
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

int
change_window_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		attr,
				   const void  *val)
{
	xcb_void_cookie_t attr_cookie =
		xcb_change_window_attributes_checked(conn, win, attr, val);
	xcb_generic_error_t *err = xcb_request_check(conn, attr_cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Failed to change window attributes: error code %d",
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

int
configure_window(xcb_conn_t	 *conn,
				 xcb_window_t win,
				 uint16_t	  attr,
				 const void	 *val)
{
	xcb_void_cookie_t config_cookie =
		xcb_configure_window_checked(conn, win, attr, val);
	xcb_generic_error_t *err = xcb_request_check(conn, config_cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Failed to configure window : error code %d",
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

int
set_input_focus(xcb_conn_t	   *conn,
				uint8_t			revert_to,
				xcb_window_t	win,
				xcb_timestamp_t time)
{
	xcb_void_cookie_t focus_cookie =
		xcb_set_input_focus_checked(conn, revert_to, win, time);
	xcb_generic_error_t *err = xcb_request_check(conn, focus_cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Failed to set input focus : error code %d",
			  err->error_code);
		free(err);
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

	const uint16_t width  = node->rectangle.width;
	const uint16_t height = node->rectangle.height;
	const int16_t  x	  = node->rectangle.x;
	const int16_t  y	  = node->rectangle.y;

	if (resize_window(node->client->window, width, height) != 0 ||
		move_window(node->client->window, x, y) != 0) {
		return -1;
	}

	xcb_void_cookie_t cookie =
		xcb_map_window_checked(wm->connection, node->client->window);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in mapping window %d: error code %d",
			  node->client->window,
			  err->error_code);
		free(err);
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

bool
supports_protocol(xcb_window_t win, xcb_atom_t atom, xcb_conn_t *conn)
{
	xcb_get_property_cookie_t		   cookie = {0};
	xcb_icccm_get_wm_protocols_reply_t protocols;
	bool							   result = false;
	xcb_atom_t WM_PROTOCOLS = get_atom("WM_PROTOCOLS", conn);

	cookie = xcb_icccm_get_wm_protocols(conn, win, WM_PROTOCOLS);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &protocols, NULL) !=
		1) {
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
	uint16_t width	= r.width;
	uint16_t height = r.height;
	int16_t	 x		= r.x;
	int16_t	 y		= r.y;

	if (resize_window(win, width, height) != 0 ||
		move_window(win, x, y) != 0) {
		return -1;
	}

	xcb_void_cookie_t cookie = xcb_map_window_checked(wm->connection, win);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in mapping window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

int16_t
get_cursor_axis(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "Failed to query pointer position\n");
		return -1;
	}

	int16_t x = p_reply->root_x;
	free(p_reply);

	return x;
}

xcb_window_t
get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "Failed to query pointer position\n");
		return XCB_NONE;
	}

	xcb_window_t x = p_reply->child;
	free(p_reply);

	return x;
}

xcb_keycode_t *
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

xcb_keysym_t
get_keysym(xcb_keycode_t keycode, xcb_conn_t *conn)
{
	xcb_key_symbols_t *keysyms = NULL;
	xcb_keysym_t	   keysym;

	if ((keysyms = xcb_key_symbols_alloc(conn)) == NULL) {
		keysym = 0;
	}

	keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);

	return keysym;
}

void
window_grab_button(xcb_window_t win, uint8_t button, uint16_t modifier)
{
#define __GRAB__(btn, mod)                                                \
	xcb_grab_button(wm->connection,                                       \
					false,                                                \
					win,                                                  \
					XCB_EVENT_MASK_BUTTON_PRESS,                          \
					XCB_GRAB_MODE_SYNC,                                   \
					XCB_GRAB_MODE_ASYNC,                                  \
					XCB_NONE,                                             \
					XCB_NONE,                                             \
					btn,                                                  \
					mod)
	__GRAB__(button, modifier);
	if (num_lock != XCB_NO_SYMBOL && caps_lock != XCB_NO_SYMBOL &&
		scroll_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | num_lock | caps_lock | scroll_lock);
	}
	if (num_lock != XCB_NO_SYMBOL && caps_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | num_lock | caps_lock);
	}
	if (caps_lock != XCB_NO_SYMBOL && scroll_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | caps_lock | scroll_lock);
	}
	if (num_lock != XCB_NO_SYMBOL && scroll_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | num_lock | scroll_lock);
	}
	if (num_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | num_lock);
	}
	if (caps_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | caps_lock);
	}
	if (scroll_lock != XCB_NO_SYMBOL) {
		__GRAB__(button, modifier | scroll_lock);
	}
#undef __GRAB__
}

void
window_grab_buttons(xcb_window_t win)
{
	for (unsigned int i = 0; i < LEN(buttons_); i++) {
		if (click_to_focus == (int8_t)XCB_BUTTON_INDEX_ANY ||
			click_to_focus == (int8_t)buttons_[i]) {
			window_grab_button(win, buttons_[i], XCB_NONE);
		}
	}
}

void
window_ungrab_buttons(xcb_window_t win)
{
	xcb_void_cookie_t cookie = xcb_ungrab_button_checked(
		wm->connection, XCB_BUTTON_INDEX_ANY, win, XCB_MOD_MASK_ANY);

	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in ungrab buttons for window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return;
	}
}

void
ungrab_buttons_for_all(node_t *n)
{
	if (n == NULL)
		return;

	bool flag = n->node_type != INTERNAL_NODE && n->client != NULL;

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
grab_buttons(xcb_window_t win)
{

	xcb_void_cookie_t	 cookie;
	xcb_generic_error_t *error;
	// cookie =
	// 	xcb_ungrab_button(conn, XCB_BUTTON_INDEX_ANY, win, XCB_GRAB_ANY);
	// if (error) {
	// 	_LOG_(
	// 		ERROR, "Error ungrabbing button: %d\n", error->error_code);
	// 	free(error);
	// 	return -1;
	// }

	//  XCB_BUTTON_INDEX_ANY = Any of the following(or none)
	//  XCB_BUTTON_INDEX_1 = The left mouse button
	//  XCB_BUTTON_INDEX_2 = The right mouse button
	// 	XCB_BUTTON_INDEX_3 = The middle mouse button
	// 	XCB_BUTTON_INDEX_4 =  Scroll wheel
	// 	XCB_BUTTON_INDEX_5 = Scroll wheel
	const size_t		 n = sizeof(buttons_) / sizeof(buttons_[0]);
	for (size_t i = 0; i < n; ++i) {
		cookie = xcb_grab_button_checked(wm->connection,
										 false,
										 win,
										 XCB_EVENT_MASK_BUTTON_PRESS,
										 XCB_GRAB_MODE_ASYNC,
										 XCB_GRAB_MODE_ASYNC,
										 wm->root_window,
										 XCB_NONE,
										 buttons_[i],
										 XCB_MOD_MASK_ANY);
		error  = xcb_request_check(wm->connection, cookie);
		if (error) {
			_LOG_(ERROR,
				  "Error grabbing right mouse button: %d\n",
				  error->error_code);
			free(error);
			return;
		}
	}

	xcb_flush(wm->connection);
}

int
grab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return -1;
	}

	if (conf_keys != NULL && _entries_ != 0) {
		// _LOG_(INFO, "----grabing conf keys------\n");
		for (int i = 0; i < _entries_; i++) {
			xcb_keycode_t *key = get_keycode(conf_keys[i]->keysym, conn);
			if (key == NULL)
				return -1;
			xcb_void_cookie_t cookie =
				xcb_grab_key_checked(conn,
									 1,
									 win,
									 (uint16_t)conf_keys[i]->mod,
									 *key,
									 XCB_GRAB_MODE_ASYNC,
									 XCB_GRAB_MODE_ASYNC);
			free(key);
			xcb_generic_error_t *err = xcb_request_check(conn, cookie);
			if (err != NULL) {
				_LOG_(ERROR, "error grabbing key %d\n", err->error_code);
				free(err);
				return -1;
			}
		}
		return 0;
	}

	_LOG_(INFO, "----grabing default keys------\n");
	const size_t n = sizeof(keys_) / sizeof(keys_[0]);

	for (size_t i = n; i--;) {
		xcb_keycode_t *key = get_keycode(keys_[i].keysym, conn);
		if (key == NULL)
			return -1;
		xcb_void_cookie_t cookie =
			xcb_grab_key_checked(conn,
								 1,
								 win,
								 (uint16_t)keys_[i].mod,
								 *key,
								 XCB_GRAB_MODE_ASYNC,
								 XCB_GRAB_MODE_ASYNC);
		free(key);
		xcb_generic_error_t *err = xcb_request_check(conn, cookie);
		if (err != NULL) {
			_LOG_(ERROR, "error grabbing key %d\n", err->error_code);
			free(err);
			return -1;
		}
	}

	return 0;
}

xcb_atom_t
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
		free(rep);
		return atom;
	}
	return 0;
}

int
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
	xcb_void_cookie_t c			  = xcb_send_event_checked(
		  conn, false, win, XCB_EVENT_MASK_NO_EVENT, (char *)e);

	xcb_generic_error_t *err = xcb_request_check(conn, c);
	if (err != NULL) {
		_LOG_(ERROR, "error sending event: %d\n", err->error_code);
		free(e);
		free(err);
		return -1;
	}

	xcb_flush(conn);
	free(e);
	return 0;
}

int
close_or_kill_wrapper()
{
	xcb_window_t win =
		get_window_under_cursor(wm->connection, wm->root_window);
	if (!window_exists(wm->connection, win))
		return 0;
	return close_or_kill(win);
}

int
close_or_kill(xcb_window_t win)
{
	xcb_atom_t wm_delete = get_atom("WM_DELETE_WINDOW", wm->connection);
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, win);

	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (supports_protocol(win, wm_delete, wm->connection)) {
		if (wr == 1) {
#ifdef _DEBUG__
			_LOG_(DEBUG,
				  "window id = %d, reply name = %s: supports "
				  "WM_DELETE_WINDOW\n",
				  win,
				  t_reply.name);
#endif
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		}
		int ret = send_client_message(
			win, wm->ewmh->WM_PROTOCOLS, wm_delete, wm->connection);
		if (ret != 0) {
			_LOG_(ERROR, "failed to send client message\n");
			return -1;
		}
		return 0;
	}

	xcb_void_cookie_t	 c = xcb_kill_client_checked(wm->connection, win);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
	if (err != NULL) {
		_LOG_(ERROR,
			  "error closing window: %d, error: %d\n",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

void
ungrab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return;
	}

	const xcb_keycode_t modifier = (xcb_keycode_t)XCB_MOD_MASK_ANY;
	xcb_void_cookie_t	cookie =
		xcb_ungrab_key_checked(conn, XCB_GRAB_ANY, win, modifier);
	xcb_generic_error_t *err = xcb_request_check(conn, cookie);
	if (err != NULL) {
		_LOG_(ERROR, "error ungrabbing keys: %d\n", err->error_code);
		free(err);
	}
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

	free(g);
	resize_window(x, rc.width, rc.height);
	move_window(x, rc.x, rc.y);
	xcb_map_window(wm->connection, x);
}

int
kill_window(xcb_window_t win)
{
	if (win == XCB_NONE) {
		return -1;
	}

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);

	if (wr == 1) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "delete window id = %d, reply name = %s\n",
			  win,
			  t_reply.name);
#endif
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}

	if (win == 0) {
		return 0;
	}

	int curi = get_focused_desktop_idx();
	if (curi == -1) {
		_LOG_(ERROR, "cannot find focused desktop");
		return curi;
	}

	desktop_t *d = wm->desktops[curi];
	node_t	  *n = find_node_by_window_id(d->tree, win);
	client_t  *c = (n != NULL) ? n->client : NULL;

	if (c == NULL) {
		_LOG_(ERROR, "cannot find client with window %d", win);
		return -1;
	}

	xcb_void_cookie_t cookie = xcb_unmap_window(wm->connection, c->window);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err != NULL) {
		_LOG_(ERROR,
			  "Error in unmapping window %d: error code %d",
			  c->window,
			  err->error_code);
		free(err);
		return -1;
	}

	delete_node(n, d);
	ewmh_update_client_list();

	if (is_tree_empty(d->tree)) {
		set_active_window_name(XCB_NONE);
	}

	if (render_tree(d->tree) != 0) {
		_LOG_(ERROR, "cannot render tree");
		return -1;
	}

	return 0;
}

int
show_window(xcb_window_t win)
{
	xcb_generic_error_t *err;
	xcb_void_cookie_t	 c;
	/* According to ewmh:
	 * Mapped windows should be placed in NormalState, according to
	 * the ICCCM.
	 **/
	c	= xcb_map_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);

	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	// set window property to NormalState
	// XCB_ICCCM_WM_STATE_NORMAL
	const long		 data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(wm->connection,
									  XCB_PROP_MODE_REPLACE,
									  win,
									  wm_s,
									  wm_s,
									  32,
									  2,
									  data);
	err						= xcb_request_check(wm->connection, c);

	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
hide_window(xcb_window_t win)
{
	xcb_generic_error_t *err;
	xcb_void_cookie_t	 c;
	/* According to ewmh:
	 * Unmapped windows should be placed in IconicState, according to
	 * the ICCCM. Windows which are actually iconified or minimized
	 * should have the _NET_WM_STATE_HIDDEN property set, to
	 * communicate to pagers that the window should not be represented
	 * as "onscreen."
	 **/
	c	= xcb_unmap_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}

	// set window property to IconicState
	// XCB_ICCCM_WM_STATE_ICONIC
	const long		 data[] = {XCB_ICCCM_WM_STATE_ICONIC, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(wm->connection,
									  XCB_PROP_MODE_REPLACE,
									  win,
									  wm_s,
									  wm_s,
									  32,
									  2,
									  data);
	err						= xcb_request_check(wm->connection, c);

	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		free(err);
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
			}
			args[arg->argc] = NULL;
			execvp(args[0], (char *const *)args);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

void
shift_cmd(xcb_key_press_event_t *key_event)
{
	xcb_keysym_t k = get_keysym(key_event->detail, wm->connection);
	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return;

	int d = get_focused_desktop_idx();
	if (d == -1)
		return;

	node_t *root = wm->desktops[d]->tree;
	node_t *node = find_node_by_window_id(root, w);

	for (int i = 1; i <= wm->n_of_desktops; ++i) {
		if (k == (xcb_keysym_t)(XK_0 + i)) {
			if (d == i - 1) {
				return;
			}
			desktop_t *nd = wm->desktops[i - 1];
			desktop_t *od = wm->desktops[d];
			if (hide_window(node->client->window) != 0) {
				return;
			}
			unlink_node(node, od);
			transfer_node(node, nd);
			if (render_tree(od->tree) != 0) {
				return;
			}
			break;
		}
	}
}

void
polybar_exec(char *dir)
{
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		char *args[] = {"polybar", "-c", (char *)dir, NULL};
		execvp("polybar", args);
		perror("execvp");
		exit(EXIT_FAILURE);
	}
}

void
update_focused_desktop(int id)
{
	for (int i = 0; i < wm->n_of_desktops; ++i) {
		if (wm->desktops[i]->id != id) {
			wm->desktops[i]->is_focused = false;
		} else {
			wm->desktops[i]->is_focused = true;
		}
	}
}

int
set_focus(node_t *n, bool flag)
{
	n->is_focused = flag;
	if (win_focus(n->client->window, flag) != 0) {
		return -1;
	}
	// if (n->client->state != FLOATING) {
	flag ? raise_window(n->client->window)
		 : lower_window(n->client->window);
	// }
	return 0;
}

int
switch_desktop_wrapper(arg_t *arg)
{
	if (switch_desktop(arg->idx) != 0) {
		return -1;
	}
	node_t *tree = wm->desktops[arg->idx]->tree;
	if (wm->desktops[arg->idx]->layout == STACK) {
		if (wm->desktops[arg->idx]->top_w == XCB_NONE) {
			_LOG_(ERROR, "Top window is empty");
			goto out;
		}
		// restack(tree);
		xcb_window_t w = wm->desktops[arg->idx]->top_w;
		node_t		*n = find_node_by_window_id(tree, w);
		set_focus(n, true);
		return 0;
	}
out:
	return render_tree(tree);
}

int
switch_desktop(const int nd)
{
	int current = get_focused_desktop_idx();
	if (current == -1)
		return current;
	if (nd == current)
		return 0;

	update_focused_desktop(nd);

	const uint32_t		 _off[] = {ROOT_EVENT_MASK &
								   ~XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	const uint32_t		 _on[]	= {ROOT_EVENT_MASK};
	xcb_generic_error_t *err;
	xcb_void_cookie_t	 c;

	c = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _off);
	err = xcb_request_check(wm->connection, c);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		free(err);
		return -1;
	}

	if (show_windows(wm->desktops[nd]->tree) != 0) {
		return -1;
	}

	if (hide_windows(wm->desktops[current]->tree) != 0) {
		return -1;
	}

	if (!conf.focus_follow_pointer) {
		set_active_window_name(XCB_NONE);
		// win_focus(XCB_NONE, false);
	}

	c = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _on);
	err = xcb_request_check(wm->connection, c);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		free(err);
		return -1;
	}

#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", nd + 1);
	log_tree_nodes(wm->desktops[nd]->tree);
	_LOG_(INFO, "old desktop %d nodes--------------", current + 1);
	log_tree_nodes(wm->desktops[current]->tree);
#endif

	if (ewmh_update_current_desktop(wm->ewmh, wm->screen_nbr, nd) != 0) {
		return -1;
	}

	xcb_flush(wm->connection);

	return 0;
}

int
cycle_desktop_wrapper(arg_t *arg)
{
	direction_t d		= arg->d;
	int			current = get_focused_desktop_idx();
	int			next	= current;
	if (current == -1)
		return current;
	if (d == RIGHT) {
		next += 1;
	} else if (d == LEFT) {
		next -= 1;
	}

	if (next >= wm->n_of_desktops) {
		next = 0;
	} else if (next < 0) {
		next = wm->n_of_desktops - 1;
	}

	switch_desktop(next);

	return render_tree(wm->desktops[next]->tree);
}

int
set_active_window_name(xcb_window_t win)
{
	xcb_void_cookie_t aw_cookie =
		xcb_ewmh_set_active_window_checked(wm->ewmh, wm->screen_nbr, win);
	xcb_generic_error_t *err =
		xcb_request_check(wm->connection, aw_cookie);

	if (err) {
		_LOG_(ERROR, "Error setting active window: %d\n", err->error_code);
		free(err);
		return -1;
	}

	return 0;
}

int
set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state)
{
	const long		  data[] = {state, XCB_NONE};
	xcb_atom_t		  t		 = get_atom("WM_STATE", wm->connection);
	xcb_void_cookie_t c		 = xcb_change_property_checked(
		 wm->connection, XCB_PROP_MODE_REPLACE, win, t, t, 32, 2, data);
	xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
	if (err != NULL) {
		_LOG_(ERROR,
			  "Errror in changing property window %d: error code %d",
			  win,
			  err->error_code);
		free(err);
		return -1;
	}
	return 0;
}

bool
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
	free(attr_reply);
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
		uint32_t size_mask = (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE |
							  XCB_ICCCM_SIZE_HINT_P_MAX_SIZE);
		int32_t	 miw	   = size_hints.min_width;
		int32_t	 mxw	   = size_hints.max_width;
		int32_t	 mih	   = size_hints.min_height;
		int32_t	 mxh	   = size_hints.max_height;

		if ((size_hints.flags & size_mask) && (miw == mxw) &&
			(mih == mxh)) {
			// window should be floated
			return 0;
		}
	}
	return -1;
}

int
window_type(xcb_window_t win)
{
	xcb_ewmh_get_atoms_reply_t w_type;
	xcb_get_property_cookie_t  c =
		xcb_ewmh_get_wm_window_type(wm->ewmh, win);
	const uint8_t r =
		xcb_ewmh_get_wm_window_type_reply(wm->ewmh, c, &w_type, NULL);

	if (r == 1) {
		for (unsigned int i = 0; i < w_type.atoms_len; ++i) {
			const xcb_atom_t a = w_type.atoms[i];
			if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_NORMAL) {
				/*
				 * _NET_WM_WINDOW_TYPE_NORMAL
				 * indicates that this is a normal, top-level window.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 1;
			} else if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
				/*
				 * _NET_WM_WINDOW_TYPE_DOCK
				 * indicates a dock or panel feature.
				 * Typically, a Window Manager would keep such windows
				 * on top of all other windows.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 2;
			} else if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR ||
					   a == wm->ewmh->_NET_WM_WINDOW_TYPE_MENU) {
				/*
				 * _NET_WM_WINDOW_TYPE_TOOLBAR and
				 * _NET_WM_WINDOW_TYPE_MENU indicate toolbar and
				 * pinnable menu windows, respectively (i.e. toolbars
				 * and menus "torn off" from the main application).
				 * Windows of this type may set the WM_TRANSIENT_FOR
				 * hint indicating the main application window.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 3;
			} else if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
				/*
				 * _NET_WM_WINDOW_TYPE_UTILITY
				 * indicates a small persistent utility window, such
				 * as a palette or toolbox. It is distinct from type
				 * TOOLBAR because it does not correspond to a toolbar
				 * torn off from the main application. It's distinct
				 * from type DIALOG because it isn't a transient
				 * dialog, the user will probably keep it open while
				 * they're working. Windows of this type may set the
				 * WM_TRANSIENT_FOR hint indicating the main
				 * application window.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 4;
			} else if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
				/*
				 * _NET_WM_WINDOW_TYPE_SPLASH
				 * indicates that the window is a splash screen
				 * displayed as an application is starting up.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 5;
			} else if (a == wm->ewmh->_NET_WM_WINDOW_TYPE_DIALOG) {
				/*
				 * _NET_WM_WINDOW_TYPE_DIALOG
				 * indicates that this is a dialog window.
				 * If _NET_WM_WINDOW_TYPE is not set,
				 * then windows with WM_TRANSIENT_FOR set MUST be
				 * taken as this type.
				 * */
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 6;
			} else {
				xcb_ewmh_get_atoms_reply_wipe(&w_type);
				return 1;
			}
		}
	}
	return -1;
}

bool
should_ingore_hints(xcb_window_t win, const char *name)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t	   cn =
		xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		if (strcasecmp(t_reply.class_name, name) == 0) {
			xcb_icccm_get_wm_class_reply_wipe(&t_reply);
			return true;
		}
	}
	return false;
}

bool
window_exists(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_tree_cookie_t c = xcb_query_tree(conn, win);
	xcb_query_tree_reply_t *tree_reply =
		xcb_query_tree_reply(conn, c, NULL);

	if (tree_reply == NULL) {
		return false;
	}

	free(tree_reply);
	return true;
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
	rectangle_t	   r = {0};
	const uint16_t w = wm->screen->width_in_pixels;
	const uint16_t h = wm->screen->height_in_pixels;

	if (wm->bar != NULL) {
		r.x		 = conf.window_gap;
		r.y		 = wm->bar->rectangle.height + conf.window_gap;
		r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
		r.height = h - wm->bar->rectangle.height - 2 * conf.window_gap -
				   2 * conf.border_width;
	} else {
		r.x		 = conf.window_gap;
		r.y		 = conf.window_gap;
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
	return tile(d->tree);
}

int
handle_subsequent_window(client_t *client, desktop_t *d)
{
	xcb_window_t wi =
		get_window_under_cursor(wm->connection, wm->root_window);

	if (wi == wm->root_window || wi == 0) {
		return 0;
	}

	node_t *n = find_node_by_window_id(d->tree, wi);

	if (n->client->state == FLOATING) {
		_LOG_(INFO, "node under cusor is floating %d", wi);
		n = find_left_leaf(d->tree);
	}

	if (n->client->state == FULLSCREEN) {
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
		d->top_w = new_node->client->window;
	}
	ewmh_update_client_list();
	return render_tree(d->tree);
}

int
handle_floating_window(client_t *client, desktop_t *d)
{
	xcb_window_t wi =
		get_window_under_cursor(wm->connection, wm->root_window);

	if (wi == wm->root_window || wi == 0) {
		free(client);
		return 0;
	}

	node_t *n = find_node_by_window_id(d->tree, wi);

	if (n == NULL || n->client == NULL) {
		free(client);
		_LOG_(ERROR, "cannot find node with window id %d", wi);
		return -1;
	}

	node_t *new_node = create_node(client);
	if (new_node == NULL) {
		free(client);
		_LOG_(ERROR, "new node is null");
		return -1;
	}

	xcb_get_geometry_reply_t *g =
		get_geometry(client->window, wm->connection);
	if (g == NULL) {
		_LOG_(ERROR, "cannot get %d geometry", wm->bar->window);
		return -1;
	}

	int			x  = (wm->screen->width_in_pixels / 2) - (g->width / 2);
	int			y  = (wm->screen->height_in_pixels / 2) - (g->height / 2);
	rectangle_t rc = {
		.x = x, .y = y, .width = g->width, .height = g->height};
	new_node->rectangle = rc;
	free(g);

	insert_node(n, new_node, d->layout);
	d->n_count += 1;
	ewmh_update_client_list();

	return render_tree(d->tree);
}

int
handle_map_request(xcb_map_request_event_t *ev)
{
	xcb_window_t win = ev->window;

	if (is_transient(win)) {
		_LOG_(INFO, "win %d, is transient.. ignoring request", win);
		// goto float_;
		// return 0;
	}

	if (!should_manage(win, wm->connection)) {
		_LOG_(
			INFO, "win %d, shouldn't be managed.. ignoring request", win);
		return 0;
	}

	const int wint = window_type(win);

	const int idx  = get_focused_desktop_idx();
	if (idx == -1) {
		_LOG_(ERROR, "cannot get focused desktop idx");
		return idx;
	}

	// for some reason, some applications (e.g. gnome-text-editor,
	// emacs, ...) are mapped multiple times, which forces zwm to
	// create empty nodes with the same window id. if the window id
	// exists, ignore the request.
	if (find_node_by_window_id(wm->desktops[idx]->tree, win) != NULL) {
		return 0;
	}

	client_t  *client = NULL;
	desktop_t *d	  = wm->desktops[idx];

	// 1- if window's hints suggests floating client
	// 2- or window is not for emacs (emacs waints floating for some
	//    reason)
	// 3- and window type is not _NET_WM_WINDOW_TYPE_DOCK (rules in
	//    apply_floating_hints match duck window)
	// then window should be floated
	if ((apply_floating_hints(win) != -1 && (wint != 2))) {
		if (!should_ingore_hints(win, "emacs"))
			goto float_;
	}

	switch (wint) {
	case -1:
	case 0:
	case 1: {
		client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
		if (client == 0x00) {
			_LOG_(ERROR, "cannot allocate memory for client");
			return -1;
		}
		client->state = TILED;
		if (!conf.focus_follow_pointer) {
			grab_buttons(client->window);
		}

		if (is_tree_empty(d->tree)) {
			return handle_first_window(client, d);
		}

		return handle_subsequent_window(client, d);
	}
	case 2: {
		if (wm->bar != NULL) {
			return 0;
		}
		wm->bar = (bar_t *)malloc(sizeof(bar_t));
		if (wm->bar == NULL) {
			return -1;
		}
		wm->bar->window				 = win;
		rectangle_t				  rc = {0};
		xcb_get_geometry_reply_t *g	 = get_geometry(win, wm->connection);
		if (g == NULL) {
			_LOG_(ERROR, "cannot get %d geometry", wm->bar->window);
			return -1;
		}
		rc.height		   = g->height;
		rc.width		   = g->width;
		rc.x			   = g->x;
		rc.y			   = g->y;
		wm->bar->rectangle = rc;
		free(g);
		g = NULL;
		if (!is_tree_empty(d->tree)) {
			d->tree->rectangle.height =
				(uint16_t)(wm->screen->height_in_pixels - conf.window_gap -
						   rc.height - 5);
			d->tree->rectangle.y = (int16_t)(rc.height + 5);
			resize_subtree(d->tree);
			if (display_client(wm->bar->rectangle, wm->bar->window) != 0) {
				return -1;
			}
			return render_tree(d->tree);
		}
		if (display_client(wm->bar->rectangle, wm->bar->window) != 0) {
			return -1;
		}
		return 0;
	}
	case 3:
	case 4:
	case 5:
	case 6: {
	float_:
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "Window %s id %d is floating", name, win);
		free(name);
#endif
		client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
		if (client == 0x00) {
			_LOG_(ERROR, "cannot allocate memory for client");
			return -1;
		}
		client->state = FLOATING;
		if (!conf.focus_follow_pointer) {
			grab_buttons(client->window);
		}

		return handle_floating_window(client, d);
	}

	default: break;
	}
	return 0;
}

int
handle_enter_notify(const xcb_enter_notify_event_t *ev)
{
	xcb_window_t win = ev->event;

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved enter notify for %d, name %s ", win, name);
	free(name);
#endif

	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (wm->bar != NULL && win == wm->bar->window) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	const int curd = get_focused_desktop_idx();
	if (curd == -1)
		return curd;

	if (wm->desktops[curd]->layout == STACK) {
		win = wm->desktops[curd]->top_w;
	}

	node_t	 *root	 = wm->desktops[curd]->tree;

	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n != NULL && n->client != NULL) ? n->client : NULL;

	if (client == NULL || n == NULL) {
		return 0;
	}

	if (win == wm->root_window) {
		return 0;
	}

	if (!conf.focus_follow_pointer) {
		if (has_floating_window(root)) {
			restack(root);
		}
		if (n->client->state == FULLSCREEN) {
			if (fulllscreen_focus(n->client->window)) {
				_LOG_(ERROR, "cannot update win attributes");
				return -1;
			}
		}
		return 0;
	}

	if (n->client->window == focused_win) {
		return 0;
	}

	const int r = set_active_window_name(win);
	if (r != 0) {
		return 0;
	}

	if (n->client->state == FLOATING) {
		restack(root);
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR,
				  "cannot focus window %d (enter)",
				  n->client->window);
			return -1;
		}
	} else if (n->client->state == FULLSCREEN) {
		if (fulllscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return -1;
		}
	} else {
		if (set_focus(n, true) != 0) {
			_LOG_(ERROR, "cannot focus node (enter)");
			return -1;
		}
	}

	// if (has_floating_window(root)) {
	// 	restack(root);
	// }
	focused_win = n->client->window;
	update_focus(root, n);
	xcb_flush(wm->connection);

	return 0;
}

int
handle_leave_notify(const xcb_leave_notify_event_t *ev)
{
	if (!conf.focus_follow_pointer) {
		return 0;
	}

	xcb_window_t win = ev->event;

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved leave notify for %d, name %s ", win, name);
	free(name);
#endif

	if (wm->bar != NULL && win == wm->bar->window) {
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
	if (wm->desktops[curd]->layout == STACK) {
		return 0;
	}

	node_t		*root		   = wm->desktops[curd]->tree;
	xcb_window_t active_window = XCB_NONE;
	node_t		*n			   = find_node_by_window_id(root, win);
	client_t *client = (n != NULL && n->client != NULL) ? n->client : NULL;
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
			  "Failed to change border attr for window %d\n",
			  client->window);
		return -1;
	}

	return 0;
}

void
handle_key_press(xcb_key_press_event_t *key_press)
{
	uint16_t	 cleaned_state = (key_press->state & ~(XCB_MOD_MASK_LOCK));
	xcb_keysym_t k = get_keysym(key_press->detail, wm->connection);
	if (conf_keys != NULL && _entries_ != 0) {
		_LOG_(INFO, "----using conf keys------\n");
		for (int i = 0; i < _entries_; i++) {
			if (cleaned_state ==
				(conf_keys[i]->mod & ~(XCB_MOD_MASK_LOCK))) {
				if (conf_keys[i]->keysym == k) {
					arg_t *a   = conf_keys[i]->arg;
					int	   ret = conf_keys[i]->function_ptr(a);
					if (ret != 0) {
						_LOG_(ERROR,
							  "error while executing function_ptr(..)");
					}
					break;
				}
			}
		}
		return;
	}

	_LOG_(INFO, "----using default keys------\n");
	size_t n = sizeof(keys_) / sizeof(keys_[0]);
	for (size_t i = n; i--;) {
		if (cleaned_state == (keys_[i].mod & ~(XCB_MOD_MASK_LOCK))) {
			if (keys_[i].keysym == k) {
				arg_t *a   = keys_[i].arg;
				int	   ret = keys_[i].function_ptr(a);
				if (ret != 0) {
					_LOG_(ERROR, "error while executing function_ptr(..)");
				}
				break;
			}
		}
	}
}

int
handle_state(node_t		 *n,
			 xcb_atom_t	  state,
			 xcb_atom_t	  state_,
			 unsigned int action)
{
	char *name = win_name(n->client->window);

	if (state == wm->ewmh->_NET_WM_STATE_FULLSCREEN ||
		state_ == wm->ewmh->_NET_WM_STATE_FULLSCREEN) {
		_LOG_(INFO,
			  "_NET_WM_STATE_FULLSCREEN received for win %d:%s",
			  n->client->window,
			  name);
		if (action == XCB_EWMH_WM_STATE_ADD) {
			free(name);
			return set_fullscreen(n, true);
		} else if (action == XCB_EWMH_WM_STATE_REMOVE) {
			/* if (n->client->state == FULLSCREEN) { */
			free(name);
			return set_fullscreen(n, false);
			/* } */
		} else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
			uint32_t mode = (n->client->state == FULLSCREEN)
								? XCB_EWMH_WM_STATE_REMOVE
								: XCB_EWMH_WM_STATE_ADD;
			free(name);
			return set_fullscreen(n, mode == XCB_EWMH_WM_STATE_ADD);
		}
	} else if (state == wm->ewmh->_NET_WM_STATE_BELOW) {
		_LOG_(INFO,
			  "_NET_WM_STATE_BELOW received for win %d:%s",
			  n->client->window,
			  name);
	} else if (state == wm->ewmh->_NET_WM_STATE_ABOVE) {
		_LOG_(INFO,
			  "_NET_WM_STATE_ABOVE received for win %d:%s",
			  n->client->window,
			  name);
	} else if (state == wm->ewmh->_NET_WM_STATE_HIDDEN) {
		_LOG_(INFO,
			  "_NET_WM_STATE_HIDDEN received for win %d:%s",
			  n->client->window,

			  name);
	} else if (state == wm->ewmh->_NET_WM_STATE_STICKY) {
		_LOG_(INFO,
			  "_NET_WM_STATE_STICKY received for win %d:%s",
			  n->client->window,
			  name);
	} else if (state == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {
		_LOG_(INFO,
			  "_NET_WM_STATE_DEMANDS_ATTENTION received for win %d:%s",
			  n->client->window,
			  name);
	}
	free(name);
	return 0;
}

int
handle_client_message(xcb_client_message_event_t *client_message)
{

#ifdef _DEBUG__
	char *name = win_name(client_message->window);
	_LOG_(DEBUG,
		  "recieved client message for %d, name %s ",
		  client_message->window,
		  name);
	free(name);
#endif

	if (client_message->format != 32) {
		return 0;
	}

	int d = get_focused_desktop_idx();
	if (d == -1)
		return d;

	node_t *root = wm->desktops[d]->tree;

	node_t *n	 = find_node_by_window_id(root, client_message->window);
	if (n == NULL)
		return 0;
#ifdef _DEBUG__
	_LOG_(DEBUG, "received data32 for win %d:\n", client_message->window);
	for (ulong i = 0; i < LEN(client_message->data.data32); i++) {
		_LOG_(
			DEBUG, "data32[%d]: %u\n", i, client_message->data.data32[i]);
	}
#endif
	if (client_message->type == wm->ewmh->_NET_CURRENT_DESKTOP) {
		uint32_t nd = client_message->data.data32[0];
		if (nd > wm->ewmh->_NET_NUMBER_OF_DESKTOPS - 1) {
			return -1;
		}
		if (switch_desktop(nd) != 0) {
			return -1;
		}
		return 0;
	} else if (client_message->type == wm->ewmh->_NET_CLOSE_WINDOW) {
		_LOG_(INFO, "window want to be closed %d", client_message->window);
	} else if (client_message->type == wm->ewmh->_NET_WM_STATE) {
		char *s = win_name(client_message->window);
		_LOG_(INFO, "wm_state for %d name", client_message->window, s);
		free(s);
		handle_state(n,
					 client_message->data.data32[1],
					 client_message->data.data32[2],
					 client_message->data.data32[0]);
	} else if (client_message->type == wm->ewmh->_NET_ACTIVE_WINDOW) {

	} else if (client_message->type ==
			   wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {

	} else if (client_message->type == wm->ewmh->_NET_WM_STATE_STICKY) {

	} else if (client_message->type == wm->ewmh->_NET_WM_DESKTOP) {
	}
	// TODO: ewmh->_NET_WM_STATE
	// TODO: ewmh->_NET_CLOSE_WINDOW
	// TODO: ewmh->_NET_WM_DESKTOP
	// TODO: ewmh->_NET_WM_STATE_FULLSCREEN
	// TODO: ewmh->_NET_ACTIVE_WINDOW
	return 0;
}

int
handle_unmap_notify(xcb_window_t win)
{
	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return -1;

	node_t *root = wm->desktops[idx]->tree;
	if (root == NULL)
		return 0;

	if (!client_exist(root, win)) {
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		free(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (unmap)", win);
		return -1;
	}

	return 0;
}

void
handle_configure_request(xcb_configure_request_event_t *e)
{
	xcb_window_t						win = e->window;

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, e->window);
	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	char name[256];
	if (wr == 1) {
		snprintf(name, sizeof(name), "%s", t_reply.name);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	} else {
		return;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "window %d  name %s wants to be at %dx%d with %dx%d\n",
		  win,
		  name,
		  e->x,
		  e->y,
		  e->width,
		  e->height);
#endif

	const int d = get_focused_desktop_idx();
	if (d == -1) {
		return;
	}

	node_t *n		   = wm->desktops[d]->tree;
	bool	is_managed = client_exist(n, win);
	if (!is_managed) {
		uint16_t mask = 0;
		uint32_t values[7];
		uint16_t i = 0;
		if (e->value_mask & XCB_CONFIG_WINDOW_X) {
			mask |= XCB_CONFIG_WINDOW_X;
			values[i++] = (uint32_t)e->x;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_Y) {
			mask |= XCB_CONFIG_WINDOW_Y;
			values[i++] = (uint32_t)e->y;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_WIDTH;
			values[i++] = e->width;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
			mask |= XCB_CONFIG_WINDOW_HEIGHT;
			values[i++] = e->height;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
			values[i++] = e->border_width;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
			mask |= XCB_CONFIG_WINDOW_SIBLING;
			values[i++] = e->sibling;
		}

		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
			mask |= XCB_CONFIG_WINDOW_STACK_MODE;
			values[i++] = e->stack_mode;
		}

		xcb_configure_window(wm->connection, win, mask, values);
	} else {
		const node_t *node = find_node_by_window_id(n, e->window);
		if (node == NULL) {
			_LOG_(ERROR,
				  "config request -> cannot find node with win id %d",
				  e->window);
			return;
		}
		// TODO: deal with *node
	}
}

int
handle_destroy_notify(xcb_window_t win)
{
	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return -1;

	node_t *root = wm->desktops[idx]->tree;
	if (root == NULL)
		return 0;

	if (!client_exist(root, win)) {
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		free(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (destroy)", win);
		return -1;
	}

	return 0;
}

void
update_grabbed_window(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = root->node_type != INTERNAL_NODE && root->client != NULL;
	if (flag && root != n) {
		set_focus(root, false);
		grab_buttons(root->client->window);
	}

	update_grabbed_window(root->first_child, n);
	update_grabbed_window(root->second_child, n);
}

void
handle_button_press_event(xcb_button_press_event_t *ev)
{
	if (conf.focus_follow_pointer) {
		return;
	}
#ifdef _DEBUG__
	char *name = win_name(ev->event);
	_LOG_(DEBUG,
		  "RCIEVED BUTTON PRESS EVENT window %d, window name %s",
		  ev->event,
		  name);
	free(name);
#endif
	/* bool replay = false; */
	/* for (unsigned int i = 0; i < LEN(buttons_); i++) { */
	/* 	if (ev->detail != buttons_[i]) { */
	/* 		continue; */
	/* 	} */
	/* 	if ((click_to_focus == (int8_t)XCB_BUTTON_INDEX_ANY || */
	/* 		 click_to_focus == (int8_t)buttons_[i]) && */
	/* 		(ev->state & ~(num_lock | scroll_lock | caps_lock)) == */
	/* 			XCB_NONE) { */
	/* 	} */
	/* } */

	xcb_window_t win = ev->event;

	if (wm->bar != NULL && win == wm->bar->window) {
		return;
	}

	if (!window_exists(wm->connection, win)) {
		return;
	}

	const int curd = get_focused_desktop_idx();
	if (curd == -1)
		return;

	if (wm->desktops[curd]->layout == STACK) {
		win = wm->desktops[curd]->top_w;
	}

	node_t	 *root	 = wm->desktops[curd]->tree;
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n != NULL && n->client != NULL) ? n->client : NULL;

	if (client == NULL) {
		return;
	}

	if (win == wm->root_window) {
		return;
	}

	// update_grabbed_window(root, n);
	// update_focus(root, n);
	window_ungrab_buttons(client->window);

	const int r = set_active_window_name(win);
	if (r != 0) {
		return;
	}

	if (n->client->state == FLOATING) {
		restack(root);
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR,
				  "cannot focus window %d (enter)",
				  n->client->window);
			return;
		}
	} else if (n->client->state == FULLSCREEN) {
		if (fulllscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return;
		}
	} else {
		if (set_focus(n, true) != 0) {
			_LOG_(ERROR, "cannot focus node (enter)");
			return;
		}
	}

	// if (has_floating_window(root)) {
	// 	restack(root);
	// }

	update_focus(root, n);
	xcb_allow_events(wm->connection, XCB_ALLOW_SYNC_POINTER, ev->time);

	xcb_flush(wm->connection);
}

void
handle_focus_in(xcb_focus_in_event_t *focus_in_event)
{
}

void
log_active_desktop()
{
	for (int i = 0; i < wm->n_of_desktops; i++) {
		_LOG_(DEBUG,
			  "desktop %d, active %s",
			  wm->desktops[i]->id,
			  wm->desktops[i]->is_focused ? "true" : "false");
	}
}

void
log_children(xcb_conn_t *conn, xcb_window_t root_window)
{
	xcb_query_tree_cookie_t tree_cookie =
		xcb_query_tree(conn, root_window);
	xcb_query_tree_reply_t *tree_reply =
		xcb_query_tree_reply(conn, tree_cookie, NULL);
	if (tree_reply == NULL) {
		_LOG_(ERROR, "Failed to query tree reply\n");
		return;
	}

	_LOG_(DEBUG, "Children of root window:\n");
	xcb_window_t *children = xcb_query_tree_children(tree_reply);
	const int num_children = xcb_query_tree_children_length(tree_reply);
	for (int i = 0; i < num_children; ++i) {
		xcb_icccm_get_text_property_reply_t t_reply;
		xcb_get_property_cookie_t			cn =
			xcb_icccm_get_wm_name(conn, children[i]);
		uint8_t wr = xcb_icccm_get_wm_name_reply(conn, cn, &t_reply, NULL);
		if (wr == 1) {
			_LOG_(DEBUG, "Child %d: %s\n", i + 1, t_reply.name);
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		} else {
			_LOG_(
				DEBUG, "Failed to get window name for child %d\n", i + 1);
		}
	}

	free(tree_reply);
}

void
free_desktops(desktop_t **d, int n)
{
	for (int i = 0; i < n; ++i) {
		free_tree(d[i]->tree);
		free(d[i]);
		d[i] = NULL;
	}
	free(d);
	d = NULL;
}

void
parse_args(int argc, char **argv)
{
	// quick and dirty approach
	char *c = NULL;
	if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-run") == 0) {
		if (argc >= 2) {
			c = argv[2];
		} else {
			_LOG_(ERROR, "Missing argument after -r/--run\n");
		}
	}
	exec_process(&((arg_t){.argc = 1, .cmd = (char *[]){c}}));
}

void
evet_loop(wm_t *w)
{
	xcb_generic_event_t *event;
	while ((event = xcb_wait_for_event(w->connection))) {
		switch (event->response_type & ~0x80) {
		case XCB_MAP_REQUEST: {
			xcb_map_request_event_t *map_request =
				(xcb_map_request_event_t *)event;
			if (handle_map_request(map_request) != 0) {
				_LOG_(ERROR,
					  "Failed to handle MAP_REQUEST for window %d\n",
					  map_request->window);
			}
			break;
		}
		case XCB_UNMAP_NOTIFY: {
			xcb_unmap_notify_event_t *unmap_notify =
				(xcb_unmap_notify_event_t *)event;
			if (handle_unmap_notify(unmap_notify->window) != 0) {
				_LOG_(ERROR,
					  "Failed to handle XCB_UNMAP_NOTIFY for "
					  "window %d\n",
					  unmap_notify->window);
			}
			break;
		}
		case XCB_DESTROY_NOTIFY: {
			xcb_destroy_notify_event_t *destroy_notify =
				(xcb_destroy_notify_event_t *)event;
			if (handle_destroy_notify(destroy_notify->window) != 0) {
				_LOG_(ERROR,
					  "Failed to handle XCB_DESTROY_NOTIFY for "
					  "window %d\n",
					  destroy_notify->window);
			}
			break;
		}
		case XCB_EXPOSE: {
			__attribute__((unused)) xcb_expose_event_t *expose_event =
				(xcb_expose_event_t *)event;
			break;
		}
		case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t *client_message =
				(xcb_client_message_event_t *)event;
			handle_client_message(client_message);
			break;
		}
		case XCB_CONFIGURE_REQUEST: {
			xcb_configure_request_event_t *config_request =
				(xcb_configure_request_event_t *)event;
			handle_configure_request(config_request);
			break;
		}
		case XCB_CONFIGURE_NOTIFY: {
			__attribute__((unused))
			xcb_configure_notify_event_t *config_notify =
				(xcb_configure_notify_event_t *)event;
			break;
		}
		case XCB_PROPERTY_NOTIFY: {
			__attribute__((unused))
			xcb_property_notify_event_t *property_notify =
				(xcb_property_notify_event_t *)event;
			break;
		}
		case XCB_ENTER_NOTIFY: {
			xcb_enter_notify_event_t *enter_event =
				(xcb_enter_notify_event_t *)event;
			if (handle_enter_notify(enter_event) != 0) {
				_LOG_(ERROR,
					  "Failed to handle XCB_ENTER_NOTIFY for "
					  "window %d\n",
					  enter_event->event);
			}
			break;
		}
		case XCB_LEAVE_NOTIFY: {
			__attribute__((unused)) xcb_leave_notify_event_t *leave_event =
				(xcb_leave_notify_event_t *)event;
			// if (handle_leave_notify(leave_event) != 0) {
			// 	_LOG_(ERROR,
			// 				"Failed to handle XCB_LEAVE_NOTIFY for "
			// 				"window %d\n",
			// 				leave_event->event);
			// }
			break;
		}
		case XCB_MOTION_NOTIFY: {
			__attribute__((unused))
			xcb_motion_notify_event_t *motion_notify =
				(xcb_motion_notify_event_t *)event;
			break;
		}
		case XCB_BUTTON_PRESS: {
			xcb_button_press_event_t *button_press =
				(xcb_button_press_event_t *)event;
			handle_button_press_event(button_press);
			break;
		}
		case XCB_BUTTON_RELEASE: {
			__attribute__((unused))
			xcb_button_release_event_t *button_release =
				(xcb_button_release_event_t *)event;
			break;
		}
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t *key_press =
				(xcb_key_press_event_t *)event;
			handle_key_press(key_press);
			break;
		}
		case XCB_KEY_RELEASE: {
			__attribute__((unused)) xcb_key_release_event_t *key_release =
				(xcb_key_release_event_t *)event;
			break;
		}
		case XCB_FOCUS_IN: {
			xcb_focus_in_event_t *focus_in_event =
				(xcb_focus_in_event_t *)event;
			handle_focus_in(focus_in_event);
			break;
		}
		case XCB_FOCUS_OUT: {
			__attribute__((unused))
			xcb_focus_out_event_t *focus_out_event =
				(xcb_focus_out_event_t *)event;
			break;
		}
		case XCB_MAPPING_NOTIFY: {
			__attribute__((unused))
			xcb_mapping_notify_event_t *mapping_notify =
				(xcb_mapping_notify_event_t *)event;
			if (0 != grab_keys(wm->connection, wm->root_window)) {
				_LOG_(ERROR, "cannot grab keys");
			}
		}
		default: {
			break;
		}
		}
		free(event);
	}
}

void
cleanup()
{
	xcb_disconnect(wm->connection);
	free_desktops(wm->desktops, wm->n_of_desktops);
	xcb_ewmh_connection_wipe(wm->ewmh);
	free_keys();
	free(wm);
	wm = NULL;
}

int
main(int argc, char **argv)
{

	wm = init_wm();
	if (wm == 0x00) {
		_LOG_(ERROR, "Failed to initialize window manager\n");
		exit(EXIT_FAILURE);
	}

	if (load_config(&conf) != 0) {
		_LOG_(ERROR, "error while loding config -> using default macros");
		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
	}

	wm->n_of_desktops = conf.virtual_desktops;
	if (!setup_desktops(wm)) {
		_LOG_(ERROR, "error while setting up desktops");
		exit(EXIT_FAILURE);
	}

	if (argc >= 2) {
		parse_args(argc, argv);
	}

	evet_loop(wm);
	cleanup();
	return 0;
}
