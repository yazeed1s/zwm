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
#ifndef ZWM_LAYOUT_H
#define ZWM_LAYOUT_H

#include "type.h"

/* clang-format off */
void calculate_base_rect(rectangle_t *r, monitor_t *m);
void arrange_tree(node_t *tree, layout_t l);
void apply_layout(desktop_t *d, layout_t t);
void master_clean_up(node_t *root);
void default_layout(node_t *root);
void master_layout(node_t *parent, node_t *n);
void stack_layout(node_t *parent);
void apply_default_layout(node_t *root);
void apply_master_layout(node_t *parent);
void apply_stack_layout(node_t *root);
void apply_grid_layout(node_t *root);
void grid_layout(node_t *root);
void monocle_layout(node_t *root);
int  render_monocle(node_t *root);
void three_col_layout(node_t *root);
void deck_layout(node_t *root);
int  render_deck(node_t *root);
void flip_node(node_t *node);
void dynamic_resize(node_t *n, resize_t t);
void split_node(node_t *n, node_t *nd);
void resize_subtree(node_t *parent);
/* clang-format on */

#endif /* ZWM_LAYOUT_H */
