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

#include "cursor.h"
#include "helper.h"
#include "state.h"
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

void
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

xcb_cursor_t
get_cursor(cursor_t c)
{
	assert(c < CURSOR_MAX);
	return cursors[c];
}

void
set_cursor(int cursor_id)
{
	xcb_cursor_t c		  = get_cursor(cursor_id);
	uint32_t	 values[] = {c};
	xcb_cookie_t cookie	  = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_CURSOR, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR, "error setting cursor on root window %d", err->error_code);
		_FREE_(err);
	}
	xcb_flush(wm->connection);
}
