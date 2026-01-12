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

#ifndef ZWM_DRAG_H
#define ZWM_DRAG_H

#include "type.h"

/* drag state - tracks active drag session */
typedef struct {
	xcb_window_t window;   /* window being dragged */
	node_t		*src_node; /* original node */
	int16_t		 start_x;  /* initial cursor x */
	int16_t		 start_y;  /* initial cursor y */
	bool		 active;   /* drag in progress */
	bool		 kbd_mode; /* keyboard-driven drag */

	/* Live Preview State */
	int16_t		 cur_x, cur_y;
	node_t		*last_target;	/* cached target leaf for preview */
	bool		 preview_active;

	/* Restore Info */
	desktop_t	*original_desktop;
	rectangle_t	 original_rect;
} drag_state_t;

/* clang-format off */
int drag_start(xcb_window_t win, int16_t x, int16_t y, bool kbd);
int drag_move(int16_t x, int16_t y);
int drag_end(int16_t x, int16_t y);
int drag_cancel(void);
int start_keyboard_drag_wrapper(arg_t *arg);

node_t *find_leaf_at_point(node_t *root, int16_t x, int16_t y);
/* clang-format on */

#endif /* ZWM_DRAG_H */
