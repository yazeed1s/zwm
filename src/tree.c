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

#include "tree.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "helper.h"
#include "type.h"
#include "zwm.h"

// clang-format off
static void master_layout(node_t *parent, node_t *);
static void stack_layout(node_t *parent);
static void default_layout(node_t *parent);
static node_t *find_tree_root(node_t *);
static bool is_parent_null(const node_t *node);
static void horizontal_resize(node_t *n, resize_t t);
// clang-format on

node_t *
create_node(client_t *c)
{
	if (c == 0x00)
		return NULL;

	node_t *node = (node_t *)malloc(sizeof(node_t));
	if (node == 0x00) {
		_FREE_(c);
		return NULL;
	}

	node->rectangle			 = (rectangle_t){0};
	node->floating_rectangle = (rectangle_t){0};
	node->client			 = c;
	node->parent			 = NULL;
	node->first_child		 = NULL;
	node->second_child		 = NULL;
	node->is_master			 = false;
	node->is_focused		 = false;

	return node;
}

node_t *
init_root(void)
{
	node_t *node = (node_t *)malloc(sizeof(node_t));
	if (node == 0x00)
		return NULL;

	node->rectangle			 = (rectangle_t){0};
	node->floating_rectangle = (rectangle_t){0};
	node->client			 = NULL;
	node->parent			 = NULL;
	node->first_child		 = NULL;
	node->second_child		 = NULL;
	node->node_type			 = ROOT_NODE;
	node->is_master			 = false;
	node->is_focused		 = false;

	return node;
}

int
render_tree(node_t *node)
{
	if (node == NULL)
		return 0;

	node_t **stack = malloc(10 * sizeof(node_t *));
	if (stack == NULL) {
		_LOG_(ERROR, "Stack allocation failed\n");
		return -1;
	}

	int stack_size = 10;
	int top		   = -1;
	stack[++top]   = node;

	while (top >= 0) {
		node_t *current_node = stack[top--];

		if (current_node == NULL)
			continue;

		if (!IS_INTERNAL(current_node) && current_node->client != NULL) {
			bool flag = IS_FULLSCREEN(current_node->client);
			if (!flag) {
				if (tile(current_node) != 0) {
					_LOG_(ERROR,
						  "error tiling window %d",
						  current_node->client->window);
					_FREE_(stack);
					return -1;
				}
			}
		}

		if (current_node->second_child != NULL) {
			if (top == stack_size - 1) {
				stack_size *= 2;
				node_t **new_stack =
					realloc(stack, stack_size * sizeof(node_t *));
				if (new_stack == NULL) {
					_LOG_(ERROR, "Stack reallocation failed\n");
					_FREE_(stack);
					return -1;
				}
				stack = new_stack;
			}
			stack[++top] = current_node->second_child;
		}

		if (current_node->first_child != NULL) {
			if (top == stack_size - 1) {
				stack_size *= 2;
				node_t **new_stack =
					realloc(stack, stack_size * sizeof(node_t *));
				if (new_stack == NULL) {
					_LOG_(ERROR, "Stack reallocation failed\n");
					_FREE_(stack);
					return -1;
				}
				stack = new_stack;
			}
			stack[++top] = current_node->first_child;
		}
	}

	_FREE_(stack);
	return 0;
}

static int
get_tree_level(node_t *node)
{
	if (node == NULL)
		return 0;

	int level_first_child  = get_tree_level(node->first_child);
	int level_second_child = get_tree_level(node->second_child);

	return 1 + MAX(level_first_child, level_second_child);
}

static bool
has_floating_children(const node_t *parent)
{
	return (parent->first_child != NULL &&
			parent->first_child->client != NULL &&
			IS_FLOATING(parent->first_child->client)) ||
		   (parent->second_child != NULL &&
			parent->second_child->client != NULL &&
			IS_FLOATING(parent->second_child->client));
}

static node_t *
get_floating_child(const node_t *parent)
{
	if (parent->first_child != NULL &&
		parent->first_child->client != NULL &&
		IS_FLOATING(parent->first_child->client)) {
		return parent->first_child;
	}

	if (parent->second_child != NULL &&
		parent->second_child->client != NULL &&
		IS_FLOATING(parent->second_child->client)) {
		return parent->second_child;
	}

	return NULL;
}

static void
insert_floating_node(node_t *node, desktop_t *d)
{
	assert(IS_FLOATING(node->client));
	node_t *n = find_any_leaf(d->tree);
	if (n == NULL)
		return;

	if (n->first_child == NULL) {
		n->first_child = node;
	} else {
		n->second_child = node;
	}
	node->node_type = EXTERNAL_NODE;
}

static void
split_node(node_t *n, node_t *nd)
{
	if (IS_FLOATING(nd->client)) {
		// if (!IS_FLOATING(n->first_child->client)) {
		n->first_child->rectangle = n->first_child->floating_rectangle =
			n->rectangle;
		// }
	} else {
		if (n->rectangle.width >= n->rectangle.height) {
			n->first_child->rectangle.x = n->rectangle.x;
			n->first_child->rectangle.y = n->rectangle.y;
			n->first_child->rectangle.width =
				(n->rectangle.width -
				 (conf.window_gap - conf.border_width)) /
				2;
			n->first_child->rectangle.height = n->rectangle.height;
			if (!IS_FLOATING(n->first_child->client)) {
				n->second_child->rectangle.x =
					(int16_t)(n->rectangle.x +
							  n->first_child->rectangle.width +
							  conf.window_gap + conf.border_width);
				n->second_child->rectangle.y = n->rectangle.y;
				n->second_child->rectangle.width =
					n->rectangle.width - n->first_child->rectangle.width -
					conf.window_gap - conf.border_width;
				n->second_child->rectangle.height = n->rectangle.height;
			} else {
				n->second_child->rectangle = n->rectangle;
			}
		} else {
			n->first_child->rectangle.x		= n->rectangle.x;
			n->first_child->rectangle.y		= n->rectangle.y;
			n->first_child->rectangle.width = n->rectangle.width;
			n->first_child->rectangle.height =
				(n->rectangle.height -
				 (conf.window_gap - conf.border_width)) /
				2;
			if (!IS_FLOATING(n->first_child->client)) {
				n->second_child->rectangle.x = n->rectangle.x;
				n->second_child->rectangle.y =
					(int16_t)(n->rectangle.y +
							  n->first_child->rectangle.height +
							  conf.window_gap + conf.border_width);
				n->second_child->rectangle.width = n->rectangle.width;
				n->second_child->rectangle.height =
					n->rectangle.height -
					n->first_child->rectangle.height - conf.window_gap -
					conf.border_width;
			} else {
				n->second_child->rectangle = n->rectangle;
			}
		}
	}
}

void
insert_node(node_t *node, node_t *new_node, layout_t layout)
{
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "Node to split %d, node to insert %d",
		  node->client->window,
		  new_node->client->window);
#endif

	if (node == NULL) {
		_LOG_(ERROR, "node is null");
		return;
	}

	if (node->client == NULL) {
		_LOG_(ERROR, "client is null in node");
		return;
	}

	if (!IS_ROOT(node))
		node->node_type = INTERNAL_NODE;

	bool move_rect = false;
	if (IS_FLOATING(node->client)) {
		move_rect = true;
	}

	node->first_child = create_node(node->client);
	if (node->first_child == NULL)
		return;

	if (node->is_master) {
		node->is_master				 = false;
		node->first_child->is_master = true;
	}
	if (node->is_focused) {
		node->is_focused			  = false;
		node->first_child->is_focused = true;
	}

	node->first_child->parent	 = node;
	node->first_child->node_type = EXTERNAL_NODE;
	node->client				 = NULL;

	if (move_rect) {
		node->first_child->floating_rectangle = node->floating_rectangle;
	}

	node->second_child = new_node;
	if (node->second_child == NULL)
		return;

	node->second_child->parent	  = node;
	node->second_child->node_type = EXTERNAL_NODE;

	if (layout == DEFAULT) {
		split_node(node, new_node);
	} else if (layout == STACK) {
		node->second_child->rectangle = node->first_child->rectangle =
			node->rectangle;

	} else if (layout == MASTER) {
		// todo
		node_t *p = find_tree_root(node);
		node_t *m = find_master_node(p);
		master_layout(p, m);
	}
}

static void
arrange_tree(node_t *tree, layout_t l)
{
	switch (l) {
	case DEFAULT: {
		default_layout(tree);
		break;
	}
	case MASTER: {
		node_t *m = find_master_node(tree);
		master_layout(tree, m);
		break;
	}
	case STACK: {
		stack_layout(tree);
		break;
	}
	case GRID: break;
	}
}

node_t *
find_node_by_window_id(node_t *root, xcb_window_t win)
{
	if (root == NULL)
		return NULL;

	if (root->client != NULL && root->client->window == win) {
		return root;
	}

	node_t *l = find_node_by_window_id(root->first_child, win);
	if (l != NULL)
		return l;

	node_t *r = find_node_by_window_id(root->second_child, win);
	if (r != NULL)
		return r;

	return NULL;
}

void
free_tree(node_t *root)
{
	if (root == NULL)
		return;

	if (root->client != NULL) {
		_FREE_(root->client);
	}

	node_t *f = root->first_child;
	free_tree(f);
	root->first_child = NULL;
	node_t *s		  = root->second_child;
	free_tree(s);
	root->second_child = NULL;
	_FREE_(root);
}

void
resize_subtree(node_t *parent)
{
	if (parent == NULL)
		return;

	rectangle_t r, r2 = {0};

	if (parent->rectangle.width >= parent->rectangle.height) {
		r.x		= parent->rectangle.x;
		r.y		= parent->rectangle.y;
		r.width = (parent->rectangle.width -
				   (conf.window_gap - conf.border_width)) /
				  2;
		r.height = parent->rectangle.height;

		r2.x = (int16_t)(parent->rectangle.x + r.width + conf.window_gap +
						 conf.border_width);
		r2.y = parent->rectangle.y;
		r2.width = parent->rectangle.width - r.width - conf.window_gap -
				   conf.border_width;
		r2.height = parent->rectangle.height;
	} else {
		r.x		 = parent->rectangle.x;
		r.y		 = parent->rectangle.y;
		r.width	 = parent->rectangle.width;
		r.height = (parent->rectangle.height -
					(conf.window_gap - conf.border_width)) /
				   2;

		r2.x = parent->rectangle.x;
		r2.y = (int16_t)(parent->rectangle.y + r.height + conf.window_gap +
						 conf.border_width);
		r2.width  = parent->rectangle.width;
		r2.height = parent->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
	}

	if (parent->first_child != NULL) {
		parent->first_child->rectangle = r;
		if (IS_INTERNAL(parent->first_child)) {
			resize_subtree(parent->first_child);
		}
	}

	if (parent->second_child != NULL) {
		parent->second_child->rectangle = r2;
		if (IS_INTERNAL(parent->second_child)) {
			resize_subtree(parent->second_child);
		}
	}
}

node_t *
find_master_node(node_t *root)
{
	if (root == NULL)
		return NULL;

	if (root->is_master)
		return root;

	node_t *l = find_master_node(root->first_child);
	if (l != NULL)
		return l;

	node_t *r = find_master_node(root->second_child);
	if (r != NULL)
		return r;

	return NULL;
}

static node_t *
find_floating_node(node_t *root)
{
	if (root == NULL)
		return NULL;

	if (IS_FLOATING(root->client))
		return root;

	node_t *l = find_floating_node(root->first_child);
	if (l != NULL)
		return l;

	node_t *r = find_floating_node(root->second_child);
	if (r != NULL)
		return r;

	return NULL;
}

static bool
is_sibling_floating(node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node)
								? parent->second_child
								: parent->first_child;

	return (sibling != NULL && sibling->client != NULL &&
			IS_FLOATING(sibling->client));
}

static void
populate_win_array(node_t *root, xcb_window_t *arr, size_t *index)
{
	if (root == NULL)
		return;

	if (root->client != NULL && root->client->window != XCB_NONE) {
		arr[*index] = root->client->window;
		(*index)++;
	}

	populate_win_array(root->first_child, arr, index);
	populate_win_array(root->second_child, arr, index);
}

static void
stack_and_lower(
	node_t *root, node_t **stack, int *top, int max_size, bool is_stacked)
{
	if (root == NULL)
		return;
	if (root->client != NULL && !IS_INTERNAL(root) &&
		IS_FLOATING(root->client)) {
		if (*top < max_size - 1) {
			stack[++(*top)] = root;
		} else {
			int		 size = max_size * 2;
			node_t **s	  = realloc(stack, sizeof(node_t *) * size);
			if (s == NULL) {
				_LOG_(ERROR, "cannot reallocate stack");
				return;
			}
			stack			= s;
			max_size		= size;
			stack[++(*top)] = root;
		}
	} else if (root->client != NULL && !IS_INTERNAL(root) &&
			   !IS_FLOATING(root->client)) { // non floating
		if (!is_stacked)
			lower_window(root->client->window);
	}
	stack_and_lower(root->first_child, stack, top, max_size, is_stacked);
	stack_and_lower(root->second_child, stack, top, max_size, is_stacked);
}

static void
sort(node_t **s, int n)
{
	for (int i = 0; i <= n; i++) {
		for (int j = i + 1; j <= n; j++) {
			int32_t area_i =
				s[i]->rectangle.height * s[i]->rectangle.width;
			int32_t area_j =
				s[j]->rectangle.height * s[j]->rectangle.width;
			if (area_j > area_i) {
				// swap
				node_t *temp = s[i];
				s[i]		 = s[j];
				s[j]		 = temp;
			}
		}
	}
}

void
restack(void)
{
	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return;

	node_t *root = cur_monitor->desktops[idx]->tree;
	if (root == NULL)
		return;

	int		 stack_size = 5;
	int		 top		= -1;

	node_t **stack		= (node_t **)malloc(sizeof(node_t *) * stack_size);

	if (stack == NULL) {
		_LOG_(ERROR, "cannot allocate stack");
		return;
	}

	stack_and_lower(root,
					stack,
					&top,
					stack_size,
					cur_monitor->desktops[idx]->layout == STACK);
    if (top == 0) {
  		if(stack[0]->client != NULL)
        	raise_window(stack[0]->client->window);
    } else if (top > 0) {
		sort(stack, top);
		for (int i = 1; i <= top; i++) {
			if (stack[i]->client && stack[i]->client->window &&
				stack[i - 1]->client && stack[i - 1]->client->window) {
				window_above(stack[i]->client->window,
							 stack[i - 1]->client->window);
			}
		}

#ifdef _DEBUG__
		char *s	 = win_name(stack[0]->client->window);
		char *ss = win_name(stack[top]->client->window);
		_LOG_(DEBUG,
			  "Largest floating window: %s, Smallest floating window: %s",
			  s,
			  ss);
		_FREE_(s);
		_FREE_(ss);
#endif
	}
	_FREE_(stack);
}

// TODO: this function fails to restack floating windows of different
// parents
// (fixed in restack(void))
void
restackv2(node_t *root)
{
	if (root == NULL) {
		return;
	}

	if (root->first_child != NULL && root->first_child->client != NULL &&
		IS_EXTERNAL(root->first_child)) {
		if (IS_FLOATING(root->first_child->client)) {
			if (root->second_child != NULL &&
				root->second_child->client != NULL &&
				IS_EXTERNAL(root->second_child) &&
				IS_FLOATING(root->second_child->client)) {
				window_below(root->first_child->client->window,
							 root->second_child->client->window);
			} else {
				raise_window(root->first_child->client->window);
			}
		} else {
			lower_window(root->first_child->client->window);
		}
	}

	if (root->second_child != NULL && root->second_child->client != NULL &&
		IS_EXTERNAL(root->second_child)) {
		if (IS_FLOATING(root->second_child->client)) {
			if (root->first_child == NULL ||
				root->first_child->client == NULL ||
				!IS_EXTERNAL(root->first_child) ||
				!IS_FLOATING(root->first_child->client)) {
				raise_window(root->second_child->client->window);
			}
		} else {
			lower_window(root->second_child->client->window);
		}
	}

	restackv2(root->first_child);
	restackv2(root->second_child);
}

static bool
has_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	const node_t *parent = node->parent;

	return (parent->first_child != NULL && parent->second_child != NULL);
}

static bool
has_internal_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	const node_t *parent = node->parent;

	return (parent->first_child != NULL && parent->second_child != NULL) &&
		   ((IS_INTERNAL(parent->first_child)) ||
			(IS_INTERNAL(parent->second_child)));
}

static bool
is_sibling_external(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node)
								? parent->second_child
								: parent->first_child;

	return (sibling != NULL && IS_EXTERNAL(sibling));
}

static node_t *
get_external_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	const node_t *parent = node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;

	return (sibling != NULL && IS_EXTERNAL(sibling)) ? sibling : NULL;
}

static bool
is_sibling_internal(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node)
								? parent->second_child
								: parent->first_child;

	return (sibling != NULL && IS_INTERNAL(sibling));
}

static node_t *
get_internal_sibling(node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	node_t *parent	= node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;

	return (sibling != NULL && IS_INTERNAL(sibling)) ? sibling : NULL;
}

static node_t *
get_sibling(node_t *node, node_type_t *type)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	node_t *parent	= node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;

	switch (*type) {
	case INTERNAL_NODE: {
		return (sibling != NULL && IS_INTERNAL(sibling)) ? sibling : NULL;
	}
	case EXTERNAL_NODE: {
		return (sibling != NULL && IS_EXTERNAL(sibling)) ? sibling : NULL;
	}
	case ROOT_NODE: break;
	}
	return NULL;
}

static bool
has_external_children(const node_t *parent)
{
	return (parent->first_child != NULL &&
			IS_EXTERNAL(parent->first_child)) &&
		   (parent->second_child != NULL &&
			IS_EXTERNAL(parent->second_child));
}

static node_t *
find_tree_root(node_t *node)
{
	if (IS_ROOT(node)) {
		return node;
	}
	return find_tree_root(node->parent);
}

static bool
has_single_external_child(const node_t *parent)
{
	if (parent == NULL)
		return false;

	return ((parent->first_child != NULL &&
			 parent->second_child != NULL) &&
			(IS_EXTERNAL(parent->first_child) &&
			 !IS_EXTERNAL(parent->second_child))) ||
		   ((parent->first_child != NULL &&
			 parent->second_child != NULL) &&
			(IS_EXTERNAL(parent->second_child) &&
			 !IS_EXTERNAL(parent->first_child)));
}

static client_t *
find_client_by_window_id(node_t *root, xcb_window_t win)
{
	if (root == NULL)
		return NULL;

	if (root->client != NULL && root->client->window == win) {
		return root->client;
	}

	node_t *l = find_node_by_window_id(root->first_child, win);
	if (l == NULL)
		return NULL;

	if (l->client != NULL) {
		return l->client;
	}

	node_t *r = find_node_by_window_id(root->second_child, win);
	if (r == NULL)
		return NULL;

	if (r->client != NULL) {
		return r->client;
	}

	return NULL;
}

void
apply_default_layout(node_t *root)
{
	_LOG_(INFO, "applying default layout to tree");
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {

		_LOG_(INFO, "returning here....");
		return;
	}

	rectangle_t r, r2 = {0};
	if (root->rectangle.width >= root->rectangle.height) {
		r.x		= root->rectangle.x;
		r.y		= root->rectangle.y;
		r.width = (root->rectangle.width -
				   (conf.window_gap - conf.border_width)) /
				  2;
		r.height = root->rectangle.height;

		r2.x = (int16_t)(root->rectangle.x + r.width + conf.window_gap +
						 conf.border_width);
		r2.y = root->rectangle.y;
		r2.width = root->rectangle.width - r.width - conf.window_gap -
				   conf.border_width;
		r2.height = root->rectangle.height;
	} else {
		r.x		 = root->rectangle.x;
		r.y		 = root->rectangle.y;
		r.width	 = root->rectangle.width;
		r.height = (root->rectangle.height -
					(conf.window_gap - conf.border_width)) /
				   2;

		r2.x = root->rectangle.x;
		r2.y = (int16_t)(root->rectangle.y + r.height + conf.window_gap +
						 conf.border_width);
		r2.width  = root->rectangle.width;
		r2.height = root->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
	}

	if (root->first_child != NULL) {
		root->first_child->rectangle =
			((root->second_child->client != NULL) &&
			 IS_FLOATING(root->second_child->client))
				? root->rectangle
			: ((root->first_child->client != NULL) &&
			   IS_FLOATING(root->first_child->client))
				? root->first_child->floating_rectangle
				: r;
		if (IS_INTERNAL(root->first_child)) {
			apply_default_layout(root->first_child);
		}
	}

	if (root->second_child != NULL) {
		root->second_child->rectangle =
			((root->first_child->client != NULL) &&
			 IS_FLOATING(root->first_child->client))
				? root->rectangle
			: ((root->second_child->client != NULL) &&
			   IS_FLOATING(root->second_child->client))
				? root->second_child->floating_rectangle
				: r2;
		if (IS_INTERNAL(root->second_child)) {
			apply_default_layout(root->second_child);
		}
	}
}

static void
default_layout(node_t *root)
{
	if (root == NULL)
		return;

	rectangle_t	   r = {0};
	uint16_t	   w = cur_monitor->rectangle.width;
	uint16_t	   h = cur_monitor->rectangle.height;
	const uint16_t x = cur_monitor->rectangle.x;
	const uint16_t y = cur_monitor->rectangle.y;
	if (wm->bar != NULL && cur_monitor == prim_monitor) {
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
	root->rectangle = r;
	apply_default_layout(root);
}

// TODO: use next_node() to implement this
void
apply_master_layout(node_t *parent)
{
	if (parent == NULL)
		return;

	if (parent->first_child->is_master) {
		parent->second_child->rectangle = parent->rectangle;
	} else if (parent->second_child->is_master) {
		parent->first_child->rectangle = parent->rectangle;
	} else {
		rectangle_t r, r2 = {0};
		r.x		 = parent->rectangle.x;
		r.y		 = parent->rectangle.y;
		r.width	 = parent->rectangle.width;
		r.height = (parent->rectangle.height -
					(conf.window_gap - conf.border_width)) /
				   2;

		r2.x = parent->rectangle.x;
		r2.y = (int16_t)(parent->rectangle.y + r.height + conf.window_gap +
						 conf.border_width);
		r2.width  = parent->rectangle.width;
		r2.height = parent->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
		parent->first_child->rectangle	= r;
		parent->second_child->rectangle = r2;
	}

	if (IS_INTERNAL(parent->first_child)) {
		apply_master_layout(parent->first_child);
	}

	if (IS_INTERNAL(parent->second_child)) {
		apply_master_layout(parent->second_child);
	}
}

static void
master_layout(node_t *root, node_t *n)
{
	const double   ratio		= 0.70;
	const uint16_t w			= cur_monitor->rectangle.width;
	const uint16_t h			= cur_monitor->rectangle.height;
	const uint16_t x			= cur_monitor->rectangle.x;
	const uint16_t y			= cur_monitor->rectangle.y;
	const uint16_t master_width = w * ratio;
	const uint16_t r_width		= (uint16_t)(w * (1 - ratio));
	const uint16_t bar_height =
		wm->bar == NULL ? 0 : wm->bar->rectangle.height;

	if (n == NULL) {
		n = find_any_leaf(root);
		if (n == NULL) {
			return;
		}
	}

	n->is_master   = true;

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

	if (n->node_type == ROOT_NODE && n->first_child == NULL &&
		n->second_child == NULL) {
		n->rectangle = (rectangle_t){
			.x		= x + conf.window_gap,
			.y		= (int16_t)(y + bar_height + conf.window_gap),
			.width	= (uint16_t)(w - 2 * conf.window_gap),
			.height = (uint16_t)(h - 2 * conf.window_gap - bar_height),
		};
		return;
	}

	n->rectangle	= r1;
	root->rectangle = r2;
	apply_master_layout(root);
}

static void
master_clean_up(node_t *root)
{
	if (root == NULL)
		return;

	if (root->is_master)
		root->is_master = false;
	master_clean_up(root->first_child);
	master_clean_up(root->second_child);
}

void
apply_stack_layout(node_t *root)
{
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {
		return;
	}

	if (root->first_child != NULL) {
		root->first_child->rectangle = root->rectangle;
		if (IS_INTERNAL(root->first_child)) {
			apply_stack_layout(root->first_child);
		}
	}

	if (root->second_child != NULL) {
		root->second_child->rectangle = root->rectangle;
		if (IS_INTERNAL(root->second_child)) {
			apply_stack_layout(root->second_child);
		}
	}
}

static void
stack_layout(node_t *root)
{
	rectangle_t	   r = {0};
	uint16_t	   w = cur_monitor->rectangle.width;
	uint16_t	   h = cur_monitor->rectangle.height;
	const uint16_t x = cur_monitor->rectangle.x;
	const uint16_t y = cur_monitor->rectangle.y;
	if (wm->bar != NULL && cur_monitor == prim_monitor) {
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
	root->rectangle = r;
	apply_stack_layout(root);
}

// void grid_layout(node_t *root) {
// }

void
apply_layout(desktop_t *d, layout_t t)
{
	d->layout	 = t;
	node_t *root = d->tree;
	master_clean_up(root);
	switch (t) {
	case DEFAULT: {
		default_layout(root);
		break;
	}
	case MASTER: {
		xcb_window_t win =
			get_window_under_cursor(wm->connection, wm->root_window);
		if (win == XCB_NONE || win == wm->root_window) {
			return;
		}
		node_t *n = find_node_by_window_id(root, win);
		/* node_t *n = get_focused_node(root); */
		if (n == NULL) {
			return;
		}
		master_layout(root, n);
		break;
	}
	case STACK: {
		xcb_window_t win =
			get_window_under_cursor(wm->connection, wm->root_window);

		if (win == XCB_NONE || win == wm->root_window) {
			return;
		}
		node_t *n = find_node_by_window_id(root, win);
		/* node_t *n = get_focused_node(root); */
		if (n == NULL) {
			return;
		}
		stack_layout(root);
		set_focus(n, true);
		break;
	}
	case GRID: {
		break;
	}
	}
}

static int
delete_node_with_external_sibling(node_t *node)
{
	/* node to delete = N, internal node = I, external node = E
	 *         I
	 *    	 /   \
	 *     N||E   N||E
	 *
	 * logic:
	 * just delete N and replace E with I and give it full I's
	 * rectangle
	 */
	node_t *external_node = NULL;
	assert(is_sibling_external(node));
	external_node = get_external_sibling(node);

	if (external_node == NULL) {
		_LOG_(ERROR, "external node is null");
		return -1;
	}

	// if 'I' has no parent
	if (node->parent->parent == NULL) {
		node->parent->node_type	   = ROOT_NODE;
		node->parent->client	   = external_node->client;
		node->parent->first_child  = NULL;
		node->parent->second_child = NULL;
	} else {
		// if 'I' has a parent
		/*
		 *         I
		 *    	 /   \
		 *   	E     I
		 *    	 	/   \
		 *   	   N     E
		 */
		node_t *grandparent = node->parent->parent;
		if (grandparent->first_child == node->parent) {
			grandparent->first_child->node_type	   = EXTERNAL_NODE;
			grandparent->first_child->client	   = external_node->client;
			grandparent->first_child->first_child  = NULL;
			grandparent->first_child->second_child = NULL;
		} else {
			grandparent->second_child->node_type   = EXTERNAL_NODE;
			grandparent->second_child->client	   = external_node->client;
			grandparent->second_child->first_child = NULL;
			grandparent->second_child->second_child = NULL;
		}
	}
	_FREE_(external_node);
	_FREE_(node->client);
	_FREE_(node);
	return 0;
}

static int
delete_node_with_internal_sibling(node_t *node, desktop_t *d)
{
	if (d == NULL) {
		return -1;
	}
	/* node to delete = N
	 * internal node (parent of N) = IN
	 * external node 1 = E1, external node 2 = E2
	 * internal node (parent of Es) = IE
	 *             IN
	 *       	 /   \
	 *          N     IE
	 *                / \
	 *              E1   E2
	 *
	 * logic: IN->IE = NULL, IN->N = NULL then link IN with E1,E2.
	 * lastly unlink IE->IN, IE->E1, IE->E2, N->IN and free N & IE
	 */
	node_t *internal_sibling = NULL;
	// if IN has no parent
	if (node->parent->parent == NULL) {
		if (is_sibling_internal(node)) {
			internal_sibling = get_internal_sibling(node);
		}

		if (internal_sibling == NULL) {
			_LOG_(ERROR, "internal node is null");
			return -1;
		}

		internal_sibling->rectangle = node->parent->rectangle;
		internal_sibling->parent	= NULL;
		internal_sibling->node_type = ROOT_NODE;
		if (d->tree == node->parent) {
			node->parent = NULL;
			if (d->tree->first_child == node) {
				d->tree->first_child = internal_sibling->first_child;
				internal_sibling->first_child->parent = d->tree;
				d->tree->second_child = internal_sibling->second_child;
				internal_sibling->second_child->parent = d->tree;
			} else {
				d->tree->second_child = internal_sibling->first_child;
				internal_sibling->first_child->parent = d->tree;
				d->tree->first_child = internal_sibling->second_child;
				internal_sibling->second_child->parent = d->tree;
			}
		}

		if (d->layout == DEFAULT) {
			apply_default_layout(d->tree);
		} else if (d->layout == STACK) {
			apply_stack_layout(d->tree);
		}

	} else {
		// if IN has a parent
		/*            ...
		 *              \
		 *              IN
		 *       	  /   \
		 *           N     IE
		 *                 / \
		 *               E1   E2
		 */
		if (is_sibling_internal(node)) {
			internal_sibling = get_internal_sibling(node);
		} else {
			_LOG_(ERROR, "internal node is null");
			return -1;
		}
		if (internal_sibling == NULL) {
			_LOG_(ERROR, "internal node is null");
			return -1;
		}
		internal_sibling->parent = NULL;
		if (node->parent->first_child == node) {
			node->parent->first_child = internal_sibling->first_child;
			internal_sibling->first_child->parent = node->parent;
			node->parent->second_child = internal_sibling->second_child;
			internal_sibling->second_child->parent = node->parent;
		} else {
			node->parent->second_child = internal_sibling->first_child;
			internal_sibling->first_child->parent = node->parent;
			node->parent->first_child = internal_sibling->second_child;
			internal_sibling->second_child->parent = node->parent;
		}
		resize_subtree(node->parent);
		if (d->layout == DEFAULT) {
			apply_default_layout(node->parent);
		} else if (d->layout == STACK) {
			apply_stack_layout(node->parent);
		}
	}

	_FREE_(internal_sibling);
	_FREE_(node->client);
	_FREE_(node);
	return 0;
}

static void
delete_floating_node(node_t *node, desktop_t *d)
{
	if (node == NULL || node->client == NULL || d == NULL) {
		_LOG_(ERROR, "node to be deleted is null");
		return;
	}

	assert(node->client->state == FLOATING);
#ifdef _DEBUG__
	_LOG_(DEBUG, "DELETE floating window %d", node->client->window);
#endif
	node_t *p = node->parent;
	if (p->first_child == node) {
		p->first_child = NULL;
	} else {
		p->second_child = NULL;
	}
	node->parent = NULL;
	_FREE_(node->client);
	_FREE_(node);
	assert(p->first_child == NULL);
	assert(p->second_child == NULL);
#ifdef _DEBUG__
	_LOG_(DEBUG, "DELETE floating window success");
#endif
	d->n_count -= 1;
}

// TODO: handle deletion in master layout
// TODO: handle deletion in stack layout
void
delete_node(node_t *node, desktop_t *d)
{
	if (node == NULL || node->client == NULL || d->tree == NULL) {
		_LOG_(ERROR, "node to be deleted is null");
		return;
	}

	if (IS_INTERNAL(node)) {
		_LOG_(ERROR,
			  "node to be deleted is not an external node type: %d",
			  node->node_type);
		return;
	}

	if (is_parent_null(node) && node != d->tree) {
		_LOG_(ERROR, "parent of node is null");
		return;
	}

	// early check if only single node/client is mapped to the screen
	if (node == d->tree) {
		_FREE_(node->client);
		_FREE_(node);
		d->tree = NULL;
		goto out;
	}

	/*
	 * case 1: node's parent has two external children
	 *         I
	 *    	 /   \
	 *   	E     E
	 */
	// if (is_sibling_external(node)) {
	// 	xcb_window_t w = node->client->window;
	// 	if (delete_node_with_external_sibling(node) != 0) {
	// 		_LOG_(ERROR, "cannot delete node with id: %d", w);
	// 		return;
	// 	}
	// 	goto out;
	// }

	/*
	 * case 2: node's parent has one internal child
	 * in other words, node has an internal sibling
	 *         I
	 *    	 /   \
	 *   	E     I
	 *             \
	 *            rest...
	 */
	// if (is_sibling_internal(node)) {
	// 	xcb_window_t w = node->client->window;
	// 	if (delete_node_with_internal_sibling(node, d) != 0) {
	// 		_LOG_(ERROR, "cannot delete node with id: %d", w);
	// 		return;
	// 	}
	// 	goto out;
	// }

	unlink_node(node, d);
	_FREE_(node->client);
	_FREE_(node);

out:
	d->n_count -= 1;
	if (!is_tree_empty(d->tree)) {
		arrange_tree(d->tree, d->layout);
	}
	return;
}

static bool
has_first_child(const node_t *parent)
{
	return parent->first_child != NULL;
}

static bool
has_second_child(const node_t *parent)
{
	return parent->second_child != NULL;
}

bool
is_tree_empty(const node_t *root)
{
	return root == NULL;
}

static bool
is_parent_null(const node_t *node)
{
	return node->parent == NULL;
}

static bool
is_parent_internal(const node_t *node)
{
	return node->parent->node_type == INTERNAL_NODE;
}

void
log_tree_nodes(node_t *node)
{
	if (node != NULL) {
		if (node->client != NULL) {
			xcb_icccm_get_text_property_reply_t t_reply;
			xcb_get_property_cookie_t			cn = xcb_icccm_get_wm_name(
				  wm->connection, node->client->window);
			uint8_t wr = xcb_icccm_get_wm_name_reply(
				wm->connection, cn, &t_reply, NULL);
			char name[256];
			if (wr == 1) {
				snprintf(name, sizeof(name), "%s", t_reply.name);
				xcb_icccm_get_text_property_reply_wipe(&t_reply);
			}
			_LOG_(DEBUG,
				  "Node Type: %d, Client Window ID: %u, name: %s, "
				  "is_focused %s",
				  node->node_type,
				  node->client->window,
				  name,
				  node->is_focused ? "true" : "false");
		} else {
			_LOG_(DEBUG, "Node Type: %d", node->node_type);
		}
		log_tree_nodes(node->first_child);
		log_tree_nodes(node->second_child);
	}
}

int
hide_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (!IS_INTERNAL(cn) && cn->client != NULL) {
		if (set_visibility(cn->client->window, false) != 0) {
			return -1;
		}

		if (set_focus(cn, false) != 0) {
			return -1;
		}

		if (!conf.focus_follow_pointer) {
			window_grab_buttons(cn->client->window);
		}
	}

	hide_windows(cn->first_child);
	hide_windows(cn->second_child);

	return 0;
}

int
show_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (!IS_INTERNAL(cn) && cn->client != NULL) {
		if (set_visibility(cn->client->window, true) != 0) {
			return -1;
		}
	}

	show_windows(cn->first_child);
	show_windows(cn->second_child);

	return 0;
}

bool
client_exist(node_t *cn, xcb_window_t win)
{
	if (cn == NULL)
		return false;

	if (cn->client != NULL) {
		if (cn->client->window == win) {
			return true;
		}
	}

	if (client_exist(cn->first_child, win)) {
		return true;
	}

	if (client_exist(cn->second_child, win)) {
		return true;
	}

	return false;
}

static bool
in_left_subtree(node_t *lc, node_t *n)
{
	if (lc == NULL)
		return false;

	if (lc == n || lc->first_child == n || lc->second_child == n) {
		return true;
	}

	if (in_left_subtree(lc->first_child, n)) {
		return true;
	}

	if (in_left_subtree(lc->second_child, n)) {
		return true;
	}

	return false;
}

static bool
in_right_subtree(node_t *rc, node_t *n)
{
	if (rc == NULL)
		return false;

	if (rc == n || rc->first_child == n || rc->second_child == n) {
		return true;
	}

	if (in_right_subtree(rc->first_child, n)) {
		return true;
	}

	if (in_right_subtree(rc->second_child, n)) {
		return true;
	}

	return false;
}

int
horizontal_resize_wrapper(arg_t *arg)
{
	const int i = get_focused_desktop_idx();
	if (i == -1)
		return -1;

	if (cur_monitor->desktops[i]->layout == STACK) {
		return 0;
	}

	node_t *root = cur_monitor->desktops[i]->tree;
	if (root == NULL)
		return -1;

	node_t *n = get_focused_node(root);
	if (n == NULL)
		return -1;
	// todo: if node was flipped, reize up or down instead
	grab_pointer(wm->root_window,
				 false); // steal the pointer and prevent it from sending
						 // enter_notify events (which focuses the window
						 // being under cursor as the resize happens);
	horizontal_resize(n, arg->r);
	render_tree(root);
	ungrab_pointer();
	return 0;
}

static void
horizontal_resize(node_t *n, resize_t t)
{
	const int16_t px			   = 5;
	direction_t	  grow_direction   = NONE;
	direction_t	  shrink_direction = NONE;

	node_t		 *root			   = find_tree_root(n);

	if (in_left_subtree(root->first_child, n)) {
		grow_direction	 = RIGHT;
		shrink_direction = LEFT;
	} else if (in_right_subtree(root->second_child, n)) {
		grow_direction	 = LEFT;
		shrink_direction = RIGHT;
	}

	_LOG_(INFO, "dir == %d", grow_direction);

	if (n->parent == NULL || IS_ROOT(n)) {
		return;
	}

	/*
	 * case 1: node's parent is the root.
	 * I = Internal node
	 * E = external node
	 *
	 * I's type is ROOT_NODE
	 * Node to delete is E.
	 * logic: grow E's width by 5 pixels
	 * then move E's sibling x by 5 pixels and shrink its width by 5
	 * pixels E's sibling can be External or Internal -> if internal,
	 * resize its children
	 *         I
	 *    	 /   \
	 *   	E     E/I
	 *             \
	 *            rest...
	 */
	if (IS_ROOT(n->parent)) {
		node_t *s = NULL;
		if (is_sibling_external(n)) {
			s = get_external_sibling(n);
			if (s == NULL)
				return;

			if (t == GROW) {
				n->rectangle.width += px;
				s->rectangle.width -= px;
				grow_direction == RIGHT ? (s->rectangle.x += px)
										: (n->rectangle.x -= px);
			} else {
				// shrink
				n->rectangle.width -= px;
				s->rectangle.width += px;
				(shrink_direction == LEFT) ? (s->rectangle.x -= px)
										   : (n->rectangle.x += px);
			}
		} else {
			// sibling is internal
			s = get_internal_sibling(n);
			if (s == NULL) {
				_LOG_(ERROR, "internal sibling is null");
				return;
			}
			if (t == GROW) {
				n->rectangle.width += px;
				s->rectangle.width -= px;
				grow_direction == RIGHT ? (s->rectangle.x += px)
										: (n->rectangle.x -= px);
				resize_subtree(s);
			} else {
				n->rectangle.width -= px;
				s->rectangle.width += px;
				(shrink_direction == LEFT) ? (s->rectangle.x -= px)
										   : (n->rectangle.x += px);
				resize_subtree(s);
			}
		}
	}
	/*
	 * case 2: node's parent is not the root (it is INTERNAL_NODE).
	 * I = Internal node
	 * E = external node
	 *
	 * I's type is INTERNAL_NODE
	 * Node to expand is E.
	 * logic: grow the whole subtree(rectangle)'s width in E's side by
	 * 5 pixels then move the opposite subtree(rectangle)'s x by 5
	 * pixels and shrink its width by 5 pixels
	 *         I
	 *    	 /   \
	 *   	I     E/I
	 *    /   \     \
	 *   E    E/I   rest...
	 */
	if (IS_INTERNAL(n->parent)) {
		if (in_left_subtree(root->first_child, n)) {
			if (root->first_child == NULL ||
				!IS_INTERNAL(root->first_child)) {
				return;
			}
			(t == GROW) ? (root->first_child->rectangle.width += px)
						: (root->first_child->rectangle.width -= px);
			(t == GROW) ? (root->second_child->rectangle.width -= px)
						: (root->second_child->rectangle.width += px);
			(t == GROW) ? (root->second_child->rectangle.x += px)
						: (root->second_child->rectangle.x -= px);

			if (root->second_child != NULL &&
				IS_EXTERNAL(root->second_child)) {
				resize_subtree(root->first_child);
			} else {
				resize_subtree(root->first_child);
				resize_subtree(root->second_child);
			}
		} else {
			if (root->second_child == NULL ||
				!IS_INTERNAL(root->second_child)) {
				return;
			}
			(t == GROW) ? (root->second_child->rectangle.width += px)
						: (root->second_child->rectangle.width -= px);
			(t == GROW) ? (root->second_child->rectangle.x -= px)
						: (root->second_child->rectangle.x += px);
			(t == GROW) ? (root->first_child->rectangle.width -= px)
						: (root->first_child->rectangle.width += px);

			if (root->first_child != NULL &&
				IS_EXTERNAL(root->first_child)) {
				resize_subtree(root->second_child);
			} else {
				resize_subtree(root->first_child);
				resize_subtree(root->second_child);
			}
		}
	}
}

node_t *
find_left_leaf(node_t *root)
{
	if (root == NULL)
		return NULL;

	if ((root->node_type != INTERNAL_NODE || root->parent == NULL) &&
		root->client != NULL) {
		return root;
	}

	node_t *left_leaf = find_left_leaf(root->first_child);

	if ((left_leaf != NULL && left_leaf->client != NULL) &&
		(IS_EXTERNAL(left_leaf) || IS_ROOT(left_leaf))) {
		return left_leaf;
	}

	return find_left_leaf(root->second_child);
}

node_t *
find_any_leaf(node_t *root)
{
	if (root == NULL)
		return NULL;

	if ((root->node_type != INTERNAL_NODE || root->parent == NULL) &&
		root->client != NULL && !IS_FLOATING(root->client)) {
		return root;
	}

	node_t *f = find_any_leaf(root->first_child);
	if (f != NULL && f->client != NULL &&
		!IS_FLOATING(f->client)) {
		return f;
	}

	node_t *s = find_any_leaf(root->second_child);
	if (s != NULL && s->client != NULL &&
		!IS_FLOATING(s->client)) {
		return s;
	}

	return NULL;
}

void
unlink_node(node_t *node, desktop_t *d)
{
	if (node == NULL)
		return;

	if (d->tree == node) {
		d->tree = NULL;
		return;
	}

	/* node to unlink = N, internal node = I, external node = E
	 *         I
	 *    	 /   \
	 *     N||E   N||E
	 *
	 * logic:
	 * just unlink N and replace E with I and give it full I's
	 * rectangle
	 */
	if (is_sibling_external(node)) {
		node_t *e = get_external_sibling(node);
		if (e == NULL) {
			return;
		}
		// if I has no parent
		if (IS_ROOT(node->parent)) {
			node->parent->client	   = e->client;
			node->parent->first_child  = NULL;
			node->parent->second_child = NULL;
		} else {
			/* if I has a prent
			 *         I
			 *    	 /   \
			 *   	E    [I]
			 *    	 	/   \
			 *   	   N     E
			 */
			node_t *g = node->parent->parent;
			if (g->first_child == node->parent) {
				g->first_child->node_type	 = EXTERNAL_NODE;
				g->first_child->client		 = e->client;
				g->first_child->first_child	 = NULL;
				g->first_child->second_child = NULL;
			} else {
				g->second_child->node_type	  = EXTERNAL_NODE;
				g->second_child->client		  = e->client;
				g->second_child->first_child  = NULL;
				g->second_child->second_child = NULL;
			}
		}
		e->parent = NULL;
		_FREE_(e);
		node->parent = NULL;
		return;
	}
	/*
	 * Node to be unlinked: N
	 * Parent of N (internal node): IN
	 * External nodes: E1 and E2
	 * Parent of E1 and E2 (internal node): IE
	 *
	 *            IN
	 *           /  \
	 *          N    IE
	 *              /  \
	 *            E1    E2
	 *
	 * Logic:
	 * - If N is in the left subtree, unlink N, delete IN and replace
	 * IN with IE.
	 * - If N is in the right subtree, unlink N delete IN and replace
	 * IN with IE.
	 *
	 * Essentially, both IN and N are deleted. IE takes the place of
	 * IN, and the layout of IE's children (E1 and E2) is adjusted
	 * accordingly.
	 */
	if (is_sibling_internal(node)) {
		node_t *n = NULL;
		// if IN has no parent
		if (IS_ROOT(node->parent)) {
			n = get_internal_sibling(node);
			if (n == NULL) {
				_LOG_(ERROR, "internal node is null");
				return;
			}
			if (d->tree == node->parent) {
				d->tree->first_child	= n->first_child;
				n->first_child->parent	= d->tree;
				d->tree->second_child	= n->second_child;
				n->second_child->parent = d->tree;
			} else {
				d->tree->second_child	= n->first_child;
				n->first_child->parent	= d->tree;
				d->tree->first_child	= n->second_child;
				n->second_child->parent = d->tree;
			}
		} else {
			// if IN has a parent
			/*            ...
			 *              \
			 *              IN
			 *       	  /   \
			 *           N     IE
			 *                 / \
			 *               E1   E2
			 */
			if (is_sibling_internal(node)) {
				n = get_internal_sibling(node);
			} else {
				_LOG_(ERROR, "internal node is null");
				return;
			}
			if (n == NULL)
				return;

			if (node->parent->first_child == node) {
				node->parent->first_child  = n->first_child;
				n->first_child->parent	   = node->parent;
				node->parent->second_child = n->second_child;
				n->second_child->parent	   = node->parent;
			} else {
				node->parent->second_child = n->first_child;
				n->first_child->parent	   = node->parent;
				node->parent->first_child  = n->second_child;
				n->second_child->parent	   = node->parent;
			}
		}
		n->parent		= NULL;
		n->first_child	= NULL;
		n->second_child = NULL;
		node->parent	= NULL;
		_FREE_(n);
	}
}

int
transfer_node_wrapper(arg_t *arg)
{
	xcb_window_t w =
		get_window_under_cursor(wm->connection, wm->root_window);
	const int i = arg->idx;
	int		  d = get_focused_desktop_idx();

	if (w == wm->root_window)
		return 0;

	if (d == -1)
		return d;

	if (is_tree_empty(cur_monitor->desktops[d]->tree)) {
		return 0;
	}

	node_t *root = cur_monitor->desktops[d]->tree;
	/* node_t *node = find_node_by_window_id(root, w); */

	node_t *node = get_focused_node(root);
	if (d == i) {
		_LOG_(INFO, "switch node to curr desktop... abort");
		return 0;
	}

	desktop_t *nd = cur_monitor->desktops[i];
	desktop_t *od = cur_monitor->desktops[d];
#ifdef _DEBUG__
	_LOG_(DEBUG, "new desktop = %d, old desktop = %d", i + 1, d + 1);
#endif
	if (set_visibility(node->client->window, false) != 0) {
		_LOG_(ERROR, "cannot hide window %d", node->client->window);
		return -1;
	}
	unlink_node(node, od);
	transfer_node(node, nd);

	if (nd->layout == STACK) {
		set_focus(node, true);
	}

	od->n_count--;
	nd->n_count++;

	if (render_tree(od->tree) != 0) {
		_LOG_(ERROR, "cannot render tree");
		return -1;
	}
	// arrange_tree(nd->tree, nd->layout);
	return 0;
}

// TODO: handle transfer in master layout
// TODO: handle transfer in stack layout
void
transfer_node(node_t *node, desktop_t *d)
{
	if (node == NULL || d == NULL) {
		return;
	}

	if (node->client == NULL) {
		return;
	}

	assert(node->parent == NULL);

	if (is_tree_empty(d->tree)) {
		rectangle_t	   r = {0};
		// uint16_t	w = wm->screen->width_in_pixels;
		// uint16_t	h = wm->screen->height_in_pixels;
		uint16_t	   w = cur_monitor->rectangle.width;
		uint16_t	   h = cur_monitor->rectangle.height;
		const uint16_t x = cur_monitor->rectangle.x;
		const uint16_t y = cur_monitor->rectangle.y;
		if (wm->bar != NULL && cur_monitor == prim_monitor) {
			r.x		 = x + conf.window_gap;
			r.y		 = y + wm->bar->rectangle.height + conf.window_gap;
			r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
			r.height = h - wm->bar->rectangle.height -
					   2 * conf.window_gap - 2 * conf.border_width;
		} else {
			r.x		 = x + conf.window_gap;
			r.y		 = y + conf.window_gap;
			r.width	 = w - 2 * conf.window_gap - 2 * conf.border_width;
			r.height = h - 2 * conf.window_gap - 2 * conf.border_width;
		}
		node->node_type	   = ROOT_NODE;
		d->tree			   = node;
		d->tree->rectangle = r;
		d->tree->node_type = ROOT_NODE;
	} else if (d->tree->first_child == NULL &&
			   d->tree->second_child == NULL) {
		client_t *c = d->tree->client;
		if ((d->tree->first_child = create_node(c)) == NULL) {
			return;
		}
		d->tree->first_child->node_type	 = EXTERNAL_NODE;
		d->tree->second_child			 = node;
		d->tree->second_child->node_type = EXTERNAL_NODE;
		d->tree->client					 = NULL;
		d->tree->first_child->parent	 = d->tree->second_child->parent =
			d->tree;
	} else {
		node_t *leaf = find_any_leaf(d->tree);
		if (leaf == NULL) {
			return;
		}
		if (!IS_ROOT(leaf)) {
			leaf->node_type = INTERNAL_NODE;
		}
		leaf->first_child = create_node(leaf->client);
		if (leaf->first_child == NULL) {
			return;
		}
		leaf->first_child->parent	 = leaf;
		leaf->first_child->node_type = EXTERNAL_NODE;
		leaf->client				 = NULL;

		leaf->second_child			 = node;
		if (leaf->second_child == NULL) {
			return;
		}
		leaf->second_child->parent	  = leaf;
		leaf->second_child->node_type = EXTERNAL_NODE;
	}
	arrange_tree(d->tree, d->layout);
}

bool
has_floating_window(node_t *root)
{
	if (root == NULL)
		return false;

	if (root->client != NULL && IS_FLOATING(root->client)) {
		return true;
	}

	if (has_floating_window(root->first_child)) {
		return true;
	}

	if (has_floating_window(root->second_child)) {
		return true;
	}

	return false;
}

node_t *
next_node(node_t *n)
{
	if (n == NULL)
		return NULL;

	if (n->parent && n->parent->second_child != n) {
		node_t *l = n->parent->second_child;
		while (!IS_EXTERNAL(l)) {
			l = l->first_child;
		}
		return l;
	}

	node_t *c = n;
	node_t *p = c->parent;
	while (p && p->second_child == c) {
		c = p;
		p = c->parent;
	}

	if (p == NULL)
		return NULL;

	node_t *r = p->second_child;
	while (!IS_EXTERNAL(r)) {
		r = r->first_child;
	}
	return r;
}

node_t *
prev_node(node_t *n)
{
	if (n == NULL)
		return NULL;

	if (n->parent && n->parent->first_child != n) {
		node_t *l = n->parent->first_child;
		while (!IS_EXTERNAL(l)) {
			if (l->second_child) {
				l = l->second_child;
			} else {
				l = l->first_child;
			}
		}
		return l;
	}

	node_t *c = n;
	node_t *p = c->parent;
	while (p && p->first_child == c) {
		c = p;
		p = c->parent;
	}

	if (p == NULL)
		return NULL;

	node_t *l = p->first_child;
	while (!IS_EXTERNAL(l)) {
		if (l->second_child) {
			l = l->second_child;
		} else {
			l = l->first_child;
		}
	}
	return l;
}

void
flip_node(node_t *node)
{
	if (node->parent == NULL) {
		return;
	}
	flip_t	flip = (node->rectangle.width >= node->rectangle.height)
					   ? VERTICAL_FLIP
					   : HORIZONTAL_FLIP;
	node_t *p	 = node->parent;
	node_t *s =
		(p->first_child == node) ? p->second_child : p->first_child;

	if (s == NULL)
		return;

	node->rectangle.x = p->rectangle.x;
	node->rectangle.y = p->rectangle.y;
	node->rectangle.width =
		(flip == VERTICAL_FLIP)
			? ((p->rectangle.width - conf.window_gap) / 2)
			: p->rectangle.width;
	node->rectangle.height =
		(flip == VERTICAL_FLIP)
			? p->rectangle.height
			: (p->rectangle.height - conf.window_gap) / 2;
	s->rectangle.x =
		(flip == VERTICAL_FLIP)
			? ((int16_t)((p->rectangle.x + node->rectangle.width) +
						 conf.window_gap))
			: p->rectangle.x;
	s->rectangle.y =
		(flip == VERTICAL_FLIP)
			? p->rectangle.y
			: ((int16_t)((p->rectangle.y + node->rectangle.height) +
						 conf.window_gap));
	s->rectangle.width =
		(flip == VERTICAL_FLIP)
			? ((p->rectangle.width - node->rectangle.width) -
			   conf.window_gap)
			: p->rectangle.width;
	s->rectangle.height =
		(flip == VERTICAL_FLIP)
			? p->rectangle.height
			: ((p->rectangle.height - node->rectangle.height) -
			   conf.window_gap);

	if (s->node_type == INTERNAL_NODE) {
		resize_subtree(s);
	}
}

void
update_focus(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client != NULL;
	if (flag && root != n) {
		set_focus(root, false);
		if (!conf.focus_follow_pointer)
			window_grab_buttons(root->client->window);
		root->is_focused = false;
	}

	update_focus(root->first_child, n);
	update_focus(root->second_child, n);
}

void
update_focus_all(node_t *root)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client != NULL;
	if (flag) {
		set_focus(root, false);
		if (!conf.focus_follow_pointer)
			window_grab_buttons(root->client->window);
		root->is_focused = false;
	}

	update_focus_all(root->first_child);
	update_focus_all(root->second_child);
}

node_t *
get_focused_node(node_t *n)
{
	if (n == NULL)
		return NULL;

	if (!IS_INTERNAL(n) && n->client != NULL && n->is_focused) {
		return n;
	}

	node_t *l = get_focused_node(n->first_child);
	if (l != NULL) {
		return l;
	}
	node_t *s = get_focused_node(n->second_child);
	if (s != NULL) {
		return s;
	}

	return NULL;
}

int
swap_node(node_t *n)
{
	if (n->parent == NULL)
		return -1;

	node_t	   *s = (n->parent->first_child == n) ? n->parent->second_child
												  : n->parent->first_child;
	rectangle_t sr = s->rectangle;
	rectangle_t nr = n->rectangle;
	n->rectangle   = sr;
	s->rectangle   = nr;

	if (IS_INTERNAL(s)) {
		resize_subtree(s);
	}

	return 0;
}

static bool
is_within_range(rectangle_t *rect1, rectangle_t *rect2, direction_t d)
{
	switch (d) {
	case LEFT:
		return rect2->x + rect2->width <= rect1->x &&
			   rect1->y < rect2->y + rect2->height &&
			   rect1->y + rect1->height > rect2->y;
	case RIGHT:
		return rect2->x >= rect1->x + rect1->width &&
			   rect1->y < rect2->y + rect2->height &&
			   rect1->y + rect1->height > rect2->y;
	case UP:
		return rect2->y + rect2->height <= rect1->y &&
			   rect1->x < rect2->x + rect2->width &&
			   rect1->x + rect1->width > rect2->x;
	case DOWN:
		return rect2->y >= rect1->y + rect1->height &&
			   rect1->x < rect2->x + rect2->width &&
			   rect1->x + rect1->width > rect2->x;
	default: return false;
	}
}

static node_t *
find_closest_neighbor(node_t *root, node_t *node, direction_t d)
{
	if (root == NULL)
		return NULL;

	node_t *closest			 = NULL;
	int		closest_distance = INT16_MAX;

	node_t *queue[50];
	int		front = 0, rear = 0;
	queue[rear++] = root;

	while (front < rear) {
		node_t *current = queue[front++];

		if (current == node)
			continue;
		if (IS_EXTERNAL(current) && current->client != NULL &&
			is_within_range(&node->rectangle, &current->rectangle, d)) {
			int distance;
			switch (d) {
			case LEFT:
				distance = node->rectangle.x - (current->rectangle.x +
												current->rectangle.width);
				break;
			case RIGHT:
				distance = current->rectangle.x -
						   (node->rectangle.x + node->rectangle.width);
				break;
			case UP:
				distance = node->rectangle.y - (current->rectangle.y +
												current->rectangle.height);
				break;
			case DOWN:
				distance = current->rectangle.y -
						   (node->rectangle.y + node->rectangle.height);
				break;
			default: distance = INT16_MAX; break;
			}
			if (distance < closest_distance) {
				closest_distance = distance;
				closest			 = current;
			}
		}

		if (current->first_child != NULL)
			queue[rear++] = current->first_child;
		if (current->second_child != NULL)
			queue[rear++] = current->second_child;
	}

	return closest;
}

node_t *
cycle_win(node_t *node, direction_t d)
{
	node_t *root = find_tree_root(node);
	if (root == NULL) {
		_LOG_(ERROR, "could not find root of tree");
		return NULL;
	}

	node_t *neighbor = find_closest_neighbor(root, node, d);
	if (neighbor == NULL) {
		_LOG_(ERROR, "could not find neighbor node");
		return NULL;
	}

	return neighbor;
}

static bool
is_closer_node(node_t	  *current,
			   node_t	  *new_node,
			   node_t	  *node,
			   direction_t d)
{
	if (current == NULL)
		return true;

	switch (d) {
	case LEFT:
		return new_node->rectangle.x > current->rectangle.x &&
			   new_node->rectangle.x < node->rectangle.x;
	case RIGHT:
		return new_node->rectangle.x < current->rectangle.x &&
			   new_node->rectangle.x > node->rectangle.x;
	case UP:
		return new_node->rectangle.y > current->rectangle.y &&
			   new_node->rectangle.y < node->rectangle.y;
	case DOWN:
		return new_node->rectangle.y < current->rectangle.y &&
			   new_node->rectangle.y > node->rectangle.y;
	default: return false;
	}
}

static node_t *
find_neighbor(node_t *root, node_t *node, direction_t d)
{
	if (root == NULL || root == node)
		return NULL;

	node_t *best_node = NULL;

	if (root->client != NULL) {
		switch (d) {
		case LEFT:
			if (root->rectangle.x < node->rectangle.x &&
				root->rectangle.x + root->rectangle.width <=
					node->rectangle.x &&
				(node->rectangle.y <
					 root->rectangle.y + root->rectangle.height &&
				 node->rectangle.y + node->rectangle.height >
					 root->rectangle.y)) {
				if (is_closer_node(best_node, root, node, d)) {
					best_node = root;
				}
			}
			break;
		case RIGHT:
			if (root->rectangle.x >
					node->rectangle.x + node->rectangle.width &&
				(node->rectangle.y <
					 root->rectangle.y + root->rectangle.height &&
				 node->rectangle.y + node->rectangle.height >
					 root->rectangle.y)) {
				if (is_closer_node(best_node, root, node, d)) {
					best_node = root;
				}
			}
			break;
		case UP:
			if (root->rectangle.y < node->rectangle.y &&
				root->rectangle.y + root->rectangle.height <=
					node->rectangle.y &&
				(node->rectangle.x <
					 root->rectangle.x + root->rectangle.width &&
				 node->rectangle.x + node->rectangle.width >
					 root->rectangle.x)) {
				if (is_closer_node(best_node, root, node, d)) {
					best_node = root;
				}
			}
			break;
		case DOWN:
			if (root->rectangle.y >
					node->rectangle.y + node->rectangle.height &&
				(node->rectangle.x <
					 root->rectangle.x + root->rectangle.width &&
				 node->rectangle.x + node->rectangle.width >
					 root->rectangle.x)) {
				if (is_closer_node(best_node, root, node, d)) {
					best_node = root;
				}
			}
			break;
		default: break;
		}
	}

	if (root->first_child != NULL) {
		node_t *child_result = find_neighbor(root->first_child, node, d);
		if (child_result != NULL &&
			is_closer_node(best_node, child_result, node, d)) {
			best_node = child_result;
		}
	}

	if (root->second_child != NULL) {
		node_t *child_result = find_neighbor(root->second_child, node, d);
		if (child_result != NULL &&
			is_closer_node(best_node, child_result, node, d)) {
			best_node = child_result;
		}
	}

	return best_node;
}
