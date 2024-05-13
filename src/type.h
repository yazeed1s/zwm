#ifndef ZWM_TYPE_H
#define ZWM_TYPE_H

#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

#define CAP					3
#define W_GAP				10
#define MAXLEN				(2 << 7)
#define DLEN				(2 << 4)
#define NULL_STR			"N/A"
#define NORMAL_BORDER_COLOR 0x30302f
#define ACTIVE_BORDER_COLOR 0x83a598
#define BORDER_WIDTH		2

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
	GROW,
	SHRINK
} resize_t;

typedef enum {
	LEFT,
	RIGHT,
	UP,
	DOWN,
	NONE
} direction_t;

typedef enum {
	ERROR,
	INFO,
	DEBUG,
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
	DEFAULT,
	MASTER,
	STACK,
	GRID,
} layout_t;

typedef struct {
	int			 border_width;
	unsigned int border_color;
	int			 window_gap;
} config_t;

typedef struct {
	uint32_t	 id;
	uint32_t	 border_width;
	xcb_window_t window;
	xcb_atom_t	 type;
	bool		 is_managed;
	state_t		 state;
	posxy_t		 position_info;
} client_t;

typedef enum {
	ROOT_NODE	  = 1 << 1,
	INTERNAL_NODE = 1 << 2,
	EXTERNAL_NODE = 1 << 3
} node_type_t;

// clang-format off
/*
		I         ROOT (root is also an INTERNAL NODE, unless it is leaf by definition)
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

	- the behviour should be -> 
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
	node_t		*parent;
	node_t		*first_child;
	node_t		*second_child;
	uint32_t	 id;
	client_t	*client;
	node_type_t	 node_type;
	split_type_t split_type;
	rectangle_t	 rectangle;
	bool		 is_focused;
	bool		 is_master;
};

typedef struct {
	size_t	  current_ptr;
	client_t *arr;
} stack_t;

typedef struct {
	node_t	*tree;
	xcb_window_t top_w;
	char	 name[DLEN];
	uint8_t	 id;
	int		 n_count;
	layout_t layout;
	bool	 is_focused;
} desktop_t;

typedef struct {
	uint32_t	 id;
	xcb_window_t window;
	rectangle_t	 rectangle;
} bar_t;

typedef struct {
	desktop_t			 **desktops;
	xcb_connection_t	  *connection;
	xcb_ewmh_connection_t *ewmh;
	xcb_screen_t		  *screen;
	bar_t				  *bar;
	xcb_window_t		   root_window;
	split_type_t		   split_type;
	int					   n_of_desktops;
	int					   screen_nbr;
} wm_t;

// arg_t {char *, wm_t, resize_t, window_t, int}
typedef struct {
	const char		**cmd;
	const int		  argc;
	const int		  idx;
	const resize_t	  r;
	const layout_t	  t;
	const direction_t d;
} arg_t;

typedef struct {
	unsigned int mod;
	xcb_keysym_t keysym;
	int (*function_ptr)(const arg_t *);
	arg_t *arg;
} _key__t;

#endif // ZWM_TYPE_H