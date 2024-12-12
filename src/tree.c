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
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "helper.h"
#include "queue.h"
#include "type.h"
#include "zwm.h"

/* clang-format off */
static void master_layout(node_t *parent, node_t *);
static void stack_layout(node_t *parent);
static void default_layout(node_t *parent);
static node_t *find_tree_root(node_t *);
static bool is_parent_null(const node_t *node);
/* clang-format on */

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

/* render tree - apply visual changes to the screen.
 * It's called whenever a window maps/unmaps, or when a layout changes or visual
 * effects need to take place.
 * It is being used extensively in this code base.
 * Note: this function assumes rectangles and positions to be pre-calculated.
 */
int
render_tree(node_t *node)
{
	if (!node)
		return 0;

	queue_t *q = create_queue();
	if (!q) {
		_LOG_(ERROR, "queue creation failed");
		return -1;
	}

	enqueue(q, node);
	while (q->front) {
		node_t *current = dequeue(q);
		if (!IS_INTERNAL(current) && current->client) {
			if (!IS_FULLSCREEN(current->client)) {
				if (tile(current) != 0) {
					_LOG_(ERROR,
						  "error tiling window %d",
						  current->client->window);
					free_queue(q);
					return -1;
				}
			}
			continue;
		}
		if (current->first_child)
			enqueue(q, current->first_child);
		if (current->second_child)
			enqueue(q, current->second_child);
	}
	free_queue(q);
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
	return (parent->first_child && parent->first_child->client &&
			IS_FLOATING(parent->first_child->client)) ||
		   (parent->second_child && parent->second_child->client &&
			IS_FLOATING(parent->second_child->client));
}

static node_t *
get_floating_child(const node_t *parent)
{
	if (parent->first_child && parent->first_child->client &&
		IS_FLOATING(parent->first_child->client)) {
		return parent->first_child;
	}

	if (parent->second_child && parent->second_child->client &&
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

/* split_node - splits a node's rectangle in half, the split could be vertical
 * or horizontal depending on (width > height)? */
static void
split_node(node_t *n, node_t *nd)
{
	if (IS_FLOATING(nd->client)) {
		n->first_child->rectangle = n->first_child->floating_rectangle =
			n->rectangle;
	} else {
		if (n->rectangle.width >= n->rectangle.height) {
			n->first_child->rectangle.x = n->rectangle.x;
			n->first_child->rectangle.y = n->rectangle.y;
			n->first_child->rectangle.width =
				(n->rectangle.width - (conf.window_gap - conf.border_width)) /
				2;
			n->first_child->rectangle.height = n->rectangle.height;
			if (!IS_FLOATING(n->first_child->client)) {
				n->second_child->rectangle.x =
					(int16_t)(n->rectangle.x + n->first_child->rectangle.width +
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
				(n->rectangle.height - (conf.window_gap - conf.border_width)) /
				2;
			if (!IS_FLOATING(n->first_child->client)) {
				n->second_child->rectangle.x = n->rectangle.x;
				n->second_child->rectangle.y =
					(int16_t)(n->rectangle.y +
							  n->first_child->rectangle.height +
							  conf.window_gap + conf.border_width);
				n->second_child->rectangle.width = n->rectangle.width;
				n->second_child->rectangle.height =
					n->rectangle.height - n->first_child->rectangle.height -
					conf.window_gap - conf.border_width;
			} else {
				n->second_child->rectangle = n->rectangle;
			}
		}
	}
}

/* insert_node - change the given focused node type to be internal, and then
 * inserts a new node as its child, along with the current node's client as
 * another child. Both children share the parent node's rectangle. */
void
insert_node(node_t *node, node_t *new_node, layout_t layout)
{
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "node to split %d, node to insert %d",
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

	/* change the node type to INTERNAL if it isn't ROOT */
	if (!IS_ROOT(node))
		node->node_type = INTERNAL_NODE;

	/* check if the node is floating and should retain its rectangle*/
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

/* arrange_tree - applies layout changes to a given tree */
void
arrange_tree(node_t *tree, layout_t l)
{
	if (!tree) {
		return;
	}

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

	if (root->client && root->client->window == win) {
		return root;
	}

	node_t *l = find_node_by_window_id(root->first_child, win);
	if (l)
		return l;

	node_t *r = find_node_by_window_id(root->second_child, win);
	if (r)
		return r;

	return NULL;
}

void
free_tree(node_t *root)
{
	if (root == NULL) {
		return;
	}
	free_tree(root->first_child);
	free_tree(root->second_child);
	_FREE_(root->client);
	_FREE_(root);
}

/* resize_subtree - recursively resizes the subtree of a given parent node based
 * on the parent's rectangle dimensions. */
void
resize_subtree(node_t *parent)
{
	if (parent == NULL)
		return;

	rectangle_t r, r2 = {0};

	if (parent->rectangle.width >= parent->rectangle.height) {
		r.x = parent->rectangle.x;
		r.y = parent->rectangle.y;
		r.width =
			(parent->rectangle.width - (conf.window_gap - conf.border_width)) /
			2;
		r.height = parent->rectangle.height;
		r2.x	 = (int16_t)(parent->rectangle.x + r.width + conf.window_gap +
						 conf.border_width);
		r2.y	 = parent->rectangle.y;
		r2.width = parent->rectangle.width - r.width - conf.window_gap -
				   conf.border_width;
		r2.height = parent->rectangle.height;
	} else {
		r.x		= parent->rectangle.x;
		r.y		= parent->rectangle.y;
		r.width = parent->rectangle.width;
		r.height =
			(parent->rectangle.height - (conf.window_gap - conf.border_width)) /
			2;
		r2.x	  = parent->rectangle.x;
		r2.y	  = (int16_t)(parent->rectangle.y + r.height + conf.window_gap +
						  conf.border_width);
		r2.width  = parent->rectangle.width;
		r2.height = parent->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
	}

	if (parent->first_child) {
		parent->first_child->rectangle = r;
		if (IS_INTERNAL(parent->first_child)) {
			resize_subtree(parent->first_child);
		}
	}

	if (parent->second_child) {
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
	if (l)
		return l;

	node_t *r = find_master_node(root->second_child);
	if (r)
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
	if (l)
		return l;

	node_t *r = find_floating_node(root->second_child);
	if (r)
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
	const node_t *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;
	return (sibling && sibling->client && IS_FLOATING(sibling->client));
}

static void
populate_win_array(node_t *root, xcb_window_t *arr, size_t *index)
{
	if (root == NULL)
		return;

	if (root->client && root->client->window != XCB_NONE) {
		arr[*index] = root->client->window;
		(*index)++;
	}
	populate_win_array(root->first_child, arr, index);
	populate_win_array(root->second_child, arr, index);
}

/* stack_and_lower - collects floating windows and optionally lowers
 * non-floating ones.
 *
 * goes through the tree and:
 * - adds floating windows to a stack, and resize the stack if it fills up.
 * - lowers non-floating windows unless the layout is stacked.
 * - calls itself on each child node to cover the whole tree. */
static void
stack_and_lower(
	node_t *root, node_t **stack, int *top, int max_size, bool is_stacked)
{
	if (root == NULL)
		return;
	if (root->client && !IS_INTERNAL(root) && IS_FLOATING(root->client)) {
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
	} else if (root->client && !IS_INTERNAL(root) &&
			   !IS_FLOATING(root->client)) { // non floating
		if (!is_stacked)
			lower_window(root->client->window);
	}
	stack_and_lower(root->first_child, stack, top, max_size, is_stacked);
	stack_and_lower(root->second_child, stack, top, max_size, is_stacked);
}

/* sort - sorts floating windows by size (largest first).
 *
 * Sorts floating windows based on their size (width * height).
 * Biggest windows go first, smallest last.
 *
 * Uses a bubble sort */
static void
sort(node_t **s, int n)
{
	for (int i = 0; i <= n; i++) {
		for (int j = i + 1; j <= n; j++) {
			int32_t area_i = s[i]->rectangle.height * s[i]->rectangle.width;
			int32_t area_j = s[j]->rectangle.height * s[j]->rectangle.width;
			if (area_j > area_i) {
				// swap
				node_t *temp = s[i];
				s[i]		 = s[j];
				s[j]		 = temp;
			}
		}
	}
}

/* restack - keeps floating windows above everything else. */
void
restack(void)
{
	int idx = get_focused_desktop_idx();
	if (idx == -1)
		return;

	node_t *root = curr_monitor->desktops[idx]->tree;
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
					curr_monitor->desktops[idx]->layout == STACK);
	if (top == 0) {
		if (stack[0]->client)
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
			  "largest floating window: %s, smallest floating window: %s",
			  s,
			  ss);
		_FREE_(s);
		_FREE_(ss);
#endif
	}
	_FREE_(stack);
}

void
restackv2(node_t *root)
{
	if (root == NULL) {
		return;
	}
	if (root->first_child && root->first_child->client &&
		IS_EXTERNAL(root->first_child)) {
		if (IS_FLOATING(root->first_child->client)) {
			if (root->second_child && root->second_child->client &&
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
	if (root->second_child && root->second_child->client &&
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
	return (parent->first_child && parent->second_child);
}

static bool
has_internal_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}
	const node_t *parent = node->parent;
	return (parent->first_child && parent->second_child) &&
		   ((IS_INTERNAL(parent->first_child)) ||
			(IS_INTERNAL(parent->second_child)));
}

static bool
is_sibling_external(node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}
	node_t *parent	= node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;
	return (sibling && IS_EXTERNAL(sibling));
}

static node_t *
get_external_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	const node_t *parent  = node->parent;
	node_t		 *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;

	return (sibling && IS_EXTERNAL(sibling)) ? sibling : NULL;
}

static bool
is_sibling_internal(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;

	return (sibling && IS_INTERNAL(sibling));
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

	return (sibling && IS_INTERNAL(sibling)) ? sibling : NULL;
}

node_t *
get_sibling(node_t *n)
{
	if (n == NULL || n->parent == NULL) {
		return NULL;
	}
	node_t *parent = n->parent;
	node_t *sibling =
		(parent->first_child == n) ? parent->second_child : parent->first_child;
	return sibling;
}

static node_t *
get_sibling_by_type(node_t *node, node_type_t *type)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	node_t *parent	= node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;

	switch (*type) {
	case INTERNAL_NODE: {
		return (sibling && IS_INTERNAL(sibling)) ? sibling : NULL;
	}
	case EXTERNAL_NODE: {
		return (sibling && IS_EXTERNAL(sibling)) ? sibling : NULL;
	}
	case ROOT_NODE: break;
	}
	return NULL;
}

static bool
has_external_children(const node_t *parent)
{
	return (parent->first_child && IS_EXTERNAL(parent->first_child)) &&
		   (parent->second_child && IS_EXTERNAL(parent->second_child));
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

	return ((parent->first_child && parent->second_child) &&
			(IS_EXTERNAL(parent->first_child) &&
			 !IS_EXTERNAL(parent->second_child))) ||
		   ((parent->first_child && parent->second_child) &&
			(IS_EXTERNAL(parent->second_child) &&
			 !IS_EXTERNAL(parent->first_child)));
}

static client_t *
find_client_by_window_id(node_t *root, xcb_window_t win)
{
	if (root == NULL)
		return NULL;

	if (root->client && root->client->window == win) {
		return root->client;
	}

	node_t *l = find_node_by_window_id(root->first_child, win);
	if (l == NULL)
		return NULL;

	if (l->client) {
		return l->client;
	}

	node_t *r = find_node_by_window_id(root->second_child, win);
	if (r == NULL)
		return NULL;

	if (r->client) {
		return r->client;
	}

	return NULL;
}

/* apply_default_layout - applies the default tiling layout to a given tree
 *
 * recursively applies the default tiling layout to a node and its
 * descendants in the tree. The default layout splits nodes either
 * vertically or horizontally based on their dimensions. */
void
apply_default_layout(node_t *root)
{
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {
		return;
	}

	rectangle_t r, r2 = {0};
	/* determine split orientation based on node dimensions */
	if (root->rectangle.width >= root->rectangle.height) {
		/* vertical split (side by side) */
		r.x = root->rectangle.x;
		r.y = root->rectangle.y;
		r.width =
			(root->rectangle.width - (conf.window_gap - conf.border_width)) / 2;
		r.height = root->rectangle.height;
		r2.x	 = (int16_t)(root->rectangle.x + r.width + conf.window_gap +
						 conf.border_width);
		r2.y	 = root->rectangle.y;
		r2.width = root->rectangle.width - r.width - conf.window_gap -
				   conf.border_width;
		r2.height = root->rectangle.height;
	} else {
		/* horizontal split (top and bottom) */
		r.x		= root->rectangle.x;
		r.y		= root->rectangle.y;
		r.width = root->rectangle.width;
		r.height =
			(root->rectangle.height - (conf.window_gap - conf.border_width)) /
			2;
		r2.x	  = root->rectangle.x;
		r2.y	  = (int16_t)(root->rectangle.y + r.height + conf.window_gap +
						  conf.border_width);
		r2.width  = root->rectangle.width;
		r2.height = root->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
	}

	/* this nested unreadable ternary code basically forces floating windows to
	 * retain their floating rectangle and give the full parent's rectangle to
	 * the other child. In some rare cases, this does not work as expected , I
	 * am still looking into it */
	if (root->first_child) {
		root->first_child->rectangle =
			((root->second_child->client) &&
			 IS_FLOATING(root->second_child->client))
				? root->rectangle
			: ((root->first_child->client) &&
			   IS_FLOATING(root->first_child->client))
				? root->first_child->floating_rectangle
				: r;
		if (IS_INTERNAL(root->first_child)) {
			apply_default_layout(root->first_child);
		}
	}

	/* same as above */
	if (root->second_child) {
		root->second_child->rectangle =
			((root->first_child->client) &&
			 IS_FLOATING(root->first_child->client))
				? root->rectangle
			: ((root->second_child->client) &&
			   IS_FLOATING(root->second_child->client))
				? root->second_child->floating_rectangle
				: r2;
		if (IS_INTERNAL(root->second_child)) {
			apply_default_layout(root->second_child);
		}
	}
}

/* default_layout - applies the default layout to the tree.
 *
 * initializes the default layout for the entire screen or
 * monitor. It sets up the initial rectangle for the root node with window gaps
 * and border widths in mind, then calls apply_default_layout to recursively
 * layout child nodes. */
static void
default_layout(node_t *root)
{
	if (root == NULL)
		return;

	rectangle_t	   r = {0};
	uint16_t	   w = curr_monitor->rectangle.width;
	uint16_t	   h = curr_monitor->rectangle.height;
	uint16_t x = curr_monitor->rectangle.x;
	uint16_t y = curr_monitor->rectangle.y;
	if (wm->bar && curr_monitor == prim_monitor) {
		r.x		 = (int16_t)(x + conf.window_gap);
		r.y		 = (int16_t)(y + wm->bar->rectangle.height + conf.window_gap);
		r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
		r.height = (uint16_t)(h - wm->bar->rectangle.height - 2 * conf.window_gap -
				   2 * conf.border_width);
	} else {
		r.x		 = (int16_t)(x + conf.window_gap);
		r.y		 = (int16_t)(y + conf.window_gap);
		r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
		r.height = (uint16_t)(h - 2 * conf.window_gap - 2 * conf.border_width);
	}
	root->rectangle = r;
	apply_default_layout(root);
}

/* apply_master_layout - applies the master layout to a tree.
 *
 * implements a master-stack layout, where one window (the master)
 * takes up a larger portion of the screen (70%), and the rest are stacked.
 * TODO: use next_node() to implement this
 */
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
		r.x		= parent->rectangle.x;
		r.y		= parent->rectangle.y;
		r.width = parent->rectangle.width;
		r.height =
			(uint16_t)((parent->rectangle.height - (conf.window_gap - conf.border_width)) /
			2);

		r2.x	  = parent->rectangle.x;
		r2.y	  = (int16_t)(parent->rectangle.y + r.height + conf.window_gap +
						  conf.border_width);
		r2.width  = parent->rectangle.width;
		r2.height = (uint16_t)(parent->rectangle.height - r.height - conf.window_gap -
					conf.border_width);
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

/* master_layout - initializes and applies the master layout to the tree.
 *
 * This func sets up the initial rectangles for the master and stack areas,
 * marks the appropriate node as the master, and then calls apply_master_layout
 * to recursively apply the layout to the entire tree. */
static void
master_layout(node_t *root, node_t *n)
{
	const double   ratio		= 0.70;
	const uint16_t w			= curr_monitor->rectangle.width;
	const uint16_t h			= curr_monitor->rectangle.height;
	const uint16_t x			= curr_monitor->rectangle.x;
	const uint16_t y			= curr_monitor->rectangle.y;
	const uint16_t master_width = (uint16_t)(w * ratio);
	const uint16_t r_width		= (uint16_t)(w * (1 - ratio));
	const uint16_t bar_height = wm->bar == NULL ? 0 : wm->bar->rectangle.height;

	/* find a node to be master if not provided */
	if (n == NULL) {
		n = find_any_leaf(root);
		if (n == NULL) {
			return;
		}
	}

	n->is_master   = true;

	/* master rectangle */
	const rectangle_t r1 = {
		.x		= (int16_t)(x + conf.window_gap),
		.y		= (int16_t)(y + bar_height + conf.window_gap),
		.width	= (uint16_t)(master_width - 2 * conf.window_gap),
		.height = (uint16_t)(h - 2 * conf.window_gap - bar_height),
	};

	/* stack rectangle */
	const rectangle_t r2 = {
		.x		= (int16_t)(x + master_width),
		.y		= (int16_t)(y + bar_height + conf.window_gap),
		.width	= (uint16_t)(r_width - (1 * conf.window_gap)),
		.height = (uint16_t)(h - 2 * conf.window_gap - bar_height),
	};

	/* if node is a root, give it full screen rectangle and ignore this
	 * requests. Note, this happens when a user keeps deleting windows in a
	 * master layout till single window is mapped, which is the root window */
	if (n->node_type == ROOT_NODE && n->first_child == NULL &&
		n->second_child == NULL) {
		n->rectangle = (rectangle_t){
			.x		= (int16_t)(x + conf.window_gap),
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

/* recursively traverses the tree and sets the is_master flag
 * to false for all nodes. It's typically called before applying a new layout
 * to ensure a clean slate. */
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

/* apply_stack_layout - applies the stack layout to a given tree .
 *
 * recursively applies the stack layout to a node and its
 * children in the tree. In a stack layout, all windows occupy
 * the same space, effectively stacking on top of each other. */
void
apply_stack_layout(node_t *root)
{
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {
		return;
	}

	if (root->first_child) {
		root->first_child->rectangle = root->rectangle;
		if (IS_INTERNAL(root->first_child)) {
			apply_stack_layout(root->first_child);
		}
	}

	if (root->second_child) {
		root->second_child->rectangle = root->rectangle;
		if (IS_INTERNAL(root->second_child)) {
			apply_stack_layout(root->second_child);
		}
	}
}

/**
 * stack_layout - initializes and applies the stack layout to the tree.
 *
 * sets up the initial rectangle for the entire stack.
 * It then calls apply_stack_layout to recursively apply the layout
 * to all nodes in the tree. */
static void
stack_layout(node_t *root)
{
	rectangle_t	   r = {0};
	uint16_t	   w = curr_monitor->rectangle.width;
	uint16_t	   h = curr_monitor->rectangle.height;
	const uint16_t x = curr_monitor->rectangle.x;
	const uint16_t y = curr_monitor->rectangle.y;
	if (wm->bar && curr_monitor == prim_monitor) {
		r.x		 = (int16_t)(x + conf.window_gap);
		r.y		 = (int16_t)(y + wm->bar->rectangle.height + conf.window_gap);
		r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
		r.height = (uint16_t)(h - wm->bar->rectangle.height - 2 * conf.window_gap -
				   2 * conf.border_width);
	} else {
		r.x		 = (int16_t)(x + conf.window_gap);
		r.y		 = (int16_t)(y + conf.window_gap);
		r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
		r.height = (uint16_t)(h - 2 * conf.window_gap - 2 * conf.border_width);
	}
	root->rectangle = r;
	apply_stack_layout(root);
}

// void grid_layout(node_t *root) {
// }

/* apply_layout - applies the specified layout to the given tree.
 *
 * responsible for switching between different layout
 * types (DEFAULT, MASTER, STACK) and applying the chosen layout
 * to the desktop's tree. */
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

	/* if 'I' has no parent */
	if (node->parent->parent == NULL) {
		node->parent->node_type	   = ROOT_NODE;
		node->parent->client	   = external_node->client;
		node->parent->first_child  = NULL;
		node->parent->second_child = NULL;
	} else {
		/* if 'I' has a parent */
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
			grandparent->second_child->node_type	= EXTERNAL_NODE;
			grandparent->second_child->client		= external_node->client;
			grandparent->second_child->first_child	= NULL;
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
	/* if IN has no parent */
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
		/* if IN has a parent */
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

/* delete_node - removes a node (and its client) from the tree.
 *
 * - It unlinks the node from the tree using `unlink_node`.
 * - Frees the memory for the node and its client.
 * - Updates the desktop's node count.
 * - Rearranges the tree if it’s not empty after deletion.
 *
 * checks for invalid or edge cases:
 * - If the node or its client is null.
 * - If the node isn’t an external node.
 * - If the parent of the node is null (but not for the root).
 *
 * TODO: Implement deletion logic for `MASTER` and `STACK` layouts.*/
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

	bool check = false;
	if (node == d->tree) {
		check = true;
	}

	if (!unlink_node(node, d)) {
		_LOG_(ERROR, "could not unlink node.. abort");
		return;
	}

	if (check) {
		assert(!d->tree);
	}

	_FREE_(node->client);
	_FREE_(node);

out:
	d->n_count -= 1;
	if (!is_tree_empty(d->tree)) {
		arrange_tree(d->tree, d->layout);
	}
}

static bool
has_first_child(const node_t *parent)
{
	return parent->first_child;
}

static bool
has_second_child(const node_t *parent)
{
	return parent->second_child;
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
	if (!node) {
		return;
	}

	if (node->client) {
		xcb_icccm_get_text_property_reply_t t_reply;
		xcb_get_property_cookie_t			cn =
			xcb_icccm_get_wm_name(wm->connection, node->client->window);
		uint8_t wr =
			xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
		char name[256];
		if (wr == 1) {
			snprintf(name, sizeof(name), "%s", t_reply.name);
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		}
		_LOG_(DEBUG,
			  "node Type: %d, client Window ID: %u, name: %s, "
			  "is_focused %s",
			  node->node_type,
			  node->client->window,
			  name,
			  node->is_focused ? "true" : "false");
	} else {
		_LOG_(DEBUG, "node Type: %d", node->node_type);
	}

	log_tree_nodes(node->first_child);
	log_tree_nodes(node->second_child);
}

int
hide_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (!IS_INTERNAL(cn) && cn->client) {
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

	if (!IS_INTERNAL(cn) && cn->client) {
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

	if (cn->client) {
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

void
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
				/* shrink */
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
			if (root->first_child == NULL || !IS_INTERNAL(root->first_child)) {
				return;
			}
			(t == GROW) ? (root->first_child->rectangle.width += px)
						: (root->first_child->rectangle.width -= px);
			(t == GROW) ? (root->second_child->rectangle.width -= px)
						: (root->second_child->rectangle.width += px);
			(t == GROW) ? (root->second_child->rectangle.x += px)
						: (root->second_child->rectangle.x -= px);

			if (root->second_child && IS_EXTERNAL(root->second_child)) {
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

			if (root->first_child && IS_EXTERNAL(root->first_child)) {
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
		root->client) {
		return root;
	}

	node_t *left_leaf = find_left_leaf(root->first_child);
	if ((left_leaf && left_leaf->client) &&
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
		root->client && !IS_FLOATING(root->client)) {
		return root;
	}

	node_t *f = find_any_leaf(root->first_child);
	if (f && f->client && !IS_FLOATING(f->client)) {
		return f;
	}

	node_t *s = find_any_leaf(root->second_child);
	if (s && s->client && !IS_FLOATING(s->client)) {
		return s;
	}

	return NULL;
}

/* unlink_node - removes a node from the tree while keeping the structure
 * intact.
 *
 * disconnects a node from the tree without freeing its
 * memory. It tweaks the parent and sibling relationships to keep the tree
 * intact.
 *
 * Note: this function does not free the memory of the unlinked node.
 * The caller is responsible for freeing the memory of the unlinked node if it's
 * no longer needed.
 */
bool
unlink_node(node_t *n, desktop_t *d)
{
	if (d == NULL || n == NULL) {
		return false;
	}

	/* If the node `n` is the root, the tree becomes NULL */
	if (is_parent_null(n)) {
		d->tree = NULL;
		return true;
	}

	node_t *parent	= n->parent;
	node_t *sibling = NULL;
	if ((sibling = get_sibling(n)) == NULL) {
		_LOG_(ERROR, "could not get sibling of n");
		return false;
	}

	node_t *grandparent = parent->parent;

	sibling->parent		= grandparent;
	if (grandparent) {
		if (grandparent->first_child == parent) {
			grandparent->first_child = sibling;
		} else {
			grandparent->second_child = sibling;
		}
	} else {
		sibling->node_type = ROOT_NODE;
		d->tree			   = sibling;
	}

	parent->second_child = NULL;
	parent->first_child	 = NULL;
	_FREE_(parent);
	n->parent = NULL;
	return true;
}

/* transfer_node - moves a node to a new desktop's tree.
 *
 * takes a node and places it into the tree of the target desktop.
 * It handles three main scenarios:
 * 1. If the target tree is empty:
 *   - The node becomes the root of the tree.
 * 2. If the tree has just one node:
 *   - Splits the root into two external nodes under a new internal root.
 * 3. Otherwise:
 *   - Finds a spot in the tree (a leaf), converts it into an internal node,
 *		and then insert the node in there.
 *
 * Note:
 * - Doesn't touch visibility or focus; that's handled outside. */
bool
transfer_node(node_t *node, desktop_t *d)
{
	if (node == NULL || d == NULL) {
		return false;
	}

	if (node->client == NULL) {
		return false;
	}

	assert(node->parent == NULL);

	if (is_tree_empty(d->tree)) {
		rectangle_t	   r = {0};
		uint16_t	   w = curr_monitor->rectangle.width;
		uint16_t	   h = curr_monitor->rectangle.height;
		uint16_t x = curr_monitor->rectangle.x;
		uint16_t y = curr_monitor->rectangle.y;
		if (wm->bar && curr_monitor == prim_monitor) {
			r.x		 = (int16_t)(x + conf.window_gap);
			r.y		 = (int16_t)(y + wm->bar->rectangle.height + conf.window_gap);
			r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
			r.height = (uint16_t)(h - wm->bar->rectangle.height - 2 * conf.window_gap -
					   2 * conf.border_width);
		} else {
			r.x		 = (int16_t)(x + conf.window_gap);
			r.y		 = (int16_t)(y + conf.window_gap);
			r.width	 = (uint16_t)(w - 2 * conf.window_gap - 2 * conf.border_width);
			r.height = (uint16_t)(h - 2 * conf.window_gap - 2 * conf.border_width);
		}
		node->node_type	   = ROOT_NODE;
		d->tree			   = node;
		d->tree->rectangle = r;
		d->tree->node_type = ROOT_NODE;
	} else if (d->tree->first_child == NULL && d->tree->second_child == NULL) {
		client_t *c = d->tree->client;
		if ((d->tree->first_child = create_node(c)) == NULL) {
			return false;
		}
		d->tree->first_child->node_type	 = EXTERNAL_NODE;
		d->tree->second_child			 = node;
		d->tree->second_child->node_type = EXTERNAL_NODE;
		d->tree->client					 = NULL;
		d->tree->first_child->parent = d->tree->second_child->parent = d->tree;
	} else {
		node_t *leaf = find_any_leaf(d->tree);
		if (leaf == NULL) {
			return false;
		}
		if (!IS_ROOT(leaf)) {
			leaf->node_type = INTERNAL_NODE;
		}
		if ((leaf->first_child = create_node(leaf->client)) == NULL) {
			return false;
		}
		leaf->first_child->parent	 = leaf;
		leaf->first_child->node_type = EXTERNAL_NODE;
		leaf->client				 = NULL;
		leaf->second_child			 = node;
		if (leaf->second_child == NULL) {
			return false;
		}
		leaf->second_child->parent	  = leaf;
		leaf->second_child->node_type = EXTERNAL_NODE;
	}
	return true;
}

bool
has_floating_window(node_t *root)
{
	if (root == NULL)
		return false;

	if (root->client && IS_FLOATING(root->client)) {
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

/* next_node - get the next external node in the tree, starting from the given
 * node. It is used to traverse nodes in stack layout */
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

/* prev_node - get the previous external node in the tree, starting from the
 * given node. It is used to traverse nodes in stack layout */
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

/* flip_node - flips the node's orientation within its parent.
 * It only works if the node has a parent and a sibling. */
void
flip_node(node_t *node)
{
	if (node->parent == NULL) {
		return;
	}
	const flip_t flip = (node->rectangle.width >= node->rectangle.height)
							? VERTICAL_FLIP
							: HORIZONTAL_FLIP;
	node_t		*p	  = node->parent;
	node_t *s = (p->first_child == node) ? p->second_child : p->first_child;
	if (s == NULL)
		return;

	node->rectangle.x = p->rectangle.x;
	node->rectangle.y = p->rectangle.y;
	if (flip == VERTICAL_FLIP) {
		node->rectangle.width  = (p->rectangle.width - conf.window_gap) / 2;
		node->rectangle.height = p->rectangle.height;
		s->rectangle.x =
			p->rectangle.x + node->rectangle.width + conf.window_gap;
		s->rectangle.y = p->rectangle.y;
		s->rectangle.width =
			p->rectangle.width - node->rectangle.width - conf.window_gap;
		s->rectangle.height = p->rectangle.height;
	} else {
		node->rectangle.width  = p->rectangle.width;
		node->rectangle.height = (p->rectangle.height - conf.window_gap) / 2;
		s->rectangle.x		   = p->rectangle.x;
		s->rectangle.y =
			p->rectangle.y + node->rectangle.height + conf.window_gap;
		s->rectangle.width = p->rectangle.width;
		s->rectangle.height =
			p->rectangle.height - node->rectangle.height - conf.window_gap;
	}

	if (s->node_type == INTERNAL_NODE) {
		resize_subtree(s);
	}
}

void
update_focus(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client;
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

	bool flag = !IS_INTERNAL(root) && root->client;
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
	if (!IS_INTERNAL(n) && n->client && n->is_focused) {
		return n;
	}
	node_t *l = get_focused_node(n->first_child);
	if (l) {
		return l;
	}
	node_t *s = get_focused_node(n->second_child);
	if (s) {
		return s;
	}
	return NULL;
}

/* swap_node - swap the positions of two nodes */
int
swap_node(node_t *n)
{
	if (n->parent == NULL)
		return -1;

	node_t	   *p  = n->parent;
	node_t	   *s  = (p->first_child == n) ? p->second_child : p->first_child;
	rectangle_t sr = s->rectangle;
	rectangle_t nr = n->rectangle;

	if (p->first_child == n) {
		p->first_child	= s;
		p->second_child = n;
	} else {
		p->first_child	= n;
		p->second_child = s;
	}

	n->rectangle = sr;
	s->rectangle = nr;

	if (IS_INTERNAL(s)) {
		resize_subtree(s);
	}
	return 0;
}

/* is_within_range - checks if one rectangle is within a certain range
 * of another rectangle */
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

/* find_closest_neighbor - find the closest neighbor node to a given node in a
 * specific direction. It is used to move focus to another node using the
 * keyboard
 *
 * searches through the tree (using a bfs) to
 * find the closest external node (a leaf node with a client) that is within a
 * certain range of the given node in the specified direction. It keeps track of
 * the closest node found and returns it.
 *
 * If no closest node is found, it returns NULL */
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
		if (IS_EXTERNAL(current) && current->client &&
			is_within_range(&node->rectangle, &current->rectangle, d)) {
			int distance;
			switch (d) {
			case LEFT:
				distance = node->rectangle.x -
						   (current->rectangle.x + current->rectangle.width);
				break;
			case RIGHT:
				distance = current->rectangle.x -
						   (node->rectangle.x + node->rectangle.width);
				break;
			case UP:
				distance = node->rectangle.y -
						   (current->rectangle.y + current->rectangle.height);
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
		if (current->first_child)
			queue[rear++] = current->first_child;
		if (current->second_child)
			queue[rear++] = current->second_child;
	}
	return closest;
}

/* cycle_win - cycles focus to the nearest window in a specified direction.
 *
 * calls find_closest_neighbor` to get the closest node in the
 * specified direction.
 *
 * If either the root or the neighbor can't be found, it logs an error and
 * returns `NULL`. */
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

/* is_closer_node - check if a new node is closer to the target node than the
 * current node in the specified direction. */
static bool
is_closer_node(node_t *current, node_t *new_node, node_t *node, direction_t d)
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
	if (root->client) {
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
			if (root->rectangle.x > node->rectangle.x + node->rectangle.width &&
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
	if (root->first_child) {
		node_t *child_result = find_neighbor(root->first_child, node, d);
		if (child_result && is_closer_node(best_node, child_result, node, d)) {
			best_node = child_result;
		}
	}
	if (root->second_child) {
		node_t *child_result = find_neighbor(root->second_child, node, d);
		if (child_result && is_closer_node(best_node, child_result, node, d)) {
			best_node = child_result;
		}
	}
	return best_node;
}
