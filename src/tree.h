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

#ifndef ZWM_TREE_H
#define ZWM_TREE_H

#include "type.h"
// clang-format off
node_t   *create_node(client_t *c);
node_t   *init_root(void);
node_t   *find_node_by_window_id(node_t *root, xcb_window_t window_id);
node_t   *find_master_node(node_t *root);
node_t   *prev_node(node_t *current);
node_t   *next_node(node_t *current);
node_t   *cycle_win(node_t *node, direction_t);
node_t   *find_left_leaf(node_t *root);
node_t   *find_any_leaf(node_t *root);
node_t 	 *get_focused_node(node_t *n);
bool     unlink_node(node_t *node, desktop_t *d);
void     horizontal_resize(node_t *n, resize_t t);
void 	 apply_master_layout(node_t *parent);
void 	 apply_default_layout(node_t *root);
void	 apply_stack_layout(node_t *root);
void	 update_focus(node_t *root, node_t *n);
void	 flip_node(node_t *node);
void	 resize_subtree(node_t *parent);
void	 apply_layout(desktop_t *d, layout_t t);
void	 free_tree(node_t *root);
void	 restack(void);
void     restackv2(node_t *root);
void	 delete_node(node_t *node, desktop_t *d);
void	 insert_node(node_t *current_node, node_t *new_node, layout_t layout);
void     arrange_tree(node_t *tree, layout_t l);
void	 log_tree_nodes(node_t *node);
bool	 transfer_node(node_t *, desktop_t *);
bool	 is_tree_empty(const node_t *root);
bool	 client_exist(node_t *cn, xcb_window_t id);
bool	 has_floating_window(node_t *root);
int	   	 render_tree(node_t *current_node);
int		 hide_windows(node_t *tree);
int 	 show_windows(node_t *tree);
int	     swap_node(node_t *root);
// clang-format on
#endif // ZWM_TREE_H
