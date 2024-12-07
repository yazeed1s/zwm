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
#define MAXLEN				 (2 << 7)
#define DLEN				 (2 << 4)
#define NULL_STR			 "N/A"		   /* default null string value */
#define MONITOR_NAME		 "DEF_MONITOR" /* default monitor name */
#define ROOT_WINDOW			 "root ZWM"	   /* root window name */
#define W_GAP				 10			   /* default window gap */
#define NORMAL_BORDER_COLOR	 0x30302f	   /* default inactive border color */
#define ACTIVE_BORDER_COLOR	 0x83a598	   /* default active border color */
#define BORDER_WIDTH		 2			   /* default border width */
#define FOCUS_FOLLOW_POINTER true		   /* default focus follows mouse */

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
	GROW = 1, /* increase window size */
	SHRINK	  /* decrease window size */
} resize_t;

typedef enum {
	LEFT = 1, /* move/resize left */
	RIGHT,	  /* move/resize right */
	UP,		  /* move/resize up */
	DOWN,	  /* move/resize down */
	NONE	  /* no direction */
} direction_t;

typedef enum {
	ERROR,
	INFO,
	DEBUG,
	WARNING
} log_level_t;

/* predefined cursor types */
typedef enum {
	CURSOR_POINTER = 0, /* standard pointer */
	CURSOR_WATCH,		/* busy/wait cursor */
	CURSOR_MOVE,		/* move window cursor */
	CURSOR_XTERM,		/* text selection cursor, i think */
	CURSOR_NOT_ALLOWED, /* action not permitted */
	CURSOR_HAND2,		/* clickable item cursor */
	CURSOR_MAX			/* maximum cursor types */
} cursor_t;

/* bit flags to determine the change in monitors' state */
typedef enum {
	_NONE		 = (1 << 0), /* 00000001  no state change */
	CONNECTED	 = (1 << 1), /* 00000010  monitor connected */
	DISCONNECTED = (1 << 2), /* 00000100  monitor disconnected */
	LAYOUT		 = (1 << 3), /* 00001000  layout changed */
	/* note: layout also indcates changes in resoltuions, positions, or scale
	 * and oreintation */
} monitor_state_t;

typedef struct {
	uint16_t previous_x, previous_y;
	uint16_t current_x, current_y;
} posxy_t;

/* defines a rectangle (the window area or the tile/section area).
 * note: x and y can be signed (negative or positive), for example when a
 * portion of a window goes out of the visible area of the screen.
 */
typedef struct {
	int16_t	 x;
	int16_t	 y;
	uint16_t width;
	uint16_t height;
} rectangle_t;

typedef enum {
	TILED,	   /* automatically tiled */
	FLOATING,  /* freely movable */
	FULLSCREEN /* occupies entire screen */
} state_t;

typedef enum {
	DEFAULT = 1, /* standard manual tiling */
	MASTER,		 /* master-stack layout */
	STACK,		 /* stacked windows */
	GRID		 /* grid-based layout */
} layout_t;

typedef enum {
	WINDOW_TYPE_NORMAL		 = 1,
	WINDOW_TYPE_DOCK		 = 2,
	WINDOW_TYPE_TOOLBAR_MENU = 3,
	WINDOW_TYPE_UTILITY		 = 4,
	WINDOW_TYPE_SPLASH		 = 5,
	WINDOW_TYPE_DIALOG		 = 6,
	WINDOW_TYPE_NOTIFICATION = 7,
	WINDOW_TYPE_UNKNOWN		 = -1
} ewmh_window_type_t;

/* defines the client, like an opened application like firefox of a text editor.
 * every leaf node in the tree contains a non-null client, internal nodes ALWAYS
 * have null clients.
 */
typedef struct {
	uint32_t	 border_width;
	xcb_window_t window;
	xcb_atom_t	 type;
	state_t		 state;
} client_t;

/* types for tree nodes */
typedef enum {
	ROOT_NODE = 1, /* root usually holds the full rectangle of the screen */
	INTERNAL_NODE, /* internal nodes hold screen sections/tiles only */
	EXTERNAL_NODE  /* external nodes hold the actual windows */
} node_type_t;

/* the definition of the tree node */
typedef struct node_t node_t;
struct node_t {
	node_t	   *parent; /* a pointer to the parent, needed when traversing up */
	node_t	   *first_child;
	node_t	   *second_child;
	client_t   *client; /* the actual window this node hold, if it's a leaf */
	node_type_t node_type; /* node type */
	rectangle_t rectangle; /* the position and size for this node */
	rectangle_t floating_rectangle;
	bool		is_focused; /* whether or not this guy is focused */
	bool		is_master;	/* whether this node is the master node */
};

/* the defintion of a desktop.
 * each desktop has its own tree and layout.
 * the wm could have up to 10 desktops.
 */
typedef struct {
	node_t	*tree;		 /* the tree in this desktop */
	char	 name[DLEN]; /* the name, it stringfeis the index of this desktop */
	uint8_t	 id;		 /* the number of this desktop */
	uint8_t	 n_count;	 /* the number of active windows/external nodes */
	layout_t layout;	 /* the layout (master, default, stack) */
	bool	 is_focused; /* whether this is focused, only focused desktops
						  * are rendered */
} desktop_t;

/* monitor representation (also a linked list of monitors).
 * It is a physical output on your graphics driver, and it usually corresponds
 * to one connected screen.
 * Each monitore has its own virtual desktops by default */
typedef struct monitor_t monitor_t;
struct monitor_t {
	desktop_t		 **desktops;	/* array of desktops */
	monitor_t		  *next;		/* next monitor in list */
	char			   name[DLEN];	/* monitor name (e.g. HDMI or eDP) */
	uint32_t		   id;			/* monitor identifier, used with xinerama */
	xcb_randr_output_t randr_id;	/* randr output id, used with xrnadr */
	xcb_window_t	   root;		/* the root window on this monitor */
	rectangle_t		   rectangle;	/* monitor dimensions */
	bool			   is_wired;	/* connection status */
	bool			   is_focused;	/* focus status */
	bool			   is_occupied; /* window presence */
	bool			   is_primary;	/* primary monitor */
	uint8_t			   n_of_desktops; /* total desktops, defined in
									   * the config file  */
};

/* status bar representation */
typedef struct {
	uint32_t	 id;		/* bar identifier */
	xcb_window_t window;	/* xcb window reference */
	rectangle_t	 rectangle; /* bar dimensions */
} bar_t;

/* window manager global state */
typedef struct {
	xcb_connection_t	  *connection; /* xcb connection */
	xcb_ewmh_connection_t *ewmh;	   /* ewmh connection */
	xcb_screen_t		  *screen;	   /* global screen */
	bar_t				  *bar;		   /* status bar, should be moved (FIXME) */
	xcb_window_t		   root_window; /* root window */
	split_type_t		   split_type;	/* current split type */
	uint8_t				   screen_nbr;	/* screen number */
} wm_t;

/* argument structure for key bindings */
typedef struct {
	char	  **cmd;  /* command arguments, used for execp family actions */
	uint8_t		argc; /* argument count */
	uint8_t		idx;  /* target index, used for desktop switching */
	resize_t	r;	  /* resize operation */
	layout_t	t;	  /* layout type */
	direction_t d;	  /* movement direction */
	state_t		s;	  /* window state, used to change window state */
} arg_t;

/* key binding structure. used for the global fallback array in zwm.c */
typedef struct {
	uint32_t	 mod;			  /* modifier key */
	xcb_keysym_t keysym;		  /* key symbol */
	int (*function_ptr)(arg_t *); /* action function */
	arg_t *arg;					  /* function arguments */
} _key__t;

/* config key structure (linked list),
 * repesents the keys in the config file
 */
typedef struct conf_key_t conf_key_t;
struct conf_key_t {
	uint32_t	 mod;			  /* modifier key */
	xcb_keysym_t keysym;		  /* key symbol */
	int (*function_ptr)(arg_t *); /* action function */
	arg_t	   *arg;			  /* function arguments */
	conf_key_t *next;			  /* next key */
};

/* function mapping structure */
typedef struct {
	char *func_name;			  /* function name */
	int (*function_ptr)(arg_t *); /* function pointer */
} conf_mapper_t;

/* key mapping structure */
typedef struct {
	char		 key[6]; /* key representation */
	xcb_keysym_t keysym; /* key symbol */
} key_mapper_t;

/* window manager configuration */
typedef struct {
	uint16_t border_width;		   /* window border width */
	uint16_t window_gap;		   /* spacing between windows */
	uint32_t active_border_color;  /* focused window border color */
	uint32_t normal_border_color;  /* unfocused window border color */
	int		 virtual_desktops;	   /* number of virtual desktops */
	bool	 focus_follow_pointer; /* mouse focus tracking */
} config_t;

/* window rule structure (linked list) */
typedef struct rule_t rule_t;
struct rule_t {
	char	win_name[256]; /* window name pattern */
	state_t state;		   /* default window state */
	int		desktop_id;	   /* target desktop, if desired */
	rule_t *next;		   /* next rule in list */
};

/* queue node */
typedef struct queue_node_t queue_node_t;
struct queue_node_t {
	node_t		 *tree_node; /* tree node reference */
	queue_node_t *next;		 /* next queue node */
};

/* queue structure for level-order tree traversal */
typedef struct {
	queue_node_t *front; /* front of queue */
	queue_node_t *rear;	 /* rear of queue */
} queue_t;

/* event handler function pointer type */
typedef int (*event_handler_t)(const xcb_generic_event_t *);

/* event handler registration structure */
typedef struct {
	uint8_t			type;
	event_handler_t handler;
} event_handler_entry_t;

#endif // ZWM_TYPE_H
