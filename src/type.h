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

#ifndef ZWM_TYPE_H
#define ZWM_TYPE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#define CAP					 3
#define W_GAP				 10
#define MAXLEN				 (2 << 7)
#define DLEN				 (2 << 4)
#define NULL_STR			 "N/A"
#define MONITOR_NAME		 "DEF_MONITOR"
#define ROOT_WINDOW			 "root ZWM"
#define NORMAL_BORDER_COLOR	 0x30302f
#define ACTIVE_BORDER_COLOR	 0x83a598
#define BORDER_WIDTH		 2
#define FOCUS_FOLLOW_POINTER true

typedef xcb_connection_t xcb_conn_t;

typedef enum {
	HORIZONTAL_TYPE,
	VERTICAL_TYPE,
	DYNAMIC_TYPE
} split_type_t;

typedef enum {
	HORIZONTAL_FLIP,
	VERTICAL_FLIP
} flip_t;

typedef enum {
	GROW   = 1 << 1,
	SHRINK = 1 << 2,
} resize_t;

typedef enum {
	LEFT  = 1 << 1,
	RIGHT = 1 << 2,
	UP	  = 1 << 3,
	DOWN  = 1 << 4,
	NONE  = 1 << 5
} direction_t;

typedef enum {
	ERROR,
	INFO,
	DEBUG,
	WARNING
} log_level_t;

typedef struct {
	// 2^16 = 65535
	uint16_t previous_x, previous_y;
	uint16_t current_x, current_y;
} posxy_t;

typedef struct {
	int16_t	 x;
	int16_t	 y;
	uint16_t width;
	uint16_t height;
} rectangle_t;

typedef enum {
	TILED,
	FLOATING,
	FULLSCREEN
} state_t;

typedef enum {
	DEFAULT = 1 << 1,
	MASTER	= 1 << 2,
	STACK	= 1 << 3,
	GRID	= 1 << 4,
} layout_t;

typedef struct {
	uint32_t	 border_width;
	xcb_window_t window;
	xcb_atom_t	 type;
	state_t		 state;
	/* uint32_t	 id; */
	/* bool		 is_managed; */
	/* posxy_t		 position_info; */
} client_t;

typedef enum {
	ROOT_NODE	  = 1 << 1,
	INTERNAL_NODE = 1 << 2,
	EXTERNAL_NODE = 1 << 3
} node_type_t;

// clang-format off
/*
		I         ROOT (root is also an INTERNAL NODE, unless it is a leaf by definition)
	  /   \
	 I     I      INTERNAL NODES
	/     / \
   E     E   E    EXTERNAL NODES (or leaves)

	I (if parent of E) = screen sections/partitions in which windows can be mapped
	(displayed). E = windows in every screen partition. windows are basically the
	leaves of a full binary tree. E nodes, on the screen, evenly share the width &
	height of their I parent, and the I's x & y as well.

		I
	  /   \
	 I     I
	/     / \
   E     E   I
			/ \
		   E   E

	[I] partitions/sections can be:
	1- container of other partitions
	2- contained by other partitions

	- the behaviour should be ->
	- 1,2,3 are leaves (EXTERNAL_NODE) 
	- a,b are internal nodes (INTERNAL_NODE), or screen sections/partitions

			 1                          a                          a
			 ^                         / \                        / \
						 --->         1   2         --->         1   b
										  ^                         / \
																   2   3
																	   ^

	+-----------------------+  +-----------------------+  +-----------------------+
	|                       |  |           |           |  |           |           |
	|                       |  |           |           |  |           |     2     |
	|                       |  |           |           |  |           |           |
	|           1           |  |     1     |     2     |  |     1     |-----------|
	|           ^           |  |           |     ^     |  |           |           |
	|                       |  |           |           |  |           |     3     |
	|                       |  |           |           |  |           |     ^     |
	+-----------------------+  +-----------------------+  +-----------------------+

*/
// clang-format on

typedef struct node_t node_t;
struct node_t {
	node_t	   *parent;
	node_t	   *first_child;
	node_t	   *second_child;
	client_t   *client;
	node_type_t node_type;
	rectangle_t rectangle;
	rectangle_t floating_rectangle;
	bool		is_focused;
	bool		is_master;
	/* uint32_t	id; */
	/* split_type_t split_type; */
};

typedef struct {
	node_t		*tree;
	xcb_window_t top_w;
	char		 name[DLEN];
	uint8_t		 id;
	uint8_t		 n_count;
	layout_t	 layout;
	bool		 is_focused;
} desktop_t;

typedef struct {
	desktop_t		 **desktops;
	char			   name[DLEN];
	uint32_t		   id;
	xcb_randr_output_t randr_id;
	xcb_window_t	   root;
	rectangle_t		   rectangle;
	bool			   is_wired;
	bool			   is_focused;
	bool			   is_occupied;
	bool			   is_primary;
	uint8_t			   n_of_desktops;
} monitor_t;

typedef struct {
	uint32_t	 id;
	xcb_window_t window;
	rectangle_t	 rectangle;
} bar_t;

typedef struct {
	monitor_t			 **monitors;
	uint8_t				   n_of_monitors;
	xcb_connection_t	  *connection;
	xcb_ewmh_connection_t *ewmh;
	xcb_screen_t		  *screen;
	bar_t				  *bar;
	xcb_window_t		   root_window;
	split_type_t		   split_type;
	// uint8_t				   n_of_desktops;
	uint8_t				   screen_nbr;
} wm_t;

typedef struct {
	char	  **cmd;
	uint8_t		argc;
	uint8_t		idx;
	resize_t	r;
	layout_t	t;
	direction_t d;
} arg_t;

typedef struct {
	uint32_t	 mod;
	xcb_keysym_t keysym;
	int (*function_ptr)(arg_t *);
	arg_t *arg;
} _key__t;

typedef struct {
	char *func_name;
	int (*function_ptr)(arg_t *);
} conf_mapper_t;

typedef struct {
	char		 key[6];
	xcb_keysym_t keysym;
} key_mapper_t;

typedef struct {
	uint16_t border_width;
	uint16_t window_gap;
	uint32_t active_border_color;
	uint32_t normal_border_color;
	int		 virtual_desktops;
	bool	 focus_follow_pointer;
	// _key__t *keys;
	// size_t	 key_size;
} config_t;

typedef struct {
	char	win_name[256];
	state_t state;
	int		desktop_id;
} rule_t;
#endif // ZWM_TYPE_H
