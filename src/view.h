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
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ZWM_VIEW_H
#define ZWM_VIEW_H

#include "type.h"
#include <stdbool.h>

/*
 * layout policy:
 *   DEFAULT, MASTER, STACK, GRID, and THREE_COL are normal visible layouts.
 *   In these layouts every tiled window should stay mapped.  logical_focus is
 *   still remembered, but it is used for the selected window / border / MRU,
 *   not for hiding other tiled windows.
 *
 *   MONOCLE and DECK are hidden layouts.  MONOCLE maps only logical_focus.
 *   DECK maps the master window plus logical_focus from the deck side.  If the
 *   saved focus is gone or not valid for the layout, view.c picks a fallback.
 *
 * Floating windows are shown in all layouts.  This file renders them, but it
 * does not raise them.  The final order belongs to restack() which decides the
 * Z order.
 */

/* clang-format off */
leaf_visibility_t _leaf_visibility_(desktop_t *d, node_t *leaf);
void _focus_node_(desktop_t *d, node_t *n);
int _focus_input_(desktop_t *d, node_t *n);
node_t *_pick_focus_(desktop_t *d);
int _render_view_(desktop_t *d);
void _flush_view_(desktop_t *d);
/* clang-format on */
#endif /* ZWM_VIEW_H */
