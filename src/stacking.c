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

#include "stacking.h"
#include "client.h"
#include "ewmh.h"
#include "helper.h"
#include "state.h"
#include "type.h"
#include "xcb_util.h"

static layer_t
compute_layer(const client_t *c)
{
	/* stacking policy is
	 * top-down == fullscreen > dock/dialog/above/floating > normal > below */
	if (ewmh_has(c->ewmh_state, EWMH_STATE_FULLSCREEN) || IS_FULLSCREEN(c))
		return LAYER_FULLSCREEN;

	/* panels and "above-ish" stuff should beat tiled windows */
	if (c->ewmh_type == WINDOW_TYPE_DOCK ||
		c->ewmh_type == WINDOW_TYPE_NOTIFICATION ||
		c->ewmh_type == WINDOW_TYPE_TOOLBAR_MENU ||
		ewmh_has(c->ewmh_state, EWMH_STATE_MODAL) ||
		ewmh_has(c->ewmh_state, EWMH_STATE_ABOVE))
		return LAYER_ABOVE;

	if (ewmh_has(c->ewmh_state, EWMH_STATE_BELOW))
		return LAYER_BELOW;

	if (IS_FLOATING(c) && !ewmh_has(c->ewmh_state, EWMH_STATE_BELOW) &&
		!ewmh_has(c->ewmh_state, EWMH_STATE_FULLSCREEN))
		return LAYER_ABOVE;

	return LAYER_NORMAL;
}

static bool
client_is_hidden(const client_t *c)
{
	if (ewmh_has(c->ewmh_state, EWMH_STATE_HIDDEN))
		return true;
	return !check_window_map_state(c->window, WIN_MAP_STATE_VIEWABLE);
}

static uint8_t
transient_depth(const client_t *c)
{
	uint8_t		 depth = 0;
	xcb_window_t cur   = c->transient_for;
	while (cur) {
		node_t *p = find_node_global(cur);
		if (!p)
			break;
		depth++;
		cur = p->client->transient_for;
	}
	/* 0 = toplevel, 1 = direct transient */
	return depth;
}

uint64_t
stack_key(const client_t *c)
{
	layer_t		   l   = compute_layer(c);
	const uint8_t  d   = transient_depth(c);
	const uint32_t mru = c->mru_seq;
	const uint8_t  v   = client_is_hidden(c) ? 0 : 1;

	/* child dialog should not end up below the parent it blocks */
	if (c->transient_for) {
		node_t *p = find_node_global(c->transient_for);
		if (p && p->client) {
			layer_t pl = compute_layer(p->client);
			if (pl > l) {
				l = pl;
			}
		}
	}

	/* [1 bit visible][7 bits layer][8 bits transient depth][40 bits MRU] */
	return ((uint64_t)v << 63) | ((uint64_t)l << 56) | ((uint64_t)d << 40) |
		   ((uint64_t)mru);
}

uint32_t
get_next_mru_seq(monitor_t *m)
{
	if (!m) {
		return 1;
	}
	return ++m->mru_counter;
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
			   !IS_FLOATING(root->client)) { /* non floating */
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
			int32_t area_i = s[i]->rectangle.height * s[i]->rectangle.width;
			int32_t area_j = s[j]->rectangle.height * s[j]->rectangle.width;
			if (area_j > area_i) {
				/* swap */
				node_t *temp = s[i];
				s[i]		 = s[j];
				s[j]		 = temp;
			}
		}
	}
}

static void
collect_clients(node_t *n, stack_item_t **out, size_t *cap, size_t *len)
{
	if (!n)
		return;
	if (n->client && n->client->window != XCB_NONE &&
		!n->client->override_redirect) {
		if (*len == *cap) {
			*cap = (*cap ? *cap * 2 : 16);
			*out = realloc(*out, *cap * sizeof(**out));
			if (!*out)
				return;
		}
		(*out)[*len].c	 = n->client;
		(*out)[*len].key = stack_key(n->client);
		(*len)++;
	}
	collect_clients(n->first_child, out, cap, len);
	collect_clients(n->second_child, out, cap, len);
}

static void
collect_clients_global(stack_item_t **out, size_t *cap, size_t *len)
{
	monitor_t *m = head_monitor;
	while (m) {
		for (int i = 0; i < m->n_of_desktops; i++) {
			desktop_t *d = m->desktops[i];
			if (!d || !d->tree)
				continue;
			collect_clients(d->tree, out, cap, len);
		}
		m = m->next;
	}
}

static int
cmp_stack_item(const void *pa, const void *pb)
{
	const stack_item_t *a = pa, *b = pb;
	if (a->key < b->key)
		return -1; /* bottom first */
	if (a->key > b->key)
		return +1;
	if (!a->c || !b->c)
		return 0;
	if (a->c->window < b->c->window)
		return -1;
	if (a->c->window > b->c->window)
		return +1;
	return 0;
}

void
restack(void)
{
	stack_item_t *v	  = NULL;
	size_t		  cap = 0, len = 0;
	collect_clients_global(&v, &cap, &len);
	if (!v || !len) {
		xcb_ewmh_set_client_list_stacking(wm->ewmh, wm->screen_nbr, 0, NULL);
		free(v);
		return;
	}

	qsort(v, len, sizeof *v, cmp_stack_item);

	/* bottom -> top, visible windows only */
	client_t *prev = NULL;
	for (size_t i = 0; i < len; i++) {
		client_t *c = v[i].c;
		if (!c || client_is_hidden(c))
			continue;
		if (!prev) {
			lower_window(c->window);
		} else {
			window_above(c->window, prev->window);
		}
		prev = c;
	}

	/* fullscreen wins over normal tiling order */
	for (size_t i = 0; i < len; i++) {
		client_t *c = v[i].c;
		if (c && !client_is_hidden(c) && IS_FULLSCREEN(c)) {
			raise_window(c->window);
		}
	}

	/* transients/dialogs need one final raise after fullscreen */
	for (size_t i = 0; i < len; i++) {
		client_t *c = v[i].c;
		if (c && !client_is_hidden(c) &&
			(c->transient_for != XCB_NONE ||
			 ewmh_has(c->ewmh_state, EWMH_STATE_MODAL) ||
			 c->ewmh_type == WINDOW_TYPE_DIALOG)) {
			raise_window(c->window);
		}
	}

	/* keep pagers */
	xcb_window_t *stack = calloc(len, sizeof(*stack));
	if (stack) {
		for (size_t i = 0; i < len; i++) {
			stack[i] = v[i].c ? v[i].c->window : XCB_NONE;
		}
		xcb_ewmh_set_client_list_stacking(wm->ewmh, wm->screen_nbr, len, stack);
		free(stack);
	}
	xcb_flush(wm->connection);

	free(v);
}

/* deprecated */
#if 0
void
restack(void)
{
	node_t *root = curr_monitor->desk->tree;
	if (root == NULL)
		return;

	int		 stack_size = 5;
	int		 top		= -1;
	node_t **stack		= (node_t **)malloc(sizeof(node_t *) * stack_size);
	if (stack == NULL) {
_LOG_(ERROR, "cannot allocate stack");
		return;
	}
	stack_and_lower(
		root, stack, &top, stack_size, curr_monitor->desk->layout == STACK);
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
#endif

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
