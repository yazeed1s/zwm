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

#include "actions.h"
#include "bindings.h"
#include "config_parser.h"
#include "desktop.h"
#include "events.h"
#include "ewmh.h"
#include "helper.h"
#include "monitor.h"
#include "state.h"
#include "type.h"
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

static wm_t *
init_wm(void)
{
	int i, default_screen;
	wm = (wm_t *)malloc(sizeof(wm_t));
	if (wm == NULL) {
		_LOG_(ERROR, "failed to malloc for window manager");
		return NULL;
	}

	wm->connection = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(wm->connection) > 0) {
		_LOG_(ERROR, "error: Unable to open X connection");
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
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {ROOT_EVENT_MASK};

	/* register events */
	xcb_cookie_t   cookie	= xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error registering for substructure redirection "
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
	uint16_t		  class	 = XCB_WINDOW_CLASS_INPUT_ONLY;

	xcb_create_window(
		dpy, depth, mw, rw, -1, -1, 1, 1, 0, class, visual, m, NULL);
	xcb_icccm_set_wm_class(dpy, mw, sizeof(WM_NAME), WM_NAME);
	return wm;
}

/* set_desktop is called when 1- zwm starts and monitors were intially set up
 * and need to have desktops assigned to them. 2- when xrandr forces monitor
 * changes */
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

static void
signal_handler(int sig)
{
	/* Set shutdown flag to exit event loop cleanly */
	should_shutdown = 1;
}

static void
cleanup(int sig)
{
	if (wm != NULL) {
		if (wm->connection != NULL) {
			xcb_disconnect(wm->connection);
		}
		if (wm->ewmh != NULL) {
			xcb_ewmh_connection_wipe(wm->ewmh);
			free(wm->ewmh);
		}
		_FREE_(wm);
	}
	free_keys();
	free_rules();
	cleanup_strut_windows();
	free_monitors(); /* frees desktops and trees as well */
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
		conf.focus_follow_spawn	  = FOCUS_FOLLOW_SPAWN;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
		conf.restore_last_focus	  = RESTORE_LAST_FOCUS;
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

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGABRT, signal_handler);

	event_loop(wm);
	cleanup(0);

	return 0;
}
