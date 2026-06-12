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

#include "layout.h"
#include "client.h"
#include "focus.h"
#include "helper.h"
#include "monitor.h"
#include "state.h"
#include "tree.h"
#include "type.h"
#include "xcb_util.h"

void
master_clean_up(node_t *root);
void
default_layout(node_t *root);
void
master_layout(node_t *parent, node_t *n);
void
stack_layout(node_t *parent);
void
grid_layout(node_t *root);
static double
clamp_layout_ratio(double ratio);
static node_t *
find_tree_root_local(node_t *n);
static node_t *
find_layout_master(node_t *root);
static void
set_master_node(node_t *root, node_t *n);
static void
set_master_under_cursor(node_t *root);

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
	case GRID: {
		grid_layout(tree);
		break;
	}
	case MONOCLE: {
		monocle_layout(tree);
		break;
	}
	case THREE_COL: {
		three_col_layout(tree);
		break;
	}
	case DECK: {
		deck_layout(tree);
		break;
	}
	}
}

/* apply_layout - applies the specified layout to the given tree.
 *
 * responsible for switching between different layout
 * types (DEFAULT, MASTER, STACK) and applying the chosen layout
 * to the desktop's tree. */
static void
show_all_tiled(node_t *root)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client))
		set_visibility(root->client->window, true);
	show_all_tiled(root->first_child);
	show_all_tiled(root->second_child);
}

void
apply_layout(desktop_t *d, layout_t t)
{
	if (d->layout == MONOCLE || d->layout == DECK)
		show_all_tiled(d->tree);
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
		grid_layout(root);
		break;
	}
	case MONOCLE: {
		monocle_layout(root);
		break;
	}
	case THREE_COL: {
		set_master_under_cursor(root);
		three_col_layout(root);
		break;
	}
	case DECK: {
		set_master_under_cursor(root);
		deck_layout(root);
		break;
	}
	}
}

static double
normalize_split_ratio(double ratio)
{
	if (ratio <= 0.0 || ratio >= 1.0)
		return 0.5;
	return ratio;
}

static double
clamp_layout_ratio(double ratio)
{
	if (ratio < 0.10)
		return 0.10;
	if (ratio > 0.90)
		return 0.90;
	return ratio;
}

static node_t *
find_tree_root_local(node_t *n)
{
	if (!n)
		return NULL;
	while (n->parent) n = n->parent;
	return n;
}

static node_t *
find_layout_master(node_t *root)
{
	node_t *n = find_master_node(root);
	if (n && n->client && IS_TILED(n->client))
		return n;
	return find_any_leaf(root);
}

static void
set_master_node(node_t *root, node_t *n)
{
	master_clean_up(root);
	if (n && n->client && IS_TILED(n->client))
		n->is_master = true;
}

static void
set_master_under_cursor(node_t *root)
{
	xcb_window_t win = get_window_under_cursor(wm->connection, wm->root_window);
	node_t		*n	 = NULL;

	if (win != XCB_NONE && win != wm->root_window)
		n = find_node_by_window_id(root, win);
	if (!n || !n->client || !IS_TILED(n->client))
		n = find_any_leaf(root);

	set_master_node(root, n);
}

static void
split_rect(node_t *n, split_type_t s)
{
	const int16_t gap		  = conf.window_gap - conf.border_width;
	const int16_t pgap		  = conf.window_gap + conf.border_width;
	const double  ratio		  = normalize_split_ratio(n->split_ratio);
	const int16_t half_width  = (int16_t)((n->rectangle.width - gap) * ratio);
	const int16_t half_height = (int16_t)((n->rectangle.height - gap) * ratio);
	node_t		 *n1		  = n->first_child;
	node_t		 *n2		  = n->second_child;
	rectangle_t	 *fr		  = &n1->rectangle;
	rectangle_t	 *sr		  = &n2->rectangle;
	rectangle_t	  nr		  = n->rectangle;
	bool		  h			  = (s == HORIZONTAL_TYPE);
	node_t		 *nc		  = (h) ? n1 : n2;

	fr->x					  = nr.x;
	fr->y					  = nr.y;
	fr->width				  = (h) ? half_width : nr.width;
	fr->height				  = (h) ? nr.height : half_height;

	sr->x					  = (h) ? nr.x + fr->width + pgap : nr.x;
	sr->y					  = (h) ? nr.y : nr.y + fr->height + pgap;
	sr->width				  = (h) ? nr.width - fr->width - gap : nr.width;
	sr->height				  = (h) ? nr.height : nr.height - fr->height - gap;

	if (IS_EXTERNAL(nc) && IS_FLOATING(nc->client)) {
		*sr = nr;
	}
}

/* split_node - splits a node's rectangle in half, the split could be
 * vertical or horizontal depending on (width > height)? */
void
split_node(node_t *n, node_t *nd)
{
	if (IS_FLOATING(nd->client)) {
		n->first_child->rectangle = n->floating_rectangle = n->rectangle;
		return;
	}
	split_type_t s = n->split_type;
	if (s == DYNAMIC_TYPE) {
		/* horizontal split */
		s = (n->rectangle.width >= n->rectangle.height) ? HORIZONTAL_TYPE
														: VERTICAL_TYPE;
	}
	split_rect(n, s);
}

/* resize_subtree - recursively resizes the subtree of a given parent node based
 * on the parent's rectangle dimensions. */
void
resize_subtree(node_t *parent)
{
	if (parent == NULL)
		return;

	split_type_t s = parent->split_type;
	if (s == DYNAMIC_TYPE) {
		s = (parent->rectangle.width >= parent->rectangle.height)
				? HORIZONTAL_TYPE
				: VERTICAL_TYPE;
	}
	split_rect(parent, s);

	if (parent->first_child) {
		if (IS_INTERNAL(parent->first_child)) {
			resize_subtree(parent->first_child);
		}
	}
	if (parent->second_child) {
		if (IS_INTERNAL(parent->second_child)) {
			resize_subtree(parent->second_child);
		}
	}
}

/* apply_default_layout - applies the default tiling layout to a given tree
 *
 * recursively applies the default tiling layout to a node and its
 * descendants in the tree. The default layout splits nodes based on
 * their stored split type (if set) or their dimensions. */
void
apply_default_layout(node_t *root)
{
	if (root == NULL)
		return;

	if (root->first_child == NULL && root->second_child == NULL) {
		return;
	}

	rectangle_t	   r, r2 = {0};
	const uint16_t mgap	 = (conf.window_gap - conf.border_width);
	split_type_t   s	 = root->split_type;
	const double   ratio = normalize_split_ratio(root->split_ratio);
	if (s == DYNAMIC_TYPE) {
		s = (root->rectangle.width >= root->rectangle.height) ? HORIZONTAL_TYPE
															  : VERTICAL_TYPE;
	}
	/* determine split orientation based on node split type */
	if (s == HORIZONTAL_TYPE) {
		/* vertical split (side by side) */
		r.x		  = root->rectangle.x;
		r.y		  = root->rectangle.y;
		r.width	  = (uint16_t)((root->rectangle.width - mgap) * ratio);
		r.height  = root->rectangle.height;
		r2.x	  = (int16_t)(root->rectangle.x + r.width + conf.window_gap +
							  conf.border_width);
		r2.y	  = root->rectangle.y;
		r2.width  = root->rectangle.width - r.width - conf.window_gap -
					conf.border_width;
		r2.height = root->rectangle.height;
	} else {
		/* horizontal split (top and bottom) */
		r.x		  = root->rectangle.x;
		r.y		  = root->rectangle.y;
		r.width	  = root->rectangle.width;
		r.height  = (uint16_t)((root->rectangle.height - mgap) * ratio);
		r2.x	  = root->rectangle.x;
		r2.y	  = (int16_t)(root->rectangle.y + r.height + conf.window_gap +
							  conf.border_width);
		r2.width  = root->rectangle.width;
		r2.height = root->rectangle.height - r.height - conf.window_gap -
					conf.border_width;
	}

	/* this nested unreadable ternary code basically forces floating windows
	 * to retain their floating rectangle and give the full parent's
	 * rectangle to the other child. In some rare cases, this does not work
	 * as expected , I am still looking into it */
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

void
calculate_base_rect(rectangle_t *r, monitor_t *m)
{
	rectangle_t usable = get_usable_area(m);
	r->x			   = (int16_t)(usable.x + conf.window_gap);
	r->y			   = (int16_t)(usable.y + conf.window_gap);
	r->width =
		(uint16_t)(usable.width - 2 * conf.window_gap - 2 * conf.border_width);
	r->height =
		(uint16_t)(usable.height - 2 * conf.window_gap - 2 * conf.border_width);
}

/* default_layout - applies the default layout to the tree.
 *
 * initializes the default layout for the entire screen or
 * monitor. It sets up the initial rectangle for the root node with window
 * gaps and border widths in mind, then calls apply_default_layout to
 * recursively layout child nodes. */
void
default_layout(node_t *root)
{
	if (root == NULL)
		return;
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
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
		r.x		  = parent->rectangle.x;
		r.y		  = parent->rectangle.y;
		r.width	  = parent->rectangle.width;
		r.height  = (uint16_t)((parent->rectangle.height -
								(conf.window_gap - conf.border_width)) /
							   2);

		r2.x	  = parent->rectangle.x;
		r2.y	  = (int16_t)(parent->rectangle.y + r.height + conf.window_gap +
							  conf.border_width);
		r2.width  = parent->rectangle.width;
		r2.height = (uint16_t)(parent->rectangle.height - r.height -
							   conf.window_gap - conf.border_width);
		/* parent->first_child->rectangle	= r;
		parent->second_child->rectangle = r2; */

		parent->first_child->rectangle =
			((parent->second_child->client) &&
			 IS_FLOATING(parent->second_child->client))
				? parent->rectangle
			: ((parent->first_child->client) &&
			   IS_FLOATING(parent->first_child->client))
				? parent->first_child->floating_rectangle
				: r;
		parent->second_child->rectangle =
			((parent->first_child->client) &&
			 IS_FLOATING(parent->first_child->client))
				? parent->rectangle
			: ((parent->second_child->client) &&
			   IS_FLOATING(parent->second_child->client))
				? parent->second_child->floating_rectangle
				: r2;
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
 * marks the appropriate node as the master, and then calls
 * apply_master_layout to recursively apply the layout to the entire tree.
 */
void
master_layout(node_t *root, node_t *n)
{
	const double   ratio		= MASTER_RATIO;

	rectangle_t	   usable		= get_usable_area(curr_monitor);
	const uint16_t master_width = (uint16_t)(usable.width * ratio);
	const uint16_t r_width		= (uint16_t)(usable.width * (1 - ratio));

	/* find a node to be master if not provided */
	if (n == NULL) {
		n = find_any_leaf(root);
		if (n == NULL) {
			return;
		}
	}

	n->is_master		 = true;

	/* master rectangle */
	const rectangle_t r1 = {
		.x		= (int16_t)(usable.x + conf.window_gap),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(master_width - 2 * conf.window_gap),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap),
	};

	/* stack rectangle */
	const rectangle_t r2 = {
		.x		= (int16_t)(usable.x + master_width),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(r_width - (1 * conf.window_gap)),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap),
	};

	/* if node is a root, give it full screen rectangle and ignore this
	 * requests. Note, this happens when a user keeps deleting windows in a
	 * master layout till single window is mapped, which is the root window
	 */
	if (n->node_type == ROOT_NODE && n->first_child == NULL &&
		n->second_child == NULL) {
		n->rectangle = (rectangle_t){
			.x		= (int16_t)(usable.x + conf.window_gap),
			.y		= (int16_t)(usable.y + conf.window_gap),
			.width	= (uint16_t)(usable.width - 2 * conf.window_gap),
			.height = (uint16_t)(usable.height - 2 * conf.window_gap),
		};
		return;
	}

	n->rectangle	= r1;
	root->rectangle = r2;
	apply_master_layout(root);
}
/* recursively traverses the tree and sets the is_master flag
 * to false for all nodes. It's typically called before applying a new
 * layout to ensure a clean slate. */
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
void
stack_layout(node_t *root)
{
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
	root->rectangle = r;
	apply_stack_layout(root);
}

static void
_apply_grid_cells(node_t	 *r,
				  int		 *i,
				  int		  cols,
				  int		  rows,
				  int		  n,
				  uint16_t	  cell_w,
				  uint16_t	  cell_h,
				  rectangle_t base_rect)
{
	if (!r)
		return;
	if (IS_EXTERNAL(r) && r->client) {
		if (IS_FLOATING(r->client)) {
			r->rectangle = r->floating_rectangle;
		} else {
			int row	   = (*i) / cols;
			int col	   = (*i) % cols;
			int last_n = n - (rows - 1) * cols;

			if (row == rows - 1 && last_n < cols) {
				/* iff last row has fewer windows, widen them to fill the row */
				uint16_t last_w = base_rect.width / last_n;
				int		 lcol	= (*i) - (rows - 1) * cols;
				r->rectangle.x	= base_rect.x + lcol * last_w + conf.window_gap;
				r->rectangle.y	= base_rect.y + row * cell_h + conf.window_gap;
				r->rectangle.width =
					last_w - conf.window_gap - 2 * conf.border_width;
				r->rectangle.height =
					cell_h - conf.window_gap - 2 * conf.border_width;
			} else {
				r->rectangle.x = base_rect.x + col * cell_w + conf.window_gap;
				r->rectangle.y = base_rect.y + row * cell_h + conf.window_gap;
				r->rectangle.width =
					cell_w - conf.window_gap - 2 * conf.border_width;
				r->rectangle.height =
					cell_h - conf.window_gap - 2 * conf.border_width;
			}
			(*i)++;
		}
	}
	_apply_grid_cells(
		r->first_child, i, cols, rows, n, cell_w, cell_h, base_rect);
	_apply_grid_cells(
		r->second_child, i, cols, rows, n, cell_w, cell_h, base_rect);
}

void
count_windows(node_t *r, int *n)
{
	if (!r)
		return;
	if (IS_EXTERNAL(r) && r->client && !IS_FLOATING(r->client))
		(*n)++;
	count_windows(r->first_child, n);
	count_windows(r->second_child, n);
}

void
apply_grid_layout(node_t *root)
{
	if (!root)
		return;

	int n = 0;
	count_windows(root, &n);
	if (n == 0)
		return;

	int rows = 1;
	while ((rows + 1) * (rows + 1) <= n) rows++;
	int			cols   = (n + rows - 1) / rows;

	rectangle_t usable = root->rectangle;

	uint16_t	cell_w = usable.width / cols;
	uint16_t	cell_h = usable.height / rows;

	int			i	   = 0;
	_apply_grid_cells(root, &i, cols, rows, n, cell_w, cell_h, usable);
}

void
grid_layout(node_t *root)
{
	if (root == NULL)
		return;
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
	root->rectangle = r;
	apply_grid_layout(root);
}

static void
fix_floating_rects(node_t *root)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_FLOATING(root->client)) {
		root->rectangle = root->floating_rectangle;
		return;
	}
	fix_floating_rects(root->first_child);
	fix_floating_rects(root->second_child);
}

static void
collect_tiled_leaves(node_t *root, node_t **buf, int *n, int cap)
{
	if (!root || *n >= cap)
		return;
	if (IS_EXTERNAL(root) && root->client && !IS_FLOATING(root->client)) {
		buf[(*n)++] = root;
		return;
	}
	collect_tiled_leaves(root->first_child, buf, n, cap);
	collect_tiled_leaves(root->second_child, buf, n, cap);
}

static void
render_floating(node_t *root)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_FLOATING(root->client)) {
		tile(root);
		return;
	}
	render_floating(root->first_child);
	render_floating(root->second_child);
}

/*  monocle  */

static void
apply_monocle_layout(node_t *root, rectangle_t full)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client) {
		if (IS_FLOATING(root->client))
			root->rectangle = root->floating_rectangle;
		else
			root->rectangle = full;
		return;
	}
	apply_monocle_layout(root->first_child, full);
	apply_monocle_layout(root->second_child, full);
}

void
monocle_layout(node_t *root)
{
	if (!root)
		return;
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
	root->rectangle = r;
	apply_monocle_layout(root, r);
}

int
render_monocle(node_t *root)
{
	if (!root)
		return 0;
	if (IS_EXTERNAL(root) && root->client) {
		if (IS_FLOATING(root->client)) {
			tile(root);
		} else if (root->is_focused) {
			set_visibility(root->client->window, true);
			tile(root);
		} else {
			set_visibility(root->client->window, false);
		}
		return 0;
	}
	render_monocle(root->first_child);
	render_monocle(root->second_child);
	return 0;
}

/*  three-column  */

void
three_col_layout(node_t *root)
{
	if (!root)
		return;

	rectangle_t base = {0};
	calculate_base_rect(&base, curr_monitor);
	root->rectangle = base;
	fix_floating_rects(root);

	node_t *l[64];
	int		n = 0;
	collect_tiled_leaves(root, l, &n, 64);
	if (n == 0)
		return;

	node_t *m = find_layout_master(root);
	if (!m)
		return;
	if (!m->is_master)
		set_master_node(root, m);

	const int16_t  gap = (int16_t)conf.window_gap;
	const int16_t  bw  = (int16_t)conf.border_width;
	const uint16_t W   = base.width;
	const uint16_t H   = base.height;
	const int16_t  bx  = base.x;
	const int16_t  by  = base.y;
	const double   ratio =
		clamp_layout_ratio(normalize_split_ratio(root->split_ratio));

	if (n == 1) {
		m->rectangle = base;
		return;
	}
	if (n == 2) {
		uint16_t hw	 = (uint16_t)((W - gap - 2 * bw) / 2);
		m->rectangle = (rectangle_t){bx, by, hw, H};
		for (int i = 0; i < n; i++) {
			if (l[i] != m) {
				l[i]->rectangle = (rectangle_t){(int16_t)(bx + hw + gap + bw),
												by,
												(uint16_t)(W - hw - gap - bw),
												H};
				break;
			}
		}
		return;
	}

	uint16_t cw			= (uint16_t)(W * ratio - gap - 2 * bw);
	uint16_t side_total = (uint16_t)(W - cw - 2 * (gap + bw));
	uint16_t sw			= (uint16_t)(side_total / 2);
	int16_t	 lx			= bx;
	int16_t	 cx			= (int16_t)(bx + sw + gap + bw);
	int16_t	 rx			= (int16_t)(cx + cw + gap + bw);

	m->rectangle		= (rectangle_t){cx, by, cw, H};

	int nr = 0, nl = 0;
	int stack_idx = 0;
	for (int i = 0; i < n; i++) {
		if (l[i] == m)
			continue;
		if (stack_idx % 2 == 0)
			nr++;
		else
			nl++;
		stack_idx++;
	}

	int ri = 0, li = 0;
	stack_idx = 0;
	for (int i = 0; i < n; i++) {
		if (l[i] == m)
			continue;
		if (stack_idx % 2 == 0) {
			uint16_t ch =
				(uint16_t)((H - (uint16_t)(nr - 1) * (gap + bw)) / nr);
			int16_t cy		= (int16_t)(by + ri * (ch + gap + bw));
			l[i]->rectangle = (rectangle_t){rx, cy, sw, ch};
			ri++;
		} else {
			uint16_t ch =
				(uint16_t)((H - (uint16_t)(nl - 1) * (gap + bw)) / nl);
			int16_t cy		= (int16_t)(by + li * (ch + gap + bw));
			l[i]->rectangle = (rectangle_t){lx, cy, sw, ch};
			li++;
		}
		stack_idx++;
	}
}

/* deck */

void
deck_layout(node_t *r)
{
	if (!r)
		return;

	rectangle_t base = {0};
	calculate_base_rect(&base, curr_monitor);
	r->rectangle = base;
	fix_floating_rects(r);

	node_t *l[64];
	int		n = 0;
	collect_tiled_leaves(r, l, &n, 64);
	if (n == 0)
		return;

	node_t *m = find_layout_master(r);
	if (!m)
		return;
	if (!m->is_master)
		set_master_node(r, m);

	if (n == 1) {
		m->rectangle = base;
		return;
	}

	const int16_t gap = (int16_t)conf.window_gap;
	const int16_t bw  = (int16_t)conf.border_width;
	const double  ratio =
		clamp_layout_ratio(normalize_split_ratio(r->split_ratio));
	const uint16_t mw	  = (uint16_t)(base.width * ratio - gap - 2 * bw);
	const uint16_t sw	  = (uint16_t)(base.width - mw - gap - 2 * bw);
	const int16_t  sx	  = (int16_t)(base.x + mw + gap + bw);

	m->rectangle		  = (rectangle_t){base.x, base.y, mw, base.height};

	rectangle_t deck_rect = {sx, base.y, sw, base.height};
	for (int i = 0; i < n; i++) {
		if (l[i] != m)
			l[i]->rectangle = deck_rect;
	}
}

int
render_deck(node_t *r)
{
	if (!r)
		return 0;

	node_t *l[64];
	int		n = 0;
	collect_tiled_leaves(r, l, &n, 64);
	node_t *m = find_layout_master(r);

	if (m) {
		set_visibility(m->client->window, true);
		tile(m);
	}

	if (n >= 2) {
		node_t *v = NULL;
		for (int i = 0; i < n; i++) {
			if (l[i] != m && l[i]->is_focused) {
				v = l[i];
				break;
			}
		}
		if (!v) {
			for (int i = 0; i < n; i++) {
				if (l[i] != m) {
					v = l[i];
					break;
				}
			}
		}

		for (int i = 0; i < n; i++) {
			if (l[i] == m)
				continue;
			if (l[i] == v) {
				set_visibility(l[i]->client->window, true);
				tile(l[i]);
			} else {
				set_visibility(l[i]->client->window, false);
			}
		}
	}

	render_floating(r);
	return 0;
}

static void
update_split_ratio(node_t *parent, split_type_t s)
{
	if (parent == NULL || parent->first_child == NULL)
		return;

	const int16_t gap = conf.window_gap - conf.border_width;
	double		  r	  = 0.5;
	if (s == HORIZONTAL_TYPE) {
		int16_t avail = (int16_t)(parent->rectangle.width - gap);
		if (avail > 0) {
			r = (double)parent->first_child->rectangle.width / (double)avail;
		}
	} else if (s == VERTICAL_TYPE) {
		int16_t avail = (int16_t)(parent->rectangle.height - gap);
		if (avail > 0) {
			r = (double)parent->first_child->rectangle.height / (double)avail;
		}
	}
	parent->split_ratio = normalize_split_ratio(r);
}

/* flip_node - flips the node's orientation within its parent.
 * It only works if the node has a parent and a sibling. */
void
flip_node(node_t *node)
{
	if (node->parent == NULL) {
		return;
	}
	bool	vflip = (node->rectangle.width >= node->rectangle.height);
	node_t *p	  = node->parent;
	node_t *s	  = (p->first_child == node) ? p->second_child : p->first_child;
	if (s == NULL)
		return;
	rectangle_t *nr = &node->rectangle;
	rectangle_t *sr = &s->rectangle;
	rectangle_t	 pr = p->rectangle;
	nr->x			= pr.x;
	nr->y			= pr.y;
	if (vflip) {
		nr->width  = (pr.width - conf.window_gap) / 2;
		nr->height = pr.height;
		sr->x	   = pr.x + nr->width + conf.window_gap;
		sr->y	   = pr.y;
		sr->width  = pr.width - nr->width - conf.window_gap;
		sr->height = pr.height;
	} else {
		nr->width  = pr.width;
		nr->height = (pr.height - conf.window_gap) / 2;
		sr->x	   = pr.x;
		sr->y	   = pr.y + nr->height + conf.window_gap;
		sr->width  = pr.width;
		sr->height = pr.height - nr->height - conf.window_gap;
	}

	if (IS_INTERNAL(s)) {
		resize_subtree(s);
	}
	p->split_type = vflip ? HORIZONTAL_TYPE : VERTICAL_TYPE;
	update_split_ratio(p, p->split_type);
}

void
dynamic_resize(node_t *n, resize_t t)
{
	const int16_t step = 5;
	if (n && (curr_monitor->desk->layout == DECK ||
			  curr_monitor->desk->layout == THREE_COL)) {
		node_t *root = find_tree_root_local(n);
		if (!root)
			return;
		double ratio = normalize_split_ratio(root->split_ratio);
		bool   grow_master =
			(n->is_master && t == GROW) || (!n->is_master && t == SHRINK);
		ratio += grow_master ? 0.03 : -0.03;
		root->split_ratio = clamp_layout_ratio(ratio);
		arrange_tree(root, curr_monitor->desk->layout);
		return;
	}

	if (n == NULL || n->parent == NULL || IS_ROOT(n)) {
		return;
	}

	/* get the sibling node */
	node_t *s = (n->parent->first_child == n) ? n->parent->second_child
											  : n->parent->first_child;
	if (s == NULL) {
		return;
	}

	rectangle_t *nr = &n->rectangle;
	rectangle_t *sr = &s->rectangle;

	/* find the split orientation dynamically */
	bool		 vs = (nr->x == sr->x); /* nodes are stacked vertically */
	bool		 hs = (nr->y == sr->y); /* nodes are side-by-side */

	if (vs) {
		/* vertical resize */
		bool up = (nr->y < sr->y); /* `n` is above `s`? */

		if (t == GROW) {
			if (up) {
				if (sr->height > step) {
					nr->height += step;
					sr->y += step;
					sr->height -= step;
				}
			} else {
				if (sr->height > step) {
					nr->height += step;
					nr->y -= step;
					sr->height -= step;
				}
			}
		} else { /* SHRINK */
			if (nr->height > step) {
				nr->height -= step;
				if (up) {
					sr->y -= step;
				} else {
					nr->y += step;
				}
				sr->height += step;
			}
		}
	} else if (hs) {
		/* horizontal resize */
		bool left = (nr->x < sr->x); /* `n` is left of `s`? */
		if (t == GROW) {
			if (left) {
				if (sr->width > step) {
					nr->width += step;
					sr->x += step;
					sr->width -= step;
				}
			} else {
				if (sr->width > step) {
					nr->width += step;
					nr->x -= step;
					sr->width -= step;
				}
			}
		} else { /* SHRINK */
			if (nr->width > step) {
				nr->width -= step;
				if (left) {
					sr->x -= step;
				} else {
					nr->x += step;
				}
				sr->width += step;
			}
		}
	}
	if (vs || hs) {
		n->parent->split_type = vs ? VERTICAL_TYPE : HORIZONTAL_TYPE;
		update_split_ratio(n->parent, n->parent->split_type);
	}
	if (IS_INTERNAL(s)) {
		resize_subtree(s);
	}
}
