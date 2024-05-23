/*
	BSD 2-Clause License
	Copyright (c) 2024, Yazeed Alharthi

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

		1. Redistributions of source code must retain the above copyright
		notice, this list of conditions and the following disclaimer.

		2. Redistributions in binary form must reproduce the above copyright
		notice, this list of conditions and the following disclaimer in the
		documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "tree.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "logger.h"
#include "type.h"
#include "zwm.h"

#define MAX(a, b)                                                              \
	({                                                                         \
		__typeof__(a) _a = (a);                                                \
		__typeof__(b) _b = (b);                                                \
		_a > _b ? _a : _b;                                                     \
	})

node_t *
create_node(client_t *c)
{
	if (c == 0x00)
		return NULL;

	node_t *node = (node_t *)malloc(sizeof(node_t));
	if (node == 0x00)
		return NULL;

	node->client	   = c;
	node->id		   = 1;
	node->parent	   = NULL;
	node->first_child  = NULL;
	node->second_child = NULL;
	node->is_master	   = false;
	node->is_focused   = false;

	return node;
}

node_t *
init_root()
{
	node_t *node = (node_t *)malloc(sizeof(node_t));
	if (node == 0x00)
		return NULL;

	node->client	   = NULL;
	node->id		   = 1;
	node->parent	   = NULL;
	node->first_child  = NULL;
	node->second_child = NULL;
	node->node_type	   = ROOT_NODE;
	node->is_master	   = false;
	node->is_focused   = false;

	return node;
}

int
render_tree(node_t *node)
{
	if (node == NULL)
		return 0;

	node_t **stack = malloc(10 * sizeof(node_t *));
	if (stack == NULL) {
		log_message(ERROR, "Stack allocation failed\n");
		return -1;
	}

	int stack_size = 10;
	int top		   = -1;
	stack[++top]   = node;

	while (top >= 0) {
		node_t *current_node = stack[top--];

		if (current_node == NULL)
			continue;

		if (current_node->node_type != INTERNAL_NODE &&
			current_node->client != NULL) {
			bool flag = current_node->client->state == FULLSCREEN;
			if (!flag) {
				if (tile(current_node) != 0) {
					log_message(ERROR,
								"error tiling window %d",
								current_node->client->window);
					free(stack);
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
					log_message(ERROR, "Stack reallocation failed\n");
					free(stack);
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
					log_message(ERROR, "Stack reallocation failed\n");
					free(stack);
					return -1;
				}
				stack = new_stack;
			}
			stack[++top] = current_node->first_child;
		}
	}

	free(stack);
	return 0;
}

int
get_tree_level(node_t *node)
{
	if (node == NULL)
		return 0;

	int level_first_child  = get_tree_level(node->first_child);
	int level_second_child = get_tree_level(node->second_child);

	return 1 + MAX(level_first_child, level_second_child);
}

bool
has_floating_children(const node_t *parent)
{
	return (parent->first_child != NULL &&
			parent->first_child->client != NULL &&
			parent->first_child->client->state == FLOATING) ||
		   (parent->second_child != NULL &&
			parent->second_child->client != NULL &&
			parent->second_child->client->state == FLOATING);
}

node_t *
get_floating_child(const node_t *parent)
{
	if (parent->first_child != NULL && parent->first_child->client != NULL &&
		parent->first_child->client->state == FLOATING) {
		return parent->first_child;
	}

	if (parent->second_child != NULL && parent->second_child->client != NULL &&
		parent->second_child->client->state == FLOATING) {
		return parent->second_child;
	}

	return NULL;
}

void
insert_floating_node(node_t *node, desktop_t *d)
{
	assert(node->client->state == FLOATING);
	node_t *n = find_left_leaf(d->tree);
	if (n == NULL)
		return;

	if (n->first_child == NULL) {
		n->first_child = node;
	} else {
		n->second_child = node;
	}
	node->node_type = EXTERNAL_NODE;
}

void
split_node(node_t *n, node_t *nd)
{
	if (nd->client->state == FLOATING) {
		n->first_child->rectangle = n->rectangle;
	} else {
		if (n->rectangle.width >= n->rectangle.height) {
			n->first_child->rectangle.x = n->rectangle.x;
			n->first_child->rectangle.y = n->rectangle.y;
			n->first_child->rectangle.width =
				(n->rectangle.width - conf.window_gap) / 2;
			n->first_child->rectangle.height = n->rectangle.height;
			n->second_child->rectangle.x =
				(int16_t)(n->rectangle.x + n->first_child->rectangle.width +
						  conf.window_gap);
			n->second_child->rectangle.y	 = n->rectangle.y;
			n->second_child->rectangle.width = n->rectangle.width -
											   n->first_child->rectangle.width -
											   conf.window_gap;
			n->second_child->rectangle.height = n->rectangle.height;
		} else {
			n->first_child->rectangle.x		= n->rectangle.x;
			n->first_child->rectangle.y		= n->rectangle.y;
			n->first_child->rectangle.width = n->rectangle.width;
			n->first_child->rectangle.height =
				(n->rectangle.height - conf.window_gap) / 2;
			n->second_child->rectangle.x = n->rectangle.x;
			n->second_child->rectangle.y =
				(int16_t)(n->rectangle.y + n->first_child->rectangle.height +
						  conf.window_gap);
			n->second_child->rectangle.width = n->rectangle.width;
			n->second_child->rectangle.height =
				n->rectangle.height - n->first_child->rectangle.height -
				conf.window_gap;
		}
	}
}

void
insert_node(node_t *node, node_t *new_node, layout_t layout)
{
#ifdef _DEBUG__
	log_message(DEBUG,
				"Node to split %d, node to insert %d",
				node->client->window,
				new_node->client->window);
#endif
	if (node == NULL) {
		log_message(ERROR, "node is null");
		return;
	}

	if (node->client == NULL) {
		log_message(ERROR, "client is null in node");
		return;
	}

	if (node->node_type != ROOT_NODE)
		node->node_type = INTERNAL_NODE;

	node->first_child = create_node(node->client);
	if (node->first_child == NULL)
		return;

	if (node->is_master) {
		node->is_master				 = false;
		node->first_child->is_master = true;
	}

	node->first_child->parent	 = node;
	node->first_child->node_type = EXTERNAL_NODE;
	node->client				 = NULL;

	node->second_child			 = new_node;
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

void
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
		free(root->client);
		root->client = NULL;
	}

	node_t *f = root->first_child;
	free_tree(f);
	root->first_child = NULL;
	node_t *s		  = root->second_child;
	free_tree(s);
	root->second_child = NULL;
	free(root);
}

void
resize_subtree(node_t *parent)
{
	if (parent == NULL)
		return;

	rectangle_t r, r2 = {0};

	if (parent->rectangle.width >= parent->rectangle.height) {
		r.x		  = parent->rectangle.x;
		r.y		  = parent->rectangle.y;
		r.width	  = (parent->rectangle.width - conf.window_gap) / 2;
		r.height  = parent->rectangle.height;

		r2.x	  = (int16_t)(parent->rectangle.x + r.width + conf.window_gap);
		r2.y	  = parent->rectangle.y;
		r2.width  = parent->rectangle.width - r.width - conf.window_gap;
		r2.height = parent->rectangle.height;
	} else {
		r.x		  = parent->rectangle.x;
		r.y		  = parent->rectangle.y;
		r.width	  = parent->rectangle.width;
		r.height  = (parent->rectangle.height - conf.window_gap) / 2;

		r2.x	  = parent->rectangle.x;
		r2.y	  = (int16_t)(parent->rectangle.y + r.height + conf.window_gap);
		r2.width  = parent->rectangle.width;
		r2.height = parent->rectangle.height - r.height - conf.window_gap;
	}

	if (parent->first_child != NULL) {
		parent->first_child->rectangle = r;
		if (parent->first_child->node_type == INTERNAL_NODE) {
			resize_subtree(parent->first_child);
		}
	}

	if (parent->second_child != NULL) {
		parent->second_child->rectangle = r2;
		if (parent->second_child->node_type == INTERNAL_NODE) {
			resize_subtree(parent->second_child);
		}
	}
}

// void
// resize_subtree(node_t *parent)
// {
// 	if (parent == NULL) return;

// 	node_t **stack		= malloc(10 * sizeof(node_t *));
// 	int		 stack_size = 10;
// 	int		 top		= -1;
// 	stack[++top]		= parent;

// 	while (top >= 0) {
// 		node_t *current_node = stack[top--];

// 		if (current_node == NULL) continue;

// 		rectangle_t r, r2 = {0};

// 		if (current_node->rectangle.width >= current_node->rectangle.height) {
// 			r.x		 = current_node->rectangle.x;
// 			r.y		 = current_node->rectangle.y;
// 			r.width	 = (current_node->rectangle.width - conf.window_gap) / 2;
// 			r.height = current_node->rectangle.height;

// 			r2.x	 = (int16_t)(current_node->rectangle.x + r.width +
// 							 conf.window_gap);
// 			r2.y	 = current_node->rectangle.y;
// 			r2.width =
// 				current_node->rectangle.width - r.width - conf.window_gap;
// 			r2.height = current_node->rectangle.height;
// 		} else {
// 			r.x		 = current_node->rectangle.x;
// 			r.y		 = current_node->rectangle.y;
// 			r.width	 = current_node->rectangle.width;
// 			r.height = (current_node->rectangle.height - conf.window_gap) / 2;

// 			r2.x	 = current_node->rectangle.x;
// 			r2.y	 = (int16_t)(current_node->rectangle.y + r.height +
// 							 conf.window_gap);
// 			r2.width = current_node->rectangle.width;
// 			r2.height =
// 				current_node->rectangle.height - r.height - conf.window_gap;
// 		}

// 		if (current_node->first_child != NULL) {
// 			current_node->first_child->rectangle = r;
// 			if (current_node->first_child->node_type == INTERNAL_NODE) {
// 				stack[++top] = current_node->first_child;
// 			}
// 		}

// 		if (current_node->second_child != NULL) {
// 			current_node->second_child->rectangle = r2;
// 			if (current_node->second_child->node_type == INTERNAL_NODE) {
// 				stack[++top] = current_node->second_child;
// 			}
// 		}

// 		if (top >= stack_size - 1) {
// 			stack_size *= 2;
// 			stack = realloc(stack, stack_size * sizeof(node_t *));
// 			if (stack == NULL) {
// 				fprintf(stderr, "Stack allocation failed\n");
// 				return;
// 			}
// 		}
// 	}

// 	free(stack);
// }

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

node_t *
find_floating_node(node_t *root)
{
	if (root == NULL)
		return NULL;

	if (root->client->state == FLOATING)
		return root;

	node_t *l = find_floating_node(root->first_child);
	if (l != NULL)
		return l;

	node_t *r = find_floating_node(root->second_child);
	if (r != NULL)
		return r;

	return NULL;
}

void
restack(node_t *root)
{
	if (root == NULL)
		return;

	if (root->client != NULL) {
		if (root->client->state == FLOATING ||
			root->client->state == FULLSCREEN) {
			raise_window(root->client->window);
		} else {
			lower_window(root->client->window);
		}
	}

	restack(root->first_child);
	restack(root->second_child);
}

bool
has_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	const node_t *parent = node->parent;

	return (parent->first_child != NULL && parent->second_child != NULL);
}

bool
has_internal_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	const node_t *parent = node->parent;

	return (parent->first_child != NULL && parent->second_child != NULL) &&
		   ((parent->first_child->node_type == INTERNAL_NODE) ||
			(parent->second_child->node_type == INTERNAL_NODE));
}

bool
is_sibling_external(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;

	return (sibling != NULL && sibling->node_type == EXTERNAL_NODE);
}

node_t *
get_external_sibling(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	const node_t *parent  = node->parent;
	node_t		 *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;

	return (sibling != NULL && sibling->node_type == EXTERNAL_NODE) ? sibling
																	: NULL;
}

bool
is_sibling_internal(const node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return false;
	}

	node_t		 *parent  = node->parent;
	const node_t *sibling = (parent->first_child == node) ? parent->second_child
														  : parent->first_child;

	return (sibling != NULL && sibling->node_type == INTERNAL_NODE);
}

node_t *
get_internal_sibling(node_t *node)
{
	if (node == NULL || node->parent == NULL) {
		return NULL;
	}

	node_t *parent	= node->parent;
	node_t *sibling = (parent->first_child == node) ? parent->second_child
													: parent->first_child;

	return (sibling != NULL && sibling->node_type == INTERNAL_NODE) ? sibling
																	: NULL;
}

node_t *
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
		return (sibling != NULL && sibling->node_type == INTERNAL_NODE)
				   ? sibling
				   : NULL;
	}
	case EXTERNAL_NODE: {
		return (sibling != NULL && sibling->node_type == EXTERNAL_NODE)
				   ? sibling
				   : NULL;
	}
	case ROOT_NODE: break;
	}
	return NULL;
}

bool
has_external_children(const node_t *parent)
{
	return (parent->first_child != NULL &&
			parent->first_child->node_type == EXTERNAL_NODE) &&
		   (parent->second_child != NULL &&
			parent->second_child->node_type == EXTERNAL_NODE);
}

node_t *
find_tree_root(node_t *node)
{
	if (node->node_type == ROOT_NODE) {
		return node;
	}
	return find_tree_root(node->parent);
}

bool
has_single_external_child(const node_t *parent)
{
	if (parent == NULL)
		return false;

	return ((parent->first_child != NULL && parent->second_child != NULL) &&
			(parent->first_child->node_type == EXTERNAL_NODE &&
			 parent->second_child->node_type != EXTERNAL_NODE)) ||
		   ((parent->first_child != NULL && parent->second_child != NULL) &&
			(parent->second_child->node_type == EXTERNAL_NODE &&
			 parent->first_child->node_type != EXTERNAL_NODE));
}

client_t *
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
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {
		return;
	}

	rectangle_t r, r2 = {0};

	if (root->rectangle.width >= root->rectangle.height) {
		r.x		  = root->rectangle.x;
		r.y		  = root->rectangle.y;
		r.width	  = (root->rectangle.width - conf.window_gap) / 2;
		r.height  = root->rectangle.height;

		r2.x	  = (int16_t)(root->rectangle.x + r.width + conf.window_gap);
		r2.y	  = root->rectangle.y;
		r2.width  = root->rectangle.width - r.width - conf.window_gap;
		r2.height = root->rectangle.height;
	} else {
		r.x		  = root->rectangle.x;
		r.y		  = root->rectangle.y;
		r.width	  = root->rectangle.width;
		r.height  = (root->rectangle.height - conf.window_gap) / 2;

		r2.x	  = root->rectangle.x;
		r2.y	  = (int16_t)(root->rectangle.y + r.height + conf.window_gap);
		r2.width  = root->rectangle.width;
		r2.height = root->rectangle.height - r.height - conf.window_gap;
	}

	if (root->first_child != NULL) {
		root->first_child->rectangle = r;
		if (root->first_child->node_type == INTERNAL_NODE) {
			apply_default_layout(root->first_child);
		}
	}

	if (root->second_child != NULL) {
		root->second_child->rectangle = r2;
		if (root->second_child->node_type == INTERNAL_NODE) {
			apply_default_layout(root->second_child);
		}
	}
}

void
default_layout(node_t *root)
{
	if (root == NULL)
		return;

	rectangle_t	   r = {0};
	const uint16_t w = wm->screen->width_in_pixels;
	const uint16_t h = wm->screen->height_in_pixels;

	if (wm->bar != NULL) {
		r.x		= conf.window_gap - conf.border_width * 1.5;
		r.y		= (int16_t)(wm->bar->rectangle.height + conf.window_gap);
		r.width = (uint16_t)(w - 2 * conf.window_gap);
		r.height =
			(uint16_t)(h - 2 * conf.window_gap - wm->bar->rectangle.height);
	} else {
		r.x		 = conf.window_gap - conf.border_width * 1.5;
		r.y		 = conf.window_gap - conf.border_width * 1.5;
		r.width	 = w - conf.window_gap * 2;
		r.height = h - conf.window_gap * 2;
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
		r.x		  = parent->rectangle.x;
		r.y		  = parent->rectangle.y;
		r.width	  = parent->rectangle.width;
		r.height  = (parent->rectangle.height - conf.window_gap) / 2;
		// r.height  = h;
		r2.x	  = parent->rectangle.x;
		r2.y	  = (int16_t)(parent->rectangle.y + r.height + conf.window_gap);
		r2.width  = parent->rectangle.width;
		r2.height = parent->rectangle.height - r.height - conf.window_gap;
		// r2.height = h;
		parent->first_child->rectangle	= r;
		parent->second_child->rectangle = r2;
	}

	if (parent->first_child->node_type == INTERNAL_NODE) {
		apply_master_layout(parent->first_child);
	}

	if (parent->second_child->node_type == INTERNAL_NODE) {
		apply_master_layout(parent->second_child);
	}
}

void
master_layout(node_t *root, node_t *n)
{
	const double ratio		  = 0.70;
	uint64_t	 w			  = wm->screen->width_in_pixels;
	uint64_t	 h			  = wm->screen->height_in_pixels;
	uint16_t	 master_width = w * ratio;
	uint16_t	 r_width	  = (uint16_t)(w * (1 - ratio));
	// uint16_t	 r_height	  = (uint16_t)(h - 2 * W_GAP - 27) / (nc - 1);
	n->is_master			  = true;
	// uint16_t r_height = (uint16_t)(h - (2 * W_GAP - 27)) / (nc - 1) - 2 *
	// W_GAP;
	// uint16_t height = (r2.height / (nc - 1)) - (W_GAP / 2) - 2;

	rectangle_t r1			  = {
				   .x	   = conf.window_gap,
				   .y	   = (int16_t)(27 + conf.window_gap),
				   .width  = (uint16_t)(master_width - 2 * conf.window_gap),
				   .height = (uint16_t)(h - 2 * conf.window_gap - 27),
	   };
	rectangle_t r2 = {
		.x		= (master_width),
		.y		= (int16_t)(27 + conf.window_gap),
		.width	= (uint16_t)(r_width - (1 * conf.window_gap)),
		.height = (uint16_t)(h - 2 * conf.window_gap - 27),
		// .height = r_height,
	};
	n->rectangle	= r1;
	root->rectangle = r2;
	apply_master_layout(root);
	// n->is_master = false;
}

void
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
		if (root->first_child->node_type == INTERNAL_NODE) {
			apply_stack_layout(root->first_child);
		}
	}

	if (root->second_child != NULL) {
		root->second_child->rectangle = root->rectangle;
		if (root->second_child->node_type == INTERNAL_NODE) {
			apply_stack_layout(root->second_child);
		}
	}
}

void
stack_layout(node_t *root)
{
	rectangle_t	   r = {0};
	const uint16_t w = wm->screen->width_in_pixels;
	const uint16_t h = wm->screen->height_in_pixels;
	if (wm->bar != NULL) {
		r.x		= conf.window_gap - conf.border_width * 1.5;
		// r.x		 = 0;
		r.y		= (int16_t)(wm->bar->rectangle.height + conf.window_gap);
		r.width = (uint16_t)(w - 2 * conf.window_gap);
		// r.width	 = w;
		r.height =
			(uint16_t)(h - 2 * conf.window_gap - wm->bar->rectangle.height);
	} else {
		r.x		 = 0;
		r.y		 = conf.window_gap;
		r.width	 = w;
		r.height = h - conf.window_gap;
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
		d->top_w = n->client->window;
		break;
	}
	case GRID: {
		break;
	}
	}
}

int
delete_node_with_external_sibling(node_t *node)
{
	/* node to delete = N, internal node = I, external node = E
	 *         I
	 *    	 /   \
	 *     N||E   N||E
	 *
	 * logic:
	 * just delete N and replace E with I and give it full I's rectangle
	 */
	node_t *external_node = NULL;
	assert(is_sibling_external(node));
	external_node = get_external_sibling(node);

	if (external_node == NULL) {
		log_message(ERROR, "external node is null");
		return -1;
	}

	// if I has no parent
	if (node->parent->parent == NULL) {
		node->parent->node_type	   = ROOT_NODE;
		node->parent->client	   = external_node->client;
		node->parent->first_child  = NULL;
		node->parent->second_child = NULL;
	} else {
		// if I has a prent
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
	free(external_node);
	external_node = NULL;
	free(node->client);
	node->client = NULL;
	free(node);
	node = NULL;
	return 0;
}

int
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
			log_message(ERROR, "internal node is null");
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
		/* resize_subtree(wm->root); */
		// if (d->layout != STACK) resize_subtree(d->tree);
		if (d->layout == DEFAULT) {
			apply_default_layout(d->tree);
		} else if (d->layout == STACK) {
			apply_stack_layout(d->tree);
		}
		// resize_subtree(d->tree);
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
			log_message(ERROR, "internal node is null");
			return -1;
		}
		if (internal_sibling == NULL) {
			log_message(ERROR, "internal node is null");
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
		// if (d->layout != STACK) resize_subtree(node->parent);
		resize_subtree(node->parent);
		if (d->layout == DEFAULT) {
			apply_default_layout(node->parent);
		} else if (d->layout == STACK) {
			apply_stack_layout(node->parent);
		}
	}

	free(internal_sibling);
	internal_sibling = NULL;
	free(node->client);
	node->client = NULL;
	free(node);
	node = NULL;
	return 0;
}

void
delete_floating_node(node_t *node, desktop_t *d)
{
	if (node == NULL || node->client == NULL || d == NULL) {
		log_message(ERROR, "node to be deleted is null");
		return;
	}

	assert(node->client->state == FLOATING);
#ifdef _DEBUG__
	log_message(DEBUG, "DELETE floating window %d", node->client->window);
#endif
	node_t *p = node->parent;
	if (p->first_child == node) {
		p->first_child = NULL;
	} else {
		p->second_child = NULL;
	}
	node->parent = NULL;
	free(node->client);
	node->client = NULL;
	free(node);
	node = NULL;
	assert(p->first_child == NULL);
	assert(p->second_child == NULL);
#ifdef _DEBUG__
	log_message(DEBUG, "DELETE floating window success");
#endif
	d->n_count -= 1;
}

// TODO: handle deletion in master layout
// TODO: handle deletion in stack layout
void
delete_node(node_t *node, desktop_t *d)
{
	if (node == NULL || node->client == NULL || d->tree == NULL) {
		log_message(ERROR, "node to be deleted is null");
		return;
	}

	if (node->node_type == INTERNAL_NODE) {
		log_message(ERROR,
					"node to be deleted is not an external node type: %d",
					node->node_type);
		return;
	}

	if (is_parent_null(node) && node != d->tree) {
		log_message(ERROR, "parent of node is null");
		return;
	}

	// node_t *parent = node->parent;

	// early check if only single node/client is mapped to the screen
	if (node == d->tree) {
		free(node->client);
		node->client = NULL;
		free(node);
		node	= NULL;
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
	// 		log_message(ERROR, "cannot delete node with id: %d", w);
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
	// 		log_message(ERROR, "cannot delete node with id: %d", w);
	// 		return;
	// 	}
	// 	goto out;
	// }

	unlink_node(node, d);
	free(node->client);
	node->client = NULL;
	free(node);
	node = NULL;

out:
	d->n_count -= 1;
	// resize_subtree(d->tree);
	if (!is_tree_empty(d->tree)) {
		arrange_tree(d->tree, d->layout);
	}
	return;
}

bool
has_first_child(const node_t *parent)
{
	return parent->first_child != NULL;
}

bool
has_second_child(const node_t *parent)
{
	return parent->second_child != NULL;
}

bool
is_internal(const node_t *node)
{
	return node->node_type == INTERNAL_NODE;
}

bool
is_external(const node_t *node)
{
	return node->node_type == EXTERNAL_NODE;
}

bool
is_tree_empty(const node_t *root)
{
	return root == NULL;
}

bool
is_parent_null(const node_t *node)
{
	return node->parent == NULL;
}

bool
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
			xcb_get_property_cookie_t			cn =
				xcb_icccm_get_wm_name(wm->connection, node->client->window);
			uint8_t wr =
				xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
			char name[256];
			if (wr == 1) {
				snprintf(name, sizeof(name), "%s", t_reply.name);
				xcb_icccm_get_text_property_reply_wipe(&t_reply);
			}
			log_message(DEBUG,
						"Node Type: %d, Client Window ID: %u, name: %s, "
						"is_focused %s",
						node->node_type,
						node->client->window,
						name,
						node->is_focused ? "true" : "false");
		} else {
			log_message(DEBUG, "Node Type: %d", node->node_type);
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

	if (cn->node_type != INTERNAL_NODE && cn->client != NULL) {
		if (hide_window(cn->client->window) != 0) {
			return -1;
		}
		// if (win_focus(wm->connection, cn->client->window, false) != 0) {
		// 	return -1;
		// }
		if (set_focus(cn, false) != 0) {
			return -1;
		}
	}

	hide_windows(cn->first_child);
	hide_windows(cn->second_child);

	return 0;
}

// int
// hide_windows(node_t *cn)
// {
// 	if (cn == NULL) return 0;
// 	node_t **stack		= malloc(10 * sizeof(node_t *));
// 	int		 stack_size = 10;
// 	int		 top		= -1;
// 	stack[++top]		= cn;

// 	while (top >= 0) {
// 		node_t *current_node = stack[top--];
// 		if (current_node == NULL) continue;
// 		if (current_node->node_type != INTERNAL_NODE &&
// 			current_node->client != NULL) {
// 			if (hide_window(current_node->client->window) != 0) {
// 				free(stack);
// 				return -1;
// 			}
// 			if (set_focus(current_node, false) != 0) {
// 				free(stack);
// 				return -1;
// 			}
// 		}
// 		// push children onto the stack
// 		if (current_node->second_child != NULL) {
// 			stack[++top] = current_node->second_child;
// 		}
// 		if (current_node->first_child != NULL) {
// 			stack[++top] = current_node->first_child;
// 		}
// 		// resize if needed
// 		if (top >= stack_size - 1) {
// 			stack_size *= 2;
// 			stack = realloc(stack, stack_size * sizeof(node_t *));
// 			if (stack == NULL) {
// 				fprintf(stderr, "Stack allocation failed\n");
// 				return -1;
// 			}
// 		}
// 	}
// 	free(stack);
// 	return 0;
// }

int
show_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (cn->node_type != INTERNAL_NODE && cn->client != NULL) {
		if (show_window(cn->client->window) != 0) {
			return -1;
		}
		// if (set_focus(cn, false) != 0) {
		// 	return -1;
		// }
	}

	show_windows(cn->first_child);
	show_windows(cn->second_child);

	return 0;
}

// int
// show_windows(node_t *cn)
// {
// 	if (cn == NULL) return 0;

// 	node_t **stack		= malloc(10 * sizeof(node_t *));
// 	int		 stack_size = 10;
// 	int		 top		= -1;
// 	stack[++top]		= cn;

// 	while (top >= 0) {
// 		node_t *current_node = stack[top--];
// 		if (current_node == NULL) continue;
// 		if (current_node->node_type != INTERNAL_NODE &&
// 			current_node->client != NULL) {
// 			if (show_window(current_node->client->window) != 0) {
// 				free(stack);
// 				return -1;
// 			}
// 		}
// 		// push the children onto the stack
// 		if (current_node->second_child != NULL) {
// 			stack[++top] = current_node->second_child;
// 		}
// 		if (current_node->first_child != NULL) {
// 			stack[++top] = current_node->first_child;
// 		}
// 		// resize if needed
// 		if (top >= stack_size - 1) {
// 			stack_size *= 2;
// 			stack = realloc(stack, stack_size * sizeof(node_t *));
// 			if (stack == NULL) {
// 				fprintf(stderr, "Stack allocation failed\n");
// 				return -1;
// 			}
// 		}
// 	}
// 	free(stack);
// 	return 0;
// }

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

bool
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

bool
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

	if (wm->desktops[i]->layout == STACK) {
		return 0;
	}

	node_t *root = wm->desktops[i]->tree;
	if (root == NULL)
		return -1;

	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	node_t		*n = find_node_by_window_id(root, w);
	if (n == NULL)
		return -1;

	horizontal_resize(n, arg->r);
	render_tree(root);
	return 0;
}

void
horizontal_resize(node_t *n, resize_t t)
{
	const int16_t px			   = 5;
	direction_t	  grow_direction   = NONE;
	direction_t	  shrink_direction = NONE;

	node_t		 *root			   = find_tree_root(n);
	assert(root->node_type == ROOT_NODE);

	if (in_left_subtree(root->first_child, n)) {
		grow_direction	 = RIGHT;
		shrink_direction = LEFT;
	} else if (in_right_subtree(root->second_child, n)) {
		grow_direction	 = LEFT;
		shrink_direction = RIGHT;
	}

	log_message(INFO, "dir == %d", grow_direction);

	if (n->parent == NULL || n->node_type == ROOT_NODE) {
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
	 * then move E's sibling x by 5 pixels and shrink its width by 5 pixels
	 * E's sibling can be External or Internal -> if internal, resize its
	 * children
	 *         I
	 *    	 /   \
	 *   	E     E/I
	 *             \
	 *            rest...
	 */
	if (n->parent->node_type == ROOT_NODE) {
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
				// return;
			} else {
				// shrink
				n->rectangle.width -= px;
				s->rectangle.width += px;
				(shrink_direction == LEFT) ? (s->rectangle.x -= px)
										   : (n->rectangle.x += px);
			}
			// return;
		} else {
			// sibling is internal
			s = get_internal_sibling(n);
			if (s == NULL) {
				log_message(ERROR, "internal sibling is null");
				return;
			}
			if (t == GROW) {
				n->rectangle.width += px;
				s->rectangle.width -= px;
				grow_direction == RIGHT ? (s->rectangle.x += px)
										: (n->rectangle.x -= px);
				resize_subtree(s);
				// return;
			} else {
				n->rectangle.width -= px;
				s->rectangle.width += px;
				(shrink_direction == LEFT) ? (s->rectangle.x -= px)
										   : (n->rectangle.x += px);
				resize_subtree(s);
			}
			// return;
		}
		// return;
	}
	/*
	 * case 2: node's parent is not the root (it is INTERNAL_NODE).
	 * I = Internal node
	 * E = external node
	 *
	 * I's type is INTERNAL_NODE
	 * Node to expand is E.
	 * logic: grow the whole subtree(rectangle)'s width in E's side by 5
	 * pixels then move the opposite subtree(rectangle)'s x by 5 pixels and
	 * shrink its width by 5 pixels
	 *         I
	 *    	 /   \
	 *   	I     E/I
	 *    /   \     \
	 *   E    E/I   rest...
	 */
	if (n->parent->node_type == INTERNAL_NODE) {
		if (in_left_subtree(root->first_child, n)) {
			if (root->first_child == NULL ||
				root->first_child->node_type != INTERNAL_NODE) {
				return;
			}
			(t == GROW) ? (root->first_child->rectangle.width += px)
						: (root->first_child->rectangle.width -= px);
			(t == GROW) ? (root->second_child->rectangle.width -= px)
						: (root->second_child->rectangle.width += px);
			(t == GROW) ? (root->second_child->rectangle.x += px)
						: (root->second_child->rectangle.x -= px);

			if (root->second_child != NULL &&
				root->second_child->node_type == EXTERNAL_NODE) {
				resize_subtree(root->first_child);
				// return;
			} else {
				resize_subtree(root->first_child);
				resize_subtree(root->second_child);
				// return;
			}
		} else {
			if (root->second_child == NULL ||
				root->second_child->node_type != INTERNAL_NODE) {
				return;
			}
			(t == GROW) ? (root->second_child->rectangle.width += px)
						: (root->second_child->rectangle.width -= px);
			(t == GROW) ? (root->second_child->rectangle.x -= px)
						: (root->second_child->rectangle.x += px);
			(t == GROW) ? (root->first_child->rectangle.width -= px)
						: (root->first_child->rectangle.width += px);

			if (root->first_child != NULL &&
				root->first_child->node_type == EXTERNAL_NODE) {
				resize_subtree(root->second_child);
				// return;
			} else {
				resize_subtree(root->first_child);
				resize_subtree(root->second_child);
				// return;
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
		(left_leaf->node_type == EXTERNAL_NODE ||
		 left_leaf->node_type == ROOT_NODE)) {
		return left_leaf;
	}

	return find_left_leaf(root->second_child);
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

	if (is_sibling_external(node)) {
		node_t *e = get_external_sibling(node);
		if (e == NULL) {
			return;
		}
		if (node->parent->node_type == ROOT_NODE) {
			node->parent->client	   = e->client;
			node->parent->first_child  = NULL;
			node->parent->second_child = NULL;
		} else {
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
		free(e);
		e			 = NULL;
		node->parent = NULL;
		return;
	} else if (is_sibling_internal(node)) {
		node_t *n = NULL;
		if (node->parent->node_type == ROOT_NODE) {
			n = get_internal_sibling(node);
			if (n == NULL) {
				log_message(ERROR, "internal node is null");
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
			// resize_subtree(d->tree);
		} else {
			if (is_sibling_internal(node)) {
				n = get_internal_sibling(node);
			} else {
				log_message(ERROR, "internal node is null");
				return;
			}
			if (n == NULL)
				return;

			// n->parent = NULL;
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
			// resize_subtree(node->parent);
		}
		n->parent		= NULL;
		n->first_child	= NULL;
		n->second_child = NULL;
		node->parent	= NULL;
		free(n);
		n = NULL;
	}
}

int
transfer_node_wrapper(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window)
		return -1;

	int d = get_focused_desktop_idx();
	if (d == -1)
		return d;

	const int i	   = arg->idx;
	node_t	 *root = wm->desktops[d]->tree;
	node_t	 *node = find_node_by_window_id(root, w);

	if (d == i) {
		return 0;
	}

	desktop_t *nd = wm->desktops[i];
	desktop_t *od = wm->desktops[d];
#ifdef _DEBUG__
	log_message(DEBUG, "new desktop = %d, old desktop = %d", i + 1, d + 1);
#endif
	if (hide_window(node->client->window) != 0) {
		return -1;
	}
	unlink_node(node, od);
	transfer_node(node, nd);

	if (nd->layout == STACK) {
		// stack_layout(nd->tree);
		set_focus(node, true);
		nd->top_w = node->client->window;
	}

	od->n_count--;
	nd->n_count++;

	if (render_tree(od->tree) != 0) {
		return -1;
	}
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
		rectangle_t r = {0};
		uint16_t	w = wm->screen->width_in_pixels;
		uint16_t	h = wm->screen->height_in_pixels;
		if (wm->bar != NULL) {
			r.x		= conf.window_gap;
			r.y		= (int16_t)(wm->bar->rectangle.height + conf.window_gap);
			r.width = (uint16_t)(w - 2 * conf.window_gap);
			r.height =
				(uint16_t)(h - 2 * conf.window_gap - wm->bar->rectangle.height);
		} else {
			r.x		 = conf.window_gap;
			r.y		 = conf.window_gap;
			r.width	 = w - conf.window_gap;
			r.height = h - conf.window_gap;
		}
		node->node_type	   = ROOT_NODE;
		d->tree			   = node;
		d->tree->rectangle = r;
		d->tree->node_type = ROOT_NODE;
	} else if (d->tree->first_child == NULL && d->tree->second_child == NULL) {
		client_t *c = d->tree->client;
		//		d->tree->first_child			 =
		// create_node(c);
		if ((d->tree->first_child = create_node(c)) == NULL) {
			return;
		}
		d->tree->first_child->node_type	 = EXTERNAL_NODE;
		d->tree->second_child			 = node;
		d->tree->second_child->node_type = EXTERNAL_NODE;
		d->tree->client					 = NULL;
		d->tree->first_child->parent = d->tree->second_child->parent = d->tree;
		// if (d->layout != STACK) resize_subtree(d->tree);
	} else {
		node_t *leaf = find_left_leaf(d->tree);
		if (leaf == NULL) {
			return;
		}
		if (leaf->node_type != ROOT_NODE) {
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
		// if (d->layout != STACK) resize_subtree(leaf);
	}
	arrange_tree(d->tree, d->layout);
}

bool
has_floating_window(node_t *root)
{
	if (root == NULL)
		return false;

	if (root->client != NULL && root->client->state == FLOATING) {
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
		while (l->node_type != EXTERNAL_NODE) {
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
	while (r->node_type != EXTERNAL_NODE) {
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
		while (l->node_type != EXTERNAL_NODE) {
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
	while (l->node_type != EXTERNAL_NODE) {
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
	node_t *s	 = (p->first_child == node) ? p->second_child : p->first_child;

	if (s == NULL)
		return;

	node->rectangle.x	   = p->rectangle.x;
	node->rectangle.y	   = p->rectangle.y;
	node->rectangle.width  = (flip == VERTICAL_FLIP)
								 ? ((p->rectangle.width - conf.window_gap) / 2)
								 : p->rectangle.width;
	node->rectangle.height = (flip == VERTICAL_FLIP)
								 ? p->rectangle.height
								 : (p->rectangle.height - conf.window_gap) / 2;
	s->rectangle.x		   = (flip == VERTICAL_FLIP)
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
			? ((p->rectangle.width - node->rectangle.width) - conf.window_gap)
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

	bool flag = root->node_type != INTERNAL_NODE && root->client != NULL;
	if (flag && root != n) {
		root->is_focused = false;
	} else if (flag && root == n) {
		root->is_focused = true;
	}

	update_focus(root->first_child, n);
	update_focus(root->second_child, n);
}

node_t *
get_focused_node(node_t *n)
{
	if (n == NULL)
		return NULL;

	if (n->client != NULL && n->is_focused) {
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

	node_t	   *s  = (n->parent->first_child == n) ? n->parent->second_child
												   : n->parent->first_child;
	rectangle_t sr = s->rectangle;
	rectangle_t nr = n->rectangle;
	n->rectangle   = sr;
	s->rectangle   = nr;

	if (s->node_type == INTERNAL_NODE) {
		resize_subtree(s);
	}

	return 0;
}