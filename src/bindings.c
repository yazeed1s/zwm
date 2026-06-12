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

#include "bindings.h"
#include "actions.h"
#include "config_parser.h"
#include "helper.h"
#include "state.h"
#include <X11/keysym.h>
#include <stddef.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* clang-format off */

/* keys_[] is used as a fallback in case of an
 * error while loading the keys from the config file */

/* see X11/keysymdef.h */
const _key__t _keys_[] = {
    DEFINE_KEY(SUPER,         _KEY(w),       close_or_kill_wrapper,     NULL),
    DEFINE_KEY(SUPER,         _KEY(Return),  exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"alacritty"}})),
    DEFINE_KEY(SUPER,         _KEY(space),   exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"dmenu_run"}})),
    DEFINE_KEY(SUPER,         _KEY(p),       exec_process,              &((arg_t){.argc = 3, .cmd = (char *[]){"rofi", "-show", "drun"}})),
    DEFINE_KEY(SUPER,         _KEY(1),       switch_desktop_wrapper,    &((arg_t){.idx  = 0})),
    DEFINE_KEY(SUPER,         _KEY(2),       switch_desktop_wrapper,    &((arg_t){.idx  = 1})),
    DEFINE_KEY(SUPER,         _KEY(3),       switch_desktop_wrapper,    &((arg_t){.idx  = 2})),
    DEFINE_KEY(SUPER,         _KEY(4),       switch_desktop_wrapper,    &((arg_t){.idx  = 3})),
    DEFINE_KEY(SUPER,         _KEY(5),       switch_desktop_wrapper,    &((arg_t){.idx  = 4})),
    DEFINE_KEY(SUPER,         _KEY(6),       switch_desktop_wrapper,    &((arg_t){.idx  = 5})),
    DEFINE_KEY(SUPER,         _KEY(7),       switch_desktop_wrapper,    &((arg_t){.idx  = 6})),
    DEFINE_KEY(SUPER,         _KEY(Left),    cycle_win_wrapper,         &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER,         _KEY(Right),   cycle_win_wrapper,         &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER,         _KEY(Up),      cycle_win_wrapper,         &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER,         _KEY(Down),    cycle_win_wrapper,         &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER,         _KEY(l),       dynamic_resize_wrapper, &((arg_t){.r    = GROW})),
    DEFINE_KEY(SUPER,         _KEY(h),       dynamic_resize_wrapper, &((arg_t){.r    = SHRINK})),
    DEFINE_KEY(SUPER,         _KEY(f),       set_fullscreen_wrapper,    NULL),
    DEFINE_KEY(SUPER,         _KEY(s),       swap_node_wrapper,         NULL),
    DEFINE_KEY(SUPER,         _KEY(i),       gap_handler,               &((arg_t){.r    = GROW})),
    DEFINE_KEY(SUPER,         _KEY(d),       gap_handler,               &((arg_t){.r    = SHRINK})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Left),    shift_floating_window,     &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Right),   shift_floating_window,     &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Up),      shift_floating_window,     &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Down),    shift_floating_window,     &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER | ALT,   _KEY(f),       change_state,              &((arg_t){.s    = FLOATING})),
    DEFINE_KEY(SUPER | ALT,   _KEY(t),       change_state,              &((arg_t){.s    = TILED})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(1),       transfer_node_wrapper,     &((arg_t){.idx  = 0})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(2),       transfer_node_wrapper,     &((arg_t){.idx  = 1})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(3),       transfer_node_wrapper,     &((arg_t){.idx  = 2})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(4),       transfer_node_wrapper,     &((arg_t){.idx  = 3})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(5),       transfer_node_wrapper,     &((arg_t){.idx  = 4})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(6),       transfer_node_wrapper,     &((arg_t){.idx  = 5})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(7),       transfer_node_wrapper,     &((arg_t){.idx  = 6})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(m),       layout_handler,            &((arg_t){.t    = MASTER})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(d),       layout_handler,            &((arg_t){.t    = DEFAULT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(s),       layout_handler,            &((arg_t){.t    = STACK})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(c),       layout_handler,            &((arg_t){.t    = GRID})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(k),       traverse_stack_wrapper,    &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(j),       traverse_stack_wrapper,    &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(f),       flip_node_wrapper,         NULL),
    DEFINE_KEY(SUPER | SHIFT, _KEY(r),       reload_config_wrapper,     NULL),
    DEFINE_KEY(SUPER | ALT,   _KEY(Left),    cycle_desktop_wrapper,     &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER | ALT,   _KEY(Right),   cycle_desktop_wrapper,     &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(y),       grow_floating_window,      &((arg_t){.rd   = HORIZONTAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(h),       grow_floating_window,      &((arg_t){.rd   = VERTICAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(t),       shrink_floating_window,    &((arg_t){.rd   = HORIZONTAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(g),       shrink_floating_window,    &((arg_t){.rd   = VERTICAL_DIR})),
    DEFINE_KEY(SUPER | CTRL,  _KEY(Right),   cycle_monitors,            &((arg_t){.tr   = NEXT})),
    DEFINE_KEY(SUPER | CTRL,  _KEY(Left),    cycle_monitors,            &((arg_t){.tr   = PREV})),
    DEFINE_KEY(SUPER,         _KEY(m),       start_keyboard_drag_wrapper, NULL),
};

const size_t _keys_len = sizeof(_keys_) / sizeof(_keys_[0]);

/* clang-format on */

int16_t
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
grab_super_button(xcb_window_t win, uint8_t button)
{
	const uint16_t numlock = (uint16_t)modfield_from_keysym(XK_Num_Lock);
	const uint16_t caps	   = XCB_MOD_MASK_LOCK;
	const uint16_t mods[]  = {
		SUPER,
		(uint16_t)(SUPER | caps),
		(uint16_t)(SUPER | numlock),
		(uint16_t)(SUPER | numlock | caps),
	};
	bool logged = false;

	for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
		uint16_t mod = mods[i];
		bool	 dup = false;
		for (size_t j = 0; j < i; j++) {
			if (mods[j] == mod) {
				dup = true;
				break;
			}
		}
		if (dup) {
			continue;
		}

		xcb_void_cookie_t c = xcb_grab_button_checked(
			wm->connection,
			0,	 /* owner_events */
			win, /* grab_window */
			XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
				XCB_EVENT_MASK_POINTER_MOTION,
			XCB_GRAB_MODE_ASYNC, /* allow processing */
			XCB_GRAB_MODE_ASYNC, /* keyboard_mode */
			XCB_NONE,			 /* confine_to */
			XCB_NONE,			 /* cursor */
			button,				 /* button */
			mod);				 /* modifiers */

		xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR,
				  "could not grab SUPER+Button%d on root (mod=0x%x): %d",
				  button,
				  mod,
				  err->error_code);
			_FREE_(err);
		} else if (!logged) {
			_LOG_(INFO, "grabbed SUPER+Button%d on root", button);
			logged = true;
		}
	}
}

int
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
		grab_super_button(win, XCB_BUTTON_INDEX_1);
		grab_super_button(win, XCB_BUTTON_INDEX_3);
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

	grab_super_button(win, XCB_BUTTON_INDEX_1);
	grab_super_button(win, XCB_BUTTON_INDEX_3);

	return 0;
}

void
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
