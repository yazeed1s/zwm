/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2024, Yazeed Alharthi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* includes */

#include <stdbool.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <stdint.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/randr.h>
#include <signal.h>
#include <xcb/xcb_cursor.h>
#include <sys/types.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <stddef.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdarg.h>
#include <time.h>
#include <xcb/xinerama.h>

/* macros */

#define MAX_RULES (2 << 2)
#define KNRM								   "\x1B[0m"
#define KRED								   "\x1B[31m"
#define KGRN								   "\x1B[32m"
#define KYEL								   "\x1B[33m"
#define KBLU								   "\x1B[34m"
#define KPNK								   "\x1B[95m"
#define KORG								   "\x1B[38;5;214m"
#define KCYN								   "\x1B[36m"
#define KWHT								   "\x1B[37m"
#define LEN(x)								   (sizeof(x) / sizeof(*x))
#define CLEANMASK(mask)						   (mask & ~(0 | XCB_MOD_MASK_LOCK))
#define IS_TILED(c)							   (c->state == TILED)
#define IS_FLOATING(c)						   (c->state == FLOATING)
#define IS_FULLSCREEN(c)					   (c->state == FULLSCREEN)
#define IS_EXTERNAL(n)						   (n->node_type == EXTERNAL_NODE)
#define IS_INTERNAL(n)						   (n->node_type == INTERNAL_NODE)
#define IS_ROOT(n)							   (n->node_type == ROOT_NODE)
#define DEFINE_KEY(mask, keysym, handler, arg) {mask, keysym, 0, handler, arg}
#define DEFINE_MAPPING(name, value)			   {name, value}
#define _KEY(k)								   XK_##k
#define _FREE_(ptr)                                                            \
	do {                                                                       \
		if (ptr) {                                                             \
			free(ptr);                                                         \
			ptr = NULL;                                                        \
		}                                                                      \
	} while (0)
#define _LOG_(level, format, ...)                                              \
	do {                                                                       \
		log_message(level,                                                     \
					"[%s:%s():%d] " format,                                    \
					__FILE__,                                                  \
					__func__,                                                  \
					__LINE__,                                                  \
					##__VA_ARGS__);                                            \
	} while (0)
#define MAX(a, b)                                                              \
	({                                                                         \
		__typeof__(a) _a = (a);                                                \
		__typeof__(b) _b = (b);                                                \
		_a > _b ? _a : _b;                                                     \
	})
#define WINDOW_X		(XCB_CONFIG_WINDOW_X)
#define WINDOW_Y		(XCB_CONFIG_WINDOW_Y)
#define WINDOW_W		(XCB_CONFIG_WINDOW_WIDTH)
#define WINDOW_H		(XCB_CONFIG_WINDOW_HEIGHT)
#define S_NOTIFY		(XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY)
#define S_REDIRECT		(XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)
#define MOVE_RESIZE		(WINDOW_X | WINDOW_Y | WINDOW_W | WINDOW_H)
#define MOVE			(WINDOW_X | WINDOW_Y)
#define RESIZE			(WINDOW_W | WINDOW_H)
#define SUBSTRUCTURE	(S_NOTIFY | S_REDIRECT)
#define PROPERTY_CHANGE (XCB_EVENT_MASK_PROPERTY_CHANGE)
#define FOCUS_CHANGE	(XCB_EVENT_MASK_FOCUS_CHANGE)
#define ENTER_WINDOW	(XCB_EVENT_MASK_ENTER_WINDOW)
#define LEAVE_WINDOW	(XCB_EVENT_MASK_LEAVE_WINDOW)
#define BUTTON_PRESS	(XCB_EVENT_MASK_BUTTON_PRESS)
#define BUTTON_RELEASE	(XCB_EVENT_MASK_BUTTON_RELEASE)
#define POINTER_MOTION	(XCB_EVENT_MASK_POINTER_MOTION)
#define ALT				(XCB_MOD_MASK_1)
#define SUPER			(XCB_MOD_MASK_4)
#define SHIFT			(XCB_MOD_MASK_SHIFT)
#define CTRL			(XCB_MOD_MASK_CONTROL)
#define CLICK_TO_FOCUS	(XCB_BUTTON_INDEX_1)
#define CLIENT_EVENT_MASK                                                      \
	(PROPERTY_CHANGE | FOCUS_CHANGE | ENTER_WINDOW | LEAVE_WINDOW)
#define ROOT_EVENT_MASK                                                        \
	(SUBSTRUCTURE | BUTTON_PRESS | FOCUS_CHANGE | ENTER_WINDOW)
#define MASTER_RATIO		 0.70
#define NUMBER_OF_DESKTOPS	 7
#define WM_NAME				 "zwm"
#define WM_CLASS_NAME		 "null"
#define WM_INSTANCE_NAME	 "null"
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
#define FOCUS_FOLLOW_SPAWN	 false		   /* default focus follows spawn */
#define RESTORE_LAST_FOCUS	 false		   /* default restore last window */

/* enums and types */











/* type aliases */
typedef xcb_connection_t	  xcb_conn_t;
typedef xcb_generic_error_t	  xcb_error_t;
typedef xcb_void_cookie_t	  xcb_cookie_t;
typedef xcb_ewmh_connection_t xcb_ewmh_conn_t;
typedef xcb_generic_event_t	  xcb_event_t;

/* Visibility type returned by _leaf_visibility_() in view.h. */
typedef enum {
	LEAF_VISIBLE_TILED,	   /* map + apply geometry */
	LEAF_HIDDEN_TILED,	   /* unmap */
	LEAF_VISIBLE_FLOATING, /* map + apply floating geometry */
	LEAF_IGNORED,		   /* internal node, skip this */
} leaf_visibility_t;

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
	HORIZONTAL_DIR = 1,
	VERTICAL_DIR,
} resize_dir_t;

typedef enum {
	LEFT = 1, /* move left */
	RIGHT,	  /* move right */
	UP,		  /* move up */
	DOWN,	  /* move down */
	NONE	  /* no direction */
} direction_t;

typedef enum {
	NEXT = 1,
	PREV
} traversal_t;

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

typedef enum {
	MOUSE_OP_NONE = 0,
	MOUSE_OP_MOVE_FLOATING,
	MOUSE_OP_RESIZE_FLOATING,
	MOUSE_OP_RESIZE_TILED,
} mouse_op_t;

enum {
	RESIZE_EDGE_LEFT   = (1 << 0),
	RESIZE_EDGE_RIGHT  = (1 << 1),
	RESIZE_EDGE_TOP	   = (1 << 2),
	RESIZE_EDGE_BOTTOM = (1 << 3),
};

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

typedef struct {
	int16_t top;
	int16_t right;
	int16_t bottom;
	int16_t left;
} padding_t;

typedef enum {
	TILED,	   /* automatically tiled */
	FLOATING,  /* freely movable */
	FULLSCREEN /* occupies entire screen */
} state_t;

typedef enum {
	DEFAULT = 1, /* standard manual tiling */
	MASTER,		 /* master-stack layout */
	STACK,		 /* stacked windows */
	GRID,		 /* grid-based layout */
	MONOCLE,	 /* one maximised window, others hidden */
	THREE_COL,	 /* left stack | center master | right stack */
	DECK		 /* master left, cycling stack right */
} layout_t;

typedef enum {
	WINDOW_TYPE_NORMAL		 = 1,
	WINDOW_TYPE_DOCK		 = 2,
	WINDOW_TYPE_TOOLBAR_MENU = 3,
	WINDOW_TYPE_UTILITY		 = 4,
	WINDOW_TYPE_SPLASH		 = 5,
	WINDOW_TYPE_DIALOG		 = 6,
	WINDOW_TYPE_NOTIFICATION = 7,
	WINDOW_TYPE_DESKTOP		 = 8,
	WINDOW_TYPE_UNKNOWN		 = -1
} ewmh_window_type_t;

typedef enum {
	STATE_UNSUPPORTED = 0,
	STATE_FULLSCREEN,
	STATE_BELOW,
	STATE_ABOVE,
	STATE_HIDDEN,
	STATE_STICKY,
	STATE_DEMANDS_ATTENTION
} window_state_type_t;

typedef enum {
	STATE_ACTION_INVALID = 0,
	STATE_ACTION_REMOVE,
	STATE_ACTION_ADD,
	STATE_ACTION_TOGGLE
} window_state_action_t;

typedef enum {
	CLIENT_MESSAGE_UNSUPPORTED = 0,
	CLIENT_MESSAGE_CURRENT_DESKTOP,
	CLIENT_MESSAGE_WINDOW_STATE,
	CLIENT_MESSAGE_ACTIVE_WINDOW,
	CLIENT_MESSAGE_WINDOW_DESKTOP,
	CLIENT_MESSAGE_CLOSE_WINDOW
} client_message_type_t;

/* window map state types for checking window visibility */
typedef enum {
	WIN_MAP_STATE_UNMAPPED =
		XCB_MAP_STATE_UNMAPPED, /* 0 - window is unmapped */
	WIN_MAP_STATE_UNVIEWABLE =
		XCB_MAP_STATE_UNVIEWABLE, /* 1 - window is unviewable */
	WIN_MAP_STATE_VIEWABLE =
		XCB_MAP_STATE_VIEWABLE, /* 2 - window is viewable */
	WIN_MAP_STATE_ANY = 0xFF	/* match any state */
} win_map_state_t;

typedef struct icccm_props_t icccm_props_t;
struct icccm_props_t {
	bool take_focus;
	bool input_hint;
	bool delete_window;
};

/* bitmask enum for _NET_WM_STATE */
typedef enum {
	EWMH_STATE_NONE			= 0,
	EWMH_STATE_ABOVE		= 1u << 0,
	EWMH_STATE_BELOW		= 1u << 1,
	EWMH_STATE_FULLSCREEN	= 1u << 2,
	EWMH_STATE_MODAL		= 1u << 3,
	EWMH_STATE_HIDDEN		= 1u << 4,
	EWMH_STATE_STICKY		= 1u << 5,
	EWMH_STATE_DEMANDS_ATTN = 1u << 6,
} ewmh_state_t;

typedef enum {
	LAYER_DESKTOP	 = 0, /* if you later add WINDOW_TYPE_DESKTOP */
	LAYER_BELOW		 = 1,
	LAYER_NORMAL	 = 2,
	LAYER_ABOVE		 = 3,
	LAYER_FULLSCREEN = 4,
} layer_t;

/* defines the client, like an opened application like firefox of a text editor.
 * every leaf node in the tree contains a non-null client, internal nodes ALWAYS
 * have null clients.
 */
typedef struct {
	/*char			 class_name[MAXLEN];*/
	/*char			 wm_name[MAXLEN];*/
	xcb_window_t	   window;
	xcb_window_t	   transient_for; /* from WM_TRANSIENT_FOR (0 if none) */
	xcb_atom_t		   type;
	uint32_t		   border_width;
	uint32_t		   mru_seq; /* bump on focus/raise */
	xcb_size_hints_t   size_hints;
	ewmh_state_t	   ewmh_state; /* from _NET_WM_STATE (bitmask enum) */
	icccm_props_t	   props;
	ewmh_window_type_t ewmh_type; /* from _NET_WM_WINDOW_TYPE */
	state_t			   state;
	bool			   override_redirect; /* from X attributes */
} client_t;

typedef struct {
	client_t *c;
	uint64_t  key;
} stack_item_t;

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
	rectangle_t rectangle; /* the position and size for this node */
	rectangle_t floating_rectangle;
	node_type_t node_type;	  /* node type */
	split_type_t split_type;  /* split orientation for DEFAULT layout */
	double		 split_ratio; /* split ratio for DEFAULT layout */
	bool		 is_focused;  /* whether or not this guy is focused */
	bool		 is_master;	  /* whether this node is the master node */
};

/* the defintion of a desktop.
 * each desktop has its own tree and layout.
 * the wm could have up to 10 desktops.
 */
typedef struct {
	node_t		*tree; /* the tree in this desktop */
	/* node_t	*node;		 focused node */
	xcb_window_t last_focused;
	node_t		*logical_focus; /* the tiled node this layout remembers as
								 * selected; separate from X input focus so
								 * focusing a floating window does not erase
								 * the MONOCLE/DECK tiled selection */
	uint16_t	 id;			/* the number of this desktop */
	uint16_t	 n_count;	 /* the number of active windows/external nodes */
	layout_t	 layout;	 /* the layout (master, default, stack) */
	bool		 is_focused; /* whether this is focused, only focused desktops
							  * are rendered */
	char name[DLEN]; /* the name, it stringfeis the index of this desktop */
} desktop_t;

/* monitor representation (also a linked list of monitors).
 * It is a physical output on your graphics driver, and it usually corresponds
 * to one connected screen.
 * Each monitore has its own virtual desktops by default */
typedef struct monitor_t monitor_t;
struct monitor_t {
	desktop_t		 **desktops;	/* array of desktops */
	desktop_t		  *desk;		/* focused desktop */
	monitor_t		  *next;		/* next monitor in list */
	rectangle_t		   rectangle;	/* monitor dimensions */
	padding_t		   padding;		/* EWMH strut padding */
	xcb_randr_output_t randr_id;	/* randr output id, used with xrnadr */
	xcb_window_t	   root;		/* the root window on this monitor */
	uint32_t		   id;			/* monitor identifier, used with xinerama */
	uint32_t		   mru_counter; /* per-monitor MRU counter */
	uint16_t		   n_of_desktops; /* total desktops, defined in
									   * the config file  */
	char			   name[DLEN];	  /* monitor name (e.g. HDMI or eDP) */
	bool			   is_wired;	  /* connection status */
	bool			   is_focused;	  /* focus status */
	bool			   is_occupied;	  /* window presence */
	bool			   is_primary;	  /* primary monitor */
};

/* window manager global state */
typedef struct {
	xcb_connection_t	  *connection;	/* xcb connection */
	xcb_ewmh_connection_t *ewmh;		/* ewmh connection */
	xcb_screen_t		  *screen;		/* global screen */
	xcb_window_t		   root_window; /* root window */
	split_type_t		   split_type;	/* current split type */
	uint8_t				   screen_nbr;	/* screen number */
} wm_t;

/* argument structure for key bindings */
typedef struct {
	char	   **cmd;  /* command arguments, used for execp family actions */
	uint8_t		 argc; /* argument count */
	uint8_t		 idx;  /* target index, used for desktop switching */
	resize_t	 r;	   /* resize operation */
	resize_dir_t rd;   /* resize direction */
	layout_t	 t;	   /* layout type */
	traversal_t	 tr;   /* traversal direction */
	direction_t	 d;	   /* movement direction */
	state_t		 s;	   /* window state, used to change window state */
} arg_t;

/* key binding structure. used for the global fallback array in zwm.c */
typedef struct {
	uint32_t	 mod;		 /* modifier key */
	xcb_keysym_t keysym;	 /* key symbol */
	xcb_keycode_t keycode;	 /* physical keycode resolved from keysym */
	int (*execute)(arg_t *); /* action function */
	arg_t *arg;				 /* function arguments */
} _key__t;

/* config key structure (linked list),
 * represents the keys in the config file
 */
typedef struct conf_key_t conf_key_t;
struct conf_key_t {
	int (*execute)(arg_t *); /* action function */
	arg_t		*arg;		 /* function arguments */
	conf_key_t	*next;		 /* next key */
	uint32_t	 mod;		 /* modifier key */
	xcb_keysym_t keysym;	 /* key symbol */
	xcb_keycode_t keycode;	 /* physical keycode resolved from keysym */
};

/* function mapping structure */
typedef struct {
	char *func_name;		 /* function name */
	int (*execute)(arg_t *); /* function pointer */
} conf_mapper_t;

/* key mapping structure */
typedef struct {
	char		 key[10]; /* key representation */
	xcb_keysym_t keysym;  /* key symbol */
} key_mapper_t;

/* window manager configuration */
typedef struct {
	uint16_t border_width;		   /* window border width */
	uint16_t window_gap;		   /* spacing between windows */
	uint32_t active_border_color;  /* focused window border color */
	uint32_t normal_border_color;  /* unfocused window border color */
	int		 virtual_desktops;	   /* number of virtual desktops */
	bool	 focus_follow_pointer; /* mouse focus tracking */
	bool	 focus_follow_spawn;   /* focus redirection tracking */
	bool	 restore_last_focus;
	/* restore previously focused window when switching
								desktops (if layout != STACK) */
} config_t;

/* drag state helps tracks active drag session */
typedef struct {
	xcb_window_t window;   /* window being dragged */
	node_t		*src_node; /* original node */
	int16_t		 start_x;  /* initial cursor x */
	int16_t		 start_y;  /* initial cursor y */
	bool		 active;   /* drag in progress */
	bool		 kbd_mode; /* keyboard-driven drag */
	int16_t		 cur_x, cur_y;
	node_t		*last_target; /* cached target leaf for preview */
	bool		 preview_active;
	desktop_t	*original_desktop;
	rectangle_t	 original_rect;
} drag_state_t;

typedef struct {
	mouse_op_t	 op;
	xcb_window_t window;
	node_t		*node;
	node_t		*parent;
	int16_t		 start_x;
	int16_t		 start_y;
	rectangle_t	 start_rect;
	split_type_t split_type;
	double		 start_ratio;
	int16_t		 first_size;
	int16_t		 avail;
	uint8_t		 edges;
} mouse_state_t;

typedef struct strut_window_node_t {
	xcb_window_t				win;
	struct strut_window_node_t *next;
} strut_win_node_t;

/* window rule structure (linked list) */
typedef struct rule_t rule_t;
struct rule_t {
	rule_t *next;		   /* next rule in list */
	int		desktop_id;	   /* target desktop, if desired */
	state_t state;		   /* default window state */
	char	win_name[256]; /* window name pattern */
};

/* queue node */
typedef struct queue_node_t queue_node_t;
struct queue_node_t {
	queue_node_t *next;		 /* next queue node */
	queue_node_t *prev;		 /* previous queue node */
	node_t		 *tree_node; /* tree node reference */
};

/* queue structure for level-order tree traversal */
typedef struct {
	queue_node_t *front; /* front of queue */
	queue_node_t *rear;	 /* rear of queue */
	size_t		  size;	 /* number of items in queue */
} queue_t;

/* event handler registration structure */
typedef struct {
	uint8_t type;
	int (*handle)(const xcb_generic_event_t *);
} event_handler_entry_t;



/* function declarations */

/* clang-format off */
static int exec_process(arg_t *arg);
static int layout_handler(arg_t *arg);
static int cycle_win_wrapper(arg_t *arg);
static int set_fullscreen_wrapper(arg_t *arg);
static int flip_node_wrapper(arg_t *arg);
static int reload_config_wrapper(arg_t *arg);
static int dynamic_resize_wrapper(arg_t *arg);
static int gap_handler(arg_t *arg);
static int traverse_stack_wrapper(arg_t *arg);
static int cycle_monitors(arg_t *arg);
static void move_mouse_to_monitor(monitor_t *m);
static int set_fullscreen(node_t *n, bool flag);
static int swap_node_wrapper(arg_t *arg);
static int change_state(arg_t *arg);
static int close_or_kill_wrapper(arg_t *arg);
static int transfer_node_wrapper(arg_t *arg);
static int switch_desktop_wrapper(arg_t *arg);
static int cycle_desktop_wrapper(arg_t *arg);
static int grow_floating_window(arg_t *arg);
static int shrink_floating_window(arg_t *arg);
static int resize_floating_window(arg_t *arg);
static int shift_floating_window(arg_t *arg);
static int start_keyboard_drag_wrapper(arg_t *arg);
static int grab_keys(xcb_conn_t *conn, xcb_window_t win);
static void ungrab_keys(xcb_conn_t *conn, xcb_window_t win);
static xcb_keycode_t *get_keycode(xcb_keysym_t keysym, xcb_conn_t *conn);
static xcb_keysym_t get_keysym(xcb_keycode_t keycode, xcb_connection_t *conn);
static int16_t modfield_from_keysym(xcb_keysym_t keysym);
static uint16_t normalize_mods(uint16_t state);
static void grab_super_button(xcb_window_t win, uint8_t button);
static client_t *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn);
static void get_class_name(xcb_window_t win, char **out);
static void get_wm_name(xcb_window_t win, char **out);
static bool should_manage(xcb_window_t win, xcb_conn_t *conn);
static int apply_floating_hints(xcb_window_t win);
static bool should_ignore_hints(xcb_window_t win, const char *name);
static bool is_transient(xcb_window_t win);
static bool supports_protocol(xcb_window_t win, xcb_atom_t atom, xcb_conn_t *conn);
static bool client_exist_in_desktops(xcb_window_t win);
static void find_window_in_desktops(desktop_t **curr_desktop, node_t **curr_node, xcb_window_t win, bool *found);
static node_t *find_node_global(xcb_window_t win);
static int find_desktop_by_window(xcb_window_t win);
static int handle_first_window(client_t *client, desktop_t *d);
static int handle_subsequent_window(client_t *client, desktop_t *d);
static int handle_floating_window(client_t *client, desktop_t *d);
static int insert_into_desktop(int idx, xcb_window_t win, bool is_tiled);
static int handle_tiled_window_request(xcb_window_t win, desktop_t *d);
static int handle_floating_window_request(xcb_window_t win, desktop_t *d);
static int kill_window(xcb_window_t win);
static int close_or_kill(xcb_window_t win);
static void map_floating(xcb_window_t x);
static int display_client(rectangle_t r, xcb_window_t win);
static int handle_net_wm_desktop(xcb_window_t win, uint32_t index);
static int set_visibility(xcb_window_t win, bool is_visible);
static int set_desktop_visibility(xcb_window_t win, bool is_visible);
static rule_t *get_window_rule(xcb_window_t win);
static int load_config(config_t *c);
static void free_keys(void);
static void free_rules(void);
static int reload_config(config_t *c);
static void         load_cursors(void);
static xcb_cursor_t get_cursor(cursor_t c);
static void         set_cursor(int cursor_id);
static desktop_t *init_desktop(void);
static bool setup_desktops(void);
static node_t *pick_desktop_focus(desktop_t *d);
static node_t *pick_deck_focus(desktop_t *d);
static int get_focused_desktop_idx(void);
static desktop_t *get_focused_desktop(void);
static int switch_desktop(int nd);
static int render_desktop(desktop_t *d);
static void render_trees(void);
static void fill_root_rectangle(rectangle_t *r);
static int handle_net_desktop_change(uint32_t nd);
static int handle_net_active_window(xcb_window_t win);
static int drag_start(xcb_window_t win, int16_t x, int16_t y, bool kbd);
static int drag_move(int16_t x, int16_t y);
static int drag_end(int16_t x, int16_t y);
static int drag_cancel(void);
static void event_loop(wm_t *w);
static bool ewmh_has(ewmh_state_t s, ewmh_state_t f);
static bool setup_ewmh(void);
static int ewmh_update_current_desktop(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t i);
static int ewmh_update_number_of_desktops(void);
static int ewmh_update_desktop_names(void);
static void ewmh_update_desktop_viewport(void);
static void ewmh_update_client_list(void);
static int set_active_window_name(xcb_window_t win);
static int set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state);
static int update_net_wm_desktop(xcb_window_t win, uint32_t desktop);
static ewmh_state_t get_net_wm_state_mask(xcb_window_t win);
static ewmh_state_t ewmh_flag_for_atom(xcb_atom_t atom);
static int update_net_wm_state_atom(xcb_window_t win, xcb_atom_t atom, bool set);
static void update_client_ewmh_state(client_t *c, ewmh_state_t flag, bool set);
static void fill_icccm_ewmh(client_t *c);
static ewmh_window_type_t window_type(xcb_window_t win);
static void remove_property(xcb_connection_t *con, xcb_window_t win, xcb_atom_t prop, xcb_atom_t atom);
static int set_focus(node_t *n, bool flag);
static int win_focus(xcb_window_t win, bool set_focus);
static int fullscreen_focus(xcb_window_t win);
static void update_grabbed_window(node_t *root, node_t *n);
static void log_message(log_level_t level, const char *format, ...);
static void log_window_id(xcb_window_t window, const char *message);
static bool setup_monitors(void);
static void free_monitors(void);
static void handle_monitor_changes(void);
static monitor_t *get_focused_monitor(void);
static monitor_t *get_monitor_within_coordinate(int16_t x, int16_t y);
static monitor_t *get_monitor_from_desktop(desktop_t *desktop);
static monitor_t *get_monitor_by_window(xcb_window_t win);
static int get_monitors_count(void);
static int handle_unmanaged_strut_window(xcb_window_t win);
static void add_strut_window(xcb_window_t win);
static bool remove_strut_window(xcb_window_t win);
static bool is_strut_window(xcb_window_t win);
static void cleanup_strut_windows(void);
static bool ewmh_handle_struts(xcb_window_t win);
static rectangle_t get_usable_area(monitor_t *m);
static void recalculate_all_struts(void);
static void reapply_tracked_struts(void);
static rectangle_t calculate_monitor_area(const monitor_t *m);
static void apply_monitor_layout_changes(monitor_t *m);
static void arrange_trees(void);
static void clear_mouse_state(void);
static bool grab_pointer_for_mouse(cursor_t cursor_id);
static double clamp_ratio(double ratio);
static uint8_t detect_resize_edges(rectangle_t r, int16_t x, int16_t y);
static bool is_resize_band_hit(node_t *parent, split_type_t split_type, int16_t x, int16_t y);
static bool start_floating_move(node_t *n, int16_t x, int16_t y);
static bool start_floating_resize(node_t *n, int16_t x, int16_t y);
static bool start_tiled_resize(node_t *n, int16_t x, int16_t y);
static void handle_mouse_motion(int16_t x, int16_t y);
static void finish_mouse_action(void);
static void cancel_mouse_action(void);
static void window_grab_buttons(xcb_window_t win);
static void window_ungrab_buttons(xcb_window_t win);
static void ungrab_buttons_for_all(node_t *n);
static queue_t *create_queue(void);
static void enqueue(queue_t *q, node_t *n);
static void enqueue_front(queue_t *q, node_t *n);
static node_t *dequeue(queue_t *q);
static node_t *dequeue_rear(queue_t *q);
static node_t *peek_front(queue_t *q);
static node_t *peek_rear(queue_t *q);
static bool remove_node(queue_t *q, node_t *n);
static bool is_queue_empty(queue_t *q);
static size_t get_queue_size(queue_t *q);
static void free_queue(queue_t *q);
static uint64_t stack_key(const client_t *c);
static uint32_t get_next_mru_seq(monitor_t *monitor);
static void restack(void);
static void restackv2(node_t *root);
static node_t *create_node(client_t *c);
static node_t *init_root(void);
static node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id);
static node_t *find_master_node(node_t *root);
static node_t *prev_node(node_t *current);
static node_t *next_node(node_t *current);
static node_t *cycle_win(node_t *node, direction_t);
static node_t *find_left_leaf(node_t *root);
static node_t *find_any_leaf(node_t *root);
static node_t *get_focused_node(node_t *n);
static node_t *get_sibling(node_t *n);
static node_t *find_leaf_at_point(node_t *root, int16_t x, int16_t y);
static node_t *clone_tree(node_t *n, node_t *p);
static bool unlink_node(node_t *node, desktop_t *d);
static void update_focus(desktop_t *d, node_t *n);
static void free_tree(node_t *root);
static void delete_node(node_t *node, desktop_t *d);
static void insert_node(node_t *current_node, node_t *new_node, layout_t layout);
static void log_tree_nodes(node_t *node);
static bool transfer_node(node_t *, desktop_t *);
static bool is_tree_empty(const node_t *root);
static bool client_exist(node_t *cn, xcb_window_t id);
static bool has_floating_window(node_t *root);
static int render_tree(node_t *current_node);
static int tile(node_t *node);
static int _handle_fullscreen_window(xcb_window_t win);
static int hide_windows(node_t *tree);
static int show_windows(node_t *tree);
static int swap_node(node_t *root);
static int render_tree_nomap(node_t *node);
static void calculate_base_rect(rectangle_t *r, monitor_t *m);
static void arrange_tree(node_t *tree, layout_t l);
static void apply_layout(desktop_t *d, layout_t t);
static void master_clean_up(node_t *root);
static void default_layout(node_t *root);
static void master_layout(node_t *parent, node_t *n);
static void stack_layout(node_t *parent);
static void apply_default_layout(node_t *root);
static void apply_master_layout(node_t *parent);
static void apply_stack_layout(node_t *root);
static void apply_grid_layout(node_t *root);
static void grid_layout(node_t *root);
static void monocle_layout(node_t *root);
static void three_col_layout(node_t *root);
static void deck_layout(node_t *root);
static void flip_node(node_t *node);
static void dynamic_resize(node_t *n, resize_t t);
static void split_node(node_t *n, node_t *nd);
static void resize_subtree(node_t *parent);
static int change_border_attr(xcb_conn_t *conn, xcb_window_t win, uint32_t bcolor, uint32_t bwidth, bool stack);
static int change_window_attr(xcb_conn_t *conn, xcb_window_t win, uint32_t attr, const void *val);
static int configure_window(xcb_conn_t *conn, xcb_window_t win, uint16_t attr, const void *val);
static int set_input_focus(xcb_conn_t *conn, uint8_t revert_to, xcb_window_t win, xcb_timestamp_t time);
static int resize_window(xcb_window_t win, uint16_t width, uint16_t height);
static int move_window(xcb_window_t win, int16_t x, int16_t y);
static int apply_window_geometry(xcb_window_t win, rectangle_t r, uint16_t bw);
static int send_configure_notify(xcb_window_t win, rectangle_t r, uint16_t bw);
static void raise_window(xcb_window_t win);
static void lower_window(xcb_window_t win);
static void window_above(xcb_window_t win1, xcb_window_t win2);
static void window_below(xcb_window_t win1, xcb_window_t win2);
static xcb_atom_t get_atom(char *atom_name, xcb_conn_t *conn);
static xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_conn_t *conn);
static bool window_exists(xcb_conn_t *conn, xcb_window_t win);
static char *win_name(xcb_window_t win);
static int check_window_map_state(xcb_window_t win, win_map_state_t state);
static xcb_window_t get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win);
static void grab_pointer(xcb_window_t win, bool wants_events);
static void ungrab_pointer(void);
static uint64_t get_time_millis(void);
static void calculate_base_rect(rectangle_t *r, monitor_t *m);
static void arrange_tree(node_t *tree, layout_t l);
static void apply_layout(desktop_t *d, layout_t t);
static void master_clean_up(node_t *root);
static void default_layout(node_t *root);
static void master_layout(node_t *parent, node_t *n);
static void stack_layout(node_t *parent);
static void apply_default_layout(node_t *root);
static void apply_master_layout(node_t *parent);
static void apply_stack_layout(node_t *root);
static void apply_grid_layout(node_t *root);
static void grid_layout(node_t *root);
static void monocle_layout(node_t *root);
static void three_col_layout(node_t *root);
static void deck_layout(node_t *root);
static void flip_node(node_t *node);
static void dynamic_resize(node_t *n, resize_t t);
static void split_node(node_t *n, node_t *nd);
static void resize_subtree(node_t *parent);
static leaf_visibility_t _leaf_visibility_(desktop_t *d, node_t *leaf);
static void _focus_node_(desktop_t *d, node_t *n);
static int _focus_input_(desktop_t *d, node_t *n);
static node_t *_pick_focus_(desktop_t *d);
static int _render_view_(desktop_t *d);
static void _flush_view_(desktop_t *d);
static int show_window(xcb_window_t win, bool update_hidden_state);
static int hide_window(xcb_window_t win, bool update_hidden_state);
static void fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r);
static int send_client_message(xcb_window_t win, xcb_atom_t	 property, xcb_atom_t	 value, xcb_conn_t	*conn);
static void update_focused_desktop(int id);
static node_t * find_deck_stack_focus(node_t *root);
static int activate_window_node(desktop_t *d, node_t *n);
static void apply_preview_layout(node_t *root);
static void preview_restore_layout(void);
static void preview_apply(node_t *target);
static void preview_clear(void);
static int handle_map_request(const xcb_event_t *);
static int handle_unmap_notify(const xcb_event_t *);
static int handle_destroy_notify(const xcb_event_t *);
static int handle_client_message(const xcb_event_t *);
static int handle_configure_request(const xcb_event_t *);
static int handle_enter_notify(const xcb_event_t *);
static int handle_leave_notify(const xcb_event_t *);
static int handle_focus_in(const xcb_event_t *);
static int handle_property_notify(const xcb_event_t *);
static int handle_key_press(const xcb_event_t *);
static int handle_mapping_notify(const xcb_event_t *);
static int handle_button_press_event(const xcb_event_t *);
static int handle_motion_notify(const xcb_event_t *);
static int handle_button_release(const xcb_event_t *);
static xcb_ewmh_conn_t * ewmh_init(xcb_conn_t *conn);
static int ewmh_set_supporting(xcb_window_t win, xcb_ewmh_conn_t *ewmh);
static int ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd);
static size_t get_active_clients_size(desktop_t **d, const int n);
static void populate_client_array(node_t *root, xcb_window_t *arr, size_t *index);
static ewmh_window_type_t determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom);
static monitor_t * init_monitor(void);
static void add_monitor(monitor_t **head, monitor_t *m);
static void unlink_monitor(monitor_t **head, monitor_t *m);
static void log_monitors(void);
static monitor_t * get_monitor_by_randr_id(xcb_randr_output_t id);
static monitor_t * get_monitor_by_root_id(xcb_window_t id);
static int get_connected_monitor_count_xinerama(void);
static int get_connected_monitor_count_xrandr(void);
static int get_connected_monitor_count(bool xrandr, bool xinerama);
static bool setup_monitors_via_xrandr(void);
static bool setup_monitors_via_xinerama(void);
static bool handle_added_monitor(xcb_randr_get_output_info_reply_t *info, xcb_randr_output_t					id);
static void destroy_monitor(monitor_t *m);
static bool is_monitor_layout_changed(xcb_randr_get_output_info_reply_t *info, rectangle_t						*r, rectangle_t						*r_out);
static bool merge_monitors(monitor_t *om, monitor_t *nm);
static bool is_disconnected(monitor_t *m, monitor_t *dl);
static void update_monitors(uint32_t *changes);
static bool ranges_overlap(int32_t a_start, int32_t a_end, int32_t b_start, int32_t b_end);
static node_t *find_tree_root(node_t *);
static bool is_parent_null(const node_t *node);
static rectangle_t _get_window_rectangle(node_t *node);
static int _handle_fullscreen_window(xcb_window_t win);
static int _handle_window_nomap(node_t *node);
static void master_clean_up(node_t *root);
static void default_layout(node_t *root);
static void master_layout(node_t *parent, node_t *n);
static void stack_layout(node_t *parent);
static void grid_layout(node_t *root);
static double clamp_layout_ratio(double ratio);
static node_t *find_tree_root_local(node_t *n);
static node_t *find_layout_master(node_t *root);
static void set_master_node(node_t *root, node_t *n);
static void set_master_under_cursor(node_t *root);
/* clang-format on */

/* arrays */

/* clang-format off */
static _key__t _keys_[] = {
    DEFINE_KEY(SUPER,         _KEY(w),       close_or_kill_wrapper,     NULL),
    DEFINE_KEY(SUPER,         _KEY(Return),  exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"alacritty"}})),
    DEFINE_KEY(SUPER,         _KEY(space),   exec_process,              &((arg_t){.argc = 1, .cmd = (char *[]){"dmenu_run"}})),
    DEFINE_KEY(SUPER,         _KEY(p),       exec_process,              &((arg_t){.argc = 3, .cmd = (char *[]){"rofi", "-show", "drun"}})),
    DEFINE_KEY(SUPER,         _KEY(1),       switch_desktop_wrapper,    &((arg_t){.idx  = 0})),
    DEFINE_KEY(SUPER,         _KEY(2),       switch_desktop_wrapper,    &((arg_t){.idx  = 1})),
    DEFINE_KEY(SUPER,         _KEY(3),       switch_desktop_wrapper,    &((arg_t){.idx  = 2})),
    DEFINE_KEY(SUPER,         _KEY(4),       switch_desktop_wrapper,    &((arg_t){.idx  = 3})),
    DEFINE_KEY(SUPER,         _KEY(5),       switch_desktop_wrapper,    &((arg_t){.idx  = 4})),
    DEFINE_KEY(SUPER,         _KEY(6),       switch_desktop_wrapper,    &((arg_t){.idx  = 5})),
    DEFINE_KEY(SUPER,         _KEY(7),       switch_desktop_wrapper,    &((arg_t){.idx  = 6})),
    DEFINE_KEY(SUPER,         _KEY(Left),    cycle_win_wrapper,         &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER,         _KEY(Right),   cycle_win_wrapper,         &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER,         _KEY(Up),      cycle_win_wrapper,         &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER,         _KEY(Down),    cycle_win_wrapper,         &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER,         _KEY(l),       dynamic_resize_wrapper, &((arg_t){.r    = GROW})),
    DEFINE_KEY(SUPER,         _KEY(h),       dynamic_resize_wrapper, &((arg_t){.r    = SHRINK})),
    DEFINE_KEY(SUPER,         _KEY(f),       set_fullscreen_wrapper,    NULL),
    DEFINE_KEY(SUPER,         _KEY(s),       swap_node_wrapper,         NULL),
    DEFINE_KEY(SUPER,         _KEY(i),       gap_handler,               &((arg_t){.r    = GROW})),
    DEFINE_KEY(SUPER,         _KEY(d),       gap_handler,               &((arg_t){.r    = SHRINK})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Left),    shift_floating_window,     &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Right),   shift_floating_window,     &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Up),      shift_floating_window,     &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(Down),    shift_floating_window,     &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER | ALT,   _KEY(f),       change_state,              &((arg_t){.s    = FLOATING})),
    DEFINE_KEY(SUPER | ALT,   _KEY(t),       change_state,              &((arg_t){.s    = TILED})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(1),       transfer_node_wrapper,     &((arg_t){.idx  = 0})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(2),       transfer_node_wrapper,     &((arg_t){.idx  = 1})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(3),       transfer_node_wrapper,     &((arg_t){.idx  = 2})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(4),       transfer_node_wrapper,     &((arg_t){.idx  = 3})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(5),       transfer_node_wrapper,     &((arg_t){.idx  = 4})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(6),       transfer_node_wrapper,     &((arg_t){.idx  = 5})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(7),       transfer_node_wrapper,     &((arg_t){.idx  = 6})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(m),       layout_handler,            &((arg_t){.t    = MASTER})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(d),       layout_handler,            &((arg_t){.t    = DEFAULT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(s),       layout_handler,            &((arg_t){.t    = STACK})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(c),       layout_handler,            &((arg_t){.t    = GRID})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(o),       layout_handler,            &((arg_t){.t    = MONOCLE})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(x),       layout_handler,            &((arg_t){.t    = THREE_COL})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(v),       layout_handler,            &((arg_t){.t    = DECK})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(k),       traverse_stack_wrapper,    &((arg_t){.d    = UP})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(j),       traverse_stack_wrapper,    &((arg_t){.d    = DOWN})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(f),       flip_node_wrapper,         NULL),
    DEFINE_KEY(SUPER | SHIFT, _KEY(r),       reload_config_wrapper,     NULL),
    DEFINE_KEY(SUPER | ALT,   _KEY(Left),    cycle_desktop_wrapper,     &((arg_t){.d    = LEFT})),
    DEFINE_KEY(SUPER | ALT,   _KEY(Right),   cycle_desktop_wrapper,     &((arg_t){.d    = RIGHT})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(y),       grow_floating_window,      &((arg_t){.rd   = HORIZONTAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(h),       grow_floating_window,      &((arg_t){.rd   = VERTICAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(t),       shrink_floating_window,    &((arg_t){.rd   = HORIZONTAL_DIR})),
    DEFINE_KEY(SUPER | SHIFT, _KEY(g),       shrink_floating_window,    &((arg_t){.rd   = VERTICAL_DIR})),
    DEFINE_KEY(SUPER | CTRL,  _KEY(Right),   cycle_monitors,            &((arg_t){.tr   = NEXT})),
    DEFINE_KEY(SUPER | CTRL,  _KEY(Left),    cycle_monitors,            &((arg_t){.tr   = PREV})),
    DEFINE_KEY(SUPER,         _KEY(m),       start_keyboard_drag_wrapper, NULL),
};
static const conf_mapper_t _cmapper_[] = {
    DEFINE_MAPPING("run",            		 exec_process),
    DEFINE_MAPPING("kill",           		 close_or_kill_wrapper),
    DEFINE_MAPPING("switch_desktop", 		 switch_desktop_wrapper),
    DEFINE_MAPPING("resize",         		 dynamic_resize_wrapper),
    DEFINE_MAPPING("fullscreen",     		 set_fullscreen_wrapper),
    DEFINE_MAPPING("swap",           		 swap_node_wrapper),
    DEFINE_MAPPING("transfer_node",  		 transfer_node_wrapper),
    DEFINE_MAPPING("layout",         		 layout_handler),
    DEFINE_MAPPING("traverse",       		 traverse_stack_wrapper),
    DEFINE_MAPPING("flip",           		 flip_node_wrapper),
    DEFINE_MAPPING("cycle_window",   		 cycle_win_wrapper),
    DEFINE_MAPPING("reload_config",  		 reload_config_wrapper),
    DEFINE_MAPPING("cycle_desktop",  		 cycle_desktop_wrapper),
    DEFINE_MAPPING("cycle_monitors",  		 cycle_monitors),
    DEFINE_MAPPING("shift_window",   		 shift_floating_window),
    DEFINE_MAPPING("grow_floating_window",   grow_floating_window),
    DEFINE_MAPPING("shrink_floating_window", shrink_floating_window),
    DEFINE_MAPPING("gap_handler",    		 gap_handler),
    DEFINE_MAPPING("change_state",  		 change_state),
    DEFINE_MAPPING("start_keyboard_drag",	 start_keyboard_drag_wrapper),
};
static key_mapper_t _kmapper_[] = {
    DEFINE_MAPPING("0",        0x0030), DEFINE_MAPPING("1",        0x0031),
    DEFINE_MAPPING("2",        0x0032), DEFINE_MAPPING("3",        0x0033),
    DEFINE_MAPPING("4",        0x0034), DEFINE_MAPPING("5",        0x0035),
    DEFINE_MAPPING("6",        0x0036), DEFINE_MAPPING("7",        0x0037),
    DEFINE_MAPPING("8",        0x0038), DEFINE_MAPPING("9",        0x0039),
    DEFINE_MAPPING("a",        0x0061), DEFINE_MAPPING("b",        0x0062),
    DEFINE_MAPPING("c",        0x0063), DEFINE_MAPPING("d",        0x0064),
    DEFINE_MAPPING("e",        0x0065), DEFINE_MAPPING("f",        0x0066),
    DEFINE_MAPPING("g",        0x0067), DEFINE_MAPPING("h",        0x0068),
    DEFINE_MAPPING("i",        0x0069), DEFINE_MAPPING("j",        0x006a),
    DEFINE_MAPPING("k",        0x006b), DEFINE_MAPPING("l",        0x006c),
    DEFINE_MAPPING("m",        0x006d), DEFINE_MAPPING("n",        0x006e),
    DEFINE_MAPPING("o",        0x006f), DEFINE_MAPPING("p",        0x0070),
    DEFINE_MAPPING("q",        0x0071), DEFINE_MAPPING("r",        0x0072),
    DEFINE_MAPPING("s",        0x0073), DEFINE_MAPPING("t",        0x0074),
    DEFINE_MAPPING("u",        0x0075), DEFINE_MAPPING("v",        0x0076),
    DEFINE_MAPPING("w",        0x0077), DEFINE_MAPPING("x",        0x0078),
    DEFINE_MAPPING("y",        0x0079), DEFINE_MAPPING("z",        0x007a),
    DEFINE_MAPPING("space",    0x0020), DEFINE_MAPPING("return",   0xff0d),
    DEFINE_MAPPING("left",     0xff51), DEFINE_MAPPING("up",       0xff52),
    DEFINE_MAPPING("right",    0xff53), DEFINE_MAPPING("down",     0xff54),
    DEFINE_MAPPING("super",     SUPER), DEFINE_MAPPING("alt",        ALT ),
    DEFINE_MAPPING("ctrl",      CTRL ), DEFINE_MAPPING("shift",     SHIFT),
    DEFINE_MAPPING("sup+sh", SUPER | SHIFT),
};
static const event_handler_entry_t _handlers_[] = {
	/* client asked to map a top-level window; WM decides how to show it */
	DEFINE_MAPPING(XCB_MAP_REQUEST, handle_map_request),
	/* window changed from mapped to unmapped */
	DEFINE_MAPPING(XCB_UNMAP_NOTIFY, handle_unmap_notify),
	/* window was destroyed */
	DEFINE_MAPPING(XCB_DESTROY_NOTIFY, handle_destroy_notify),
	/* message sent by another client; EWMH/ICCCM requests come through here */
	DEFINE_MAPPING(XCB_CLIENT_MESSAGE, handle_client_message),
	/* client asked to move, resize, or restack itself */
	DEFINE_MAPPING(XCB_CONFIGURE_REQUEST, handle_configure_request),
	/* pointer entered a window */
	DEFINE_MAPPING(XCB_ENTER_NOTIFY, handle_enter_notify),
	/* pointer button was pressed */
	DEFINE_MAPPING(XCB_BUTTON_PRESS, handle_button_press_event),
	/* key was pressed */
	DEFINE_MAPPING(XCB_KEY_PRESS, handle_key_press),
	/* keyboard mapping changed; key grabs may need a refresh here */
	DEFINE_MAPPING(XCB_MAPPING_NOTIFY, handle_mapping_notify),
	/* pointer moved */
	DEFINE_MAPPING(XCB_MOTION_NOTIFY, handle_motion_notify),
	/* pointer button was released */
	DEFINE_MAPPING(XCB_BUTTON_RELEASE, handle_button_release),
	/* pointer left a window */
	/* DEFINE_MAPPING(XCB_LEAVE_NOTIFY, handle_leave_notify), */
	/* key was released */
	/* DEFINE_MAPPING(XCB_KEY_RELEASE, handle_key_release), */
	/* input focus moved to a window */
	/* DEFINE_MAPPING(XCB_FOCUS_IN, handle_focus_in), */
	/* input focus moved away from a window */
	/* DEFINE_MAPPING(XCB_FOCUS_OUT, handle_focus_out), */
	/* window position, size, border, or stacking changed */
	/* DEFINE_MAPPING(XCB_CONFIGURE_NOTIFY, handle_configure_notify), */
	/* window property changed or was deleted */
	DEFINE_MAPPING(XCB_PROPERTY_NOTIFY, handle_property_notify),
};
static xcb_cursor_t		  cursors[CURSOR_MAX];
/* clang-format on */

/* globals */

rule_t	   *rule_head = NULL;
conf_key_t *key_head  = NULL;
drag_state_t ds = {0};
wm_t				 *wm			 = NULL;
monitor_t			 *prim_monitor	 = NULL;
monitor_t			 *curr_monitor	 = NULL;
monitor_t			 *head_monitor	 = NULL;
strut_win_node_t	 *strut_windows	 = NULL;
xcb_cursor_context_t *cursor_ctx	 = NULL;
xcb_window_t		  focused_win	 = XCB_NONE;
xcb_window_t		  meta_window	 = XCB_NONE;
bool				  is_kgrabbed	 = false;
bool				  using_xrandr	 = false;
bool				  multi_monitors = false;
bool				  using_xinerama = false;
bool ignore_ewmh_struts = false; /* this is hardcoded for now, plan on making it
									configurable via IPC or config file */
config_t			  conf						= {0};
volatile sig_atomic_t should_shutdown			= 0;
uint8_t				  randr_base				= 0;
uint64_t			  last_desk_switch_time		= 0;
uint64_t			  suppress_enter_until_time = 0;
mouse_state_t		  ms = {0};

/* func impls */


/* ./src/actions.c */



static void
collect_deck_stack_leaves(node_t *root, node_t **buf, int *n, int cap)
{
	if (!root || *n >= cap)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client) &&
		!root->is_master) {
		buf[(*n)++] = root;
		return;
	}
	collect_deck_stack_leaves(root->first_child, buf, n, cap);
	collect_deck_stack_leaves(root->second_child, buf, n, cap);
}

static node_t *
next_deck_stack_node(node_t *root, direction_t d, xcb_window_t cursor_win)
{
	node_t *leaves[64];
	int		n = 0;
	collect_deck_stack_leaves(root, leaves, &n, 64);
	if (n == 0)
		return NULL;

	node_t *curr = NULL;
	if (cursor_win != XCB_NONE && cursor_win != wm->root_window) {
		node_t *under_cursor = find_node_by_window_id(root, cursor_win);
		if (under_cursor && under_cursor->client && !under_cursor->is_master)
			curr = under_cursor;
	}
	if (!curr) {
		node_t *focused = get_focused_node(root);
		if (focused && !focused->is_master)
			curr = focused;
	}
	if (!curr)
		return (d == UP) ? leaves[0] : leaves[n - 1];

	for (int i = 0; i < n; i++) {
		if (leaves[i] == curr) {
			return (d == UP) ? leaves[(i + 1) % n] : leaves[(i + n - 1) % n];
		}
	}

	return (d == UP) ? leaves[0] : leaves[n - 1];
}

static int
layout_handler(arg_t *arg)
{
	desktop_t *d = curr_monitor->desk;
	if ((arg->t == STACK || arg->t == MONOCLE || arg->t == DECK) &&
		d->n_count < 2)
		return 0;

	suppress_enter_until_time = get_time_millis() + 250;
	apply_layout(d, arg->t);

	node_t *f = _pick_focus_(d);
	if (f)
		_focus_node_(d, f);
	int ret = _render_view_(d);
	if (ret == 0 && f)
		ret = _focus_input_(d, f);
	_flush_view_(d);
	return ret;
}

static int
change_state(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == XCB_NONE)
		return -1;

	node_t *n = find_node_by_window_id(curr_monitor->desk->tree, w);
	if (n == NULL)
		return -1;

	if (IS_ROOT(n))
		return 0;

	state_t state = arg->s;
	node_t *p	  = n->parent;
	if (state == TILED) {
		if (IS_TILED(n->client))
			return 0;
		n->client->state = TILED;
		if (n->rectangle.width >= n->rectangle.height) {
			p->first_child->rectangle.x = p->rectangle.x;
			p->first_child->rectangle.y = p->rectangle.y;
			p->first_child->rectangle.width =
				(p->rectangle.width - (conf.window_gap - conf.border_width)) /
				2;
			p->first_child->rectangle.height = p->rectangle.height;

			p->second_child->rectangle.x =
				(int16_t)(p->rectangle.x + p->first_child->rectangle.width +
						  conf.window_gap + conf.border_width);
			p->second_child->rectangle.y = p->rectangle.y;
			p->second_child->rectangle.width =
				p->rectangle.width - p->first_child->rectangle.width -
				conf.window_gap - conf.border_width;
			p->second_child->rectangle.height = p->rectangle.height;
		} else {
			p->first_child->rectangle.x		= p->rectangle.x;
			p->first_child->rectangle.y		= p->rectangle.y;
			p->first_child->rectangle.width = p->rectangle.width;
			p->first_child->rectangle.height =
				(p->rectangle.height - (conf.window_gap - conf.border_width)) /
				2;

			p->second_child->rectangle.x = p->rectangle.x;
			p->second_child->rectangle.y =
				(int16_t)(p->rectangle.y + p->first_child->rectangle.height +
						  conf.window_gap + conf.border_width);
			p->second_child->rectangle.width = p->rectangle.width;
			p->second_child->rectangle.height =
				p->rectangle.height - p->first_child->rectangle.height -
				conf.window_gap - conf.border_width;
		}
		if (IS_INTERNAL(p->second_child)) {
			resize_subtree(p->second_child);
		}
		if (IS_INTERNAL(p->first_child)) {
			resize_subtree(p->first_child);
		}
	} else if (state == FLOATING) {
		if (IS_FLOATING(n->client))
			return 0;
		xcb_get_geometry_reply_t *g =
			get_geometry(n->client->window, wm->connection);
		uint16_t	h		  = g->height / 2;
		uint16_t	wi		  = g->width / 2;
		int16_t		x		  = curr_monitor->rectangle.x +
								(curr_monitor->rectangle.width / 2) - (wi / 2);
		int16_t		y		  = curr_monitor->rectangle.y +
								(curr_monitor->rectangle.height / 2) - (h / 2);
		rectangle_t rc		  = {.x = x, .y = y, .width = wi, .height = h};
		n->floating_rectangle = rc;
		_FREE_(g);
		n->client->state = FLOATING;
		if (p) {
			if (p->first_child == n) {
				p->second_child->rectangle = p->rectangle;
				if (IS_INTERNAL(p->second_child)) {
					resize_subtree(p->second_child);
				}
			} else {
				p->first_child->rectangle = p->rectangle;
				if (IS_INTERNAL(p->first_child)) {
					resize_subtree(p->first_child);
				}
			}
		}
	}

	int ret = _render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	return ret;
}

static int
swap_node_wrapper(arg_t *arg)
{
	(void)arg;
	if (curr_monitor == NULL) {
		_LOG_(ERROR, "failed to swap node, current monitor is NULL");
		return -1;
	}

	layout_t lay = curr_monitor->desk->layout;
	if (lay != DEFAULT && lay != MASTER && lay != GRID && lay != THREE_COL)
		return 0;

	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (swap_node(n) != 0)
		return -1;

	int ret = _render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	return ret;
}

static int
dynamic_resize_wrapper(arg_t *arg)
{
	if (curr_monitor->desk->layout == STACK ||
		curr_monitor->desk->layout == MASTER ||
		curr_monitor->desk->layout == MONOCLE) {
		return 0;
	}

	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	/* todo: if node was flipped, reize up or down instead
	 * i think this is done already... as of 2026-01-14. confirmed?? */
	grab_pointer(wm->root_window,
				 false); /* steal the pointer and prevent it from sending
						  * enter_notify events (which focuses the window
						  * being under cursor as the resize happens); */
	dynamic_resize(n, arg->r);
	_render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	ungrab_pointer();
	return 0;
}

static int
set_fullscreen_wrapper(arg_t *arg)
{
	(void)arg;
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window) {
		return 0;
	}

	node_t *n = find_node_by_window_id(curr_monitor->desk->tree, w);
	if (n == NULL) {
		_LOG_(ERROR, "cannot find focused node");
		return -1;
	}

	n->client->state == FULLSCREEN ? set_fullscreen(n, false)
								   : set_fullscreen(n, true);
	return 0;
}

static int
set_fullscreen(node_t *n, bool flag)
{
	/* fullscreen touches too many things
	 * x geometry, border, EWMH state, view visibility and input focus */
	if (n == NULL || n->client == NULL)
		return -1;

	desktop_t *d	  = curr_monitor->desk;
	node_t	  *f	  = NULL;
	bool	   exists = false;
	find_window_in_desktops(&d, &f, n->client->window, &exists);
	if (!exists)
		d = curr_monitor->desk;

	monitor_t  *m			 = get_monitor_by_window(n->client->window);
	bool		_active		 = (m && m->desk == d);
	bool		should_focus = IS_TILED(n->client) || !flag;
	rectangle_t r			 = {0};
	if (flag) {
		if (!m) {
			m = curr_monitor;
		}
		long data[]		 = {wm->ewmh->_NET_WM_STATE_FULLSCREEN};
		r.x				 = m->rectangle.x;
		r.y				 = m->rectangle.y;
		r.width			 = m->rectangle.width;
		r.height		 = m->rectangle.height;
		n->client->state = FULLSCREEN;
		if (change_border_attr(wm->connection,
							   n->client->window,
							   conf.normal_border_color,
							   0,
							   false) != 0) {
			return -1;
		}
		if (apply_window_geometry(n->client->window, r, 0) != 0) {
			return -1;
		}
		xcb_cookie_t c	 = xcb_change_property_checked(wm->connection,
													   XCB_PROP_MODE_REPLACE,
													   n->client->window,
													   wm->ewmh->_NET_WM_STATE,
													   XCB_ATOM_ATOM,
													   32,
													   true,
													   data);
		xcb_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR, "error changing window property: %d", err->error_code);
			_FREE_(err);
			return -1;
		}
		update_client_ewmh_state(n->client, EWMH_STATE_FULLSCREEN, true);
		goto out;
	}

	r				 = n->rectangle;
	n->client->state = TILED;
	if (apply_window_geometry(n->client->window, r, conf.border_width) != 0) {
		return -1;
	}
	remove_property(wm->connection,
					n->client->window,
					wm->ewmh->_NET_WM_STATE,
					wm->ewmh->_NET_WM_STATE_FULLSCREEN);
	update_client_ewmh_state(n->client, EWMH_STATE_FULLSCREEN, false);
	if (change_border_attr(wm->connection,
						   n->client->window,
						   conf.normal_border_color,
						   conf.border_width,
						   true) != 0) {
		return -1;
	}
out:
	/* fullscreen request can target a window on hidden desktop.
	 * update its state, then hide it again. */
	if (!_active) {
		if (set_desktop_visibility(n->client->window, false) != 0)
			return -1;
		restack();
		xcb_flush(wm->connection);
		return 0;
	}

	if (should_focus)
		_focus_node_(d, n);
	if (_render_view_(d) != 0)
		return -1;

	if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window) != 0)
			return -1;
	} else if (_focus_input_(d, n) != 0) {
		return -1;
	}
	focused_win		   = n->client->window;
	n->client->mru_seq = get_next_mru_seq(m ? m : curr_monitor);
	set_active_window_name(focused_win);
	_flush_view_(d);
	return 0;
}

static int
change_colors(node_t *root)
{
	if (root == NULL)
		return 0;
	/* win_foucs internally changes the border color of a window, if the focus
	 * param is set to true it applies the active_border_color, otherwise the
	 * normal_border_color is chosen */
	if (root->node_type != INTERNAL_NODE && root->client) {
		if (win_focus(root->client->window, root->is_focused) != 0) {
			_LOG_(ERROR, "cannot focus node");
			return -1;
		}
	}

	if (root->first_child)
		change_colors(root->first_child);
	if (root->second_child)
		change_colors(root->second_child);

	return 0;
}

static int
reload_config_wrapper(arg_t *arg)
{
	(void)arg;
	/* store the old config values so i can compare them later with the new
	 * values to determine what needs to be done */
	uint16_t prev_border_width		  = conf.border_width;
	uint16_t prev_window_gap		  = conf.window_gap;
	uint32_t prev_active_border_color = conf.active_border_color;
	uint32_t prev_normal_border_color = conf.normal_border_color;
	int		 prev_virtual_desktops	  = conf.virtual_desktops;
	/* clear the config data structures */
	memset(&conf, 0, sizeof(config_t));

	ungrab_keys(wm->connection, wm->root_window);
	is_kgrabbed = false;
	free_keys();
	free_rules();
	assert(key_head == NULL && rule_head == NULL);

	if (reload_config(&conf) != 0) {
		_LOG_(ERROR, "error while reloading config -> using default macros");
		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		conf.focus_follow_spawn	  = FOCUS_FOLLOW_SPAWN;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
		conf.restore_last_focus	  = RESTORE_LAST_FOCUS;
		if (0 != grab_keys(wm->connection, wm->root_window)) {
			_LOG_(ERROR, "cannot grab keys after reload");
			return -1;
		}
		return 0;
	}

	bool color_changed =
		(prev_normal_border_color != conf.normal_border_color) ||
		(prev_active_border_color != conf.active_border_color);
	bool layout_changed	 = (conf.window_gap != prev_window_gap) ||
						   (conf.border_width != prev_border_width);
	bool desktop_changed = (prev_virtual_desktops != conf.virtual_desktops);

	if (color_changed) {
		monitor_t *current_monitor = head_monitor;
		while (current_monitor) {
			for (int j = 0; j < current_monitor->n_of_desktops; j++) {
				if (!is_tree_empty(current_monitor->desktops[j]->tree)) {
					if (change_colors(current_monitor->desktops[j]->tree) !=
						0) {
						_LOG_(ERROR,
							  "error while reloading config for "
							  "desktop %d",
							  current_monitor->desktops[j]->id);
					}
				}
			}
			current_monitor = current_monitor->next;
		}
	}

	if (layout_changed) {
		monitor_t *current_monitor = head_monitor;
		while (current_monitor) {
			apply_monitor_layout_changes(current_monitor);
			current_monitor = current_monitor->next;
		}
	}

	if (desktop_changed) {
		_LOG_(INFO, "reloading desktop changes is not implemented yet");
		if (conf.virtual_desktops > prev_virtual_desktops) {
			monitor_t *current_monitor = head_monitor;
			while (current_monitor) {
				current_monitor->n_of_desktops = conf.virtual_desktops;
				desktop_t **n				   = (desktop_t **)realloc(
					current_monitor->desktops,
					sizeof(desktop_t *) * current_monitor->n_of_desktops);
				if (n == NULL) {
					_LOG_(ERROR, "failed to realloc desktops");
					goto out;
				}
				current_monitor->desktops = n;
				for (int j = prev_virtual_desktops;
					 j < current_monitor->n_of_desktops;
					 j++) {
					desktop_t *d = init_desktop();
					if (d == NULL) {
						_LOG_(ERROR, "failed to initialize new desktop");
						goto out;
					}
					d->id		  = (uint16_t)j;
					d->is_focused = false;
					d->layout	  = DEFAULT;
					snprintf(d->name, sizeof(d->name), "%d", j + 1);
					current_monitor->desktops[j] = d;
				}
				current_monitor = current_monitor->next;
			}
		} else if (conf.virtual_desktops < prev_virtual_desktops) {
			monitor_t *current_monitor = head_monitor;
			while (current_monitor) {
				for (int j = conf.virtual_desktops; j < prev_virtual_desktops;
					 j++) {
					if (curr_monitor->desk->id ==
						current_monitor->desktops[j]->id) {
						switch_desktop_wrapper(&(arg_t){
							.idx = curr_monitor->desk->id == 0 ? 1 : 0});
					}
					if (current_monitor->desktops[j]) {
						if (!is_tree_empty(
								current_monitor->desktops[j]->tree)) {
							free_tree(current_monitor->desktops[j]->tree);
							current_monitor->desktops[j]->tree = NULL;
						}
						_FREE_(current_monitor->desktops[j]);
					}
				}
				current_monitor->n_of_desktops = conf.virtual_desktops;
				desktop_t **n				   = (desktop_t **)realloc(
					current_monitor->desktops,
					sizeof(desktop_t *) * current_monitor->n_of_desktops);
				if (n == NULL) {
					_LOG_(ERROR, "failed to realloc desktops");
					goto out;
				}
				current_monitor->desktops = n;
				current_monitor			  = current_monitor->next;
			}
		}

		if (ewmh_update_number_of_desktops() != 0) {
			return false;
		}

		if (ewmh_update_desktop_names() != 0) {
			return false;
		}

		if (ewmh_update_current_desktop(
				wm->ewmh, wm->screen_nbr, (uint32_t)curr_monitor->desk->id) !=
			0) {
			return false;
		}

		if (ewmh_update_desktop_names() != 0) {
			return false;
		}
	}

	if (0 != grab_keys(wm->connection, wm->root_window)) {
		_LOG_(ERROR, "cannot grab keys after reload");
		return -1;
	}

out:
	_render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	return 0;
}

static int
gap_handler(arg_t *arg)
{
	const int pxl = 5;
	if (arg->r == GROW) {
		conf.window_gap += pxl;
	} else {
		conf.window_gap =
			(conf.window_gap - pxl <= 0) ? 0 : conf.window_gap - pxl;
	}

	monitor_t *current_monitor = head_monitor;
	while (current_monitor) {
		apply_monitor_layout_changes(current_monitor);
		current_monitor = current_monitor->next;
	}
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	_render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	return 0;
}

static int
flip_node_wrapper(arg_t *arg)
{
	(void)arg;
	layout_t lay = curr_monitor->desk->layout;
	if (lay != DEFAULT && lay != MASTER && lay != GRID && lay != THREE_COL)
		return 0;

	node_t *tree = curr_monitor->desk->tree;
	node_t *node = NULL;
	if (!(node = get_focused_node(tree)))
		return -1;

	flip_node(node);
	int ret = _render_view_(curr_monitor->desk);
	_flush_view_(curr_monitor->desk);
	return ret;
}

static int
cycle_win_wrapper(arg_t *arg)
{
	direction_t d = arg->d;
	node_t	   *f = NULL;
	if (!(f = get_focused_node(curr_monitor->desk->tree))) {
		_LOG_(INFO, "cannot find focused window");
		xcb_window_t w =
			get_window_under_cursor(wm->connection, wm->root_window);
		f = find_node_by_window_id(curr_monitor->desk->tree, w);
	}
	node_t *next = cycle_win(f, d);
	if (next == NULL) {
		return 0;
	}
#ifdef _DEBUG__
	char *s = win_name(next->client->window);
	_LOG_(DEBUG, "found node %d name %s", next->client->window, s);
	_FREE_(s);
#endif
	_focus_node_(curr_monitor->desk, next);
	set_active_window_name(next->client->window);
	next->client->mru_seq = get_next_mru_seq(curr_monitor);
	int ret				  = _render_view_(curr_monitor->desk);
	if (ret == 0)
		ret = _focus_input_(curr_monitor->desk, next);
	_flush_view_(curr_monitor->desk);
	return ret;
}

static void
move_mouse_to_monitor(monitor_t *m)
{
	if (!m)
		return;
	xcb_query_pointer_reply_t *ptr = xcb_query_pointer_reply(
		wm->connection,
		xcb_query_pointer(wm->connection, wm->root_window),
		NULL);

	if (ptr == NULL) {
		_LOG_(ERROR, "failed to query pointer");
		return;
	}

	int pointer_x = ptr->root_x;
	int pointer_y = ptr->root_y;

	int x		  = (m->rectangle.x + m->rectangle.width / 2) + 100;
	int y		  = (m->rectangle.y + m->rectangle.height / 2) + 100;

	xcb_warp_pointer(wm->connection,
					 wm->root_window,
					 wm->root_window,
					 pointer_x,
					 pointer_y,
					 curr_monitor->rectangle.width,
					 curr_monitor->rectangle.height,
					 x,
					 y);
	xcb_flush(wm->connection);
}

static int
cycle_monitors(arg_t *arg)
{
	if (!multi_monitors)
		return 0;

	traversal_t dir	 = arg->tr;
	monitor_t  *curr = curr_monitor;
	monitor_t  *m	 = NULL;

	if (dir == NEXT) {
		m = curr->next;
		if (!m) {
			/* wrap around to the first monitor */
			m = head_monitor;
		}
	} else {
		monitor_t *head = head_monitor;
		monitor_t *prev = NULL;
		while (head && head->next) {
			if (head->next == curr) {
				m = head;
				break;
			}
			head = head->next;
		}

		/* wrap around to the last monitor */
		if (!m) {
			while (head && head->next) {
				head = head->next;
			}
			m = head;
		}
	}

	if (m && m != curr) {
		_LOG_(INFO, "switching monitor: '%s' -> '%s'", curr->name, m->name);
		/* when a mouse moves, we update the current monitor in the enter_notify
		 * handler */
		move_mouse_to_monitor(m);
		/*curr_monitor = m;*/
		return 0;
	}

	_LOG_(INFO, "no monitor change occurred");
	return 0;
}

static int
traverse_stack_wrapper(arg_t *arg)
{
	direction_t	 d = arg->d;
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);

	if (w == wm->root_window)
		return 0;

	layout_t lay  = curr_monitor->desk->layout;
	node_t	*node = get_focused_node(curr_monitor->desk->tree);
	if (!node && lay != DECK)
		return -1;

	node_t *n = (lay == DECK)
					? next_deck_stack_node(curr_monitor->desk->tree, d, w)
					: (d == UP ? next_node(node) : prev_node(node));

	if (n == NULL) {
		return -1;
	}

	_focus_node_(curr_monitor->desk, n);
	if (n->client)
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
	if (_render_view_(curr_monitor->desk) != 0)
		return -1;
	if (_focus_input_(curr_monitor->desk, n) != 0)
		return -1;
	_flush_view_(curr_monitor->desk);
	return 0;
}

static int
exec_process(arg_t *arg)
{
	pid_t pid = fork();

	if (pid < 0) {
		perror("Fork failed");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		if (arg->argc == 1) {
			char *p		 = (char *)arg->cmd[0];
			char *args[] = {p, NULL};
			execvp(p, args);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		} else {
			const char *args[arg->argc + 1];
			for (int i = 0; i < arg->argc; i++) {
				args[i] = arg->cmd[i];
#ifdef _DEBUG__
				_LOG_(DEBUG, "args areee %s", args[i]);
#endif
			}
			args[arg->argc] = NULL;
			execvp(args[0], (char *const *)args);
			perror("execvp failed");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}

static int
close_or_kill_wrapper(arg_t *arg)
{
	(void)arg;
	xcb_window_t win = get_window_under_cursor(wm->connection, wm->root_window);
	if (!window_exists(wm->connection, win))
		return 0;
	return close_or_kill(win);
}

static int
transfer_node_wrapper(arg_t *arg)
{
	xcb_window_t w = get_window_under_cursor(wm->connection, wm->root_window);
	if (w == wm->root_window)
		return 0;
	const int i = arg->idx;

	if (curr_monitor->desk->id == i) {
		_LOG_(INFO, "switch node to curr desktop... abort");
		return 0;
	}

	if (is_tree_empty(curr_monitor->desk->tree)) {
		return 0;
	}

	node_t *node = NULL;
	if (!(node = get_focused_node(curr_monitor->desk->tree))) {
		_LOG_(ERROR, "focused node is null");
		return 0;
	}

	desktop_t *nd = curr_monitor->desktops[i];
	desktop_t *od = curr_monitor->desk;
#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", i + 1);
	log_tree_nodes(nd->tree);
	_LOG_(INFO, "old desktop %d nodes--------------", od->id + 1);
	log_tree_nodes(od->tree);
#endif
	if (set_desktop_visibility(node->client->window, false) != 0) {
		_LOG_(ERROR, "cannot hide window %d", node->client->window);
		return -1;
	}
	if (unlink_node(node, od)) {
		if (!transfer_node(node, nd)) {
			_LOG_(ERROR, "could not transfer node.. abort");
			return -1;
		}
	} else {
		_LOG_(ERROR, "could not unlink node.. abort");
		return -1;
	}

	od->n_count--;
	nd->n_count++;
	update_net_wm_desktop(node->client->window, nd->id);
	arrange_tree(nd->tree, nd->layout);
	if (nd->layout == STACK) {
		set_focus(node, true);
	}
	if (!is_tree_empty(od->tree)) {
		arrange_tree(od->tree, od->layout);
	}
	int ret = _render_view_(od);
	_flush_view_(od);
	return ret;
}

static int
switch_desktop_wrapper(arg_t *arg)
{
	if (arg->idx > conf.virtual_desktops) {
		return 0;
	}
	if (switch_desktop(arg->idx) != 0) {
		return -1;
	}
	last_desk_switch_time = get_time_millis();
	return 0;
}

static int
cycle_desktop_wrapper(arg_t *arg)
{
	int current = get_focused_desktop_idx();
	if (current == -1) {
		_LOG_(ERROR, "cnnot find current desktop");
		return -1;
	}

	int n_desktops = curr_monitor->n_of_desktops;
	int next = (current + (arg->d == RIGHT ? 1 : -1) + n_desktops) % n_desktops;

	switch_desktop(next);
	last_desk_switch_time = get_time_millis();
	return 0;
}

static int
grow_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const uint16_t	   step		  = 10;
	const resize_dir_t resize_dir = arg->rd;
	const uint16_t	   max_dim	  = (resize_dir == HORIZONTAL_DIR)
										? curr_monitor->rectangle.width
										: curr_monitor->rectangle.height;
	uint16_t		  *dim		  = (resize_dir == HORIZONTAL_DIR)
										? &n->floating_rectangle.width
										: &n->floating_rectangle.height;
	int16_t *pos = (resize_dir == HORIZONTAL_DIR) ? &n->floating_rectangle.x
												  : &n->floating_rectangle.y;

	if (*dim + (step * 2) > max_dim)
		return 0;

	*dim += (step * 2);
	*pos -= step;

	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

static int
shrink_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;
	const uint16_t	   step		  = 10;
	const resize_dir_t resize_dir = arg->rd;
	const uint16_t	   min_dim	  = step * 40;
	uint16_t		  *dim		  = (resize_dir == HORIZONTAL_DIR)
										? &n->floating_rectangle.width
										: &n->floating_rectangle.height;
	int16_t *pos = (resize_dir == HORIZONTAL_DIR) ? &n->floating_rectangle.x
												  : &n->floating_rectangle.y;
	if (*dim - (step * 2) < min_dim) {
		return 0;
	}
	*dim -= (step * 2);
	*pos += step;
	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

static int
resize_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const uint16_t	   step			 = 10;
	const resize_t	   resize_type	 = arg->r;
	const resize_dir_t resize_dir	 = arg->rd;
	int16_t			   delta		 = (resize_type == GROW ? step : -step);
	uint16_t		  *dim_to_resize = (resize_dir == HORIZONTAL_DIR)
										   ? &n->floating_rectangle.width
										   : &n->floating_rectangle.height;
	int16_t			  *pos_to_adjust = (resize_dir == HORIZONTAL_DIR)
										   ? &n->floating_rectangle.x
										   : &n->floating_rectangle.y;
	*pos_to_adjust -= delta / 2;
	*dim_to_resize += delta;
	if (*dim_to_resize <= 0) {
		*dim_to_resize = step;
		*pos_to_adjust += delta / 2;
	}
	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(
			n->client->window, n->floating_rectangle, conf.border_width) != 0) {
		return -1;
	}
	ungrab_pointer();
	return 0;
}

static int
shift_floating_window(arg_t *arg)
{
	node_t *n = NULL;
	if (!(n = get_focused_node(curr_monitor->desk->tree)))
		return -1;

	if (n->client && n->client->state != FLOATING)
		return 0;

	const int16_t	   step			= 10;
	rectangle_t		  *rect			= &n->floating_rectangle;
	const rectangle_t *monitor_rect = &curr_monitor->rectangle;
	const direction_t  dir			= arg->d;

	switch (dir) {
	case LEFT: rect->x -= step; break;
	case RIGHT: rect->x += step; break;
	case UP: rect->y -= step; break;
	case DOWN: rect->y += step; break;
	case NONE: return 0;
	}

	if (rect->x < monitor_rect->x) {
		rect->x = monitor_rect->x;
		return 0;
	}
	if (rect->x + rect->width > monitor_rect->x + monitor_rect->width) {
		rect->x = monitor_rect->x + monitor_rect->width - rect->width;
		return 0;
	}
	if (rect->y < monitor_rect->y) {
		rect->y = monitor_rect->y;
		return 0;
	}
	if (rect->y + rect->height > monitor_rect->y + monitor_rect->height) {
		rect->y = monitor_rect->y + monitor_rect->height - rect->height;
		return 0;
	}

	grab_pointer(wm->root_window, false);
	if (apply_window_geometry(n->client->window, *rect, conf.border_width) !=
		0) {
		return -1;
	}

	ungrab_pointer();
	return 0;
}

static int
start_keyboard_drag_wrapper(arg_t *arg)
{
	(void)arg;

	if (!curr_monitor || !curr_monitor->desk)
		return -1;

	node_t *root = curr_monitor->desk->tree;
	node_t *n	 = get_focused_node(root);

	if (!n || !n->client) {
		_LOG_(WARNING, "no focused window to drag");
		return -1;
	}

	int16_t cx = n->rectangle.x + n->rectangle.width / 2;
	int16_t cy = n->rectangle.y + n->rectangle.height / 2;

	xcb_warp_pointer(
		wm->connection, XCB_NONE, wm->root_window, 0, 0, 0, 0, cx, cy);
	xcb_flush(wm->connection);

	return drag_start(n->client->window, cx, cy, true);
}

/* ./src/bindings.c */



/* clang-format off */

/* keys_[] is used as a fallback in case of an
 * error while loading the keys from the config file */

/* see X11/keysymdef.h */

const size_t _keys_len = sizeof(_keys_) / sizeof(_keys_[0]);

/* clang-format on */

/* XKB stores the active keyboard group in the high state bits. Ignore it so WM
 * shortcuts stay physical while clients keep receiving the active layout. */
#define XKB_GROUP_STATE_MASK 0x6000

static int16_t
modfield_from_keysym(xcb_keysym_t keysym)
{
	uint16_t						  modfield = 0;
	xcb_keycode_t					 *keycodes = NULL, *mod_keycodes = NULL;
	xcb_get_modifier_mapping_reply_t *reply = NULL;
	xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(wm->connection);

	if ((keycodes = xcb_key_symbols_get_keycode(symbols, keysym)) == NULL ||
		(reply = xcb_get_modifier_mapping_reply(
			 wm->connection, xcb_get_modifier_mapping(wm->connection), NULL)) ==
			NULL ||
		reply->keycodes_per_modifier < 1 ||
		(mod_keycodes = xcb_get_modifier_mapping_keycodes(reply)) == NULL) {
		goto end;
	}

	unsigned int num_mod = xcb_get_modifier_mapping_keycodes_length(reply) /
						   reply->keycodes_per_modifier;
	for (unsigned int i = 0; i < num_mod; i++) {
		for (unsigned int j = 0; j < reply->keycodes_per_modifier; j++) {
			xcb_keycode_t mk =
				mod_keycodes[i * reply->keycodes_per_modifier + j];
			if (mk == XCB_NO_SYMBOL) {
				continue;
			}
			for (xcb_keycode_t *k = keycodes; *k != XCB_NO_SYMBOL; k++) {
				if (*k == mk) {
					modfield |= (1 << i);
				}
			}
		}
	}

end:
	xcb_key_symbols_free(symbols);
	_FREE_(keycodes);
	_FREE_(reply);
	return modfield;
}

static xcb_keycode_t *
get_keycode(xcb_keysym_t keysym, xcb_conn_t *conn)
{
	xcb_key_symbols_t *keysyms = NULL;
	xcb_keycode_t	  *keycode = NULL;

	if ((keysyms = xcb_key_symbols_alloc(conn)) == NULL) {
		xcb_key_symbols_free(keysyms);
		return NULL;
	}

	keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);

	return keycode;
}

static xcb_keysym_t
get_keysym(xcb_keycode_t keycode, xcb_connection_t *conn)
{
	xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(conn);
	xcb_keysym_t	   keysym  = 0;

	if (keysyms) {
		keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
		xcb_key_symbols_free(keysyms);
	}

	return keysym;
}

static uint16_t
normalize_mods(uint16_t state)
{
	const uint16_t nl = (uint16_t)modfield_from_keysym(XK_Num_Lock);

	return (uint16_t)(state & ~(XCB_MOD_MASK_LOCK | nl | XKB_GROUP_STATE_MASK));
}

static bool
mod_seen(const uint16_t *msn, size_t n, uint16_t m)
{
	for (size_t i = 0; i < n; i++) {
		if (msn[i] == m) {
			return true;
		}
	}
	return false;
}

static int
grab_key_variants(xcb_window_t win, uint16_t mod, xcb_keycode_t keycode)
{
	const uint16_t n_lock	  = (uint16_t)modfield_from_keysym(XK_Num_Lock);
	const uint16_t caps		  = XCB_MOD_MASK_LOCK;
	const uint16_t _ignored[] = {0, caps, n_lock, (uint16_t)(caps | n_lock)};
	uint16_t	   g[LEN(_ignored)] = {0};
	size_t		   g_len			= 0;

	for (size_t i = 0; i < LEN(_ignored); i++) {
		uint16_t v = (uint16_t)(mod | _ignored[i]);
		if (mod_seen(g, g_len, v)) {
			continue;
		}
		g[g_len++]		 = v;

		xcb_cookie_t cc	 = xcb_grab_key_checked(wm->connection,
												1,
												win,
												v,
												keycode,
												XCB_GRAB_MODE_ASYNC,
												XCB_GRAB_MODE_ASYNC);
		xcb_error_t *err = xcb_request_check(wm->connection, cc);
		if (err) {
			_LOG_(ERROR,
				  "error grabbing keycode %u with modifiers 0x%x: %d",
				  keycode,
				  v,
				  err->error_code);
			_FREE_(err);
			return -1;
		}
	}

	return 0;
}

static int
resolve_keycode(xcb_keysym_t keysym, xcb_keycode_t *out)
{
	xcb_keycode_t *key = get_keycode(keysym, wm->connection);
	if (key == NULL || *key == XCB_NO_SYMBOL) {
		_FREE_(key);
		return -1;
	}

	*out = *key;
	_FREE_(key);
	return 0;
}

static void
grab_super_button(xcb_window_t win, uint8_t button)
{
	const uint16_t numlock = (uint16_t)modfield_from_keysym(XK_Num_Lock);
	const uint16_t caps	   = XCB_MOD_MASK_LOCK;
	const uint16_t mods[]  = {
		SUPER,
		(uint16_t)(SUPER | caps),
		(uint16_t)(SUPER | numlock),
		(uint16_t)(SUPER | numlock | caps),
	};
	bool logged = false;

	for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
		uint16_t mod = mods[i];
		bool	 dup = false;
		for (size_t j = 0; j < i; j++) {
			if (mods[j] == mod) {
				dup = true;
				break;
			}
		}
		if (dup) {
			continue;
		}

		xcb_void_cookie_t c = xcb_grab_button_checked(
			wm->connection,
			0,	 /* owner_events */
			win, /* grab_window */
			XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
				XCB_EVENT_MASK_POINTER_MOTION,
			XCB_GRAB_MODE_ASYNC, /* allow processing */
			XCB_GRAB_MODE_ASYNC, /* keyboard_mode */
			XCB_NONE,			 /* confine_to */
			XCB_NONE,			 /* cursor */
			button,				 /* button */
			mod);				 /* modifiers */

		xcb_generic_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR,
				  "could not grab SUPER+Button%d on root (mod=0x%x): %d",
				  button,
				  mod,
				  err->error_code);
			_FREE_(err);
		} else if (!logged) {
			_LOG_(INFO, "grabbed SUPER+Button%d on root", button);
			logged = true;
		}
	}
}

static int
grab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return -1;
	}

	if (key_head) {
		conf_key_t *current = key_head;
		while (current) {
			if (resolve_keycode(current->keysym, &current->keycode) != 0) {
				return -1;
			}
			if (grab_key_variants(
					win, (uint16_t)current->mod, current->keycode) != 0) {
				return -1;
			}
			current = current->next;
		}
		is_kgrabbed = true;
		grab_super_button(win, XCB_BUTTON_INDEX_1);
		grab_super_button(win, XCB_BUTTON_INDEX_3);
		return 0;
	}

	_LOG_(INFO, "----grabbing default keys------");
	const size_t n = sizeof(_keys_) / sizeof(_keys_[0]);

	for (size_t i = n; i--;) {
		if (resolve_keycode(_keys_[i].keysym, &_keys_[i].keycode) != 0) {
			return -1;
		}
		if (grab_key_variants(
				win, (uint16_t)_keys_[i].mod, _keys_[i].keycode) != 0) {
			return -1;
		}
	}
	is_kgrabbed = true;

	grab_super_button(win, XCB_BUTTON_INDEX_1);
	grab_super_button(win, XCB_BUTTON_INDEX_3);

	return 0;
}

static void
ungrab_keys(xcb_conn_t *conn, xcb_window_t win)
{
	if (conn == NULL || win == XCB_NONE) {
		return;
	}

	const xcb_keycode_t modifier = (xcb_keycode_t)XCB_MOD_MASK_ANY;
	xcb_cookie_t		cookie =
		xcb_ungrab_key_checked(conn, XCB_GRAB_ANY, win, modifier);
	xcb_error_t *err = xcb_request_check(conn, cookie);
	if (err) {
		_LOG_(ERROR, "error ungrabbing keys: %d", err->error_code);
		_FREE_(err);
	}
}

/* ./src/client.c */



static int
show_window(xcb_window_t win, bool update_hidden_state);
static int
hide_window(xcb_window_t win, bool update_hidden_state);

static void
fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r);
static int
send_client_message(xcb_window_t win,
					xcb_atom_t	 property,
					xcb_atom_t	 value,
					xcb_conn_t	*conn);

static void
get_class_name(xcb_window_t win, char **out)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		/* out should be freed after it's copied over in the caller function */
		*out = strdup(t_reply.class_name);
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
}

static void
get_wm_name(xcb_window_t win, char **out)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	uint8_t					  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		/* out should be freed after it's copied over in the caller function */
		*out = strdup(t_reply.name);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}
}

static client_t *
create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *conn)
{
	client_t *c = (client_t *)calloc(1, sizeof(client_t));
	if (c == 0x00)
		return NULL;

	/*memset(c->class_name, 0, sizeof(c->class_name));
	memset(c->wm_name, 0, sizeof(c->wm_name));
	char *wm_name	 = NULL;
	char *class_name = NULL;
	get_class_name(win, &class_name);
	get_wm_name(win, &wm_name);
	if (wm_name && class_name) {
		strncpy(c->class_name, class_name, sizeof(c->class_name) - 1);
		strncpy(c->wm_name, wm_name, sizeof(c->wm_name) - 1);
_LOG_(INFO, "created client wm_class %s", c->class_name);
_LOG_(INFO, "created client wm_name %s", c->wm_name);
		_FREE_(wm_name);
		_FREE_(class_name);
	}*/

	c->window				= win;
	c->type					= wtype;
	c->border_width			= (uint32_t)-1;
	/* props are not being utlized, will use them in the future */
	c->props.delete_window	= false;
	c->props.input_hint		= false;
	c->props.take_focus		= false;
	c->mru_seq				= 0;
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {CLIENT_EVENT_MASK};
	xcb_cookie_t   cookie =
		xcb_change_window_attributes_checked(conn, c->window, mask, values);
	xcb_error_t *err = xcb_request_check(conn, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error setting window attributes for client %u: %d",
			  c->window,
			  err->error_code);
		_FREE_(err);
		_FREE_(c);
		return NULL;
	}

	if (change_border_attr(wm->connection,
						   win,
						   conf.normal_border_color,
						   conf.border_width,
						   false) != 0) {
		_LOG_(ERROR, "failed to change border attr for window %d", win);
		_FREE_(c);
		return NULL;
	}

	return c;
}

static bool
supports_protocol(xcb_window_t win, xcb_atom_t atom, xcb_conn_t *conn)
{
	xcb_get_property_cookie_t		   cookie = {0};
	xcb_icccm_get_wm_protocols_reply_t protocols;
	bool							   result = false;
	xcb_atom_t WM_PROTOCOLS					  = get_atom("WM_PROTOCOLS", conn);

	cookie = xcb_icccm_get_wm_protocols(conn, win, WM_PROTOCOLS);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1) {
		return false;
	}

	for (uint32_t i = 0; i < protocols.atoms_len; i++) {
		if (protocols.atoms[i] == atom) {
			result = true;
		}
	}

	xcb_icccm_get_wm_protocols_reply_wipe(&protocols);

	return result;
}

static int
display_client(rectangle_t r, xcb_window_t win)
{
	if (apply_window_geometry(win, r, conf.border_width) != 0) {
		return -1;
	}

	xcb_cookie_t cookie = xcb_map_window_checked(wm->connection, win);
	xcb_error_t *err	= xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(
			ERROR, "in mapping window %d: error code %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

static int
send_client_message(xcb_window_t win,
					xcb_atom_t	 property,
					xcb_atom_t	 value,
					xcb_conn_t	*conn)
{
	xcb_client_message_event_t *e = calloc(32, 1);
	e->response_type			  = XCB_CLIENT_MESSAGE;
	e->window					  = win;
	e->type						  = property;
	e->format					  = 32;
	e->data.data32[0]			  = value;
	e->data.data32[1]			  = XCB_CURRENT_TIME;
	xcb_cookie_t c				  = xcb_send_event_checked(
		conn, false, win, XCB_EVENT_MASK_NO_EVENT, (char *)e);

	xcb_error_t *err = xcb_request_check(conn, c);
	if (err) {
		_LOG_(ERROR, "error sending event: %d", err->error_code);
		_FREE_(e);
		_FREE_(err);
		return -1;
	}

	xcb_flush(conn);
	_FREE_(e);
	return 0;
}

static int
close_or_kill(xcb_window_t win)
{
	xcb_atom_t wm_delete = get_atom("WM_DELETE_WINDOW", wm->connection);
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);

	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (supports_protocol(win, wm_delete, wm->connection)) {
		if (wr == 1) {
#ifdef _DEBUG__
			_LOG_(DEBUG,
				  "window id = %d, reply name = %s: supports "
				  "WM_DELETE_WINDOW",
				  win,
				  t_reply.name);
#endif
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		}
		int ret = send_client_message(
			win, wm->ewmh->WM_PROTOCOLS, wm_delete, wm->connection);
		if (ret != 0) {
			_LOG_(ERROR, "failed to send client message");
			return -1;
		}
		return 0;
	}

	xcb_cookie_t c	 = xcb_kill_client_checked(wm->connection, win);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(
			ERROR, "error closing window: %d, error: %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static void
map_floating(xcb_window_t x)
{
	rectangle_t				  rc = {0};
	xcb_get_geometry_reply_t *g	 = get_geometry(x, wm->connection);
	if (g == NULL) {
		return;
	}

	rc.height = g->height;
	rc.width  = g->width;
	rc.x	  = g->x;
	rc.y	  = g->y;

	_FREE_(g);
	apply_window_geometry(x, rc, conf.border_width);
	xcb_map_window(wm->connection, x);
}

static bool
client_exist_in_desktops(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; ++i) {
			if (!is_tree_empty(curr->desktops[i]->tree)) {
				if (client_exist(curr->desktops[i]->tree, win))
					return true;
			}
		}
		curr = curr->next;
	}
	return false;
}

static void
find_window_in_desktops(desktop_t  **curr_desktop,
						node_t	   **curr_node,
						xcb_window_t win,
						bool		*found)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			desktop_t *d = curr->desktops[i];
			node_t	  *n = find_node_by_window_id(d->tree, win);
			if (n) {
				*curr_desktop = d;
				*curr_node	  = n;
				*found		  = true;
				_LOG_(DEBUG, "window %d found in desktop %d", win, i);
				return;
			}
		}
		curr = curr->next;
	}
	_LOG_(ERROR, "window %d not found in any desktop", win);
}

static int
kill_window(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "[KILL_WINDOW] KILL WINDOW START ");
	_LOG_(DEBUG,
		  "[KILL_WINDOW] killing win=%d name='%s'",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	if (win == XCB_NONE) {
#ifdef _DEBUG__
		_LOG_(DEBUG, "[KILL_WINDOW] win is XCB_NONE, aborting");
#endif
		return -1;
	}

	if (win == wm->root_window) {
		_LOG_(INFO, "root window, returning %d", win);
		return 0;
	}

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);

	if (wr == 1) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG, "delete window id = %d, reply name = %s", win, t_reply.name);
#endif
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}

	desktop_t *d			   = curr_monitor->desk;
	node_t	  *n			   = find_node_by_window_id(d->tree, win);
	client_t  *c			   = (n) ? n->client : NULL;
	bool	   another_desktop = false;
	if (c == NULL) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[KILL_WINDOW] win %d not in current desktop %d, searching other "
			  "desktops",
			  win,
			  d->id);
#endif
		/* window isn't in current desktop */
		find_window_in_desktops(&d, &n, win, &another_desktop);
		c = (n) ? n->client : NULL;
		if (c == NULL) {
			_LOG_(ERROR, "cannot find client with window %d", win);
			return -1;
		}
#ifdef _DEBUG__
		_LOG_(DEBUG, "[KILL_WINDOW] found win %d in desktop %d", win, d->id);
#endif
	} else {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[KILL_WINDOW] found win %d in current desktop %d",
			  win,
			  d->id);
#endif
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] unmapping win=%d before deletion", c->window);
#endif
	xcb_cookie_t cookie = xcb_unmap_window(wm->connection, c->window);
	xcb_error_t *err	= xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR,
			  "error in unmapping window %d: error code %d",
			  c->window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] calling delete_node for win=%d", c->window);
#endif
	delete_node(n, d);
	ewmh_update_client_list();

	if (is_tree_empty(d->tree)) {
		d->logical_focus = NULL;
		d->last_focused	 = XCB_NONE;
		if (!another_desktop) {
			set_active_window_name(XCB_NONE);
			focused_win = XCB_NONE;
		}
	} else {
		/* pick fallback before rendering so MONOCLE/DECK never flicker to a
		 * hidden window between delete and the next render */
		node_t *nn = _pick_focus_(d);
		if (nn) {
			_focus_node_(d, nn);
			if (!another_desktop)
				nn->client->mru_seq = get_next_mru_seq(curr_monitor);
		}
	}

	if (!another_desktop) {
		if (_render_view_(d) != 0) {
			_LOG_(ERROR, "cannot render tree");
			return -1;
		}
		if (!is_tree_empty(d->tree) && d->logical_focus &&
			d->logical_focus->client) {
			_focus_input_(d, d->logical_focus);
			focused_win = d->logical_focus->client->window;
			set_active_window_name(focused_win);
		}
		_flush_view_(d);
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[KILL_WINDOW] kill_window complete for win=%d", win);
#endif
	return 0;
}

static node_t *
find_node_global(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(
		DEBUG,
		"[FIND_NODE_GLOBAL] searching for win=%d name='%s' across all desktops",
		win,
		name ? name : "(null)");
	_FREE_(name);
#endif
	monitor_t *m = head_monitor;

	while (m) {
		for (int i = 0; i < m->n_of_desktops; i++) {
			desktop_t *d = m->desktops[i];
			if (!d || !d->tree)
				continue;
			node_t *n = find_node_by_window_id(d->tree, win);
			if (n) {
#ifdef _DEBUG__
				_LOG_(DEBUG,
					  "[FIND_NODE_GLOBAL] window %d found in monitor='%s' "
					  "desktop=%d",
					  win,
					  m->name,
					  d->id);
#endif
				return n;
			}
		}
		m = m->next;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG, "[FIND_NODE_GLOBAL] window %d not found in ANY desktop", win);
#endif
	return NULL;
}

static bool
should_manage(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_window_attributes_cookie_t attr_cookie;
	xcb_get_window_attributes_reply_t *attr_reply;

	attr_cookie		 = xcb_get_window_attributes(conn, win);
	xcb_error_t *err = NULL;
	attr_reply		 = xcb_get_window_attributes_reply(conn, attr_cookie, &err);

	if (err) {
		_LOG_(DEBUG,
			  "cannot get attributes for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return false;
	}
	if (attr_reply == NULL) {
		return false;
	}

	bool manage = !attr_reply->override_redirect;
	_FREE_(attr_reply);
	return manage;
}

static int
apply_floating_hints(xcb_window_t win)
{
	xcb_get_property_cookie_t c =
		xcb_icccm_get_wm_normal_hints(wm->connection, win);
	xcb_size_hints_t size_hints;

	uint8_t			 r = xcb_icccm_get_wm_normal_hints_reply(
		wm->connection, c, &size_hints, NULL);
	if (1 == r) {
		/* if min-h == max-h && min-w == max-w, */
		/* then window should be floated */
		uint32_t size_mask =
			(XCB_ICCCM_SIZE_HINT_P_MIN_SIZE | XCB_ICCCM_SIZE_HINT_P_MAX_SIZE);
		int32_t miw = size_hints.min_width;
		int32_t mxw = size_hints.max_width;
		int32_t mih = size_hints.min_height;
		int32_t mxh = size_hints.max_height;

		if ((size_hints.flags & size_mask) && (miw == mxw) && (mih == mxh)) {
			return 0;
		}
	}
	return -1;
}

static bool
should_ignore_hints(xcb_window_t win, const char *name)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		if (strcasecmp(t_reply.class_name, name) == 0) {
			xcb_icccm_get_wm_class_reply_wipe(&t_reply);
			return true;
		}
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
	return false;
}

static bool
is_transient(xcb_window_t win)
{
	xcb_window_t			  transient = XCB_NONE;
	xcb_get_property_cookie_t c =
		xcb_icccm_get_wm_transient_for(wm->connection, win);
	const uint8_t r = xcb_icccm_get_wm_transient_for_reply(
		wm->connection, c, &transient, NULL);

	if (r != 1) {
		return false;
	}

	if (transient != XCB_NONE) {
		return true;
	}
	return false;
}

static int
handle_first_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling first ever window %s id %d", name, client->window);
	_FREE_(name);
#endif
	rectangle_t r = {0};
	fill_root_rectangle(&r);

	if (client == NULL) {
		_LOG_(ERROR, "client is null");
		return -1;
	}

	d->tree			   = init_root();
	d->tree->client	   = client;
	d->tree->rectangle = r;
	d->n_count += 1;
	update_net_wm_desktop(client->window, d->id);
	set_focus(d->tree, true);
	/*d->node = d->tree;*/
	ewmh_update_client_list();
	client->mru_seq = get_next_mru_seq(curr_monitor);
	int ret			= tile(d->tree);
	restack();
	return ret;
}

static int
handle_subsequent_window(client_t *client, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling tiled window %s id %d", name, client->window);
	_FREE_(name);
#endif
	xcb_window_t wi = get_window_under_cursor(wm->connection, wm->root_window);
	node_t		*n	= NULL;

	if (client == NULL) {
		_LOG_(ERROR, "client is null");
		return -1;
	}

	n = get_focused_node(d->tree);

	if (n == NULL || n->client == NULL) {
		_LOG_(ERROR, "cannot find node with window id");
		return -1;
	}

	if (IS_FLOATING(n->client) && !IS_ROOT(n)) {
		_LOG_(INFO, "node under cursor is floating %d", n->client->window);
		n = find_any_leaf(d->tree);
		if (n == NULL) {
			return 0;
		}
	}

	if (IS_FULLSCREEN(n->client)) {
		set_fullscreen(n, false);
	}

	node_t *new_node = create_node(client);
	if (new_node == NULL) {
		_LOG_(ERROR, "new node is null");
		return -1;
	}

	insert_node(n, new_node, d->layout);
	d->n_count += 1;
	update_net_wm_desktop(client->window, d->id);
	/*curr_monitor->desk->node = new_node;*/
	ewmh_update_client_list();
	client->mru_seq = get_next_mru_seq(curr_monitor);

	/* set logical focus before rendering so MONOCLE/DECK show the new window */
	_focus_node_(d, new_node);

	int ret = _render_view_(d);
	if (ret == 0)
		ret = _focus_input_(d, new_node);
	_flush_view_(d);
	return ret;
}

static void
fill_floating_rectangle(xcb_get_geometry_reply_t *geometry, rectangle_t *r)
{
	int x  = curr_monitor->rectangle.x + (curr_monitor->rectangle.width / 2) -
			 (geometry->width / 2);
	int y  = curr_monitor->rectangle.y + (curr_monitor->rectangle.height / 2) -
			 (geometry->height / 2);
	(*r).x = x;
	(*r).y = y;
	(*r).width	= geometry->width;
	(*r).height = geometry->height;
}

static bool
client_is_modal_transient(const client_t *c)
{
	if (!c)
		return false;
	return c->ewmh_type == WINDOW_TYPE_DIALOG ||
		   ewmh_has(c->ewmh_state, EWMH_STATE_MODAL) ||
		   c->transient_for != XCB_NONE;
}

static int
focus_modal_transient(desktop_t *d, node_t *n)
{
	if (!d || !n || !n->client)
		return 0;

	if (focused_win != XCB_NONE)
		win_focus(focused_win, false);
	if (_focus_input_(d, n) != 0)
		return -1;

	n->is_focused	   = true;
	focused_win		   = n->client->window;
	n->client->mru_seq = get_next_mru_seq(curr_monitor);
	set_active_window_name(focused_win);
	return 0;
}

static int
handle_floating_window(client_t *client, desktop_t *d)
{
	/* floating is not just "tile=false".
	 * dialogs/transients can take real focus, but hidden layouts still need
	 * their old tiled logical_focus or monocle/deck jumps around. */
#ifdef _DEBUG__
	char *name = win_name(client->window);
	_LOG_(DEBUG, "handling floating window %s id %d", name, client->window);
	_FREE_(name);
#endif

	xcb_get_geometry_reply_t *g = NULL;
	if (is_tree_empty(d->tree)) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[HANDLE_FLOATING] tree is empty, creating root node for win=%d",
			  client->window);
#endif
		d->tree			= init_root();
		d->tree->client = client;
		g				= get_geometry(client->window, wm->connection);
		if (g == NULL) {
			_LOG_(ERROR, "cannot get %d geometry", client->window);
			return -1;
		}
		fill_floating_rectangle(g, &d->tree->floating_rectangle);
		fill_root_rectangle(&d->tree->rectangle);
		_FREE_(g);
		d->n_count += 1;
		update_net_wm_desktop(client->window, d->id);
		ewmh_update_client_list();
		set_focus(d->tree, true);
		client->mru_seq = get_next_mru_seq(curr_monitor);
		int ret			= tile(d->tree);
		if (client_is_modal_transient(client)) {
			focused_win = client->window;
			set_active_window_name(focused_win);
		}
		restack();
		return ret;
	} else {
		const bool modal_transient = client_is_modal_transient(client);
		node_t	  *n			   = NULL;

		if (client->transient_for != XCB_NONE)
			n = find_node_by_window_id(d->tree, client->transient_for);

		if (!n) {
			xcb_window_t wi =
				get_window_under_cursor(wm->connection, wm->root_window);
			if (wi != wm->root_window && wi != 0)
				n = find_node_by_window_id(d->tree, wi);
		}

		n = n == NULL ? find_any_leaf(d->tree) : n;
		if (n == NULL || n->client == NULL) {
			_FREE_(client);
			return -1;
		}

		node_t *new_node = create_node(client);
		if (new_node == NULL) {
			_FREE_(client);
			_LOG_(ERROR, "new node is null");
			return -1;
		}

		g = get_geometry(client->window, wm->connection);
		if (g == NULL) {
			_LOG_(ERROR, "cannot get %d geometry", client->window);
			return -1;
		}
		fill_floating_rectangle(g, &new_node->floating_rectangle);
		new_node->rectangle = new_node->floating_rectangle;
		_FREE_(g);
		insert_node(n, new_node, d->layout);
		if (d->logical_focus == n && n->first_child && n->first_child->client) {
			d->logical_focus = n->first_child;
			if (IS_TILED(n->first_child->client))
				d->last_focused = n->first_child->client->window;
		}
		d->n_count += 1;
		update_net_wm_desktop(client->window, d->id);
		ewmh_update_client_list();
		client->mru_seq = get_next_mru_seq(curr_monitor);
		/* modal/transient blocks parent. focus it for real, but do not make
		 * it the hidden layout tiled selection. */
		if (modal_transient) {
			int ret = _render_view_(d);
			if (ret == 0)
				ret = focus_modal_transient(d, new_node);
			_flush_view_(d);
			return ret;
		}
		/* floating spawn must not steal logical_focus in monocle/deck.
		 * normal layouts are fine, they dont hide tiled windows. */
		if (d->layout != MONOCLE && d->layout != DECK)
			_focus_node_(d, new_node);
		int ret = _render_view_(d);
		if (ret == 0)
			ret = _focus_input_(d, new_node);
		_flush_view_(d);
		return ret;
	}
}

static int
insert_into_desktop(int idx, xcb_window_t win, bool is_tiled)
{
	desktop_t *d = curr_monitor->desktops[--idx];
	assert(d);
	if (find_node_by_window_id(d->tree, win)) {
		return 0;
	}

	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = is_tiled ? TILED : FLOATING;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	update_net_wm_desktop(client->window, d->id);
	set_window_state(client->window, XCB_ICCCM_WM_STATE_ICONIC);
	if (client->state == FLOATING) {
		xcb_get_geometry_reply_t *g = NULL;
		if (is_tree_empty(d->tree)) {
			d->tree			= init_root();
			d->tree->client = client;
			g				= get_geometry(client->window, wm->connection);
			if (g == NULL) {
				_LOG_(ERROR, "cannot get %d geometry", client->window);
				return -1;
			}
			fill_floating_rectangle(g, &d->tree->floating_rectangle);
			fill_root_rectangle(&d->tree->rectangle);
			_FREE_(g);
			d->n_count += 1;
			ewmh_update_client_list();
		} else {
			node_t *n = NULL;
			n		  = find_any_leaf(d->tree);
			if (n == NULL || n->client == NULL) {
				char *name = win_name(win);
				_LOG_(INFO, "cannot find win  %s:%d", name, win);
				_FREE_(name);
				return 0;
			}
			node_t *new_node = create_node(client);
			if (new_node == NULL) {
				_FREE_(client);
				_LOG_(ERROR, "new node is null");
				return -1;
			}
			g = get_geometry(client->window, wm->connection);
			if (g == NULL) {
				_LOG_(ERROR, "cannot get %d geometry", client->window);
				return -1;
			}
			fill_floating_rectangle(g, &new_node->floating_rectangle);
			new_node->rectangle = new_node->floating_rectangle;
			_FREE_(g);
			insert_node(n, new_node, d->layout);
			d->n_count += 1;
			ewmh_update_client_list();
		}
	} else {
		if (is_tree_empty(d->tree)) {
			rectangle_t r = {0};
			fill_root_rectangle(&r);
			d->tree			   = init_root();
			d->tree->client	   = client;
			d->tree->rectangle = r;
			d->n_count += 1;
			ewmh_update_client_list();
		} else {
			node_t *n = NULL;
			n		  = find_any_leaf(d->tree);
			if (n == NULL || n->client == NULL) {
				_LOG_(ERROR, "cannot find node with window id");
				return -1;
			}
			if (IS_FULLSCREEN(n->client)) {
				set_fullscreen(n, false);
			}
			if (n->client->state == FLOATING) {
				return 0;
			}
			node_t *new_node = create_node(client);
			if (new_node == NULL) {
				_FREE_(client);
				_LOG_(ERROR, "new node is null");
				return -1;
			}
			insert_node(n, new_node, d->layout);
			d->n_count += 1;
			if (d->layout == STACK) {
				set_focus(new_node, true);
			}
			ewmh_update_client_list();
		}
	}
	restack();
	return 0;
}

static int
handle_tiled_window_request(xcb_window_t win, desktop_t *d)
{
	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = TILED;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	fill_icccm_ewmh(client);

	if (is_tree_empty(d->tree)) {
		return handle_first_window(client, d);
	}

	return handle_subsequent_window(client, d);
}

static int
handle_floating_window_request(xcb_window_t win, desktop_t *d)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "window %s id %d is floating", name, win);
	_FREE_(name);
#endif
	client_t *client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
	if (client == NULL) {
		_LOG_(ERROR, "cannot allocate memory for client");
		return -1;
	}

	client->state = FLOATING;
	if (!conf.focus_follow_pointer) {
		window_grab_buttons(client->window);
	}
	fill_icccm_ewmh(client);
	return handle_floating_window(client, d);
}

static int
find_desktop_by_window(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			desktop_t *d = curr->desktops[i];
			node_t	  *n = find_node_by_window_id(d->tree, win);
			if (n) {
				return d->id;
			}
		}
		curr = curr->next;
	}
	return -1;
}

static int
handle_net_wm_desktop(xcb_window_t win, uint32_t index)
{
	if (index > wm->ewmh->_NET_NUMBER_OF_DESKTOPS - 1) {
		return 0;
	}
	desktop_t *td	 = curr_monitor->desktops[index];
	node_t	  *n	 = NULL;
	desktop_t *d	 = NULL;
	bool	   found = false;
	find_window_in_desktops(&d, &n, win, &found);
	if (!found) {
		return 0;
	}
	if (!n || !d) {
		_LOG_(ERROR, "desktop or node is null");
		return -1;
	}
	if (set_desktop_visibility(n->client->window, false) != 0) {
		_LOG_(ERROR, "cannot hide window %d", n->client->window);
		return -1;
	}
	if (unlink_node(n, d)) {
		if (!transfer_node(n, td)) {
			_LOG_(ERROR, "could not transfer node.. abort");
			return -1;
		}
	} else {
		_LOG_(ERROR, "could not unlink node.. abort");
		return -1;
	}

	d->n_count--;
	td->n_count++;
	update_net_wm_desktop(n->client->window, td->id);
	arrange_tree(td->tree, td->layout);
	if (td->layout == STACK) {
		set_focus(n, true);
	}
	if (!is_tree_empty(d->tree)) {
		arrange_tree(d->tree, d->layout);
	}

	bool render = curr_monitor->desk == d;
	return render ? _render_view_(d) : 0;
}

#if 0
static int
show_window(xcb_window_t win, node_t *n)
{
	if (win == XCB_NONE || n == NULL) {
_LOG_(ERROR, "show_window: invalid args (win=%d, node=%p)", win, n);
		return -1;
	}

	if (!n->client) {
_LOG_(ERROR, "show_window: node has no client for win=%d", win);
		return -1;
	}

	rectangle_t r =
		IS_FLOATING(n->client) ? n->floating_rectangle : n->rectangle;
	uint32_t	   vals[4] = {r.x, r.y, r.width, r.height};

	const uint16_t mask	   = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
						  XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

	_LOG_(DEBUG,
		  "showing window %d at (%d, %d) size (%d x %d)%s",
		  win,
		  r.x,
		  r.y,
		  r.width,
		  r.height,
		  IS_FLOATING(n->client) ? " [FLOATING]" : "");

	xcb_cookie_t ck =
		xcb_configure_window_checked(wm->connection, win, mask, vals);

	xcb_error_t *err = xcb_request_check(wm->connection, ck);
	if (err) {
		_LOG_(ERROR,
			  "show_window: failed to show window %d "
			  "pos (%d,%d) size (%d,%d) error=%d",
			  win,
			  r.x,
			  r.y,
			  r.width,
			  r.height,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}
#endif

static int
show_window(xcb_window_t win, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[SHOW_WINDOW] showing win=%d name='%s' (setting WM_STATE to NORMAL, "
		  "then mapping)",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	xcb_error_t		*err;
	xcb_cookie_t	 c;
	/* According to ewmh:
	 * Mapped windows should be placed in NormalState, according to
	 * the ICCCM */
	const long		 data[] = {XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, wm_s, wm_s, 32, 2, data);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	if (update_hidden_state) {
		update_net_wm_state_atom(win, wm->ewmh->_NET_WM_STATE_HIDDEN, false);
		node_t *n = find_node_global(win);
		if (n && n->client) {
			update_client_ewmh_state(n->client, EWMH_STATE_HIDDEN, false);
		}
	}

	c	= xcb_map_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SHOW_WINDOW] successfully mapped win=%d", win);
#endif
	return 0;
}

#if 0
static int
hide_window(xcb_window_t win)
{
	if (win == XCB_NONE) {
_LOG_(ERROR, "invalid window (XCB_NONE)");
		return -1;
	}

	const int32_t  offscreen = -20000;
	const uint32_t values[2] = {offscreen, offscreen};
	const uint16_t mask		 = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;

_LOG_(DEBUG, "hiding window %d at (%d, %d)", win, offscreen, offscreen);

	xcb_cookie_t ck =
		xcb_configure_window_checked(wm->connection, win, mask, values);

	xcb_error_t *err = xcb_request_check(wm->connection, ck);
	if (err) {
		_LOG_(ERROR,
			  "failed to configure window %d offscreen (error=%d)",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}
#endif

static int
hide_window(xcb_window_t win, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[HIDE_WINDOW] hiding win=%d name='%s' (setting WM_STATE to ICONIC, "
		  "then unmapping)",
		  win,
		  name ? name : "(null)");
	_FREE_(name);
#endif
	xcb_error_t		*err;
	xcb_cookie_t	 c;
	/* According to ewmh:
	 * Unmapped windows should be placed in IconicState, according to
	 * the ICCCM. Windows which are actually iconified or minimized
	 * should have the _NET_WM_STATE_HIDDEN property set, to
	 * communicate to pagers that the window should not be represented
	 * as "onscreen."
	 **/
	const long		 data[] = {XCB_ICCCM_WM_STATE_ICONIC, XCB_NONE};
	const xcb_atom_t wm_s	= get_atom("WM_STATE", wm->connection);
	c						= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, wm_s, wm_s, 32, 2, data);
	err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR,
			  "cannot change window property %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	if (update_hidden_state) {
		update_net_wm_state_atom(win, wm->ewmh->_NET_WM_STATE_HIDDEN, true);
		node_t *n = find_node_global(win);
		if (n && n->client) {
			update_client_ewmh_state(n->client, EWMH_STATE_HIDDEN, true);
		}
	}

	c	= xcb_unmap_window_checked(wm->connection, win);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot hide window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[HIDE_WINDOW] successfully unmapped win=%d", win);
#endif
	return 0;
}

static int
set_visibility_mode(xcb_window_t win, bool is_visible, bool update_hidden_state)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[VISIBILITY] set_visibility called: win=%d name='%s' is_visible=%s",
		  win,
		  name ? name : "(null)",
		  is_visible ? "TRUE" : "FALSE");
	_FREE_(name);
#endif
	/* zwm must NOT recieve events before mapping (showing) or unmapping
	 * (hiding) windows.
	 * otherwise, it will recieve unmap/map notify and handle it as it
	 * should, this results in deleting or spanning the window that is
	 * meant to be hidden or shown */
	const uint32_t _off[] = {ROOT_EVENT_MASK &
							 ~XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	const uint32_t _on[]  = {ROOT_EVENT_MASK};
	xcb_error_t	  *err;
	xcb_cookie_t   c;
	int			   ret = 0;

	/* stop zwm from recieving events */
	c				   = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _off);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	/* node_t *n = find_node_global(win); */
	/* if (!n) { */
	/* _LOG_(ERROR, "cannot fin win %d", win); */
	/* return -1; */
	/* } */
	/* ret = is_visible ? show_window(win, n) : hide_window(win); */
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[VISIBILITY] calling %s for win=%d",
		  is_visible ? "show_window" : "hide_window",
		  win);
#endif
	ret = is_visible ? show_window(win, update_hidden_state)
					 : hide_window(win, update_hidden_state);
	if (ret == -1) {
		_LOG_(
			ERROR, "cannot set visibilty to %s", is_visible ? "true" : "false");
#ifdef _DEBUG__
	} else {
		_LOG_(DEBUG,
			  "[VISIBILITY] successfully set visibility to %s for win=%d",
			  is_visible ? "true" : "false",
			  win);
#endif
	}

	/* subscribe for events again */
	c = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_EVENT_MASK, _on);
	err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot change root window %d attrs: error code %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[VISIBILITY] set_visibility completed successfully for win=%d",
		  win);
#endif
	return 0;
}

static int
set_visibility(xcb_window_t win, bool is_visible)
{
	return set_visibility_mode(win, is_visible, true);
}

static int
set_desktop_visibility(xcb_window_t win, bool is_visible)
{
	return set_visibility_mode(win, is_visible, false);
}

/* ./src/config_parser.c */


#define MAX_LINE_LENGTH (2 << 9)
#define MAX_KEYBINDINGS 45

#ifdef __LTEST__
#define CONF_PATH	  "./zwm.conf"
#define TEMPLATE_PATH "./zwm.conf"
#else
#define CONF_PATH	  ".config/zwm/zwm.conf"
#define TEMPLATE_PATH "/usr/share/zwm/zwm.conf"
#endif

typedef enum {
	WHITE_SPACE,
	CURLY_BRACKET,
	PARENTHESIS,
	SQUARE_BRACKET,
	QUOTATION
} trim_token_t;


static void
free_tokens(char **, int);

/* clang-format off */

/* clang-format on */

static int (*str_to_func(char *ch))(arg_t *)
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(_cmapper_[i].func_name, ch) == 0) {
			return _cmapper_[i].execute;
		}
	}
	return NULL;
}

static char *
func_to_str(int (*ptr)(arg_t *))
{
	int n = sizeof(_cmapper_) / sizeof(_cmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_cmapper_[i].execute == ptr) {
			return _cmapper_[i].func_name;
		}
	}
	return NULL;
}

static uint32_t
str_to_key(char *ch)
{
	int n = sizeof(_kmapper_) / sizeof(_kmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (strcmp(_kmapper_[i].key, ch) == 0) {
			return _kmapper_[i].keysym;
		}
	}

	return -1;
}

static char *
key_to_str(uint32_t val)
{
	int n = sizeof(_kmapper_) / sizeof(_kmapper_[0]);
	for (int i = 0; i < n; i++) {
		if (_kmapper_[i].keysym == val) {
			return _kmapper_[i].key;
		}
	}

	return NULL;
}

static int
file_exists(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (file) {
		fclose(file);
		return -1;
	}
	return 0;
}

static void
print_key_array(void)
{
	conf_key_t *current = key_head;
	int			c		= 0;
	while (current) {
		if (current->arg) {
			if (current->arg->cmd) {
				for (int j = 0; j < current->arg->argc; ++j) {
					_LOG_(DEBUG, "cmd = %s", current->arg->cmd[j]);
				}
			}
			_LOG_(DEBUG,
				  "key %d = { \n mod = %s \n keysym = %s, func = %s, "
				  "\nargs = {.idx = %d, .d = %d, .r = %d, .t = %d}",
				  c,
				  key_to_str(current->mod),
				  key_to_str(current->keysym),
				  func_to_str(current->execute),
				  current->arg->idx,
				  current->arg->d,
				  current->arg->r,
				  current->arg->t,
				  current->arg->t);
		}
		c++;
		current = current->next;
	}
}

static int
write_default_config(const char *filename, config_t *c)
{
	const char *tp = TEMPLATE_PATH;

	char		dir_path[strlen(filename) + 1];
	strcpy(dir_path, filename);
	char *last_slash = strrchr(dir_path, '/');
	if (last_slash) {
		*last_slash = '\0';
		struct stat st;
		if (stat(dir_path, &st) == -1) {
			if (mkdir(dir_path, 0777) == -1) {
				_LOG_(ERROR, "failed to create directory: %s", dir_path);
				return -1;
			}
		}
	}

	FILE *tf = fopen(tp, "r");
	if (tf == NULL) {
		_LOG_(ERROR, "failed to open template file: %s", tp);
		return -1;
	}

	FILE *df = fopen(filename, "w");
	if (df == NULL) {
		_LOG_(ERROR, "failed to create config file: %s", filename);
		fclose(tf);
		return -1;
	}

	char   buffer[4096];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), tf)) > 0) {
		if (fwrite(buffer, 1, bytes, df) != bytes) {
			_LOG_(ERROR, "error writing to file: %s", filename);
			fclose(tf);
			fclose(df);
			return -1;
		}
	}

	fclose(tf);
	fclose(df);

	/* default config values */
	c->active_border_color	= 0x4a4a48;
	c->normal_border_color	= 0x30302f;
	c->border_width			= 2;
	c->window_gap			= 10;
	c->virtual_desktops		= 7;
	c->focus_follow_pointer = false;
	c->focus_follow_spawn	= false;

	return 0;
}

static void
trim(char *str, trim_token_t t)
{
	if (str == NULL) {
		return;
	}

	char *end	= str + strlen(str) - 1;
	char *start = str;
	char  start_token, end_token;

	switch (t) {
	case WHITE_SPACE: {
		start_token = ' ';
		end_token	= ' ';
		break;
	}
	case CURLY_BRACKET: {
		start_token = '{';
		end_token	= '}';
		break;
	}
	case PARENTHESIS: {
		start_token = '(';
		end_token	= ')';
		break;
	}
	case SQUARE_BRACKET: {
		start_token = '[';
		end_token	= ']';
		break;
	}
	case QUOTATION: {
		start_token = '"';
		end_token	= '"';
		break;
	}
	default: return;
	}

	while (end >= str &&
		   (*end == end_token || (t == WHITE_SPACE && isspace(*end)))) {
		*end = '\0';
		end--;
	}

	while (*start == start_token || (t == WHITE_SPACE && isspace(*start))) {
		start++;
	}

	if (start != str) {
		memmove(str, start, strlen(start) + 1);
	}
}

/* caller must free using free_tokens(...) */
static char **
split_string(const char *str, char delimiter, int *count)
{
	int i		   = 0;
	int num_tokens = 1;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] == delimiter) {
			num_tokens++;
		}
	}

	char **tokens = (char **)malloc((num_tokens + 1) * sizeof(char *));
	if (tokens == NULL) {
		_LOG_(ERROR, "failed to allocate memory");
		return NULL;
	}

	char *str_copy = strdup(str);
	if (str_copy == NULL) {
		_LOG_(ERROR, "failed to duplicate string");
		_FREE_(tokens);
		return NULL;
	}

	char  delim_str[2] = {delimiter, '\0'};
	char *token		   = strtok(str_copy, delim_str);
	i				   = 0;
	while (token && i < num_tokens) {
		tokens[i] = strdup(token);
		if (tokens[i] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			_FREE_(str_copy);
			free_tokens(tokens, i);
			return NULL;
		}
		i++;
		token = strtok(NULL, delim_str);
	}
	tokens[i] = NULL;

	*count	  = i;
	_FREE_(str_copy);
	return tokens;
}

static void
free_tokens(char **tokens, int count)
{
	if (tokens) {
		for (int i = 0; i < count; i++) {
			if (tokens[i]) {
				_FREE_(tokens[i]);
			}
		}
	}
	_FREE_(tokens);
}

static bool
key_exist(conf_key_t *key)
{
	conf_key_t *current = key_head;
	while (current) {
		if (current->execute == key->execute &&
			current->keysym == key->keysym) {
			return true;
		}
		current = current->next;
	}

	return false;
}

/* caller must free */
static char *
extract_body(const char *str)
{
	const char *start = strchr(str, '(');
	if (start == NULL) {
		return NULL;
	}

	const char *end = strchr(start, ')');
	if (end == NULL) {
		return NULL;
	}
	size_t length = end - start + 1;
	char  *result = (char *)malloc(length + 1);
	if (result == NULL) {
		_LOG_(ERROR, "failed to allocate memory");
		return NULL;
	}

	strncpy(result, start, length);
	result[length] = '\0';
	return result;
}

static uint32_t
parse_mod_key(char *mod)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "recieved mod key = (%s)", mod);
#endif
	uint32_t _mod = str_to_key(mod);
	uint32_t mask = -1;
	if ((int)_mod == -1) {
		int	   count;
		char **mods = split_string(mod, '|', &count);
		if (mods == NULL) {
			_LOG_(ERROR, "failed to split string %s", mod);
			return -1;
		}
		uint32_t mask1 = str_to_key(mods[0]);
		if ((int)mask1 == -1) {
			_LOG_(ERROR, "failed to find key (%s)", mods[0]);
		}
		uint32_t mask2 = str_to_key(mods[1]);
		if ((int)mask2 == -1) {
			_LOG_(ERROR, "failed to find key (%s)", mods[1]);
		}
		mask = mask1 | mask2;
		free_tokens(mods, count);
	} else {
		mask = _mod;
	}
	return mask;
}

static uint32_t
parse_keysym(char *keysym)
{
	uint32_t keysym_ = str_to_key(keysym);
	if ((int)keysym_ == -1) {
		_LOG_(ERROR, "failed to find keysym %s", keysym);
		return -1;
	}

	return keysym_;
}

static void
err_cleanup(conf_key_t *k)
{
	if (k) {
		if (k->arg) {
			if (k->arg->cmd) {
				for (int i = 0; i < k->arg->argc; i++) {
					_FREE_(k->arg->cmd[i]);
				}
				_FREE_(k->arg->cmd);
			}
			_FREE_(k->arg);
		}
		_FREE_(k);
	}
}

static void
build_run_func(char *func_param, conf_key_t *key, uint32_t mod, uint32_t keysym)
{
	key->mod	= mod;
	key->keysym = (xcb_keysym_t)keysym;
	if (strchr(func_param, '[')) {
		trim(func_param, SQUARE_BRACKET);
		int	   count = 0;
		char **args	 = split_string(func_param, ',', &count);
		if (args == NULL) {
			_LOG_(ERROR, "failed to split string %s", func_param);
			return;
		}
		key->arg->cmd = (char **)malloc((count + 1) * sizeof(char *));
		if (key->arg->cmd == NULL) {
			_LOG_(ERROR, "failed to allocate memory for cmd array");
			free_tokens(args, count);
			return;
		}
		key->arg->argc = count;
		for (int i = 0; i < key->arg->argc; i++) {
			trim(args[i], WHITE_SPACE);
			trim(args[i], QUOTATION);
			key->arg->cmd[i] = strdup(args[i]);
			if (key->arg->cmd[i] == NULL) {
				_LOG_(ERROR, "failed to duplicate token");
				free_tokens(args, count);
				return;
			}
		}
		free_tokens(args, count);
	} else {
		trim(func_param, WHITE_SPACE);
		trim(func_param, QUOTATION);
		key->arg->cmd = (char **)malloc(1 * sizeof(char *));
		if (key->arg->cmd == NULL) {
			_LOG_(ERROR, "failed to allocate memory for cmd array");
			return;
		}
		key->arg->cmd[0] = strdup(func_param);
		if (key->arg->cmd[0] == NULL) {
			_LOG_(ERROR, "failed to duplicate token");
			key->arg->cmd = NULL;
			return;
		}
		key->arg->argc = 1;
	}
}

static void
set_key_args(conf_key_t *key, char *func, char *arg)
{
	if (strcmp(func, "cycle_window") == 0) {
		if (strcmp(arg, "up") == 0) {
			key->arg->d = UP;
		} else if (strcmp(arg, "right") == 0) {
			key->arg->d = RIGHT;
		} else if (strcmp(arg, "left") == 0) {
			key->arg->d = LEFT;
		} else if (strcmp(arg, "down") == 0) {
			key->arg->d = DOWN;
		}
	} else if (strcmp(func, "shift_window") == 0) {
		if (strcmp(arg, "up") == 0) {
			key->arg->d = UP;
		} else if (strcmp(arg, "right") == 0) {
			key->arg->d = RIGHT;
		} else if (strcmp(arg, "left") == 0) {
			key->arg->d = LEFT;
		} else if (strcmp(arg, "down") == 0) {
			key->arg->d = DOWN;
		}
	} else if (strcmp(func, "layout") == 0) {
		if (strcmp(arg, "master") == 0) {
			key->arg->t = MASTER;
		} else if (strcmp(arg, "default") == 0) {
			key->arg->t = DEFAULT;
		} else if (strcmp(arg, "grid") == 0) {
			key->arg->t = GRID;
		} else if (strcmp(arg, "stack") == 0) {
			key->arg->t = STACK;
		} else if (strcmp(arg, "monocle") == 0) {
			key->arg->t = MONOCLE;
		} else if (strcmp(arg, "three_col") == 0) {
			key->arg->t = THREE_COL;
		} else if (strcmp(arg, "deck") == 0) {
			key->arg->t = DECK;
		}
	} else if (strcmp(func, "cycle_desktop") == 0) {
		if (strcmp(arg, "left") == 0) {
			key->arg->d = LEFT;
		} else if (strcmp(arg, "right") == 0) {
			key->arg->d = RIGHT;
		}
	} else if (strcmp(func, "resize") == 0) {
		if (strcmp(arg, "grow") == 0) {
			key->arg->r = GROW;
		} else if (strcmp(arg, "shrink") == 0) {
			key->arg->r = SHRINK;
		}
	} else if (strcmp(func, "gap_handler") == 0) {
		if (strcmp(arg, "grow") == 0) {
			key->arg->r = GROW;
		} else if (strcmp(arg, "shrink") == 0) {
			key->arg->r = SHRINK;
		}
	} else if (strcmp(func, "switch_desktop") == 0) {
		char *_num = key_to_str(key->keysym);
		int	  idx  = atoi(_num);
		idx--;
		key->arg->idx = idx;
	} else if (strcmp(func, "transfer_node") == 0) {
		char *_num = key_to_str(key->keysym);
		int	  idx  = atoi(_num);
		idx--;
		key->arg->idx = idx;
	} else if (strcmp(func, "traverse") == 0) {
		if (strcmp(arg, "up") == 0) {
			key->arg->d = UP;
		} else if (strcmp(arg, "down") == 0) {
			key->arg->d = DOWN;
		}
	} else if (strcmp(func, "change_state") == 0) {
		if (strcmp(arg, "float") == 0) {
			key->arg->s = FLOATING;
		} else if (strcmp(arg, "tile") == 0) {
			key->arg->s = TILED;
		}
	} else if (strcmp(func, "shrink_floating_window") == 0) {
		if (strcmp(arg, "horizontal") == 0) {
			key->arg->rd = HORIZONTAL_DIR;
		} else if (strcmp(arg, "vertical") == 0) {
			key->arg->rd = VERTICAL_DIR;
		}
	} else if (strcmp(func, "grow_floating_window") == 0) {
		if (strcmp(arg, "horizontal") == 0) {
			key->arg->rd = HORIZONTAL_DIR;
		} else if (strcmp(arg, "vertical") == 0) {
			key->arg->rd = VERTICAL_DIR;
		}
	} else if (strcmp(func, "cycle_monitors") == 0) {
		if (strcmp(arg, "next") == 0) {
			key->arg->tr = NEXT;
		} else if (strcmp(arg, "prev") == 0) {
			key->arg->tr = PREV;
		}
	}
}

static int
construct_key(char *mod, char *keysym, char *func, conf_key_t *key)
{
	bool	 run_func	= false;
	uint32_t _keysym	= -1;
	uint32_t _mod		= -1;
	int (*ptr)(arg_t *) = NULL;

	/* parse mod key */
	_mod				= parse_mod_key(mod);
	if ((int)_mod == -1) {
		_LOG_(ERROR, "failed to parse mod key for %s, func %s", mod, func);
		return -1;
	}

	/* parse keysym if not null */
	if (keysym) {
		_keysym = parse_keysym(keysym);
		if ((int)_keysym == -1) {
			_LOG_(ERROR, "failed to parse keysym for %s", keysym);
			return -1;
		}
	} else {
		_LOG_(INFO, "keysym is null, func must be switch or transfer %s", func);
	}

	if (strncmp(func, "run", 3) == 0) {
		run_func = true;
#ifdef _DEBUG__
		_LOG_(INFO, "found run func %s, ...", func);
#endif
	}

	char *func_param = extract_body(func);
	if (func_param == NULL) {
		_LOG_(ERROR, "failed to extract func body for %s", func);
		return -1;
	}

	trim(func_param, PARENTHESIS);

	if (strchr(func_param, ':')) {
		int	   count = 0;
		char **s	 = split_string(func_param, ':', &count);
		if (s == NULL || count != 2) {
			_LOG_(ERROR,
				  "failed to split string or incorrect count for %s",
				  func_param);
			_FREE_(func_param);
			if (s)
				_FREE_(s);
			return -1;
		}

		char *f = strdup(s[0]);
		char *a = strdup(s[1]);
		ptr		= str_to_func(f);
		if (ptr == NULL) {
			_LOG_(ERROR, "failed to find function pointer for %s", f);
			_FREE_(func_param);
			_FREE_(s);
			return -1;
		}
		key->mod	 = _mod;
		key->keysym	 = _keysym;
		key->execute = ptr;

		set_key_args(key, f, a);
		free_tokens(s, count);
		_FREE_(f);
		_FREE_(a);
		_FREE_(func_param);
		return 0;
	}

	/* handle "run" functions */
	if (run_func) {
		ptr = str_to_func("run");
		if (ptr == NULL) {
			_LOG_(ERROR, "failed to find run func pointer for %s", func_param);
			_FREE_(func_param);
			return -1;
		}
		build_run_func(func_param, key, _mod, _keysym);
		key->execute = ptr;
		_FREE_(func_param);
		return 0;
	}

	/* handle other functions */
	ptr = str_to_func(func_param);
	if (ptr == NULL) {
		_LOG_(ERROR, "failed to find function pointer for %s", func_param);
		_FREE_(func_param);
		return -1;
	}

	key->mod	 = _mod;
	key->keysym	 = _keysym;
	key->execute = ptr;
	_FREE_(func_param);
	return 0;
}

static int
parse_keybinding(char *str, conf_key_t *key)
{
	if (strstr(str, "->") == NULL) {
		_LOG_(ERROR, "invalide key format %s ", str);
		return -1;
	}

	char plus		   = '+';
	bool keysym_exists = false;
	int	 i			   = 0;
	while (str[i] != '\0') {
		if (str[i] == plus) {
			keysym_exists = !keysym_exists;
			break;
		}
		i++;
	}

	char *mod	 = NULL;
	char *keysym = NULL;
	char *func	 = NULL;

	if (keysym_exists) {
		char *plus_token = strtok(str, "+");
		mod				 = plus_token ? plus_token : NULL;
		plus_token		 = strtok(NULL, "->");
		keysym			 = plus_token ? plus_token : NULL;
		func			 = strtok(NULL, "");
		if (mod)
			trim(mod, WHITE_SPACE);
		if (keysym)
			trim(keysym, WHITE_SPACE);
		if (func) {
			func++;
			trim(func, WHITE_SPACE);
		}
	} else {
		char *arrow_token = strtok(str, "->");
		mod				  = arrow_token ? arrow_token : NULL;
		func			  = strtok(NULL, "");
		if (mod)
			trim(mod, WHITE_SPACE);
		if (func) {
			func++;
			trim(func, WHITE_SPACE);
		}
	}
	return construct_key(mod, keysym, func, key);
}

static conf_key_t *
init_key(void)
{
	conf_key_t *key = (conf_key_t *)calloc(1, sizeof(conf_key_t));
	if (key == NULL) {
		_LOG_(ERROR, "failed to calloc conf_key_t");
		return NULL;
	}

	key->arg = (arg_t *)calloc(1, sizeof(arg_t));
	if (key->arg == NULL) {
		_LOG_(ERROR, "failed to calloc arg_t");
		_FREE_(key);
		return NULL;
	}

	key->arg->cmd  = NULL;
	key->arg->argc = 0;
	key->next	   = NULL;

	return key;
}

static void
add_key(conf_key_t **head, conf_key_t *k)
{
	if (*head == NULL) {
		*head = k;
		return;
	}
	conf_key_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = k;
}

static rule_t *
init_rule(void)
{
	rule_t *rule = (rule_t *)calloc(1, sizeof(rule_t));

	if (rule == NULL) {
		_LOG_(ERROR, "failed to calloc rule_t");
		return NULL;
	}
	rule->next = NULL;
	return rule;
}

static void
add_rule(rule_t **head, rule_t *r)
{
	if (*head == NULL) {
		*head = r;
		return;
	}
	rule_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = r;
}

static void
handle_exec_cmd(char *cmd)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "exec command = (%s)", cmd);
#endif

	pid_t pid = fork();

	if (pid == 0) {
		if (strchr(cmd, ',')) {
			trim(cmd, SQUARE_BRACKET);
			int	   count = 0;
			char **s	 = split_string(cmd, ',', &count);
			if (s == NULL)
				_exit(EXIT_FAILURE);
			const char *args[count + 1];
			for (int i = 0; i < count; ++i) {
				trim(s[i], WHITE_SPACE);
				trim(s[i], QUOTATION);
				args[i] = s[i];
#ifdef _DEBUG__
				_LOG_(INFO, "arg exec = %s", s[i]);
#endif
			}
			args[count] = NULL;
			execvp(args[0], (char *const *)args);
			free_tokens(s, count);
			_LOG_(ERROR, "execvp failed");
			_exit(EXIT_FAILURE);
		} else {
			trim(cmd, QUOTATION);
			execlp(cmd, cmd, (char *)NULL);
			_LOG_(ERROR, "execlp failed");
			_exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		_LOG_(ERROR, "fork failed");
		_exit(EXIT_FAILURE);
	}
}

static int
construct_rule(char *class, char *state, char *desktop_number, rule_t *rule)
{
	if (class == NULL || state == NULL || desktop_number == NULL) {
		_LOG_(ERROR, "rules are empty");
		return -1;
	}

	/* wm_class */
	char *c = extract_body(class);
	if (c == NULL) {
		_LOG_(ERROR, "while extracting class rule body (%s)", class);
		return -1;
	}

	trim(c, PARENTHESIS);
	trim(c, QUOTATION);
	uint32_t c_len = strlen(c);
	strncpy(rule->win_name, c, c_len);

	/* w_state */
	char *s = extract_body(state);
	if (s == NULL) {
		_LOG_(ERROR, "while extracting state rule body");
		return -1;
	}
	state_t enum_state = -1;
	trim(s, PARENTHESIS);
	if (strcmp(s, "tiled") == 0) {
		enum_state = TILED;
	} else if (strcmp(s, "floated") == 0) {
		enum_state = FLOATING;
	}
	rule->state = enum_state;

	/* w_desktop */
	char *d		= extract_body(desktop_number);
	if (d == NULL) {
		_LOG_(ERROR, "while extracting desktop rule body");
		return -1;
	}

	trim(d, PARENTHESIS);
	rule->desktop_id = atoi(d);

	_LOG_(INFO,
		  "constructed rule = win name = (%s), state = (%s), desktop = (%d)",
		  rule->win_name,
		  rule->state == TILED ? "TILED" : "FLOATED",
		  rule->desktop_id);
	_FREE_(c);
	_FREE_(s);
	_FREE_(d);

	return 0;
}

rule_t *
get_window_rule(xcb_window_t win)
{
	xcb_icccm_get_wm_class_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_class(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_class_reply(wm->connection, cn, &t_reply, NULL);
	if (wr == 1) {
		rule_t *current = rule_head;
		while (current) {
			if (strcasecmp(current->win_name, t_reply.class_name) == 0) {
				xcb_icccm_get_wm_class_reply_wipe(&t_reply);
				return current;
			}
			current = current->next;
		}
		xcb_icccm_get_wm_class_reply_wipe(&t_reply);
	}
	return NULL;
}

static int
parse_rule(char *value, rule_t *rule)
{
	if (value == NULL) {
		return -1;
	}

	trim(value, WHITE_SPACE);
	int	   count = 0;
	char **rules = split_string(value, ',', &count);
	if (rules == NULL)
		return -1;

	if (count != 3) {
		_LOG_(ERROR, "while splitting window rule");
		free_tokens(rules, count);
		return -1;
	}

	char *win_name	  = rules[0];
	char *win_state	  = rules[1];
	char *win_desktop = rules[2];

	int	  result	  = construct_rule(win_name, win_state, win_desktop, rule);

	free_tokens(rules, count);

	return result;
}

static int
parse_config_line(char *key, char *value, config_t *c, bool reload)
{
	if (strcmp(key, "exec") == 0) {
		if (!reload)
			handle_exec_cmd(value);
	} else if (strcmp(key, "border_width") == 0) {
		c->border_width = atoi(value);
	} else if (strcmp(key, "active_border_color") == 0) {
		c->active_border_color = (unsigned int)strtoul(value, NULL, 16);
	} else if (strcmp(key, "normal_border_color") == 0) {
		c->normal_border_color = (unsigned int)strtoul(value, NULL, 16);
	} else if (strcmp(key, "window_gap") == 0) {
		c->window_gap = atoi(value);
	} else if (strcmp(key, "virtual_desktops") == 0) {
		c->virtual_desktops = atoi(value);
	} else if (strcmp(key, "focus_follow_pointer") == 0) {
		if (strcmp(value, "true") == 0) {
			c->focus_follow_pointer = true;
		} else if (strcmp(value, "false") == 0) {
			c->focus_follow_pointer = false;
		} else {
			_LOG_(ERROR, "invalid value for focus_follow_pointer: %s", value);
			return -1;
		}
	} else if (strcmp(key, "focus_follow_spawn") == 0) {
		if (strcmp(value, "true") == 0) {
			c->focus_follow_spawn = true;
		} else if (strcmp(value, "false") == 0) {
			c->focus_follow_spawn = false;
		} else {
			_LOG_(ERROR, "invalid value for focus_follow_spawn: %s", value);
			return -1;
		}
	} else if (strcmp(key, "restore_last_focus") == 0) {
		if (strcmp(value, "true") == 0) {
			c->restore_last_focus = true;
		} else if (strcmp(value, "false") == 0) {
			c->restore_last_focus = false;
		} else {
			_LOG_(ERROR, "invalid value for focus_follow_spawn: %s", value);
			return -1;
		}
	} else if (strcmp(key, "rule") == 0) {
		rule_t *rule = init_rule();
		if (rule == NULL) {
			_LOG_(ERROR, "failed to allocate memory for rule_t");
			return -1;
		}
		if (parse_rule(value, rule) != 0) {
			_FREE_(rule);
			_LOG_(ERROR, "error while parsing rule %s", value);
			return -1;
		}
		add_rule(&rule_head, rule);
	} else if (strcmp(key, "bind") == 0) {
		conf_key_t *k = init_key();
		if (k == NULL) {
			_LOG_(ERROR, "failed to allocate memory for _key__t");
			return -1;
		}
		if (parse_keybinding(value, k) != 0) {
			err_cleanup(k);
			_LOG_(ERROR, "error while parsing keys");
			return -1;
		}
		add_key(&key_head, k);
	} else {
		_LOG_(WARNING, "unknown config key: %s", key);
	}
	return 0;
}

static int
parse_config(const char *filename, config_t *c, bool reload)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		_LOG_(ERROR, "error: could not open file '%s'", filename);
		return -1;
	}

	char line[MAX_LINE_LENGTH];
	while (fgets(line, MAX_LINE_LENGTH, file)) {
		if (line[0] == ' ' || line[0] == '\t' || line[0] == '\n' ||
			line[0] == '\v' || line[0] == '\f' || line[0] == '\r' ||
			line[0] == ';') {
			continue;
		}
		char *key	= strtok(line, "=");
		char *value = strtok(NULL, "\n");

		if (key == NULL || value == NULL) {
			continue;
		}

		trim(key, WHITE_SPACE);
		trim(value, WHITE_SPACE);

#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "config line = (%s) key = (%s) value = (%s)",
			  line,
			  key,
			  value);
#endif

		if (parse_config_line(key, value, c, reload) != 0) {
			fclose(file);
			return -1;
		}
	}

	fclose(file);
	return 0;
}

void
free_rules(void)
{
	rule_t *current = rule_head;
	while (current) {
		rule_t *next = current->next;
		_FREE_(current);
		current = next;
	}
	rule_head = NULL;
}

void
free_keys(void)
{
	conf_key_t *current = key_head;
	while (current) {
		conf_key_t *next = current->next;
		if (!current->arg) {
			current = next;
			continue;
		}
		if (!current->arg->cmd) {
			current = next;
			continue;
		}
		for (int j = 0; j < current->arg->argc; j++) {
			if (current->arg->cmd && current->arg->cmd[j]) {
				_FREE_(current->arg->cmd[j]);
			}
		}
		_FREE_(current->arg->cmd);
		_FREE_(current->arg);
		_FREE_(current);
		current = next;
	}
	key_head = NULL;
}

int
reload_config(config_t *c)
{
	const char *filename = CONF_PATH;
	return parse_config(filename, c, true);
}

int
load_config(config_t *c)
{
	const char *filename = CONF_PATH;
	if (!file_exists(filename)) {
		write_default_config(filename, c);
	}
	return parse_config(filename, c, false);
}

/* ./src/cursor.c */



static void
load_cursors(void)
{
	if (xcb_cursor_context_new(wm->connection, wm->screen, &cursor_ctx) < 0) {
		_LOG_(ERROR, "failed to allocate xcursor context");
		return;
	}
/* _LOAD_CURSOR_ is reserved by some other lib */
#define __LOAD__CURSOR__(cursor, name)                                         \
	do {                                                                       \
		cursors[cursor] = xcb_cursor_load_cursor(cursor_ctx, name);            \
	} while (0)
	__LOAD__CURSOR__(CURSOR_POINTER, "left_ptr");
	__LOAD__CURSOR__(CURSOR_WATCH, "watch");
	__LOAD__CURSOR__(CURSOR_MOVE, "fleur");
	__LOAD__CURSOR__(CURSOR_XTERM, "xterm");
	__LOAD__CURSOR__(CURSOR_NOT_ALLOWED, "not-allowed");
	__LOAD__CURSOR__(CURSOR_HAND2, "hand2");
#undef __LOAD__CURSOR__
}

static xcb_cursor_t
get_cursor(cursor_t c)
{
	assert(c < CURSOR_MAX);
	return cursors[c];
}

static void
set_cursor(int cursor_id)
{
	xcb_cursor_t c		  = get_cursor(cursor_id);
	uint32_t	 values[] = {c};
	xcb_cookie_t cookie	  = xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, XCB_CW_CURSOR, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);

	if (err) {
		_LOG_(ERROR, "error setting cursor on root window %d", err->error_code);
		_FREE_(err);
	}
	xcb_flush(wm->connection);
}

/* ./src/desktop.c */



static void
update_focused_desktop(int id);
static node_t *
find_deck_stack_focus(node_t *root);

static void
remember_desktop_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return;

	node_t *n = get_focused_node(d->tree);
	if (n && n->client)
		d->last_focused = n->client->window;
}

static node_t *
pick_desktop_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return NULL;

	node_t *n = get_focused_node(d->tree);
	if (n && n->client && IS_TILED(n->client))
		return n;

	if (d->last_focused != XCB_NONE &&
		window_exists(wm->connection, d->last_focused)) {
		n = find_node_by_window_id(d->tree, d->last_focused);
		if (n && n->client && IS_TILED(n->client))
			return n;
	}

	return find_any_leaf(d->tree);
}

static int
activate_window_node(desktop_t *d, node_t *n)
{
	if (!d || !n || !n->client)
		return 0;

	if (IS_TILED(n->client))
		_focus_node_(d, n);

	if (_render_view_(d) != 0)
		return -1;

	if (focused_win != XCB_NONE && focused_win != n->client->window)
		win_focus(focused_win, false);
	if (_focus_input_(d, n) != 0)
		return -1;

	n->is_focused	   = true;
	focused_win		   = n->client->window;
	n->client->mru_seq = get_next_mru_seq(curr_monitor);
	set_active_window_name(focused_win);
	_flush_view_(d);
	return 0;
}

static node_t *
pick_deck_focus(desktop_t *d)
{
	if (!d || !d->tree)
		return NULL;

	if (d->last_focused != XCB_NONE &&
		window_exists(wm->connection, d->last_focused)) {
		node_t *n = find_node_by_window_id(d->tree, d->last_focused);
		if (n && n->client && IS_TILED(n->client) && !n->is_master)
			return n;
	}

	node_t *n = get_focused_node(d->tree);
	if (n && n->client && IS_TILED(n->client) && !n->is_master)
		return n;

	return find_deck_stack_focus(d->tree);
}

static node_t *
find_deck_stack_focus(node_t *root)
{
	if (!root)
		return NULL;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client) &&
		!root->is_master)
		return root;

	node_t *n = find_deck_stack_focus(root->first_child);
	if (n)
		return n;
	return find_deck_stack_focus(root->second_child);
}

static int
render_desktop(desktop_t *d)
{
	if (d == NULL || is_tree_empty(d->tree))
		return 0;

	return _render_view_(d);
}

static void
render_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (!curr->desktops) {
			curr = curr->next;
			continue;
		}
		for (int i = 0; i < curr->n_of_desktops; i++) {
			if (is_tree_empty(curr->desktops[i]->tree))
				continue;
			if (curr->desktops[i]->is_focused)
				render_desktop(curr->desktops[i]);
			else /* keep X geometry correct for hidden desktops */
				render_tree_nomap(curr->desktops[i]->tree);
		}
		curr = curr->next;
	}
}

static int
get_focused_desktop_idx(void)
{
	if (curr_monitor == NULL) {
		_LOG_(ERROR, "curr_monitor is null");
		return -1;
	}
	for (int i = curr_monitor->n_of_desktops; i--;) {
		if (curr_monitor->desktops[i]->is_focused) {
			return curr_monitor->desktops[i]->id;
		}
	}
	_LOG_(ERROR, "cannot find curr monitor focused desktop");
	return -1;
}

static desktop_t *
get_focused_desktop(void)
{
	monitor_t *fm = get_focused_monitor();
	for (int i = fm->n_of_desktops; i--;) {
		if (fm->desktops[i] && fm->desktops[i]->is_focused) {
			return fm->desktops[i];
		}
	}

	return NULL;
}

/* useles after
 * https://github.com/yazeed1s/zwm/pull/36 */
static node_t *
get_foucsed_desktop_tree__(void)
{
	int i = get_focused_desktop_idx();
	assert(i >= 0 && i < conf.virtual_desktops);
	node_t *root = curr_monitor->desktops[i]->tree;
	return root;
}

static desktop_t *
init_desktop(void)
{
	desktop_t *d = (desktop_t *)malloc(sizeof(desktop_t));
	if (d == 0x00)
		return NULL;
	d->id			 = 0;
	d->is_focused	 = false;
	d->n_count		 = 0;
	d->tree			 = NULL;
	d->last_focused	 = XCB_NONE;
	d->logical_focus = NULL;
	/*d->node	  = NULL;*/
	return d;
}

static bool
setup_desktops(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		/* becaues this function is also called when monitors change, we need
		 * to skip old monitors in the list. */
		if (curr && curr->desktops) {
			_LOG_(INFO,
				  "monitor %s already has desktops... skipping",
				  curr->name);
			curr = curr->next;
			continue;
		}
		curr->n_of_desktops = conf.virtual_desktops;
		desktop_t **desktops =
			(desktop_t **)malloc(sizeof(desktop_t *) * curr->n_of_desktops);
		if (desktops == NULL) {
			_LOG_(ERROR, "failed to malloc desktops");
			return false;
		}
		curr->desktops = desktops;
		for (int j = 0; j < curr->n_of_desktops; j++) {
			desktop_t *d  = init_desktop();
			d->id		  = (uint16_t)j;
			d->is_focused = (j == 0);
			d->layout	  = DEFAULT;
			snprintf(d->name, sizeof(d->name), "%d", j + 1);
			curr->desktops[j] = d;
		}
		curr->desk = curr->desktops[0];
		_LOG_(INFO, "successfuly assigned desktops for monitor %s", curr->name);
		curr = curr->next;
	}
	return true;
}

static void
update_focused_desktop(int id)
{
	if (curr_monitor == NULL) {
		return;
	}
	for (int i = 0; i < curr_monitor->n_of_desktops; ++i) {
		if (curr_monitor->desktops[i]->id != id) {
			curr_monitor->desktops[i]->is_focused = false;
		} else {
			curr_monitor->desktops[i]->is_focused = true;
			curr_monitor->desk					  = curr_monitor->desktops[i];
		}
	}
}

static int
switch_desktop(const int nd)
{
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SWITCH_DESKTOP] ========== DESKTOP SWITCH START ==========");
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] switching from desktop %d to desktop %d",
		  curr_monitor->desk->id,
		  nd);
#endif
	if (nd > conf.virtual_desktops) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[SWITCH_DESKTOP] requested desktop %d > max %d, aborting",
			  nd,
			  conf.virtual_desktops);
#endif
		return 0;
	}

	desktop_t *old_desktop	  = curr_monitor->desk;
	node_t	  *tree_to_hide	  = old_desktop->tree;
	node_t	  *tree_to_show	  = curr_monitor->desktops[nd]->tree;
	desktop_t *target_desktop = curr_monitor->desktops[nd];

	if (curr_monitor->desk == curr_monitor->desktops[nd]) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG, "[SWITCH_DESKTOP] already on desktop %d, nothing to do", nd);
#endif
		return 0;
	}
#ifdef _DEBUG__
	_LOG_(DEBUG, "[SWITCH_DESKTOP] updating focused desktop to %d", nd);
#endif
	remember_desktop_focus(old_desktop);
	update_focused_desktop(nd);

#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] calling hide_windows for desktop %d tree",
		  curr_monitor->desk->id);
#endif
	if (hide_windows(tree_to_hide) != 0) {
#ifdef _DEBUG__
		_LOG_(ERROR, "[SWITCH_DESKTOP] hide_windows failed for old desktop");
#endif
		return -1;
	}

	set_active_window_name(XCB_NONE);
	win_focus(focused_win, false);
	focused_win = XCB_NONE;

	if (!is_tree_empty(tree_to_show)) {
		/* pick logical focus for the target desktop, and honours
		 * restore_last_focus for all layouts except STACK, which never restores
		 * by position */
		node_t *focus = NULL;
		if (conf.restore_last_focus && target_desktop->layout != STACK) {
			xcb_window_t win = target_desktop->last_focused;
			if (win != XCB_NONE && window_exists(wm->connection, win)) {
				node_t *n = find_node_by_window_id(tree_to_show, win);
				/* only restore tiled nodes as logical focus */
				if (n && n->client && IS_TILED(n->client))
					focus = n;
			}
		}
		if (!focus)
			focus = _pick_focus_(target_desktop);

		if (focus) {
			_focus_node_(target_desktop, focus);
			focus->client->mru_seq = get_next_mru_seq(curr_monitor);
#ifdef _DEBUG__
			_LOG_(DEBUG,
				  "[SWITCH_DESKTOP] logical focus -> win=%d on desktop %d",
				  focus->client->window,
				  nd);
#endif
		}

		/* render, maps/unmaps windows according to layout policy */
		if (_render_view_(target_desktop) != 0)
			return -1;

		/* apply X input focus after windows are mapped only */
		if (focus && focus->client) {
			if (_focus_input_(target_desktop, focus) != 0)
				return -1;
			focused_win = focus->client->window;
			set_active_window_name(focused_win);
		}

		/* restack + flush x server */
		_flush_view_(target_desktop);
	}

#ifdef _DEBUG__
	_LOG_(INFO, "new desktop %d nodes--------------", nd + 1);
	log_tree_nodes(tree_to_show);
	_LOG_(INFO, "old desktop %d nodes--------------", old_desktop->id + 1);
	log_tree_nodes(tree_to_hide);
#endif

	if (ewmh_update_current_desktop(wm->ewmh, wm->screen_nbr, nd) != 0) {
#ifdef _DEBUG__
		_LOG_(ERROR, "[SWITCH_DESKTOP] ewmh_update_current_desktop failed");
#endif
		return -1;
	}

#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "[SWITCH_DESKTOP] ========== DESKTOP SWITCH COMPLETE ==========");
#endif
	return 0;
}

static void
fill_root_rectangle(rectangle_t *r)
{
	rectangle_t usable = get_usable_area(curr_monitor);
	(*r).x			   = usable.x + conf.window_gap;
	(*r).y			   = usable.y + conf.window_gap;
	(*r).width	= usable.width - 2 * conf.window_gap - 2 * conf.border_width;
	(*r).height = usable.height - 2 * conf.window_gap - 2 * conf.border_width;
}

static int
handle_net_desktop_change(uint32_t nd)
{
	if (!curr_monitor || nd >= (uint32_t)curr_monitor->n_of_desktops) {
		return -1;
	}
	return switch_desktop(nd);
}

static int
handle_net_active_window(xcb_window_t win)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(
		DEBUG,
		"[NET_ACTIVE_WINDOW] received _NET_ACTIVE_WINDOW for win=%d name='%s'",
		win,
		name ? name : "(null)");
	_FREE_(name);
#endif
	desktop_t *d	 = NULL;
	node_t	  *n	 = NULL;
	bool	   found = false;
	find_window_in_desktops(&d, &n, win, &found);
	if (!found || !d || !n || !n->client) {
#ifdef _DEBUG__
		_LOG_(
			DEBUG,
			"[NET_ACTIVE_WINDOW] window %d not found in any desktop, ignoring",
			win);
#endif
		return 0;
	}

	monitor_t *m = get_monitor_by_window(win);
	if (m && curr_monitor != m)
		curr_monitor = m;

	if (curr_monitor->desk != d) {
		if (switch_desktop(d->id) != 0)
			return -1;
		n = find_node_by_window_id(curr_monitor->desk->tree, win);
		if (!n || !n->client)
			return 0;
		d = curr_monitor->desk;
	}

	return activate_window_node(d, n);
}

/* ./src/drag.c */





/* clang-format off */
/* clang-format on */

static void
apply_preview_layout(node_t *root)
{
	if (!root)
		return;

	if (!IS_INTERNAL(root) && root->client) {
		if (IS_FULLSCREEN(root->client))
			return;
		if (root->client->window != ds.window) {
			const rectangle_t r = IS_FLOATING(root->client)
									  ? root->floating_rectangle
									  : root->rectangle;
			apply_window_geometry(
				root->client->window, r, conf.border_width);
		}
		return;
	}

	apply_preview_layout(root->first_child);
	apply_preview_layout(root->second_child);
}

/* starts the drag session */
static int
drag_start(xcb_window_t win, int16_t x, int16_t y, bool kbd)
{
	node_t *root = curr_monitor->desk->tree;
	node_t *n	 = find_node_by_window_id(root, win);

	if (!n || !n->client) {
		_LOG_(WARNING, "cannot drag: window not found");
		return -1;
	}

	/* we only care about dragging tiled windows here. floating windows
	 * are handled elsewhere, since they behave differently... */
	if (IS_FLOATING(n->client) || IS_FULLSCREEN(n->client)) {
		_LOG_(WARNING, "cannot drag floating or fullscreen windows");
		return -1;
	}

	ds.window			 = win;
	ds.src_node			 = n;
	ds.start_x			 = x;
	ds.start_y			 = y;
	ds.cur_x			 = x;
	ds.cur_y			 = y;
	ds.active			 = true;
	ds.kbd_mode			 = kbd;
	ds.last_target		 = NULL;
	ds.preview_active	 = false;

	/* save the original state in case we need
	 * to revert on cancel or error */
	ds.original_desktop	 = curr_monitor->desk;
	ds.original_rect	 = n->rectangle;

	/* pop the window to the top layer so it doesn't get covered.
	 * dragged windows are always on top */
	const uint32_t val[] = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, win, XCB_CONFIG_WINDOW_STACK_MODE, val);

	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_MOVE)});*/

	xcb_grab_pointer_cookie_t cookie =
		xcb_grab_pointer(wm->connection,
						 false,			  /* owner_events */
						 wm->root_window, /* grab_window */
						 XCB_EVENT_MASK_BUTTON_RELEASE |
							 XCB_EVENT_MASK_POINTER_MOTION, /* event_mask */
						 XCB_GRAB_MODE_ASYNC,				/* pointer_mode */
						 XCB_GRAB_MODE_ASYNC,				/* keyboard_mode */
						 XCB_NONE,							/* confine_to */
						 get_cursor(CURSOR_MOVE),			/* cursor */
						 XCB_CURRENT_TIME);

	xcb_grab_pointer_reply_t *reply =
		xcb_grab_pointer_reply(wm->connection, cookie, NULL);
	if (reply)
		free(reply);

	drag_move(x, y);

	xcb_flush(wm->connection);
	_LOG_(INFO, "drag started for window %d (LIVE PREVIEW)", win);
	return 0;
}

/* handles cursor movement while dragging */
static int
drag_move(int16_t x, int16_t y)
{
	if (!ds.active)
		return 0;

	ds.cur_x	   = x;
	ds.cur_y	   = y;

	/* figure out which partition is under the cursor */
	node_t *root   = curr_monitor->desk->tree;
	node_t *target = find_leaf_at_point(root, x, y);

	if (!target || target == ds.src_node) {
		if (ds.last_target) {
			preview_clear();
			ds.last_target = NULL;
		}
	} else if (target != ds.last_target) {
		preview_clear();
		preview_apply(target);
		ds.last_target = ds.preview_active ? target : NULL;
	}

	/* center the window on the cursor */
	int16_t new_x = x - (ds.original_rect.width / 2);
	int16_t new_y = y - (ds.original_rect.height / 2);
	rectangle_t r = ds.original_rect;
	r.x			  = new_x;
	r.y			  = new_y;
	apply_window_geometry(ds.window, r, conf.border_width);

	return 0;
}

/* ends the drag session, committing changes */
static int
drag_end(int16_t x, int16_t y)
{
	if (!ds.active)
		return 0;

	node_t *root   = curr_monitor->desk->tree;
	node_t *target = find_leaf_at_point(root, x, y);

	preview_clear();
	ds.last_target = NULL;

	if (!target || target == ds.src_node) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	if (!unlink_node(ds.src_node, curr_monitor->desk)) {
		arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
		render_tree_nomap(curr_monitor->desk->tree);
		goto cleanup;
	}

	insert_node(target, ds.src_node, curr_monitor->desk->layout);
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	_render_view_(curr_monitor->desk);

cleanup:
	ungrab_pointer();
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/
	ds.active		  = false;
	ds.preview_active = false;

	xcb_flush(wm->connection);

	_LOG_(INFO, "drag ended");
	return 0;
}

/* cancel the drag and put everything back how it was */
static int
drag_cancel(void)
{
	if (!ds.active)
		return 0;

	_LOG_(INFO, "drag cancelled");

	preview_clear();
	ds.last_target = NULL;

	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree_nomap(curr_monitor->desk->tree);

	ungrab_pointer();
	/* xcb_change_window_attributes(wm->connection,
	 * 							 wm->root_window,
	 * 							 XCB_CW_CURSOR,
	 * 							 (uint32_t[]){get_cursor(CURSOR_POINTER)});*/

	ds.active		  = false;
	ds.preview_active = false;

	xcb_flush(wm->connection);

	return 0;
}

static void
preview_restore_layout(void)
{
	arrange_tree(curr_monitor->desk->tree, curr_monitor->desk->layout);
	render_tree_nomap(curr_monitor->desk->tree);
}

static void
preview_apply(node_t *t)
{
	if (!t || !t->client)
		return;

	node_t *r = curr_monitor->desk->tree;
	if (!r)
		return;

	node_t *pr = clone_tree(r, NULL);
	if (!pr)
		return;

	desktop_t desk = {0};
	desk.tree	   = pr;
	desk.layout	   = curr_monitor->desk->layout;

	node_t *ps	   = find_node_by_window_id(pr, ds.window);
	node_t *pt	   = find_node_by_window_id(pr, t->client->window);
	if (!ps || !pt || ps == pt) {
		free_tree(pr);
		return;
	}

	if (!unlink_node(ps, &desk)) {
		free_tree(desk.tree);
		return;
	}

	insert_node(pt, ps, desk.layout);
	arrange_tree(desk.tree, desk.layout);
	apply_preview_layout(desk.tree);
	free_tree(desk.tree);

	ds.preview_active = true;
}

static void
preview_clear(void)
{
	if (!ds.preview_active)
		return;

	preview_restore_layout();
	ds.preview_active = false;
}

/* ./src/events.c */



/* clang-format off */

/* xcb event -> handler
 * see https://xorg.freedesktop.org/releases/X11R7.7/doc/xproto/x11protocol.html */
/* clang-format on */

static char *
xcb_event_to_string(uint8_t type)
{
	switch (type) {
	case XCB_MAP_REQUEST: return "XCB_MAP_REQUEST";
	case XCB_UNMAP_NOTIFY: return "XCB_UNMAP_NOTIFY";
	case XCB_DESTROY_NOTIFY: return "XCB_DESTROY_NOTIFY";
	case XCB_EXPOSE: return "XCB_EXPOSE";
	case XCB_CLIENT_MESSAGE: return "XCB_CLIENT_MESSAGE";
	case XCB_CONFIGURE_REQUEST: return "XCB_CONFIGURE_REQUEST";
	case XCB_CONFIGURE_NOTIFY: return "XCB_CONFIGURE_NOTIFY";
	case XCB_PROPERTY_NOTIFY: return "XCB_PROPERTY_NOTIFY";
	case XCB_ENTER_NOTIFY: return "XCB_ENTER_NOTIFY";
	case XCB_LEAVE_NOTIFY: return "XCB_LEAVE_NOTIFY";
	case XCB_MOTION_NOTIFY: return "XCB_MOTION_NOTIFY";
	case XCB_BUTTON_PRESS: return "XCB_BUTTON_PRESS";
	case XCB_BUTTON_RELEASE: return "XCB_BUTTON_RELEASE";
	case XCB_KEY_PRESS: return "XCB_KEY_PRESS";
	case XCB_KEY_RELEASE: return "XCB_KEY_RELEASE";
	case XCB_FOCUS_IN: return "XCB_FOCUS_IN";
	case XCB_FOCUS_OUT: return "XCB_FOCUS_OUT";
	case XCB_MAPPING_NOTIFY: return "XCB_MAPPING_NOTIFY";
	default: return "UNKNOWN_EVENT";
	}
}

static int
handle_event(xcb_event_t *event)
{
	uint8_t event_type = event->response_type & ~0x80;

	/* xinerima is ignored here */
	if (using_xrandr &&
		event_type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		_LOG_(INFO, "monitor update was requested");
		handle_monitor_changes();
		return 0;
	}

#if 0
	switch (event_type) {
#define _EVENT_HANDLER_(type, handler)                                         \
	case type: return handler((void *)event);
		_EVENT_HANDLER_(XCB_MAP_REQUEST, handle_map_request);
		_EVENT_HANDLER_(XCB_UNMAP_NOTIFY, handle_unmap_notify);
		_EVENT_HANDLER_(XCB_DESTROY_NOTIFY, handle_destroy_notify);
		_EVENT_HANDLER_(XCB_CLIENT_MESSAGE, handle_client_message);
		_EVENT_HANDLER_(XCB_CONFIGURE_REQUEST, handle_configure_request);
		_EVENT_HANDLER_(XCB_ENTER_NOTIFY, handle_enter_notify);
		_EVENT_HANDLER_(XCB_BUTTON_PRESS, handle_button_press_event);
		_EVENT_HANDLER_(XCB_KEY_PRESS, handle_key_press);
		_EVENT_HANDLER_(XCB_MAPPING_NOTIFY, handle_mapping_notify);
#undef _EVENT_HANDLER_
	}
#endif

	size_t n = sizeof(_handlers_) / sizeof(_handlers_[0]);
	for (size_t i = 0; i < n; i++) {
		if (_handlers_[i].type == event_type) {
			return _handlers_[i].handle(event);
		}
	}

	return 0;
}

/* the main loop that listens to redirected x events */
static void
event_loop(wm_t *w)
{
	xcb_event_t *event;
	while (!should_shutdown && (event = xcb_wait_for_event(w->connection))) {
		if (event->response_type == 0) {
			_FREE_(event);
			continue;
		}
		if (handle_event(event) != 0) {
			uint8_t type = event->response_type & ~0x80;
			char   *es	 = xcb_event_to_string(type);
			_LOG_(ERROR, "error processing event: %s ", es);
		}
		_FREE_(event);
	}
}

static int
handle_map_request(const xcb_event_t *event)
{
	xcb_map_request_event_t *ev			= (xcb_map_request_event_t *)event;
	xcb_window_t			 win		= ev->window;
	bool					 is_visible = true;
	int						 ret		= 0;

	if (multi_monitors) {
		monitor_t *mm = get_focused_monitor();
		if (mm && mm != curr_monitor) {
			curr_monitor = mm;
		}
	}

	if (!should_manage(win, wm->connection)) {
		_LOG_(INFO, "win %d, shouldn't be managed.. ignoring request", win);
		return 0;
	}

	/* check if the window already exists in ANY desktop to avoid duplication */
	if (client_exist_in_desktops(win)) {
#ifdef _DEBUG__
		_LOG_(DEBUG,
			  "[MAP_REQUEST] win %d already exists in a desktop, ignoring ",
			  win);
#endif
		return 0;
	}

	rule_t *rule = get_window_rule(win);

	if (rule) {
		if (rule->desktop_id != -1) {
			int target = rule->desktop_id - 1;
			if (curr_monitor->desk->id != target) {
				/* insert into the target desktop, but do not map/focus now */
				ret = insert_into_desktop(
					rule->desktop_id, win, rule->state == TILED);
				if (ret != 0)
					goto manage_failed;
				desktop_t *target_desk = curr_monitor->desktops[target];
				render_tree_nomap(target_desk->tree);
				is_visible = false;
				goto out;
			}
			/* else, in current desktop, fall through to normal logic below */
		}
		/* for both cases, if a state is specified, use it */
		if (rule->state == FLOATING) {
			ret = handle_floating_window_request(win, curr_monitor->desk);
			if (ret != 0)
				goto manage_failed;
			goto out;
		} else if (rule->state == TILED) {
			ret = handle_tiled_window_request(win, curr_monitor->desk);
			if (ret != 0)
				goto manage_failed;
			goto out;
		}
	}

	ewmh_window_type_t wint = window_type(win);
	if (wint != WINDOW_TYPE_DOCK && wint != WINDOW_TYPE_DESKTOP &&
		wint != WINDOW_TYPE_NOTIFICATION && apply_floating_hints(win) != -1) {
		ret = handle_floating_window_request(win, curr_monitor->desk);
		if (ret != 0)
			goto manage_failed;
		goto out;
	}

	/* notification windows are short-lived, they dont deserve entering the
	 * tree. Thus, we just map them as is, and they go away on their own shortly
	 * after */
	/*if (wint == WINDOW_TYPE_NOTIFICATION) {
		map_floating(win);
		return 0;
	}*/

	switch (wint) {
	case WINDOW_TYPE_DESKTOP:
		/* fallthrough */
	case WINDOW_TYPE_DOCK:
	case WINDOW_TYPE_NOTIFICATION: return handle_unmanaged_strut_window(win);
	case WINDOW_TYPE_UNKNOWN:
	case WINDOW_TYPE_NORMAL:
		ret = handle_tiled_window_request(win, curr_monitor->desk);
		break;
	case WINDOW_TYPE_TOOLBAR_MENU:
	case WINDOW_TYPE_UTILITY:
	case WINDOW_TYPE_SPLASH:
	case WINDOW_TYPE_DIALOG:
		ret = handle_floating_window_request(win, curr_monitor->desk);
		break;
	default: break;
	}
	if (ret != 0)
		goto manage_failed;
out:
	if (is_visible && conf.focus_follow_spawn &&
		curr_monitor->desk->layout != STACK) {
		node_t *f = NULL;
		if ((f = find_node_by_window_id(curr_monitor->desk->tree, win)) ==
			NULL) {
			_LOG_(DEBUG, "cannot find window %d, in tree", win);
			xcb_flush(wm->connection);
			return 0;
		}
		set_focus(f, true);
		set_active_window_name(f->client->window);
		focused_win = f->client->window;
		update_focus(curr_monitor->desk, f);
	}
	set_window_state(win,
					 is_visible ? XCB_ICCCM_WM_STATE_NORMAL
								: XCB_ICCCM_WM_STATE_ICONIC);
	ewmh_update_client_list();
	xcb_flush(wm->connection);

	return 0;

manage_failed:
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_enter_notify(const xcb_event_t *event)
{
	xcb_enter_notify_event_t *ev  = (xcb_enter_notify_event_t *)event;
	xcb_window_t			  win = ev->event;
	uint64_t				  now = get_time_millis();
	/* suppress enter-notify events caused by window mapping floods */
	if (now < suppress_enter_until_time || now - last_desk_switch_time < 250) {
#ifdef _DEBUG__
		_LOG_(DEBUG, "ignoring enter notify: map-flood cooldown in effect");
#endif
		return 0;
	}

	monitor_t *mm = get_focused_monitor();
	if (mm && mm != curr_monitor) {
		curr_monitor = mm;
	}

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved enter notify for %d, name %s ", win, name);
	_FREE_(name);
#endif
	/* ignore events with 1- non-normal modes. Those are because a grab
	 * activated/deactivated. 2- events with detail "inferior".  This detail
	 * means that the cursor was previously inside of a child window and now
	 * left that child window, it happens with broswer menues, code edeitor
	 * completion menues etc */
	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	/* if (win == focused_win) {
		return 0;
	} */

	node_t *root = curr_monitor->desk->tree;
	if (!root) {
		return -1;
	}
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n && n->client) ? n->client : NULL;

	if (client == NULL || n == NULL) {
		return 0;
	}

	if (win == wm->root_window) {
		return 0;
	}

	if (!conf.focus_follow_pointer) {
		if (has_floating_window(root)) {
			restack();
		}
		if (IS_FULLSCREEN(n->client)) {
			if (fullscreen_focus(n->client->window)) {
				_LOG_(ERROR, "cannot update win attributes");
				return -1;
			}
		}
		/* set_cursor(CURSOR_NOT_ALLOWED); */
		return 0;
	}

	if (n->client->window == focused_win) {
		return 0;
	}

	layout_t lay = curr_monitor->desk->layout;

	/* DECK master hover, do X input focus only,
	  never reshuffles deck-stack order
	 */
	if (lay == DECK && n->is_master && IS_TILED(n->client)) {
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR,
				  "cannot focus deck master %d (enter)",
				  n->client->window);
			return -1;
		}
		focused_win = n->client->window;
		set_active_window_name(win);
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
		_flush_view_(curr_monitor->desk);
		return 0;
	}

	const int r = set_active_window_name(win);
	if (r != 0) {
		return 0;
	}

	if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return -1;
		}
		focused_win		   = n->client->window;
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
		_flush_view_(curr_monitor->desk);
		return 0;
	}

	if (IS_FLOATING(n->client)) {
		if (focused_win != XCB_NONE)
			win_focus(focused_win, false);
		if (_focus_input_(curr_monitor->desk, n) != 0) {
			_LOG_(ERROR, "cannot focus window %d (enter)", n->client->window);
			return -1;
		}
		n->is_focused = true;
	} else {
		/* iff tiled, set logical focus + rerender
		   so MONOCLE/DECK show this window
		 */
		_focus_node_(curr_monitor->desk, n);
		_render_view_(curr_monitor->desk);
		_focus_input_(curr_monitor->desk, n);
	}

	focused_win = n->client->window;
	if (n && n->client)
		n->client->mru_seq = get_next_mru_seq(curr_monitor);
	_flush_view_(curr_monitor->desk);
	return 0;
}

static int
handle_leave_notify(const xcb_event_t *event)
{
	if (!conf.focus_follow_pointer) {
		return 0;
	}
	xcb_leave_notify_event_t *ev  = (xcb_leave_notify_event_t *)event;
	xcb_window_t			  win = ev->event;

#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved leave notify for %d, name %s ", win, name);
	_FREE_(name);
#endif

	if (ev->mode != XCB_NOTIFY_MODE_NORMAL ||
		ev->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
		return 0;
	}

	if (!window_exists(wm->connection, win)) {
		return 0;
	}

	if (curr_monitor->desk->layout == STACK) {
		return 0;
	}

	node_t		*root		   = curr_monitor->desk->tree;
	xcb_window_t active_window = XCB_NONE;
	node_t		*n			   = find_node_by_window_id(root, win);
	client_t	*client		   = (n && n->client) ? n->client : NULL;
	if (client == NULL) {
		return 0;
	}

	xcb_get_property_cookie_t c =
		xcb_ewmh_get_active_window(wm->ewmh, wm->screen_nbr);

	xcb_ewmh_get_active_window_reply(wm->ewmh, c, &active_window, NULL);
	if (active_window != client->window) {
		return 0;
	}

	if (set_focus(n, false) != 0) {
		_LOG_(ERROR,
			  "failed to change border attr for window %d",
			  client->window);
		return -1;
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_property_notify(const xcb_event_t *event)
{
	const xcb_property_notify_event_t *ev =
		(const xcb_property_notify_event_t *)event;
	if (wm && wm->ewmh && ev->atom == wm->ewmh->_NET_WM_STRUT_PARTIAL) {
		/* bar/panel resized or changed reserved space*/
		if (!ignore_ewmh_struts)
			recalculate_all_struts();
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_focus_in(const xcb_event_t *event)
{
	xcb_focus_in_event_t *ev  = (xcb_focus_in_event_t *)event;
	xcb_window_t		  win = ev->event;
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG, "recieved focus in for %d, name %s ", win, name);
	_FREE_(name);
#endif
	if (ev->mode == XCB_NOTIFY_MODE_GRAB ||
		ev->mode == XCB_NOTIFY_MODE_UNGRAB ||
		ev->detail == XCB_NOTIFY_DETAIL_POINTER ||
		ev->detail == XCB_NOTIFY_DETAIL_POINTER_ROOT ||
		ev->detail == XCB_NOTIFY_DETAIL_NONE) {
		return 0;
	}

	node_t *n = NULL;
	if ((n = get_focused_node(curr_monitor->desk->tree)) == NULL) {
		return -1;
	}

	if (n->client->window == win)
		return 0;

	set_focus(n, true);
	xcb_flush(wm->connection);
	return 0;
}

static inline window_state_action_t
convert_state_action(uint32_t action)
{
	switch (action) {
	case XCB_EWMH_WM_STATE_REMOVE: return STATE_ACTION_REMOVE;
	case XCB_EWMH_WM_STATE_ADD: return STATE_ACTION_ADD;
	case XCB_EWMH_WM_STATE_TOGGLE: return STATE_ACTION_TOGGLE;
	default: return STATE_ACTION_INVALID;
	}
}

static inline client_message_type_t
get_client_message_type(xcb_atom_t type, xcb_ewmh_conn_t *ewmh)
{
	if (type == ewmh->_NET_CURRENT_DESKTOP)
		return CLIENT_MESSAGE_CURRENT_DESKTOP;
	if (type == ewmh->_NET_WM_STATE)
		return CLIENT_MESSAGE_WINDOW_STATE;
	if (type == ewmh->_NET_ACTIVE_WINDOW)
		return CLIENT_MESSAGE_ACTIVE_WINDOW;
	if (type == ewmh->_NET_WM_DESKTOP)
		return CLIENT_MESSAGE_WINDOW_DESKTOP;
	if (type == ewmh->_NET_CLOSE_WINDOW)
		return CLIENT_MESSAGE_CLOSE_WINDOW;

	return CLIENT_MESSAGE_UNSUPPORTED;
}

static window_state_type_t
get_window_state_type(uint32_t state, xcb_ewmh_conn_t *ewmh)
{
	if (state == ewmh->_NET_WM_STATE_FULLSCREEN)
		return STATE_FULLSCREEN;
	if (state == ewmh->_NET_WM_STATE_BELOW)
		return STATE_BELOW;
	if (state == ewmh->_NET_WM_STATE_ABOVE)
		return STATE_ABOVE;
	if (state == ewmh->_NET_WM_STATE_HIDDEN)
		return STATE_HIDDEN;
	if (state == ewmh->_NET_WM_STATE_STICKY)
		return STATE_STICKY;
	if (state == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
		return STATE_DEMANDS_ATTENTION;

	return STATE_UNSUPPORTED;
}

static int
handle_fullscreen_state(node_t				 *node,
						window_state_action_t action,
						const char			 *name)
{
	if (!node || !node->client || !name) {
		return -1;
	}

	xcb_window_t		w	  = node->client->window;
	window_state_type_t state = STATE_FULLSCREEN;
#define _LOG_WM_STATE_ACTION_(action, state, w, name)                          \
	_LOG_(INFO, "received %s %s for window %d:%s", #action, #state, w, name)

	switch (action) {
	case STATE_ACTION_ADD: {
		_LOG_WM_STATE_ACTION_(ADD, FULLSCREEN, w, name);
		return set_fullscreen(node, true);
	}
	case STATE_ACTION_REMOVE: {
		_LOG_WM_STATE_ACTION_(REMOVE, FULLSCREEN, w, name);
		return set_fullscreen(node, false);
	}
	case STATE_ACTION_TOGGLE: {
		bool is_fullscreen = (node->client->state == FULLSCREEN);
		_LOG_WM_STATE_ACTION_(TOGGLE, FULLSCREEN, w, name);
		return set_fullscreen(node, !is_fullscreen);
	}
	default: return 0;
	}
}

static int
handle_net_wm_state(xcb_window_t win_event, uint32_t action, uint32_t state)
{
	if (state == 0) {
		return 0;
	}

	node_t *node = find_node_global(win_event);
	if (!node) {
		return 0;
	}

	xcb_window_t win  = node->client->window;
	char		*name = win_name(win);
	if (!name) {
		return -1;
	}

	int					  result	   = 0;
	window_state_type_t	  type		   = get_window_state_type(state, wm->ewmh);
	window_state_action_t state_action = convert_state_action(action);
	ewmh_state_t		  flag		   = ewmh_flag_for_atom(state);
	bool				  set_state	   = false;

#define _LOG_WM_STATE_(state, w, name)                                         \
	_LOG_(INFO, "received %s for window %d:%s", #state, w, name)

	if (flag != EWMH_STATE_NONE && state_action != STATE_ACTION_INVALID) {
		if (state_action == STATE_ACTION_ADD) {
			set_state = true;
		} else if (state_action == STATE_ACTION_REMOVE) {
			set_state = false;
		} else if (state_action == STATE_ACTION_TOGGLE) {
			set_state = !ewmh_has(node->client->ewmh_state, flag);
		}
		update_net_wm_state_atom(win, state, set_state);
		update_client_ewmh_state(node->client, flag, set_state);
	}

	switch (type) {
	case STATE_FULLSCREEN:
		_LOG_WM_STATE_(FULLSCREEN, win, name);
		result = handle_fullscreen_state(node, state_action, name);
		break;
	case STATE_BELOW: {
		_LOG_WM_STATE_(BELOW, win, name);
		break;
	}
	case STATE_ABOVE: {
		_LOG_WM_STATE_(ABOVE, win, name);
		break;
	}
	case STATE_HIDDEN: {
		_LOG_WM_STATE_(HIDDEN, win, name);
		break;
	}
	case STATE_STICKY: {
		_LOG_WM_STATE_(STICKY, win, name);
		break;
	}
	case STATE_DEMANDS_ATTENTION: {
		_LOG_WM_STATE_(DEMANDS_ATTENTION, win, name);
		break;
	}
	default: break;
	}
	if (flag != EWMH_STATE_NONE) {
		restack();
	}

	_FREE_(name);
	return result;
}

static int
handle_client_message(const xcb_event_t *event)
{
	xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;

	if (ev->format != 32) {
		return 0;
	}

	xcb_window_t		  win	 = ev->window;
	client_message_type_t type	 = get_client_message_type(ev->type, wm->ewmh);
	char				 *name	 = win_name(win);
	int					  result = 0;

#define _LOG_CLIENT_MESSAGE_(type, w, name)                                    \
	_LOG_(INFO, "received %s for window %d:%s", #type, w, name)

	switch (type) {
	case CLIENT_MESSAGE_CURRENT_DESKTOP: {
		_LOG_CLIENT_MESSAGE_(CURRENT_DESKTOP, win, name);
		/* bars/pagers use this to switch desktops. root window gets the msg. */
		result = handle_net_desktop_change(ev->data.data32[0]);
		break;
	}
	case CLIENT_MESSAGE_WINDOW_STATE: {
		_LOG_CLIENT_MESSAGE_(WM_STATE, win, name);
		/* client asks for fullscreen/above/below/etc. first data is action. */
		size_t n = sizeof(ev->data.data32) / sizeof(ev->data.data32[0]);
		for (size_t i = 0; i < n - 1; i++) {
			uint32_t state = ev->data.data32[i + 1];
			result = handle_net_wm_state(win, ev->data.data32[0], state);
		}
		break;
	}
	case CLIENT_MESSAGE_ACTIVE_WINDOW: {
		/* app wants this win active, so jump to the desktop that owns it. */
		_LOG_CLIENT_MESSAGE_(ACTIVE_WINDOW, win, name);
		result = handle_net_active_window(win);
		break;
	}
	case CLIENT_MESSAGE_WINDOW_DESKTOP: {
		/* move win to another desktop, unless my rule already pinned it. */
		_LOG_CLIENT_MESSAGE_(WM_DESKTOP, win, name);
		uint32_t i = ev->data.data32[0];
		rule_t	*r = get_window_rule(win);
		if (r && r->desktop_id != -1) {
			break;
		}
		result = handle_net_wm_desktop(win, i);
		break;
	}
	case CLIENT_MESSAGE_CLOSE_WINDOW: {
		/* pager/taskbar close button. try polite close, kill if needed */
		_LOG_CLIENT_MESSAGE_(CLOSE_WINDOW, win, name);
		close_or_kill(win);
		break;
	}
	default: break;
	}
	_LOG_CLIENT_MESSAGE_(UNKNOWN_EVENT, win, name);
	_FREE_(name);
	xcb_flush(wm->connection);
	return result;
}

static int
handle_unmap_notify(const xcb_event_t *event)
{
	xcb_unmap_notify_event_t *ev  = (xcb_unmap_notify_event_t *)event;
	xcb_window_t			  win = ev->window;
#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved unmap notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	bool is_managed = client_exist(curr_monitor->desk->tree, win) ||
					  client_exist_in_desktops(win);
	if (!is_managed) {
		if (!ignore_ewmh_struts && remove_strut_window(win)) {
			reapply_tracked_struts();
		}
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		_FREE_(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (unmap)", win);
		return -1;
	}
	ewmh_update_client_list();
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_configure_request(const xcb_event_t *event)
{
	xcb_configure_request_event_t *ev  = (xcb_configure_request_event_t *)event;
	xcb_window_t				   win = ev->window;

	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t			cn =
		xcb_icccm_get_wm_name(wm->connection, ev->window);
	const uint8_t wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	char name[256];
	if (wr == 1) {
		snprintf(name, sizeof(name), "%s", t_reply.name);
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
	}
#ifdef _DEBUG__
	_LOG_(DEBUG,
		  "window %d  name %s wants to be at %dx%d with %dx%d",
		  win,
		  name,
		  ev->x,
		  ev->y,
		  ev->width,
		  ev->height);
#endif
	node_t *node = find_node_global(win);
	if (!node) {
		uint16_t mask = 0;
		uint32_t values[7];
		memset(values, 0, sizeof(values));
		uint16_t i = 0;
		if (ev->value_mask & XCB_CONFIG_WINDOW_X) {
			mask |= XCB_CONFIG_WINDOW_X;
			values[i++] = (uint32_t)ev->x;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_Y) {
			mask |= XCB_CONFIG_WINDOW_Y;
			values[i++] = (uint32_t)ev->y;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_WIDTH;
			values[i++] = ev->width;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
			mask |= XCB_CONFIG_WINDOW_HEIGHT;
			values[i++] = ev->height;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
			mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
			values[i++] = ev->border_width;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
			mask |= XCB_CONFIG_WINDOW_SIBLING;
			values[i++] = ev->sibling;
		}

		if (ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
			mask |= XCB_CONFIG_WINDOW_STACK_MODE;
			values[i++] = ev->stack_mode;
		}

		xcb_configure_window(wm->connection, win, mask, values);
	} else {
		/* reflect actual geometry (floating/fullscreen) in ConfigureNotify. */
		rectangle_t r = node->rectangle;
		if (node->client && IS_FLOATING(node->client)) {
			r = node->floating_rectangle;
		} else if (node->client && IS_FULLSCREEN(node->client)) {
			monitor_t *m = get_monitor_by_window(node->client->window);
			if (m) {
				r = m->rectangle;
			} else if (curr_monitor) {
				r = curr_monitor->rectangle;
			}
		}
		xcb_configure_notify_event_t evt;
		memset(&evt, 0, sizeof(evt));
		unsigned int bw		  = (node->client && IS_FULLSCREEN(node->client))
									? 0
									: conf.border_width;
		evt.response_type	  = XCB_CONFIGURE_NOTIFY;
		evt.event			  = win;
		evt.window			  = win;
		evt.above_sibling	  = XCB_NONE;
		evt.x				  = r.x;
		evt.y				  = r.y;
		evt.width			  = r.width;
		evt.height			  = r.height;
		evt.border_width	  = bw;
		evt.override_redirect = false;
		xcb_send_event(wm->connection,
					   false,
					   win,
					   XCB_EVENT_MASK_STRUCTURE_NOTIFY,
					   (const char *)&evt);
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_destroy_notify(const xcb_event_t *event)
{
	xcb_destroy_notify_event_t *ev	= (xcb_destroy_notify_event_t *)event;
	xcb_window_t				win = ev->window;
#ifdef _DEBUG__
	char *s = win_name(win);
	_LOG_(DEBUG, "recieved destroy notify for %d, name %s ", win, s);
	_FREE_(s);
#endif

	bool is_managed = client_exist(curr_monitor->desk->tree, win) ||
					  client_exist_in_desktops(win);
	if (!is_managed) {
		if (!ignore_ewmh_struts && remove_strut_window(win)) {
			reapply_tracked_struts();
		}
#ifdef _DEBUG__
		char *name = win_name(win);
		_LOG_(DEBUG, "cannot find win %d, name %s", win, name);
		_FREE_(name);
#endif
		return 0;
	}

	if (kill_window(win) != 0) {
		_LOG_(ERROR, "cannot kill window %d (destroy)", win);
		return -1;
	}
	ewmh_update_client_list();
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_key_press(const xcb_event_t *event)
{
	xcb_key_press_event_t *ev			 = (xcb_key_press_event_t *)event;
	uint16_t			   cleaned_state = normalize_mods(ev->state);

	if (ds.active || ms.op != MOUSE_OP_NONE) {
		xcb_keysym_t k = get_keysym(ev->detail, wm->connection);
		if (ds.active && k == XK_Escape) {
			drag_cancel();
			return 0;
		}
		if (ms.op != MOUSE_OP_NONE && k == XK_Escape) {
			cancel_mouse_action();
			return 0;
		}
	}

	if (key_head) {
		conf_key_t *current = key_head;
		while (current) {
			if (cleaned_state == normalize_mods((uint16_t)current->mod)) {
				/* Keybind dispatch uses physical keycodes so shortcuts keep
				 * working under non-US XKB layouts. */
				if (current->keycode == ev->detail) {
					arg_t	 *a	  = current->arg;
					const int ret = current->execute(a);
					if (ret != 0) {
						_LOG_(ERROR, "error while executing function_ptr(..)");
					}
					break;
				}
			}
			current = current->next;
		}
		return 0;
	}

	for (size_t i = _keys_len; i--;) {
		if (cleaned_state == normalize_mods((uint16_t)_keys_[i].mod)) {
			if (_keys_[i].keycode == ev->detail) {
				arg_t	 *a	  = _keys_[i].arg;
				const int ret = _keys_[i].execute(a);
				if (ret != 0) {
					_LOG_(ERROR, "error while executing function_ptr(..)");
				}
				break;
			}
		}
	}
	return 0;
}

static int
handle_mapping_notify(const xcb_event_t *event)
{
	xcb_mapping_notify_event_t *ev = (xcb_mapping_notify_event_t *)event;

	if (ev->request != XCB_MAPPING_KEYBOARD &&
		ev->request != XCB_MAPPING_MODIFIER) {
		return 0;
	}

	if (is_kgrabbed) {
		ungrab_keys(wm->connection, wm->root_window);
		is_kgrabbed = !is_kgrabbed;
	}

	if (0 != grab_keys(wm->connection, wm->root_window)) {
		_LOG_(ERROR, "cannot grab keys");
		return -1;
	}
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_button_press_event(const xcb_event_t *event)
{
	xcb_button_press_event_t *ev  = (xcb_button_press_event_t *)event;
	xcb_window_t			  win = ev->event;
	if (win == wm->root_window && ev->child != XCB_NONE) {
		win = ev->child;
	}

	_LOG_(INFO,
		  "=== BUTTON PRESS: detail=%d, state=0x%x, SUPER=0x%x ===",
		  ev->detail,
		  ev->state,
		  SUPER);
	_LOG_(INFO,
		  "    win=%d, root_x=%d, root_y=%d, detail==BTN1? %d, state&SUPER? %d",
		  win,
		  ev->root_x,
		  ev->root_y,
		  ev->detail == XCB_BUTTON_INDEX_1,
		  (ev->state & SUPER) != 0);

	if (ev->detail == XCB_BUTTON_INDEX_1 && (ev->state & SUPER)) {
		_LOG_(INFO, ">>> SUPER+Button1 match, checking drag conditions...");
		if (!window_exists(wm->connection, win))
			goto normal_handling;
		node_t *root = curr_monitor->desk->tree;
		node_t *n	 = find_node_by_window_id(root, win);
		if (!n) {
			_LOG_(INFO, "    node not found in tree");
			goto normal_handling;
		}
		if (!n->client) {
			_LOG_(INFO, "    node has no client");
			goto normal_handling;
		}
		_LOG_(INFO, "    client state=%d (TILED=%d)", n->client->state, TILED);
		if (IS_TILED(n->client)) {
			_LOG_(INFO, "    >>> calling drag_start()...");
			if (drag_start(win, ev->root_x, ev->root_y, false) == 0) {
				_LOG_(INFO, "    >>> drag started..");
			}
		} else if (IS_FLOATING(n->client)) {
			_LOG_(INFO, "    >>> calling start_floating_move()...");
			start_floating_move(n, ev->root_x, ev->root_y);
		}
	}

	if (ev->detail == XCB_BUTTON_INDEX_3 && (ev->state & SUPER)) {
		if (!window_exists(wm->connection, win))
			goto normal_handling;
		node_t *root = curr_monitor->desk->tree;
		node_t *n	 = find_node_by_window_id(root, win);
		if (!n || !n->client) {
			goto normal_handling;
		}
		if (IS_FLOATING(n->client)) {
			start_floating_resize(n, ev->root_x, ev->root_y);
		} else if (IS_TILED(n->client)) {
			start_tiled_resize(n, ev->root_x, ev->root_y);
		}
	}

normal_handling:
	if (conf.focus_follow_pointer) {
		return 0;
	}
#ifdef _DEBUG__
	char *name = win_name(ev->event);
	_LOG_(DEBUG,
		  "RCIEVED BUTTON PRESS EVENT window %d, window name %s",
		  ev->event,
		  name);
	_FREE_(name);
#endif

	if (!window_exists(wm->connection, win)) {
		return 0;
	}
	node_t	 *root	 = curr_monitor->desk->tree;
	node_t	 *n		 = find_node_by_window_id(root, win);
	client_t *client = (n && n->client) ? n->client : NULL;

	if (client == NULL) {
		return -1;
	}

	if (win == wm->root_window) {
		return 0;
	}

	window_ungrab_buttons(client->window);

	const int r = set_active_window_name(win);
	if (r != 0) {
		return 0;
	}

	if (IS_FLOATING(n->client)) {
		if (win_focus(n->client->window, true) != 0) {
			_LOG_(ERROR, "cannot focus window %d (enter)", n->client->window);
			return -1;
		}
		n->is_focused = true;
	} else if (IS_FULLSCREEN(n->client)) {
		if (fullscreen_focus(n->client->window)) {
			_LOG_(ERROR, "cannot update win attributes");
			return -1;
		}
	} else {
		if (curr_monitor->desk->layout == STACK) {
			if (win_focus(n->client->window, true) != 0) {
				_LOG_(
					ERROR, "cannot focus window %d (enter)", n->client->window);
				return -1;
			}
			n->is_focused = true;
		} else {
			if (set_focus(n, true) != 0) {
				_LOG_(ERROR, "cannot focus node (enter)");
				return -1;
			}
		}
	}

	focused_win = n->client->window;
	update_focus(curr_monitor->desk, n);

	if (has_floating_window(root)) {
		restack();
	}

	xcb_allow_events(wm->connection, XCB_ALLOW_SYNC_POINTER, ev->time);
	xcb_flush(wm->connection);
	return 0;
}

static int
handle_motion_notify(const xcb_event_t *event)
{
	xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
#ifdef _DEBUG__
	_LOG_(INFO,
		  "recevied motion notify on root %dx%d event %dx%d",
		  ev->root_x,
		  ev->root_y,
		  ev->event_x,
		  ev->event_y);
#endif

	if (ms.op != MOUSE_OP_NONE) {
		handle_mouse_motion(ev->root_x, ev->root_y);
		return 0;
	}
	if (ds.active) {
		_LOG_(INFO, "MOTION NOTIFY: x=%d, y=%d", ev->root_x, ev->root_y);
		drag_move(ev->root_x, ev->root_y);
		return 0;
	}

	if (ev->child != XCB_NONE) {
		return 0;
	}
	int16_t	   rx = ev->root_x;
	int16_t	   ry = ev->root_y;
	monitor_t *m  = get_monitor_within_coordinate(rx, ry);
	if (!m) {
		return 0;
	}
	if (curr_monitor && curr_monitor != m) {
		curr_monitor = m;
	}
	return 0;
}

static int
handle_button_release(const xcb_event_t *event)
{
	xcb_button_release_event_t *ev = (xcb_button_release_event_t *)event;

	if (ms.op != MOUSE_OP_NONE) {
		finish_mouse_action();
		return 0;
	}
	if (ds.active) {
		drag_end(ev->root_x, ev->root_y);
		return 0;
	}

	return 0;
}

/* ./src/ewmh.c */



static xcb_ewmh_conn_t *
ewmh_init(xcb_conn_t *conn);
static int
ewmh_set_supporting(xcb_window_t win, xcb_ewmh_conn_t *ewmh);
static int
ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd);
static size_t
get_active_clients_size(desktop_t **d, const int n);
static void
populate_client_array(node_t *root, xcb_window_t *arr, size_t *index);
static ewmh_window_type_t
determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom);

static bool
ewmh_has(ewmh_state_t s, ewmh_state_t f)
{
	return (s & f) != 0;
}

static xcb_ewmh_conn_t *
ewmh_init(xcb_conn_t *conn)
{
	if (conn == 0x00) {
		_LOG_(ERROR, "connection is NULL");
		return NULL;
	}

	xcb_ewmh_conn_t *ewmh = calloc(1, sizeof(xcb_ewmh_conn_t));
	if (ewmh == NULL) {
		_LOG_(ERROR, "cannot calloc ewmh");
		return NULL;
	}

	xcb_intern_atom_cookie_t *c = xcb_ewmh_init_atoms(conn, ewmh);
	if (c == 0x00) {
		_LOG_(ERROR, "cannot init intern atom");
		return NULL;
	}

	const uint8_t res = xcb_ewmh_init_atoms_replies(ewmh, c, NULL);
	if (res != 1) {
		_LOG_(ERROR, "cannot init intern atom");
		return NULL;
	}
	return ewmh;
}

static int
ewmh_set_supporting(xcb_window_t win, xcb_ewmh_conn_t *ewmh)
{
	pid_t		 wm_pid = getpid();
	xcb_cookie_t supporting_cookie_root =
		xcb_ewmh_set_supporting_wm_check_checked(ewmh, wm->root_window, win);
	xcb_cookie_t supporting_cookie =
		xcb_ewmh_set_supporting_wm_check_checked(ewmh, win, win);
	xcb_cookie_t name_cookie =
		xcb_ewmh_set_wm_name_checked(ewmh, win, strlen(WM_NAME), WM_NAME);
	xcb_cookie_t pid_cookie =
		xcb_ewmh_set_wm_pid_checked(ewmh, win, (uint32_t)wm_pid);

	xcb_error_t *err;
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie_root))) {
		_LOG_(ERROR, "error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, supporting_cookie))) {
		_LOG_(ERROR, "error setting supporting window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, name_cookie))) {
		_LOG_(ERROR, "error setting WM name: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	if ((err = xcb_request_check(ewmh->connection, pid_cookie))) {
		_LOG_(ERROR, "error setting WM PID: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
ewmh_set_number_of_desktops(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t nd)
{
	xcb_cookie_t c =
		xcb_ewmh_set_number_of_desktops_checked(ewmh, screen_nbr, nd);
	xcb_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
ewmh_update_desktop_names(void)
{
	char		 names[MAXLEN];
	uint32_t	 names_len = 0;
	unsigned int offset	   = 0;
	memset(names, 0, sizeof(names));

	for (int n = 0; n < prim_monitor->n_of_desktops; n++) {
		desktop_t *d  = prim_monitor->desktops[n];
		size_t	   nn = sizeof(names);
		for (int j = 0; d->name[j] != '\0' && (offset + j) < nn; j++) {
			names[offset + j] = d->name[j];
		}
		offset += strlen(d->name);
		if (offset < sizeof(names)) {
			names[offset++] = '\0';
		}
	}

	names_len	   = offset - 1;
	xcb_cookie_t c = xcb_ewmh_set_desktop_names_checked(
		wm->ewmh, wm->screen_nbr, names_len, names);
	xcb_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting names of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static size_t
get_active_clients_size(desktop_t **d, const int n)
{
	if (!d)
		return -1;
	size_t t = 0;
	for (int i = 0; i < n; ++i) {
		if (!d[i])
			continue;
		t += d[i]->n_count;
	}
	return t;
}

static void
populate_client_array(node_t *root, xcb_window_t *arr, size_t *index)
{
	if (root == NULL)
		return;

	if (root->client && root->client->window != XCB_NONE) {
		arr[*index] = root->client->window;
		(*index)++;
	}

	populate_client_array(root->first_child, arr, index);
	populate_client_array(root->second_child, arr, index);
}

static void
ewmh_update_client_list(void)
{
	monitor_t *m	= head_monitor;
	size_t	   size = 0;
	while (m) {
		if (!m->desktops)
			continue;
		size += get_active_clients_size(m->desktops, m->n_of_desktops);
		m = m->next;
	}
	if (size == 0) {
		xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, 0, NULL);
		return;
	}
	xcb_window_t *active_clients =
		(xcb_window_t *)malloc((size + 1) * sizeof(xcb_window_t));
	if (active_clients == NULL) {
		return;
	}
	size_t	   index = 0;
	monitor_t *curr	 = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; ++i) {
			node_t *root = curr->desktops[i]->tree;
			populate_client_array(root, active_clients, &index);
		}
		curr = curr->next;
	}
	xcb_ewmh_set_client_list(wm->ewmh, wm->screen_nbr, size, active_clients);
	_FREE_(active_clients);
}

static int
ewmh_update_current_desktop(xcb_ewmh_conn_t *ewmh, int screen_nbr, uint32_t i)
{
	xcb_cookie_t c = xcb_ewmh_set_current_desktop_checked(ewmh, screen_nbr, i);
	xcb_error_t *err = xcb_request_check(ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting number of desktops: %d", err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static void
ewmh_update_desktop_viewport(void)
{
	uint32_t   count = 0;
	monitor_t *curr	 = head_monitor;
	while (curr) {
		count += curr->n_of_desktops;
		curr = curr->next;
	}
	if (count == 0) {
		xcb_ewmh_set_desktop_viewport(wm->ewmh, wm->screen_nbr, 0, NULL);
		return;
	}
	xcb_ewmh_coordinates_t coords[count];
	uint16_t			   desktop = 0;
	curr						   = head_monitor;
	while (curr) {
		for (int j = 0; j < curr->n_of_desktops; j++) {
			coords[desktop++] =
				(xcb_ewmh_coordinates_t){curr->rectangle.x, curr->rectangle.y};
		}
		curr = curr->next;
	}
	xcb_ewmh_set_desktop_viewport(wm->ewmh, wm->screen_nbr, desktop, coords);
}

static int
ewmh_update_number_of_desktops(void)
{
	uint32_t desktops_count = 0;
	desktops_count			= prim_monitor->n_of_desktops;
	return ewmh_set_number_of_desktops(
		wm->ewmh, wm->screen_nbr, desktops_count);
}

static bool
setup_ewmh(void)
{
	wm->ewmh = ewmh_init(wm->connection);
	if (wm->ewmh == NULL) {
		return false;
	}

	xcb_atom_t	 net_atoms[] = {wm->ewmh->_NET_SUPPORTED,
								wm->ewmh->_NET_SUPPORTING_WM_CHECK,
								wm->ewmh->_NET_DESKTOP_NAMES,
								wm->ewmh->_NET_DESKTOP_VIEWPORT,
								wm->ewmh->_NET_NUMBER_OF_DESKTOPS,
								wm->ewmh->_NET_CURRENT_DESKTOP,
								wm->ewmh->_NET_CLIENT_LIST,
								wm->ewmh->_NET_ACTIVE_WINDOW,
								wm->ewmh->_NET_WM_NAME,
								wm->ewmh->_NET_CLOSE_WINDOW,
								wm->ewmh->_NET_WM_STRUT_PARTIAL,
								wm->ewmh->_NET_WM_DESKTOP,
								wm->ewmh->_NET_WM_STATE,
								wm->ewmh->_NET_WM_STATE_FULLSCREEN,
								wm->ewmh->_NET_WM_STATE_BELOW,
								wm->ewmh->_NET_WM_STATE_ABOVE,
								wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION,
								wm->ewmh->_NET_WM_WINDOW_TYPE,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DOCK,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DESKTOP,
								wm->ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION,
								wm->ewmh->_NET_WM_WINDOW_TYPE_DIALOG,
								wm->ewmh->_NET_WM_WINDOW_TYPE_SPLASH,
								wm->ewmh->_NET_WM_WINDOW_TYPE_UTILITY,
								wm->ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR};

	xcb_cookie_t c			 = xcb_ewmh_set_supported_checked(
		wm->ewmh, wm->screen_nbr, LEN(net_atoms), net_atoms);
	xcb_error_t *err = xcb_request_check(wm->ewmh->connection, c);
	if (err) {
		_LOG_(ERROR, "error setting supported ewmh masks: %d", err->error_code);
		_FREE_(err);
		return false;
	}

	if (ewmh_set_supporting(wm->root_window, wm->ewmh) != 0) {
		return false;
	}

	if (ewmh_update_number_of_desktops() != 0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	if (ewmh_update_current_desktop(
			wm->ewmh, wm->screen_nbr, (uint32_t)curr_monitor->desk->id) != 0) {
		return false;
	}

	if (ewmh_update_desktop_names() != 0) {
		return false;
	}

	ewmh_update_desktop_viewport();

	return true;
}

static ewmh_state_t
get_net_wm_state_mask(xcb_window_t win)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return EWMH_STATE_NONE;

	ewmh_state_t			   mask = EWMH_STATE_NONE;
	xcb_get_property_cookie_t  ck	= xcb_ewmh_get_wm_state(wm->ewmh, win);
	xcb_ewmh_get_atoms_reply_t rep;
	if (!xcb_ewmh_get_wm_state_reply(wm->ewmh, ck, &rep, NULL)) {
		return EWMH_STATE_NONE;
	}

	for (unsigned i = 0; i < rep.atoms_len; ++i) {
		xcb_atom_t a = rep.atoms[i];
		if (a == wm->ewmh->_NET_WM_STATE_ABOVE)
			mask |= EWMH_STATE_ABOVE;
		else if (a == wm->ewmh->_NET_WM_STATE_BELOW)
			mask |= EWMH_STATE_BELOW;
		else if (a == wm->ewmh->_NET_WM_STATE_FULLSCREEN)
			mask |= EWMH_STATE_FULLSCREEN;
		else if (a == wm->ewmh->_NET_WM_STATE_MODAL)
			mask |= EWMH_STATE_MODAL;
		else if (a == wm->ewmh->_NET_WM_STATE_HIDDEN)
			mask |= EWMH_STATE_HIDDEN;
		else if (a == wm->ewmh->_NET_WM_STATE_STICKY)
			mask |= EWMH_STATE_STICKY;
		else if (a == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
			mask |= EWMH_STATE_DEMANDS_ATTN;
	}

	xcb_ewmh_get_atoms_reply_wipe(&rep);
	return mask;
}

static ewmh_state_t
ewmh_flag_for_atom(xcb_atom_t atom)
{
	if (atom == wm->ewmh->_NET_WM_STATE_ABOVE)
		return EWMH_STATE_ABOVE;
	if (atom == wm->ewmh->_NET_WM_STATE_BELOW)
		return EWMH_STATE_BELOW;
	if (atom == wm->ewmh->_NET_WM_STATE_FULLSCREEN)
		return EWMH_STATE_FULLSCREEN;
	if (atom == wm->ewmh->_NET_WM_STATE_MODAL)
		return EWMH_STATE_MODAL;
	if (atom == wm->ewmh->_NET_WM_STATE_HIDDEN)
		return EWMH_STATE_HIDDEN;
	if (atom == wm->ewmh->_NET_WM_STATE_STICKY)
		return EWMH_STATE_STICKY;
	if (atom == wm->ewmh->_NET_WM_STATE_DEMANDS_ATTENTION)
		return EWMH_STATE_DEMANDS_ATTN;
	return EWMH_STATE_NONE;
}

static void
update_client_ewmh_state(client_t *c, ewmh_state_t flag, bool set)
{
	if (!c || flag == EWMH_STATE_NONE)
		return;
	if (set)
		c->ewmh_state |= flag;
	else
		c->ewmh_state &= ~flag;
}

static void
remove_property(xcb_connection_t *con,
				xcb_window_t	  win,
				xcb_atom_t		  prop,
				xcb_atom_t		  atom)
{
	xcb_grab_server(con);
	xcb_get_property_cookie_t c = xcb_get_property(
		con, false, win, prop, XCB_GET_PROPERTY_TYPE_ANY, 0, 4096);
	xcb_get_property_reply_t *reply = xcb_get_property_reply(con, c, NULL);
	if (reply == NULL || xcb_get_property_value_length(reply) == 0)
		goto release_grab;
	const xcb_atom_t *atoms = xcb_get_property_value(reply);
	if (atoms == NULL) {
		goto release_grab;
	}

	{
		int		  num = 0;
		const int curr_size =
			xcb_get_property_value_length(reply) / (reply->format / 8);
		xcb_atom_t values[curr_size];
		memset(values, 0, sizeof(values));
		for (int i = 0; i < curr_size; i++) {
			if (atoms[i] != atom)
				values[num++] = atoms[i];
		}
		xcb_change_property(con,
							XCB_PROP_MODE_REPLACE,
							win,
							prop,
							XCB_ATOM_ATOM,
							32,
							num,
							values);
	}

release_grab:
	if (reply)
		_FREE_(reply);
	xcb_ungrab_server(con);
}

static int
update_net_wm_state_atom(xcb_window_t win, xcb_atom_t atom, bool set)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return -1;

	const ewmh_state_t flag = ewmh_flag_for_atom(atom);
	const ewmh_state_t mask = get_net_wm_state_mask(win);

	if (set) {
		if (flag != EWMH_STATE_NONE && (mask & flag))
			return 0;
		xcb_atom_t	 values[] = {atom};
		xcb_cookie_t c	 = xcb_change_property_checked(wm->connection,
													   XCB_PROP_MODE_APPEND,
													   win,
													   wm->ewmh->_NET_WM_STATE,
													   XCB_ATOM_ATOM,
													   32,
													   1,
													   values);
		xcb_error_t *err = xcb_request_check(wm->connection, c);
		if (err) {
			_LOG_(ERROR,
				  "cannot append _NET_WM_STATE for %d: error code %d",
				  win,
				  err->error_code);
			_FREE_(err);
			return -1;
		}
		return 0;
	}

	remove_property(wm->connection, win, wm->ewmh->_NET_WM_STATE, atom);
	return 0;
}

static void
fill_icccm_ewmh(client_t *c)
{
	xcb_window_t			  transient = XCB_NONE;
	xcb_get_property_cookie_t cv =
		xcb_icccm_get_wm_transient_for(wm->connection, c->window);
	xcb_icccm_get_wm_transient_for_reply(wm->connection, cv, &transient, NULL);

	c->transient_for = transient;

	xcb_get_window_attributes_cookie_t atc =
		xcb_get_window_attributes(wm->connection, c->window);
	xcb_get_window_attributes_reply_t *atr =
		xcb_get_window_attributes_reply(wm->connection, atc, NULL);
	c->override_redirect = (atr && atr->override_redirect);
	free(atr);

	c->ewmh_type  = window_type(c->window);
	c->ewmh_state = get_net_wm_state_mask(c->window);
}

static int
set_active_window_name(xcb_window_t win)
{
	xcb_cookie_t aw_cookie =
		xcb_ewmh_set_active_window_checked(wm->ewmh, wm->screen_nbr, win);
	xcb_error_t *err = xcb_request_check(wm->connection, aw_cookie);

	if (err) {
		_LOG_(ERROR, "cannot setting active window: %d", err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state)
{
	const long	 data[] = {state, XCB_NONE};
	xcb_atom_t	 t		= get_atom("WM_STATE", wm->connection);
	xcb_cookie_t c		= xcb_change_property_checked(
		wm->connection, XCB_PROP_MODE_REPLACE, win, t, t, 32, 2, data);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in changing property window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
update_net_wm_desktop(xcb_window_t win, uint32_t desktop)
{
	if (!wm || !wm->ewmh || win == XCB_NONE)
		return -1;
	xcb_cookie_t c	 = xcb_ewmh_set_wm_desktop_checked(wm->ewmh, win, desktop);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "cannot set _NET_WM_DESKTOP for %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static ewmh_window_type_t
determine_window_type(xcb_ewmh_conn_t *ewmh, xcb_atom_t atom)
{
	if (atom == ewmh->_NET_WM_WINDOW_TYPE_NORMAL) {
		return WINDOW_TYPE_NORMAL;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
		return WINDOW_TYPE_DOCK;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP) {
		return WINDOW_TYPE_DESKTOP;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR ||
			   atom == ewmh->_NET_WM_WINDOW_TYPE_MENU) {
		return WINDOW_TYPE_TOOLBAR_MENU;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_UTILITY) {
		return WINDOW_TYPE_UTILITY;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_SPLASH) {
		return WINDOW_TYPE_SPLASH;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_DIALOG) {
		return WINDOW_TYPE_DIALOG;
	} else if (atom == ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION) {
		return WINDOW_TYPE_NOTIFICATION;
	}
	return WINDOW_TYPE_NORMAL;
}

static ewmh_window_type_t
window_type(xcb_window_t win)
{
	xcb_ewmh_get_atoms_reply_t w_type;
	xcb_get_property_cookie_t  c = xcb_ewmh_get_wm_window_type(wm->ewmh, win);

	const uint8_t			   r =
		xcb_ewmh_get_wm_window_type_reply(wm->ewmh, c, &w_type, NULL);

	if (r != 1) {
		return WINDOW_TYPE_UNKNOWN;
	}

	ewmh_window_type_t type = WINDOW_TYPE_NORMAL;
	for (unsigned int i = 0; i < w_type.atoms_len; ++i) {
		type = determine_window_type(wm->ewmh, w_type.atoms[i]);
		if (type != WINDOW_TYPE_NORMAL) {
			break;
		}
	}

	xcb_ewmh_get_atoms_reply_wipe(&w_type);
	return type;
}

/* ./src/focus.c */



static int
fullscreen_focus(xcb_window_t win)
{
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor	   = 0;
	uint32_t bwidth	   = 0;

	/* do not focus unmapped windows */
	if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
		return 0;
	}

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) != 0) {
		_LOG_(ERROR, "cannot set input focus");
		return -1;
	}

	xcb_flush(wm->connection);
	return 0;
}

static int
win_focus(xcb_window_t win, bool set_focus)
{
#ifdef _DEBUG__
	char *name = win_name(win);
	_LOG_(DEBUG,
		  "[WIN_FOCUS] win=%d name='%s' set_focus=%s",
		  win,
		  name ? name : "(null)",
		  set_focus ? "TRUE" : "FALSE");
	_FREE_(name);
#endif
	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;
	uint32_t bcolor =
		set_focus ? conf.active_border_color : conf.normal_border_color;
	uint32_t bwidth = conf.border_width;

	/* border updates are ok for hidden windows, real X focus is not tho */
	if (set_focus) {
		if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
			return 0;
		}
	}

	if (change_window_attr(wm->connection, win, bpx_width, &bcolor) != 0) {
		_LOG_(ERROR, "cannot update win attributes");
		return -1;
	}

	if (configure_window(wm->connection, win, b_width, &bwidth) != 0) {
		_LOG_(ERROR, "cannot configure window");
		return -1;
	}

	if (set_focus) {
		if (set_input_focus(wm->connection, input, win, XCB_CURRENT_TIME) !=
			0) {
			_LOG_(ERROR, "cannot set input focus");
			return -1;
		}
	}

	xcb_flush(wm->connection);
	return 0;
}

static int
set_focus(node_t *n, bool flag)
{
#ifdef _DEBUG__
	char *name = (n && n->client) ? win_name(n->client->window) : NULL;
	_LOG_(DEBUG,
		  "[SET_FOCUS] set_focus called: win=%d name='%s' flag=%s state=%s",
		  (n && n->client) ? n->client->window : 0,
		  name ? name : "(null)",
		  flag ? "TRUE" : "FALSE",
		  (n && n->client && IS_FLOATING(n->client)) ? "FLOATING" : "TILED");
	_FREE_(name);
#endif
	n->is_focused = flag;

	/* skip focus attempt if trying to set focus on unmapped window */
	if (flag) {
		if (!check_window_map_state(n->client->window,
									WIN_MAP_STATE_VIEWABLE)) {
			return 0; /* Not an error just skip focusing */
		}
	}

	if (win_focus(n->client->window, flag) != 0) {
		_LOG_(ERROR, "cannot set focus");
		return -1;
	}

	return 0;
}

static void
update_grabbed_window(node_t *root, node_t *n)
{
	if (root == NULL)
		return;

	bool flag = !IS_INTERNAL(root) && root->client;
	if (flag && root != n) {
		set_focus(root, false);
		window_grab_buttons(root->client->window);
	}

	update_grabbed_window(root->first_child, n);
	update_grabbed_window(root->second_child, n);
}

/* ./src/logger.c */



#define LOG_DIR		 "/.local/share/xorg"
#define LOG_FILE	 "zwm.log"
#define MAX_PATH_LEN (2 << 7)
#ifdef _DEBUG__
#define MAX_LOG_SIZE (2 << 15) /* ~64kb */
#else
#define MAX_LOG_SIZE (2 << 12) /* ~8kb */
#endif

static void
log_message(log_level_t level, const char *format, ...)
{
	static char full_path[MAX_PATH_LEN] = {0};
	static int	initialized				= false;

	if (!initialized) {
		const char *homedir;
		if ((homedir = getenv("HOME")) == NULL) {
			__uid_t		   id = getuid();
			struct passwd *pw = getpwuid(id);
			if (pw == NULL) {
				fprintf(stderr, "Failed to get home directory\n");
				return;
			}
			homedir = pw->pw_dir;
		}

		snprintf(full_path,
				 sizeof(full_path),
				 "%s%s/%s",
				 homedir,
				 LOG_DIR,
				 LOG_FILE);
		initialized = true;
	}

	time_t	   t   = time(NULL);
	struct tm *ptr = localtime(&t);
	va_list	   args;
	char	   buf[100];

	strftime(buf, sizeof(buf), "%F/%I:%M:%S %p", ptr);

	struct stat st;
	FILE	   *log_file;
	if (stat(full_path, &st) == 0) {
		if (st.st_size >= MAX_LOG_SIZE) {
			log_file = fopen(full_path, "w");
		} else {
			log_file = fopen(full_path, "a");
		}
	} else {
		log_file = fopen(full_path, "a");
	}

	if (log_file == NULL) {
		fprintf(stderr, "Failed to open log file for writing\n");
		return;
	}

	fprintf(log_file, "%s ", buf);
	switch (level) {
	case ERROR: fprintf(log_file, KRED "[ERROR]" KNRM " "); break;
	case INFO: fprintf(log_file, KYEL "[INFO]" KNRM " "); break;
	case DEBUG: fprintf(log_file, KCYN "[DEBUG]" KNRM " "); break;
	case WARNING: fprintf(log_file, KORG "[WARNING]" KNRM " "); break;
	default: break;
	}

	va_start(args, format);
	vfprintf(log_file, format, args);
	va_end(args);

	fprintf(log_file, "\n");
	fclose(log_file);
}

/* ./src/monitor.c */



static monitor_t *
init_monitor(void);
static void
add_monitor(monitor_t **head, monitor_t *m);
static void
unlink_monitor(monitor_t **head, monitor_t *m);
static void
log_monitors(void);
static monitor_t *
get_monitor_by_randr_id(xcb_randr_output_t id);
static monitor_t *
get_monitor_by_root_id(xcb_window_t id);
static int
get_connected_monitor_count_xinerama(void);
static int
get_connected_monitor_count_xrandr(void);
static int
get_connected_monitor_count(bool xrandr, bool xinerama);
static bool
setup_monitors_via_xrandr(void);
static bool
setup_monitors_via_xinerama(void);
static bool
handle_added_monitor(xcb_randr_get_output_info_reply_t *info,
					 xcb_randr_output_t					id);
static void
destroy_monitor(monitor_t *m);
static bool
is_monitor_layout_changed(xcb_randr_get_output_info_reply_t *info,
						  rectangle_t						*r,
						  rectangle_t						*r_out);
static bool
merge_monitors(monitor_t *om, monitor_t *nm);
static bool
is_disconnected(monitor_t *m, monitor_t *dl);
static void
update_monitors(uint32_t *changes);
static bool
ranges_overlap(int32_t a_start, int32_t a_end, int32_t b_start, int32_t b_end);

static monitor_t *
init_monitor(void)
{
	monitor_t *m = (monitor_t *)malloc(sizeof(monitor_t));
	if (m == 0x00)
		return NULL;
	m->id		= 0;
	m->randr_id = XCB_NONE;
	snprintf(m->name, sizeof(m->name), "%s", MONITOR_NAME);
	m->root		   = XCB_NONE;
	m->rectangle   = (rectangle_t){0};
	m->padding	   = (padding_t){0};
	m->is_focused  = false;
	m->is_occupied = false;
	m->is_wired	   = false;
	m->next		   = NULL;
	m->desktops	   = NULL;
	m->desk		   = NULL;
	m->mru_counter = 1;
	return m;
}

static void
add_strut_window(xcb_window_t win)
{
	if (is_strut_window(win))
		return;

	strut_win_node_t *n = (strut_win_node_t *)malloc(sizeof(strut_win_node_t));
	if (n == NULL)
		return;
	n->win		  = win;
	n->next		  = strut_windows;
	strut_windows = n;
}

static bool
remove_strut_window(xcb_window_t win)
{
	strut_win_node_t *curr = strut_windows;
	strut_win_node_t *prev = NULL;

	while (curr) {
		if (curr->win == win) {
			if (prev) {
				prev->next = curr->next;
			} else {
				strut_windows = curr->next;
			}
#ifdef _DEBUG_
			char *name = win_name(win);
			_LOG_(DEBUG,
				  "[STRUT] removed strut window: %s 0x%x",
				  name ? name : "<unknown>",
				  win);
			_FREE_(name);
#endif
			_FREE_(curr);
			return true;
		}
		prev = curr;
		curr = curr->next;
	}
	return false;
}

static bool
is_strut_window(xcb_window_t win)
{
	for (strut_win_node_t *curr = strut_windows; curr; curr = curr->next) {
		if (curr->win == win)
			return true;
	}
	return false;
}

static void
cleanup_strut_windows(void)
{
	strut_win_node_t *curr = strut_windows;
	while (curr) {
		strut_win_node_t *next = curr->next;
		_FREE_(curr);
		curr = next;
	}
	strut_windows = NULL;
}

static int
handle_unmanaged_strut_window(xcb_window_t win)
{
	if (!wm || !wm->connection || win == XCB_NONE)
		return -1;

	if (!ignore_ewmh_struts)
		recalculate_all_struts();
	xcb_map_window(wm->connection, win);
	return 0;
}

static bool
ranges_overlap(int32_t a_start, int32_t a_end, int32_t b_start, int32_t b_end)
{
	if (a_start > a_end) {
		int32_t tmp = a_start;
		a_start		= a_end;
		a_end		= tmp;
	}
	if (b_start > b_end) {
		int32_t tmp = b_start;
		b_start		= b_end;
		b_end		= tmp;
	}
	return a_start <= b_end && a_end >= b_start;
}

static bool
ewmh_handle_struts(xcb_window_t win)
{
	/* struts are root-screen coords, not monitor coords.
	 * so multi monitor math here is root-based on purpose. */
	/* in X, montiors are just a range of x,y on a big canvas (root screen)*/
	if (!wm || !wm->ewmh || !wm->screen || win == XCB_NONE)
		return false;

	xcb_get_property_cookie_t ck = xcb_ewmh_get_wm_strut_partial(wm->ewmh, win);
	xcb_ewmh_wm_strut_partial_t strut;
	if (!xcb_ewmh_get_wm_strut_partial_reply(wm->ewmh, ck, &strut, NULL))
		return false;

	int32_t screen_w = wm->screen->width_in_pixels;
	int32_t screen_h = wm->screen->height_in_pixels;

	if (strut.left >= (uint32_t)screen_w || strut.right >= (uint32_t)screen_w ||
		strut.top >= (uint32_t)screen_h || strut.bottom >= (uint32_t)screen_h) {
		return false;
	}

	add_strut_window(win);

	bool changed = false;

	for (monitor_t *m = head_monitor; m; m = m->next) {
		int32_t mx1 = m->rectangle.x;
		int32_t my1 = m->rectangle.y;
		int32_t mx2 = m->rectangle.x + m->rectangle.width;
		int32_t my2 = m->rectangle.y + m->rectangle.height;

		/* left strut gives the right edge of the reserved area.
		 * apply it only to monitor that edge actually touches. */
		if (strut.left > 0 && (int32_t)strut.left > mx1 &&
			(int32_t)strut.left <= mx2 &&
			ranges_overlap((int32_t)strut.left_start_y,
						   (int32_t)strut.left_end_y,
						   my1,
						   my2)) {
			int32_t dx = (int32_t)strut.left - mx1;
			if (dx > INT16_MAX)
				dx = INT16_MAX;
			int16_t prev	= m->padding.left;
			m->padding.left = MAX((int16_t)dx, m->padding.left);
			if (m->padding.left != prev)
				changed = true;
		}

		/* right strut gives distance from screen right edge.
		 * convert it back to the bar left edge first. */
		if (strut.right > 0) {
			int32_t bar_left = screen_w - (int32_t)strut.right;
			if (bar_left >= mx1 && bar_left < mx2 &&
				ranges_overlap((int32_t)strut.right_start_y,
							   (int32_t)strut.right_end_y,
							   my1,
							   my2)) {
				int32_t dx = mx2 - bar_left;
				if (dx > 0) {
					if (dx > INT16_MAX)
						dx = INT16_MAX;
					int16_t prev	 = m->padding.right;
					m->padding.right = MAX((int16_t)dx, m->padding.right);
					if (m->padding.right != prev)
						changed = true;
				}
			}
		}

		if (strut.top > 0 && ranges_overlap((int32_t)strut.top_start_x,
											(int32_t)strut.top_end_x,
											mx1,
											mx2)) {
			int32_t dy = (int32_t)strut.top - my1;
			if (dy > 0) {
				if (dy > INT16_MAX)
					dy = INT16_MAX;
				int16_t prev   = m->padding.top;
				m->padding.top = MAX((int16_t)dy, m->padding.top);
				if (m->padding.top != prev)
					changed = true;
			}
		}

		if (strut.bottom > 0 && ranges_overlap((int32_t)strut.bottom_start_x,
											   (int32_t)strut.bottom_end_x,
											   mx1,
											   mx2)) {
			int32_t dy = my2 - (screen_h - (int32_t)strut.bottom);
			if (dy > 0) {
				if (dy > INT16_MAX)
					dy = INT16_MAX;
				int16_t prev	  = m->padding.bottom;
				m->padding.bottom = MAX((int16_t)dy, m->padding.bottom);
				if (m->padding.bottom != prev)
					changed = true;
			}
		}
	}

	return changed;
}

static rectangle_t
get_usable_area(monitor_t *m)
{
	rectangle_t r = {0};
	if (!m)
		return r;

	int32_t x = m->rectangle.x + m->padding.left;
	int32_t y = m->rectangle.y + m->padding.top;
	int32_t w =
		(int32_t)m->rectangle.width - m->padding.left - m->padding.right;
	int32_t h =
		(int32_t)m->rectangle.height - m->padding.top - m->padding.bottom;

	if (w < 0)
		w = 0;
	if (h < 0)
		h = 0;

	r.x		 = (int16_t)x;
	r.y		 = (int16_t)y;
	r.width	 = (uint16_t)w;
	r.height = (uint16_t)h;

	return r;
}

static void
reapply_tracked_struts(void)
{
	/* do not rescan root children here. dead bar windows can still be there
	 * with old strut prop, and that brings back padding we just removed. */
	if (!wm || !wm->connection || !wm->ewmh)
		return;

	for (monitor_t *m = head_monitor; m; m = m->next)
		m->padding = (padding_t){0};

	for (strut_win_node_t *s = strut_windows; s; s = s->next)
		ewmh_handle_struts(s->win);

	arrange_trees();
	render_trees();
}

static void
recalculate_all_struts(void)
{
	if (!wm || !wm->connection || !wm->ewmh)
		return;

	for (monitor_t *m = head_monitor; m; m = m->next)
		m->padding = (padding_t){0};

	xcb_query_tree_cookie_t ck =
		xcb_query_tree(wm->connection, wm->root_window);
	xcb_query_tree_reply_t *rep =
		xcb_query_tree_reply(wm->connection, ck, NULL);
	if (rep == NULL) {
		_LOG_(ERROR, "failed to query root window tree");
		return;
	}

	int			  len	   = xcb_query_tree_children_length(rep);
	xcb_window_t *children = xcb_query_tree_children(rep);

	for (int i = 0; i < len; ++i) ewmh_handle_struts(children[i]);

	free(rep);

	arrange_trees();
	render_trees();
}

static void
add_monitor(monitor_t **head, monitor_t *m)
{
	if (*head == NULL) {
		*head = m;
		return;
	}
	monitor_t *current = *head;
	while (current->next) {
		current = current->next;
	}
	current->next = m;
}

static void
unlink_monitor(monitor_t **head, monitor_t *m)
{
	if (!head || !*head || !m) {
		return;
	}

	monitor_t *curr = *head;
	monitor_t *prev = NULL;

	while (curr) {
		if (curr == m) {
			if (prev == NULL) {
				*head = curr->next;
			} else {
				prev->next = curr->next;
			}
			curr->next = NULL;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

static void
log_monitors(void)
{
	if (!head_monitor) {
		_LOG_(INFO, "monitors list is empty");
		return;
	}
	monitor_t *curr = head_monitor;
	while (curr) {
		_LOG_(INFO,
			  "found monitor %s:%d, rectangle {.x = %d, .y = %d, .w = %d, .h = "
			  "%d}",
			  curr->name,
			  curr->randr_id,
			  curr->rectangle.x,
			  curr->rectangle.y,
			  curr->rectangle.width,
			  curr->rectangle.height);
		curr = curr->next;
	}
}

static monitor_t *
get_monitor_by_randr_id(xcb_randr_output_t id)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (curr->randr_id == id) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}

static monitor_t *
get_monitor_by_root_id(xcb_window_t id)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (curr->root == id) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}

static monitor_t *
get_focused_monitor(void)
{
	xcb_query_pointer_cookie_t pc =
		xcb_query_pointer(wm->connection, wm->root_window);
	xcb_query_pointer_reply_t *pr =
		xcb_query_pointer_reply(wm->connection, pc, NULL);

	if (pr == NULL) {
		_LOG_(ERROR, "failed to query pointer");
		return NULL;
	}

	int		   px	= pr->root_x;
	int		   py	= pr->root_y;

	monitor_t *curr = head_monitor;
	while (curr) {
		if (px >= curr->rectangle.x &&
			px < (curr->rectangle.x + curr->rectangle.width) &&
			py >= curr->rectangle.y &&
			py < (curr->rectangle.y + curr->rectangle.height)) {
			_FREE_(pr);
			return curr;
		}
		curr = curr->next;
	}

	_FREE_(pr);
	return NULL;
}

static int
get_connected_monitor_count_xinerama(void)
{
	xcb_xinerama_query_screens_cookie_t c =
		xcb_xinerama_query_screens(wm->connection);
	xcb_xinerama_query_screens_reply_t *xquery =
		xcb_xinerama_query_screens_reply(wm->connection, c, NULL);
	int len = xcb_xinerama_query_screens_screen_info_length(xquery);
	_FREE_(xquery);
	return len;
}

static int
get_connected_monitor_count_xrandr(void)
{
	xcb_randr_get_screen_resources_current_cookie_t c =
		xcb_randr_get_screen_resources_current(wm->connection, wm->root_window);
	xcb_randr_get_screen_resources_current_reply_t *sres =
		xcb_randr_get_screen_resources_current_reply(wm->connection, c, NULL);
	if (sres == NULL) {
		_LOG_(ERROR, "failed to get screen resources");
		return -1;
	}
	int len = xcb_randr_get_screen_resources_current_outputs_length(sres);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(sres);
	int monitor_count = 0;
	for (int i = 0; i < len; i++) {
		xcb_randr_get_output_info_cookie_t info_c = xcb_randr_get_output_info(
			wm->connection, outputs[i], XCB_CURRENT_TIME);
		xcb_randr_get_output_info_reply_t *info =
			xcb_randr_get_output_info_reply(wm->connection, info_c, NULL);
		if (info) {
			if (info->connection == XCB_RANDR_CONNECTION_CONNECTED) {
				monitor_count++;
			}
			_FREE_(info);
		}
	}
	_FREE_(sres);
	return monitor_count;
}

static int
get_connected_monitor_count(bool xrandr, bool xinerama)
{
	int n = 0;
	if (xrandr == true && xinerama == false) {
		n = get_connected_monitor_count_xrandr();
	} else if (xrandr == false && xinerama == true) {
		n = get_connected_monitor_count_xinerama();
	} else if (xrandr == true && xinerama == true) {
		_LOG_(WARNING, "huh?...");
	} else {
		n = 1;
	}
	return n;
}

static bool
setup_monitors_via_xrandr(void)
{
	xcb_connection_t							   *conn = wm->connection;
	xcb_window_t									root = wm->root_window;
	xcb_randr_get_screen_resources_current_cookie_t sc =
		xcb_randr_get_screen_resources_current(conn, root);
	xcb_randr_get_screen_resources_current_reply_t *sr =
		xcb_randr_get_screen_resources_current_reply(conn, sc, NULL);
	if (sr == NULL) {
		_LOG_(ERROR, "failed to query screen resources");
		return false;
	}
	const xcb_timestamp_t time = sr->config_timestamp;
	const int len = xcb_randr_get_screen_resources_current_outputs_length(sr);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(sr);
	xcb_randr_get_output_info_cookie_t oc[len];
	for (int i = 0; i < len; i++) {
		oc[i] = xcb_randr_get_output_info(conn, outputs[i], time);
	}
	int monitors = 0;
	for (int i = 0; i < len; i++) {
		xcb_randr_get_output_info_reply_t *info;
		if ((info = xcb_randr_get_output_info_reply(conn, oc[i], NULL)) ==
			NULL) {
			_LOG_(INFO, "could not query output info... skipping this output");
			continue;
		}
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			_LOG_(INFO, "output is disconnected... skipping this output");
			_FREE_(info);
			continue;
		}
		if (info->crtc == XCB_NONE) {
			_LOG_(INFO, "output crtc is empty... skipping this output");
			_FREE_(info);
			continue;
		}
		xcb_randr_get_crtc_info_cookie_t ic;
		xcb_randr_get_crtc_info_reply_t *crtc;
		ic = xcb_randr_get_crtc_info(conn, info->crtc, time);
		if ((crtc = xcb_randr_get_crtc_info_reply(conn, ic, NULL)) == NULL) {
			_LOG_(INFO,
				  "could not get CRTC (0x%08x)... skipping output",
				  info->crtc);
			_FREE_(info);
			continue;
		}
		char	  *name		= (char *)xcb_randr_get_output_info_name(info);
		size_t	   name_len = xcb_randr_get_output_info_name_length(info);
		monitor_t *m		= init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			_FREE_(info);
			_FREE_(crtc);
			_FREE_(sr);
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), "%.*s", (int)name_len, name);
		m->rectangle   = (rectangle_t){.x	   = crtc->x,
									   .y	   = crtc->y,
									   .width  = crtc->width,
									   .height = crtc->height};
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = outputs[i];
		m->next		   = NULL;
		m->desktops	   = NULL;
		add_monitor(&head_monitor, m);
		_LOG_(INFO,
			  "monitor name = %.*s:%d, out %d Monitor rectangle = x = %d, y = "
			  "%d, w = %d, h = %d",
			  (int)name_len,
			  name,
			  m->randr_id,
			  outputs[i],
			  crtc->x,
			  crtc->y,
			  crtc->width,
			  crtc->height);
		monitors++;
		_FREE_(crtc);
		_FREE_(info);
	}
	_FREE_(sr);
	_LOG_(INFO, "%d connected monitors", monitors);
	return true;
}

static bool
setup_monitors_via_xinerama(void)
{
	xcb_xinerama_query_screens_cookie_t query_screens_c =
		xcb_xinerama_query_screens(wm->connection);
	xcb_xinerama_query_screens_reply_t *query_screens_r =
		xcb_xinerama_query_screens_reply(wm->connection, query_screens_c, NULL);
	if (query_screens_r == NULL) {
		_LOG_(ERROR, "failed to query Xinerama screens");
		return false;
	}
	xcb_xinerama_screen_info_t *xinerama_screen_i =
		xcb_xinerama_query_screens_screen_info(query_screens_r);
	int n = xcb_xinerama_query_screens_screen_info_length(query_screens_r);
	for (int i = 0; i < n; i++) {
		xcb_xinerama_screen_info_t info = xinerama_screen_i[i];
		rectangle_t				   r =
			(rectangle_t){info.x_org, info.y_org, info.width, info.height};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			_FREE_(query_screens_r);
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), "Xinerama %d", i);
		m->rectangle   = r;
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = 0;
		add_monitor(&head_monitor, m);
	}

	_FREE_(query_screens_r);
	return true;
}

static void
free_monitors(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		monitor_t *next = curr->next;
		for (int j = 0; j < curr->n_of_desktops; j++) {
			if (curr->desktops[j]) {
				if (curr->desktops[j]->tree) {
					free_tree(curr->desktops[j]->tree);
					curr->desktops[j]->tree = NULL;
				}
				_FREE_(curr->desktops[j]);
			}
		}
		_FREE_(curr->desktops);
		_FREE_(curr);
		curr = next;
	}
	head_monitor = NULL;
}

static int
get_monitors_count(void)
{
	monitor_t *curr = head_monitor;
	int		   n	= 0;
	while (curr) {
		n++;
		curr = curr->next;
	}
	return n;
}

static bool
setup_monitors(void)
{
	bool							   use_global_screen = false;
	const xcb_query_extension_reply_t *query_xr			 = NULL;
	const xcb_query_extension_reply_t *query_x			 = NULL;

	query_xr = xcb_get_extension_data(wm->connection, &xcb_randr_id);
	query_x	 = xcb_get_extension_data(wm->connection, &xcb_xinerama_id);

	if (query_xr->present) {
		using_xrandr = true;
		randr_base	 = query_xr->first_event;
		xcb_randr_select_input(wm->connection,
							   wm->root_window,
							   XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	} else if (query_x->present) {
		bool							xinerama_is_active = false;
		xcb_xinerama_is_active_cookie_t xc =
			xcb_xinerama_is_active(wm->connection);
		xcb_xinerama_is_active_reply_t *xis_active =
			xcb_xinerama_is_active_reply(wm->connection, xc, NULL);
		if (xis_active) {
			xinerama_is_active = xis_active->state;
			_FREE_(xis_active);
			using_xinerama = xinerama_is_active;
		}
	} else {
		using_xrandr = using_xinerama = false;
	}

	int n = get_connected_monitor_count(using_xrandr, using_xinerama);

	if (!using_xrandr && !using_xinerama && n == 1) {
		_LOG_(ERROR, "neither Xrandr nor Xinerama extensions are available");
		use_global_screen = true;
	}

	if (use_global_screen) {
		rectangle_t r = (rectangle_t){
			0, 0, wm->screen->width_in_pixels, wm->screen->height_in_pixels};
		monitor_t *m = init_monitor();
		if (m == NULL) {
			_LOG_(ERROR, "failed to allocate single monitor");
			return false;
		}
		memset(m->name, 0, sizeof(m->name));
		snprintf(m->name, sizeof(m->name), ROOT_WINDOW);
		m->rectangle   = r;
		m->root		   = wm->root_window;
		m->is_focused  = false;
		m->is_occupied = false;
		m->is_wired	   = false;
		m->randr_id	   = 0;
		m->desktops	   = NULL;
		add_monitor(&head_monitor, m);
		prim_monitor = curr_monitor = m;
		goto out;
	}

	bool setup_success = false;
	if (using_xrandr) {
		setup_success = setup_monitors_via_xrandr();
		if (setup_success) {
			_LOG_(INFO, "monitors successfully set up using Xrandr");
		}
	} else if (using_xinerama) {
		setup_success = setup_monitors_via_xinerama();
		if (setup_success) {
			_LOG_(INFO, "monitors successfully set up using Xinerama");
		}
	}

	if (!setup_success) {
		_LOG_(ERROR, "failed to set up monitors, defaulting to global screen");
		return false;
	}

	xcb_randr_get_output_primary_cookie_t ccc =
		xcb_randr_get_output_primary(wm->connection, wm->root_window);
	xcb_randr_get_output_primary_reply_t *primary_output_reply =
		xcb_randr_get_output_primary_reply(wm->connection, ccc, NULL);
	if (primary_output_reply) {
		monitor_t *mm = get_monitor_by_randr_id(primary_output_reply->output);
		if (mm) {
			mm->is_primary = true;
			prim_monitor = curr_monitor = mm;
		} else {
			prim_monitor = curr_monitor = head_monitor;
		}
	} else {
		prim_monitor = curr_monitor = head_monitor;
	}

	_LOG_(INFO,
		  "primary monitor %s:%d id %d, rect = x %d, y %d,width %d,height %d",
		  prim_monitor->name,
		  prim_monitor->randr_id,
		  prim_monitor->root,
		  prim_monitor->rectangle.x,
		  prim_monitor->rectangle.y,
		  prim_monitor->rectangle.width,
		  prim_monitor->rectangle.height);

	_FREE_(primary_output_reply);

out:
	multi_monitors = (get_monitors_count() > 1);
	_LOG_(INFO, "multi monitors = %s", multi_monitors ? "true" : "false");
	xcb_flush(wm->connection);
	return true;
}

static bool
handle_added_monitor(xcb_randr_get_output_info_reply_t *info,
					 xcb_randr_output_t					id)
{
	xcb_randr_get_crtc_info_cookie_t crtc_c =
		xcb_randr_get_crtc_info(wm->connection, info->crtc, XCB_CURRENT_TIME);
	xcb_randr_get_crtc_info_reply_t *crtc =
		xcb_randr_get_crtc_info_reply(wm->connection, crtc_c, NULL);
	if (!crtc) {
		_LOG_(ERROR, "failed to query crtc for %d", id);
		return false;
	}
	char	  *name		= (char *)xcb_randr_get_output_info_name(info);
	size_t	   name_len = xcb_randr_get_output_info_name_length(info);
	monitor_t *m		= init_monitor();
	if (!m) {
		_LOG_(ERROR, "failed to allocate single monitor for output %d", id);
		_FREE_(crtc);
		return false;
	}
	memset(m->name, 0, sizeof(m->name));
	snprintf(m->name, sizeof(m->name), "%.*s", (int)name_len, name);
	m->rectangle   = (rectangle_t){.x	   = crtc->x,
								   .y	   = crtc->y,
								   .width  = crtc->width,
								   .height = crtc->height};
	m->is_focused  = false;
	m->is_occupied = false;
	m->is_wired	   = false;
	m->randr_id	   = id;
	m->next		   = NULL;
	m->desktops	   = NULL;
	add_monitor(&head_monitor, m);
	_LOG_(INFO,
		  "monitor name = %.*s:%d, out %d Monitor rectangle = x = %d, y = %d, "
		  "w = %d, h = %d was ADDED",
		  (int)name_len,
		  name,
		  m->randr_id,
		  id,
		  crtc->x,
		  crtc->y,
		  crtc->width,
		  crtc->height);
	_FREE_(crtc);
	return true;
}

static void
destroy_monitor(monitor_t *m)
{
	if (!m) {
		_LOG_(ERROR, "attempted to destroy a NULL monitor.");
		return;
	}
	unlink_monitor(&head_monitor, m);
	assert(!get_monitor_by_randr_id(m->randr_id));

	_LOG_(INFO, "destroying monitor %s", m->name);
	for (int i = 0; i < m->n_of_desktops; i++) {
		desktop_t *desktop = m->desktops[i];
		if (!desktop) {
			continue;
		}
		if (desktop->tree) {
			free_tree(desktop->tree);
			desktop->tree = NULL;
		}
		_FREE_(desktop);
	}
	_FREE_(m->desktops);
	_FREE_(m);
	_LOG_(INFO, "monitor was destroyed.");
}

static bool
is_monitor_layout_changed(xcb_randr_get_output_info_reply_t *info,
						  rectangle_t						*r,
						  rectangle_t						*r_out)
{
	xcb_randr_get_crtc_info_cookie_t crtc_c =
		xcb_randr_get_crtc_info(wm->connection, info->crtc, XCB_CURRENT_TIME);
	xcb_randr_get_crtc_info_reply_t *crtc =
		xcb_randr_get_crtc_info_reply(wm->connection, crtc_c, NULL);
	if (!crtc) {
		_LOG_(ERROR, "failed to query crtc for");
		return false;
	}
	*r_out = (rectangle_t){.x	   = crtc->x,
						   .y	   = crtc->y,
						   .width  = crtc->width,
						   .height = crtc->height};
	_FREE_(crtc);
	return (r->x != r_out->x || r->y != r_out->y || r->width != r_out->width ||
			r->height != r_out->height);
}

static bool
merge_monitors(monitor_t *om, monitor_t *nm)
{
	assert(om->n_of_desktops == nm->n_of_desktops);

	for (int i = 0; i < om->n_of_desktops; i++) {
		desktop_t *od = om->desktops[i];
		desktop_t *nd = nm->desktops[i];

		if (!od->tree) {
			continue;
		}

		queue_t *q = create_queue();
		if (!q)
			return false;

		enqueue(q, od->tree);
		while (!is_queue_empty(q)) {
			node_t *node = dequeue(q);
			if (!IS_INTERNAL(node) && node->client) {
				if (!unlink_node(node, od)) {
					_LOG_(ERROR, "failed to unlink node.... abort");
					_FREE_(q);
					return false;
				}

				if (!transfer_node(node, nd)) {
					_LOG_(ERROR, "failed to transfer node... abort");
					_FREE_(q);
					return false;
				}
				node->client->mru_seq = get_next_mru_seq(nm);
			}

			if (node->first_child) {
				enqueue(q, node->first_child);
			}
			if (node->second_child) {
				enqueue(q, node->second_child);
			}
		}
		free_queue(q);
		assert(!od->tree);
		arrange_tree(nd->tree, nd->layout);
	}
	return true;
}

static bool
is_disconnected(monitor_t *m, monitor_t *dl)
{
	for (monitor_t *d = dl; d; d = d->next) {
		if (d == m)
			return true;
	}
	return false;
}

static void
update_monitors(uint32_t *changes)
{
	monitor_t									   *dl		  = NULL;
	xcb_connection_t							   *conn	  = wm->connection;
	xcb_randr_get_screen_resources_current_cookie_t rc		  = {0};
	xcb_randr_get_screen_resources_current_reply_t *resources = NULL;

	rc		  = xcb_randr_get_screen_resources_current(conn, wm->root_window);
	resources = xcb_randr_get_screen_resources_current_reply(conn, rc, NULL);

	if (!resources) {
		_LOG_(ERROR, "failed to get screen resources");
		return;
	}

	int len = xcb_randr_get_screen_resources_current_outputs_length(resources);
	xcb_randr_output_t *outputs =
		xcb_randr_get_screen_resources_current_outputs(resources);
	int								   monitor_count = 0;
	xcb_randr_get_output_info_cookie_t ic			 = {0};
	xcb_randr_get_output_info_reply_t *info			 = NULL;
	for (int i = 0; i < len; i++) {
		ic	 = xcb_randr_get_output_info(conn, outputs[i], XCB_CURRENT_TIME);
		info = xcb_randr_get_output_info_reply(conn, ic, NULL);
		if (!info)
			continue;
		if (info->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				_FREE_(info);
				continue;
			}
			exist->next = dl;
			dl			= exist;
		}
		if (info->crtc == XCB_NONE) {
			_FREE_(info);
			continue;
		}
		if (info->connection == XCB_RANDR_CONNECTION_CONNECTED) {
			monitor_t *exist = get_monitor_by_randr_id(outputs[i]);
			if (!exist) {
				if (!handle_added_monitor(info, outputs[i])) {
					_LOG_(ERROR, "failed to add new output %d", outputs[i]);
					_FREE_(info);
					continue;
				}
				monitor_count++;
				*changes &= ~_NONE;
				*changes |= CONNECTED;
			} else {
				rectangle_t r = {0};
				if (is_monitor_layout_changed(info, &exist->rectangle, &r)) {
					exist->rectangle = r;
					*changes &= ~_NONE;
					*changes |= LAYOUT;
				}
			}
		}
		_FREE_(info);
	}
	_FREE_(resources);
	if (dl) {
		monitor_t *m = NULL;
		if (prim_monitor && !is_disconnected(prim_monitor, dl)) {
			m = prim_monitor;
		} else {
			for (monitor_t *cur = head_monitor; cur; cur = cur->next) {
				if (!is_disconnected(cur, dl)) {
					m = cur;
					break;
				}
			}
		}

		if (!m) {
			_LOG_(ERROR, "no monitor found to merge with");
			return;
		}
		while (dl) {
			monitor_t *r = dl;
			dl			 = dl->next;
			_LOG_(INFO, "merging desktops from %s to %s", r->name, m->name);
			if (!merge_monitors(r, m)) {
				_LOG_(ERROR, "failed to merge desktops from %s", r->name);
				continue;
			}
			destroy_monitor(r);
		}
		*changes &= ~_NONE;
		*changes |= DISCONNECTED;
	}
	_LOG_(INFO, "%d newly connected monitor", monitor_count);
}

static void
handle_monitor_changes(void)
{
	if (using_xinerama) {
		return;
	}

	uint32_t m_change = 0 | _NONE;
	bool	 render	  = false;
	update_monitors(&m_change);

	if (m_change & _NONE) {
		_LOG_(INFO, "no monitor changes was found");
		return;
	}
	if (m_change & CONNECTED) {
		_LOG_(INFO, "a monitor was connected");
		setup_desktops();
	} else if (m_change & DISCONNECTED) {
		_LOG_(INFO, "a monitor was disconnected");
		curr_monitor = prim_monitor = head_monitor;
		render						= true;
	} else if (m_change & LAYOUT) {
		_LOG_(INFO, "a monitor's layout was changed");
		render = true;
	}

	if (render) {
		if (!ignore_ewmh_struts) {
			recalculate_all_struts();
		} else {
			arrange_trees();
			render_trees();
		}
	}

	log_monitors();

	multi_monitors = (get_monitors_count() > 1);

	_LOG_(INFO,
		  "in update: multi monitors = %s",
		  multi_monitors ? "true" : "false");
}

static monitor_t *
get_monitor_within_coordinate(int16_t x, int16_t y)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		if (x >= curr->rectangle.x &&
			x < (curr->rectangle.x + curr->rectangle.width) &&
			y >= curr->rectangle.y &&
			y < (curr->rectangle.y + curr->rectangle.height)) {
			return curr;
		}
		curr = curr->next;
	}
	return NULL;
}

static monitor_t *
get_monitor_from_desktop(desktop_t *desktop)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int j = 0; j < curr->n_of_desktops; j++) {
			if (curr->desktops[j] == desktop) {
				return curr;
			}
		}
		curr = curr->next;
	}
	return NULL;
}

static monitor_t *
get_monitor_by_window(xcb_window_t win)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		for (int i = 0; i < curr->n_of_desktops; i++) {
			node_t *node = find_node_by_window_id(curr->desktops[i]->tree, win);
			if (node) {
				return curr;
			}
		}
		curr = curr->next;
	}
	return NULL;
}

static rectangle_t
calculate_monitor_area(const monitor_t *m)
{
	rectangle_t usable = get_usable_area((monitor_t *)m);
	return (rectangle_t){
		.x		= (int16_t)(usable.x + conf.window_gap),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(usable.width - 2 * conf.window_gap -
							 2 * conf.border_width),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap -
							 2 * conf.border_width)};
}

static void
apply_monitor_layout_changes(monitor_t *m)
{
	for (int d = 0; d < m->n_of_desktops; ++d) {
		if (!m->desktops[d] || is_tree_empty(m->desktops[d]->tree))
			continue;

		layout_t l	  = m->desktops[d]->layout;
		node_t	*tree = m->desktops[d]->tree;

		if (l == DEFAULT || l == STACK || l == GRID) {
			tree->rectangle = calculate_monitor_area(m);

			if (l == DEFAULT)
				apply_default_layout(tree);
			else if (l == STACK)
				apply_stack_layout(tree);
			else if (l == GRID)
				apply_grid_layout(tree);

		} else if (l == MASTER) {
			node_t *mn = find_master_node(tree);
			if (!mn && !(mn = find_any_leaf(tree)))
				return;

			mn->is_master			  = true;
			const double r			  = MASTER_RATIO;
			rectangle_t	 u			  = get_usable_area(m);
			uint16_t	 master_width = (uint16_t)(u.width * r);
			uint16_t	 r_width	  = (uint16_t)(u.width * (1 - r));
			rectangle_t	 r1			  = {
				.x		= (int16_t)(u.x + conf.window_gap),
				.y		= (int16_t)(u.y + conf.window_gap),
				.width	= (uint16_t)(master_width - 2 * conf.window_gap),
				.height = (uint16_t)(u.height - 2 * conf.window_gap),
			};
			rectangle_t r2 = {
				.x		= (int16_t)(u.x + master_width),
				.y		= (int16_t)(u.y + conf.window_gap),
				.width	= (uint16_t)(r_width - conf.window_gap),
				.height = (uint16_t)(u.height - 2 * conf.window_gap),
			};
			mn->rectangle	= r1;
			tree->rectangle = r2;
			apply_master_layout(tree);
		}
	}
}

static void
arrange_trees(void)
{
	monitor_t *curr = head_monitor;
	while (curr) {
		apply_monitor_layout_changes(curr);
		curr = curr->next;
	}
}

/* ./src/mouse.c */



static node_t *
mouse_tree_root(node_t *n)
{
	if (!n)
		return NULL;
	while (n->parent) n = n->parent;
	return n;
}

static void
window_grab_buttons(xcb_window_t win)
{
#define _GRAB_BUTTON_(button)                                                  \
	do {                                                                       \
		xcb_grab_button_checked(wm->connection,                                \
								false,                                         \
								win,                                           \
								XCB_EVENT_MASK_BUTTON_PRESS,                   \
								XCB_GRAB_MODE_ASYNC,                           \
								XCB_GRAB_MODE_ASYNC,                           \
								wm->root_window,                               \
								XCB_NONE,                                      \
								button,                                        \
								XCB_MOD_MASK_ANY);                             \
	} while (0)
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_1);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_2);
	_GRAB_BUTTON_(XCB_BUTTON_INDEX_3);
#undef _GRAB_BUTTON_
}

static void
window_ungrab_buttons(xcb_window_t win)
{
	xcb_cookie_t c = xcb_ungrab_button_checked(
		wm->connection, XCB_BUTTON_INDEX_ANY, win, XCB_MOD_MASK_ANY);

	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in ungrab buttons for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return;
	}
}

static void
ungrab_buttons_for_all(node_t *n)
{
	if (n == NULL)
		return;

	bool flag = n->node_type != INTERNAL_NODE && n->client;

	if (flag) {
		xcb_ungrab_button(wm->connection,
						  XCB_BUTTON_INDEX_ANY,
						  n->client->window,
						  XCB_MOD_MASK_ANY);
	}

	ungrab_buttons_for_all(n->first_child);
	ungrab_buttons_for_all(n->second_child);
}

static bool
grab_pointer_for_mouse(cursor_t cursor_id)
{
	xcb_grab_pointer_reply_t *r;
	xcb_grab_pointer_cookie_t c = xcb_grab_pointer(
		wm->connection,
		false,			 /* owner_events */
		wm->root_window, /* grab_window */
		XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
		XCB_GRAB_MODE_ASYNC, /* pointer_mode */
		XCB_GRAB_MODE_ASYNC, /* keyboard_mode */
		XCB_NONE,			 /* confine_to */
		get_cursor(cursor_id),
		XCB_CURRENT_TIME);

	r = xcb_grab_pointer_reply(wm->connection, c, NULL);
	if (!r) {
		return false;
	}
	bool ok = (r->status == XCB_GRAB_STATUS_SUCCESS);
	_FREE_(r);
	return ok;
}

static void
clear_mouse_state(void)
{
	ms = (mouse_state_t){0};
}

static double
clamp_ratio(double r)
{
	const double m = 0.05;
	if (r < m) {
		return m;
	}
	if (r > (1.0 - m)) {
		return 1.0 - m;
	}
	return r;
}

static uint8_t
detect_resize_edges(rectangle_t r, int16_t x, int16_t y)
{
	const int16_t edge	   = 10;
	uint8_t		  edges	   = 0;
	int16_t		  left_d   = (int16_t)(x - r.x);
	int16_t		  right_d  = (int16_t)((r.x + (int16_t)r.width) - x);
	int16_t		  top_d	   = (int16_t)(y - r.y);
	int16_t		  bottom_d = (int16_t)((r.y + (int16_t)r.height) - y);

	if (left_d >= 0 && left_d <= edge) {
		edges |= RESIZE_EDGE_LEFT;
	}
	if (right_d >= 0 && right_d <= edge) {
		edges |= RESIZE_EDGE_RIGHT;
	}
	if (top_d >= 0 && top_d <= edge) {
		edges |= RESIZE_EDGE_TOP;
	}
	if (bottom_d >= 0 && bottom_d <= edge) {
		edges |= RESIZE_EDGE_BOTTOM;
	}

	if ((edges & (RESIZE_EDGE_LEFT | RESIZE_EDGE_RIGHT)) ==
		(RESIZE_EDGE_LEFT | RESIZE_EDGE_RIGHT)) {
		edges &= (left_d <= right_d) ? ~RESIZE_EDGE_RIGHT : ~RESIZE_EDGE_LEFT;
	}
	if ((edges & (RESIZE_EDGE_TOP | RESIZE_EDGE_BOTTOM)) ==
		(RESIZE_EDGE_TOP | RESIZE_EDGE_BOTTOM)) {
		edges &= (top_d <= bottom_d) ? ~RESIZE_EDGE_BOTTOM : ~RESIZE_EDGE_TOP;
	}

	return edges;
}

static bool
is_resize_band_hit(node_t *p, split_type_t split_type, int16_t x, int16_t y)
{
	if (!p || !p->first_child || !p->second_child) {
		return false;
	}

	const int16_t edge = 8;
	if (split_type == HORIZONTAL_TYPE) {
		rectangle_t a	   = p->first_child->rectangle;
		rectangle_t b	   = p->second_child->rectangle;
		bool		a_left = (a.x <= b.x);
		/* left edge */
		int16_t		le	  = (int16_t)((a_left ? a.x + a.width : b.x + b.width));
		/* rigt edge */
		int16_t		re	  = (int16_t)(a_left ? b.x : a.x);
		int16_t		min_x = (le < re) ? le : re;
		int16_t		max_x = (le > re) ? le : re;
		return (x >= (min_x - edge) && x <= (max_x + edge));
	}
	if (split_type == VERTICAL_TYPE) {
		rectangle_t a	  = p->first_child->rectangle;
		rectangle_t b	  = p->second_child->rectangle;
		bool		a_top = (a.y <= b.y);
		/* top edge */
		int16_t		te = (int16_t)((a_top ? a.y + a.height : b.y + b.height));
		/* bottom edge */
		int16_t		be = (int16_t)(a_top ? b.y : a.y);
		int16_t		min_y = (te < be) ? te : be;
		int16_t		max_y = (te > be) ? te : be;
		return (y >= (min_y - edge) && y <= (max_y + edge));
	}

	return false;
}

static bool
start_floating_move(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || !IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}

	ms.op				 = MOUSE_OP_MOVE_FLOATING;
	ms.node				 = n;
	ms.window			 = n->client->window;
	ms.start_x			 = x;
	ms.start_y			 = y;
	ms.start_rect		 = n->floating_rectangle;
	ms.edges			 = 0;

	const uint32_t val[] = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, n->client->window, XCB_CONFIG_WINDOW_STACK_MODE, val);

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

static bool
start_floating_resize(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || !IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}

	uint8_t edges = detect_resize_edges(n->floating_rectangle, x, y);
	if (edges == 0) {
		return false;
	}

	ms.op				 = MOUSE_OP_RESIZE_FLOATING;
	ms.node				 = n;
	ms.window			 = n->client->window;
	ms.start_x			 = x;
	ms.start_y			 = y;
	ms.start_rect		 = n->floating_rectangle;
	ms.edges			 = edges;

	const uint32_t val[] = {XCB_STACK_MODE_ABOVE};
	xcb_configure_window(
		wm->connection, n->client->window, XCB_CONFIG_WINDOW_STACK_MODE, val);

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

static bool
start_tiled_resize(node_t *n, int16_t x, int16_t y)
{
	if (!n || !n->client || n->parent == NULL || IS_FLOATING(n->client) ||
		IS_FULLSCREEN(n->client)) {
		return false;
	}
	if (curr_monitor->desk->layout == DECK ||
		curr_monitor->desk->layout == THREE_COL) {
		node_t *root = mouse_tree_root(n);
		if (!root)
			return false;

		const int16_t edge = 10;
		const int16_t gap  = (int16_t)conf.window_gap;
		const int16_t bw   = (int16_t)conf.border_width;
		const double  r = (root->split_ratio <= 0.0 || root->split_ratio >= 1.0)
							  ? 0.5
							  : clamp_ratio(root->split_ratio);
		const int16_t mw  = (int16_t)(root->rectangle.width * r - gap - 2 * bw);
		uint8_t		  hit = RESIZE_EDGE_RIGHT;

		if (curr_monitor->desk->layout == THREE_COL) {
			const int16_t cw = mw;
			const int16_t side_total =
				(int16_t)(root->rectangle.width - cw - 2 * (gap + bw));
			const int16_t sw = (int16_t)(side_total / 2);
			/* left edge */
			const int16_t le = (int16_t)(root->rectangle.x + sw);
			/* right edge */
			const int16_t re = (int16_t)(le + gap + bw + cw);
			if (x >= le - edge && x <= le + edge) {
				hit = RESIZE_EDGE_LEFT;
			} else if (x >= re - edge && x <= re + edge) {
				hit = RESIZE_EDGE_RIGHT;
			} else {
				return false;
			}
		} else {
			/* master edge */
			const int16_t me = (int16_t)(root->rectangle.x + mw);
			/* deck edge */
			const int16_t de = (int16_t)(root->rectangle.x + mw + gap + bw);
			if (!((x >= me - edge && x <= me + edge) ||
				  (x >= de - edge && x <= de + edge)))
				return false;
		}

		ms.op		   = MOUSE_OP_RESIZE_TILED;
		ms.node		   = n;
		ms.parent	   = root;
		ms.start_x	   = x;
		ms.start_y	   = y;
		ms.split_type  = HORIZONTAL_TYPE;
		ms.start_ratio = r;
		ms.first_size  = (int16_t)(root->rectangle.width * r);
		ms.avail	   = root->rectangle.width;
		ms.edges	   = hit;

		if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
			clear_mouse_state();
			return false;
		}

		return true;
	}
	if (curr_monitor->desk->layout != DEFAULT) {
		return false;
	}

	node_t *p = n->parent;
	node_t *s = (p->first_child == n) ? p->second_child : p->first_child;
	if (!s) {
		return false;
	}

	bool vs = (n->rectangle.x == s->rectangle.x);
	bool hs = (n->rectangle.y == s->rectangle.y);
	if (!vs && !hs) {
		return false;
	}

	split_type_t st = hs ? HORIZONTAL_TYPE : VERTICAL_TYPE;
	if (!is_resize_band_hit(p, st, x, y)) {
		return false;
	}

	const int16_t gap	= conf.window_gap - conf.border_width;
	const int16_t avail = (st == HORIZONTAL_TYPE) ? (p->rectangle.width - gap)
												  : (p->rectangle.height - gap);
	if (avail <= 0) {
		return false;
	}

	const int16_t fz = (st == HORIZONTAL_TYPE)
						   ? p->first_child->rectangle.width
						   : p->first_child->rectangle.height;
	const double  r	 = (double)fz / (double)avail;

	ms.op			 = MOUSE_OP_RESIZE_TILED;
	ms.node			 = n;
	ms.parent		 = p;
	ms.start_x		 = x;
	ms.start_y		 = y;
	ms.split_type	 = st;
	ms.start_ratio	 = clamp_ratio(r);
	ms.first_size	 = fz;
	ms.avail		 = avail;
	ms.edges		 = 0;

	if (!grab_pointer_for_mouse(CURSOR_MOVE)) {
		clear_mouse_state();
		return false;
	}

	return true;
}

static void
handle_mouse_motion(int16_t x, int16_t y)
{
	if (ms.op == MOUSE_OP_MOVE_FLOATING) {
		int16_t		dx				= (int16_t)(x - ms.start_x);
		int16_t		dy				= (int16_t)(y - ms.start_y);
		rectangle_t r				= ms.start_rect;
		r.x							= (int16_t)(r.x + dx);
		r.y							= (int16_t)(r.y + dy);
		ms.node->floating_rectangle = r;
		apply_window_geometry(ms.window, r, conf.border_width);
		return;
	}

	if (ms.op == MOUSE_OP_RESIZE_FLOATING) {
		const int32_t min_dim = 40;
		int32_t		  dx	  = (int32_t)(x - ms.start_x);
		int32_t		  dy	  = (int32_t)(y - ms.start_y);
		int32_t		  nx	  = ms.start_rect.x;
		int32_t		  ny	  = ms.start_rect.y;
		int32_t		  nw	  = ms.start_rect.width;
		int32_t		  nh	  = ms.start_rect.height;

		if (ms.edges & RESIZE_EDGE_LEFT) {
			nx += dx;
			nw -= dx;
		}
		if (ms.edges & RESIZE_EDGE_RIGHT) {
			nw += dx;
		}
		if (ms.edges & RESIZE_EDGE_TOP) {
			ny += dy;
			nh -= dy;
		}
		if (ms.edges & RESIZE_EDGE_BOTTOM) {
			nh += dy;
		}

		if (nw < min_dim) {
			if (ms.edges & RESIZE_EDGE_LEFT) {
				nx = ms.start_rect.x + (ms.start_rect.width - min_dim);
			}
			nw = min_dim;
		}
		if (nh < min_dim) {
			if (ms.edges & RESIZE_EDGE_TOP) {
				ny = ms.start_rect.y + (ms.start_rect.height - min_dim);
			}
			nh = min_dim;
		}

		rectangle_t r = {
			.x		= (int16_t)nx,
			.y		= (int16_t)ny,
			.width	= (uint16_t)nw,
			.height = (uint16_t)nh,
		};
		ms.node->floating_rectangle = r;
		apply_window_geometry(ms.window, r, conf.border_width);
		return;
	}

	if (ms.op == MOUSE_OP_RESIZE_TILED) {
		int32_t delta = (ms.split_type == HORIZONTAL_TYPE)
							? (int32_t)(x - ms.start_x)
							: (int32_t)(y - ms.start_y);
		if (curr_monitor->desk->layout == THREE_COL &&
			(ms.edges & RESIZE_EDGE_LEFT)) {
			delta = -delta;
		}
		int32_t nf		 = ms.first_size + delta;
		int32_t min_size = 40;
		if (ms.avail < min_size * 2) {
			min_size = ms.avail / 2;
		}
		if (min_size < 1) {
			min_size = 1;
		}
		if (nf < min_size) {
			nf = min_size;
		}
		if (nf > (ms.avail - min_size)) {
			nf = ms.avail - min_size;
		}

		double ratio = (double)nf / (double)ms.avail;
		if (curr_monitor->desk->layout == DECK ||
			curr_monitor->desk->layout == THREE_COL) {
			ms.parent->split_ratio = clamp_ratio(ratio);
			arrange_tree(ms.parent, curr_monitor->desk->layout);
			_render_view_(curr_monitor->desk);
			return;
		}
		ms.parent->split_type  = ms.split_type;
		ms.parent->split_ratio = clamp_ratio(ratio);
		resize_subtree(ms.parent);
		render_tree_nomap(ms.parent);
		return;
	}
}

static void
finish_mouse_action(void)
{
	ungrab_pointer();
	clear_mouse_state();
	xcb_flush(wm->connection);
}

static void
cancel_mouse_action(void)
{
	if (ms.op == MOUSE_OP_MOVE_FLOATING || ms.op == MOUSE_OP_RESIZE_FLOATING) {
		if (ms.node && ms.node->client) {
			ms.node->floating_rectangle = ms.start_rect;
			apply_window_geometry(ms.window, ms.start_rect, conf.border_width);
		}
	} else if (ms.op == MOUSE_OP_RESIZE_TILED) {
		if (ms.parent) {
			ms.parent->split_type  = ms.split_type;
			ms.parent->split_ratio = ms.start_ratio;
			if (curr_monitor->desk->layout == DECK ||
				curr_monitor->desk->layout == THREE_COL) {
				arrange_tree(ms.parent, curr_monitor->desk->layout);
				_render_view_(curr_monitor->desk);
			} else {
				resize_subtree(ms.parent);
				render_tree_nomap(ms.parent);
			}
		}
	}
	ungrab_pointer();
	clear_mouse_state();
	xcb_flush(wm->connection);
}

/* ./src/queue.c */



static queue_t *
create_queue(void)
{
	queue_t *q = (queue_t *)malloc(sizeof(queue_t));
	if (!q)
		return NULL;
	q->front = q->rear = NULL;
	q->size = 0;
	return q;
}

static void
enqueue(queue_t *q, node_t *n)
{
	queue_node_t *nnode = (queue_node_t *)malloc(sizeof(queue_node_t));
	if (!nnode)
		return;
	nnode->tree_node = n;
	nnode->next		 = NULL;
	nnode->prev      = q->rear;
	if (!q->rear) {
		q->front = q->rear = nnode;
	} else {
		q->rear->next = nnode;
		q->rear		  = nnode;
	}
	q->size++;
}

static void
enqueue_front(queue_t *q, node_t *n)
{
	queue_node_t *nnode = (queue_node_t *)malloc(sizeof(queue_node_t));
	if (!nnode)
		return;
	nnode->tree_node = n;
	nnode->next		 = q->front;
	nnode->prev      = NULL;
	if (!q->front) {
		q->front = q->rear = nnode;
	} else {
		q->front->prev = nnode;
		q->front = nnode;
	}
	q->size++;
}

static node_t *
dequeue(queue_t *q)
{
	if (!q->front)
		return NULL;
	queue_node_t *temp = q->front;
	node_t		 *node = temp->tree_node;
	q->front		   = q->front->next;
	if (!q->front)
		q->rear = NULL;
	else
		q->front->prev = NULL;
	free(temp);
	q->size--;
	return node;
}

static node_t *
dequeue_rear(queue_t *q)
{
	if (!q->rear)
		return NULL;
	queue_node_t *temp = q->rear;
	node_t		 *node = temp->tree_node;
	q->rear		       = q->rear->prev;
	if (!q->rear)
		q->front = NULL;
	else
		q->rear->next = NULL;
	free(temp);
	q->size--;
	return node;
}

static node_t *
peek_front(queue_t *q)
{
	if (!q->front) return NULL;
	return q->front->tree_node;
}

static node_t *
peek_rear(queue_t *q)
{
	if (!q->rear) return NULL;
	return q->rear->tree_node;
}

static bool
remove_node(queue_t *q, node_t *n)
{
	queue_node_t *curr = q->front;
	while (curr) {
		if (curr->tree_node == n) {
			if (curr->prev)
				curr->prev->next = curr->next;
			else
				q->front = curr->next;
			
			if (curr->next)
				curr->next->prev = curr->prev;
			else
				q->rear = curr->prev;

			free(curr);
			q->size--;
			return true;
		}
		curr = curr->next;
	}
	return false;
}

static bool
is_queue_empty(queue_t *q)
{
	return q->front == NULL;
}

static size_t
get_queue_size(queue_t *q)
{
	return q->size;
}

static void
free_queue(queue_t *q)
{
	while (q->front) {
		queue_node_t *temp = q->front;
		q->front		   = q->front->next;
		free(temp);
	}
	free(q);
}

/* ./src/stacking.c */



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

static uint64_t
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

static uint32_t
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

static void
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
static void
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

static void
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

/* ./src/state.c */




/* ./src/tree.c */






/* clang-format off */
/* clang-format on */

static node_t *
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
	node->split_type		 = DYNAMIC_TYPE;
	node->split_ratio		 = 0.0;

	return node;
}

static node_t *
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
	node->split_type		 = DYNAMIC_TYPE;
	node->split_ratio		 = 0.0;

	return node;
}

static int
render_tree_internal(node_t *node, bool do_map)
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
			int result =
				IS_FULLSCREEN(current->client)
					? _handle_fullscreen_window(current->client->window)
					: (do_map ? tile(current) : _handle_window_nomap(current));

			if (result != 0) {
				free_queue(q);
				return -1;
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

/* wrapper functions */
static int
render_tree(node_t *node)
{
	return render_tree_internal(node, true);
}

static int
render_tree_nomap(node_t *node)
{
	return render_tree_internal(node, false);
}

static rectangle_t
_get_window_rectangle(node_t *node)
{
	if (IS_FLOATING(node->client)) {
		return node->floating_rectangle;
	}
	return node->rectangle;
}

static int
_handle_fullscreen_window(xcb_window_t win)
{
	monitor_t  *m	  = get_monitor_by_window(win);
	rectangle_t r	  = m ? m->rectangle : curr_monitor->rectangle;
	rectangle_t nudge = r;
	nudge.width -= 1;
	xcb_configure_window(
		wm->connection,
		win,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
			XCB_CONFIG_WINDOW_HEIGHT,
		(uint32_t[]){nudge.x, nudge.y, nudge.width, nudge.height});

	if (apply_window_geometry(win, r, 0) != 0) {
		_LOG_(ERROR, "error resizing/moving fullscreen window %d", win);
		return -1;
	}
	return 0;
}

static int
_handle_window_nomap(node_t *node)
{
	rectangle_t r = _get_window_rectangle(node);

	if (apply_window_geometry(
			node->client->window,
			r,
			IS_FULLSCREEN(node->client) ? 0 : conf.border_width) != 0) {
		_LOG_(ERROR, "error resizing/moving window %d", node->client->window);
		return -1;
	}
	return 0;
}

#if 0
static int
render_tree_internal(node_t *node, bool do_map)
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
			if (IS_FULLSCREEN(current->client)) {
				monitor_t  *m = get_monitor_by_window(current->client->window);
				rectangle_t r = m ? m->rectangle : curr_monitor->rectangle;
				if (resize_window(current->client->window, r.width, r.height) !=
						0 ||
					move_window(current->client->window, r.x, r.y) != 0) {
					_LOG_(ERROR,
						  "error resizing/moving fullscreen window %d",
						  current->client->window);
					free_queue(q);
					return -1;
				}
			} else {
				if (do_map) {
					if (tile(current) != 0) {
						_LOG_(ERROR,
							  "error tiling window %d",
							  current->client->window);
						free_queue(q);
						return -1;
					}
				} else {
					const uint16_t width =
						IS_FLOATING(current->client)
							? current->floating_rectangle.width
							: current->rectangle.width;
					const uint16_t height =
						IS_FLOATING(current->client)
							? current->floating_rectangle.height
							: current->rectangle.height;
					const int16_t x = IS_FLOATING(current->client)
										  ? current->floating_rectangle.x
										  : current->rectangle.x;
					const int16_t y = IS_FLOATING(current->client)
										  ? current->floating_rectangle.y
										  : current->rectangle.y;

					if (resize_window(current->client->window, width, height) !=
							0 ||
						move_window(current->client->window, x, y) != 0) {
						_LOG_(ERROR,
							  "error resizing/moving window %d",
							  current->client->window);
						free_queue(q);
						return -1;
					}
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
#endif

static int
tile(node_t *node)
{
	if (node == NULL || node->client == NULL) {
		return -1;
	}

	const uint16_t width  = IS_FLOATING(node->client)
								? node->floating_rectangle.width
								: node->rectangle.width;
	const uint16_t height = IS_FLOATING(node->client)
								? node->floating_rectangle.height
								: node->rectangle.height;
	const int16_t  x = IS_FLOATING(node->client) ? node->floating_rectangle.x
												 : node->rectangle.x;
	const int16_t  y = IS_FLOATING(node->client) ? node->floating_rectangle.y
												 : node->rectangle.y;

	rectangle_t	   r = {.x = x, .y = y, .width = width, .height = height};
	if (apply_window_geometry(
			node->client->window,
			r,
			IS_FULLSCREEN(node->client) ? 0 : conf.border_width) != 0) {
		return -1;
	}

	xcb_cookie_t cookie =
		xcb_map_window_checked(wm->connection, node->client->window);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "in mapping window %d: error code %d",
			  node->client->window,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	xcb_flush(wm->connection);
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

/* split a leaf into an internal node.
 * old client becomes first child, new client becomes second child */
static void
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

	/* root keeps ROOT_NODE type, other leaves become internal split nodes */
	if (!IS_ROOT(node))
		node->node_type = INTERNAL_NODE;

	/* floating client must keep its own saved rect after the split */
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
		/* todo */
		node_t *p = find_tree_root(node);
		node_t *m = find_master_node(p);
		master_layout(p, m);
	} else if (layout == GRID) {
		node_t *p = find_tree_root(node);
		grid_layout(p);
	} else if (layout == MONOCLE) {
		node_t *p = find_tree_root(node);
		monocle_layout(p);
	} else if (layout == THREE_COL) {
		node_t *p = find_tree_root(node);
		three_col_layout(p);
	} else if (layout == DECK) {
		node_t *p = find_tree_root(node);
		deck_layout(p);
	}
}

/* clone_tree creates a deep copy of a tree */
static node_t *
clone_tree(node_t *r, node_t *p)
{
	if (!r)
		return NULL;

	node_t *n = (node_t *)calloc(1, sizeof(node_t));
	if (!n)
		return NULL;

	n->parent			  = p;
	n->node_type		  = r->node_type;
	n->is_focused		  = r->is_focused;
	n->is_master		  = r->is_master;
	n->split_type		  = r->split_type;
	n->split_ratio		  = r->split_ratio;
	n->rectangle		  = r->rectangle;
	n->floating_rectangle = r->floating_rectangle;

	if (r->client) {
		client_t *c = (client_t *)malloc(sizeof(client_t));
		if (!c) {
			_FREE_(n);
			return NULL;
		}
		*c		  = *r->client;
		n->client = c;
	}

	if (r->first_child) {
		n->first_child = clone_tree(r->first_child, n);
		if (!n->first_child) {
			free_tree(n);
			return NULL;
		}
	}
	if (r->second_child) {
		n->second_child = clone_tree(r->second_child, n);
		if (!n->second_child) {
			free_tree(n);
			return NULL;
		}
	}

	return n;
}

static node_t *
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

static void
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

static node_t *
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

static node_t *
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

/* find_leaf_at_point maps cursor position to BSP leaf node */
static node_t *
find_leaf_at_point(node_t *root, int16_t x, int16_t y)
{
	if (root == NULL)
		return NULL;

	/* if external node, check if point is inside */
	if (IS_EXTERNAL(root)) {
		rectangle_t r = root->rectangle;
		if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) {
			/* skip floating clients, we don't want them to be drop targets. */
			if (root->client && IS_FLOATING(root->client))
				return NULL;
			return root;
		}
		return NULL;
	}

	if (root->first_child) {
		node_t *f = find_leaf_at_point(root->first_child, x, y);
		if (f)
			return f;
	}
	if (root->second_child) {
		node_t *s = find_leaf_at_point(root->second_child, x, y);
		if (s)
			return s;
	}

	return NULL;
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
		node->parent->is_master	   = external_node->is_master;
		node->parent->is_focused   = external_node->is_focused;
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
			grandparent->first_child->is_master	   = external_node->is_master;
			grandparent->first_child->is_focused   = external_node->is_focused;
			grandparent->first_child->first_child  = NULL;
			grandparent->first_child->second_child = NULL;
		} else {
			grandparent->second_child->node_type	= EXTERNAL_NODE;
			grandparent->second_child->client		= external_node->client;
			grandparent->second_child->is_master	= external_node->is_master;
			grandparent->second_child->is_focused	= external_node->is_focused;
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
		} else if (d->layout == GRID) {
			apply_grid_layout(d->tree);
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
		} else if (d->layout == GRID) {
			apply_grid_layout(node->parent);
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

/* delete_node removes a node (and its client) from the tree*/
static void
delete_node(node_t *node, desktop_t *d)
{
	if (node == NULL || node->client == NULL || d->tree == NULL) {
		_LOG_(ERROR, "node to be deleted is null");
		return;
	}
	/*bool swap = node == d->node;*/
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

	/*node_t *n = prev_node(node);
	if (!n) {
		n = next_node(node);
	}*/

	if (!unlink_node(node, d)) {
		_LOG_(ERROR, "could not unlink node.. abort");
		return;
	}

	if (check) {
		assert(!d->tree);
		/*d->node = NULL;*/
	}

	_FREE_(node->client);
	_FREE_(node);

	/*if (!check) {
		d->node = n;
	}*/

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

static bool
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

static void
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

static int
hide_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (!IS_INTERNAL(cn) && cn->client) {
		if (set_desktop_visibility(cn->client->window, false) != 0) {
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

static int
show_windows(node_t *cn)
{
	if (cn == NULL)
		return 0;

	if (!IS_INTERNAL(cn) && cn->client) {
		if (set_desktop_visibility(cn->client->window, true) != 0) {
			return -1;
		}
	}

	show_windows(cn->first_child);
	show_windows(cn->second_child);

	return 0;
}

static bool
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

static node_t *
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

static node_t *
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

/* Note: this function does not free the memory of the unlinked node.
 * The caller is responsible for freeing the memory of the unlinked node if
 * it's no longer needed.
 */
static bool
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

/* Note: doesn't touch visibility or focus; that's handled outside. */
static bool
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
		rectangle_t r = {0};
		calculate_base_rect(&r, curr_monitor);
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

static bool
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

static node_t *
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

static node_t *
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

static void
update_focus_walk(node_t *root, node_t *n)
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
	update_focus_walk(root->first_child, n);
	update_focus_walk(root->second_child, n);
}

static void
update_focus(desktop_t *d, node_t *n)
{
	if (!d)
		return;
	/* store last focused on the desktop that owns this tree */
	if (n && n->client) {
		d->last_focused	 = n->client->window;
		d->logical_focus = n;
	}
	update_focus_walk(d->tree, n);
}

static void
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

static node_t *
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

/* swap the positions of two nodes */
static int
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

/* checks if one rectangle is within a certain range
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

/* find the closest neighbor node to a given node in
 * a specific direction. It is used to move focus to another node using the
 * keyboard
 */
static node_t *
find_closest_neighbor(node_t *root, node_t *node, direction_t d)
{
	if (root == NULL)
		return NULL;

	node_t	*closest		  = NULL;
	int		 closest_distance = INT16_MAX;

	queue_t *q				  = create_queue();
	if (!q)
		return NULL;

	enqueue(q, root);

	while (!is_queue_empty(q)) {
		node_t *current = dequeue(q);
		if (!current)
			continue;

		if (current == node)
			goto skip;

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
	skip:
		if (current->first_child)
			enqueue(q, current->first_child);
		if (current->second_child)
			enqueue(q, current->second_child);
	}

	free_queue(q);
	return closest;
}

static node_t *
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

/* ./src/layout.c */



/* clang-format off */
/* clang-format on */

static void
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

static void
show_all_tiled(node_t *root)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_TILED(root->client))
		set_desktop_visibility(root->client->window, true);
	show_all_tiled(root->first_child);
	show_all_tiled(root->second_child);
}

static void
apply_layout(desktop_t *d, layout_t t)
{
	layout_t ol = d->layout;
	d->layout	= t;
	if (ol == MONOCLE || ol == DECK)
		show_all_tiled(d->tree);
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

static void
split_node(node_t *n, node_t *nd)
{
	if (IS_FLOATING(nd->client)) {
		n->first_child->rectangle = n->floating_rectangle = n->rectangle;
		return;
	}
	split_type_t s = n->split_type;
	if (s == DYNAMIC_TYPE) {
		/* horizontal split means children sit side by side. */
		s = (n->rectangle.width >= n->rectangle.height) ? HORIZONTAL_TYPE
														: VERTICAL_TYPE;
	}
	split_rect(n, s);
}

static void
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

static void
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
	/* decide how this parent is divided before handing space to children. */
	if (s == HORIZONTAL_TYPE) {
		/* side by side */
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
		/* top and bottom */
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

static void
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

static void
default_layout(node_t *root)
{
	if (root == NULL)
		return;
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
	root->rectangle = r;
	apply_default_layout(root);
}

/* TODO: use next_node() to make stack order explicit*/
static void
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

static void
master_layout(node_t *root, node_t *n)
{
	const double   ratio		= MASTER_RATIO;

	rectangle_t	   usable		= get_usable_area(curr_monitor);
	const uint16_t master_width = (uint16_t)(usable.width * ratio);
	const uint16_t r_width		= (uint16_t)(usable.width * (1 - ratio));

	/* fall back to any tiled leaf when the caller did not give us one. */
	if (n == NULL) {
		n = find_any_leaf(root);
		if (n == NULL) {
			return;
		}
	}

	n->is_master		 = true;

	/* master side */
	const rectangle_t r1 = {
		.x		= (int16_t)(usable.x + conf.window_gap),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(master_width - 2 * conf.window_gap),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap),
	};

	/* stack side */
	const rectangle_t r2 = {
		.x		= (int16_t)(usable.x + master_width),
		.y		= (int16_t)(usable.y + conf.window_gap),
		.width	= (uint16_t)(r_width - (1 * conf.window_gap)),
		.height = (uint16_t)(usable.height - 2 * conf.window_gap),
	};

	/* a single root client gets the whole usable area.
	 * this happens after deleting windows in master layout until only one
	 * client remains.
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

static void
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

static void
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
				/* last row can be short.
				 * stretch it so no empty cells remain. */
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

static void
count_windows(node_t *r, int *n)
{
	if (!r)
		return;
	if (IS_EXTERNAL(r) && r->client && !IS_FLOATING(r->client))
		(*n)++;
	count_windows(r->first_child, n);
	count_windows(r->second_child, n);
}

static void
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

static void
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

#if 0
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

static void
raise_floating(node_t *root)
{
	if (!root)
		return;
	if (IS_EXTERNAL(root) && root->client && IS_FLOATING(root->client)) {
		raise_window(root->client->window);
		return;
	}
	raise_floating(root->first_child);
	raise_floating(root->second_child);
}
#endif

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

static void
monocle_layout(node_t *root)
{
	if (!root)
		return;
	rectangle_t r = {0};
	calculate_base_rect(&r, curr_monitor);
	root->rectangle = r;
	apply_monocle_layout(root, r);
}

/* deprecated */
#if 0
static int
render_monocle(node_t *root)
{
	if (!root)
		return 0;
	if (IS_EXTERNAL(root) && root->client) {
		if (IS_FLOATING(root->client)) {
			tile(root);
		} else if (root->is_focused) {
			set_desktop_visibility(root->client->window, true);
			tile(root);
		} else {
			set_desktop_visibility(root->client->window, false);
		}
		return 0;
	}
	render_monocle(root->first_child);
	render_monocle(root->second_child);
	raise_floating(root);
	return 0;
}
#endif

static void
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

static void
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

/* deprecated */
#if 0

static int
render_deck(node_t *r)
{
	if (!r)
		return 0;

	node_t *l[64];
	int		n = 0;
	collect_tiled_leaves(r, l, &n, 64);
	node_t *m = find_layout_master(r);

	if (m) {
		set_desktop_visibility(m->client->window, true);
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
				set_desktop_visibility(l[i]->client->window, true);
				tile(l[i]);
			} else {
				set_desktop_visibility(l[i]->client->window, false);
			}
		}
	}

	render_floating(r);
	raise_floating(r);
	return 0;
}
#endif

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

static void
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

static void
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

	/* resizing always trades space with the sibling inside the same parent. */
	node_t *s = (n->parent->first_child == n) ? n->parent->second_child
											  : n->parent->first_child;
	if (s == NULL) {
		return;
	}

	rectangle_t *nr = &n->rectangle;
	rectangle_t *sr = &s->rectangle;

	/* current geometry tells us which axis can be resized. */
	bool		 vs = (nr->x == sr->x); /* nodes are stacked vertically */
	bool		 hs = (nr->y == sr->y); /* nodes are side-by-side */

	if (vs) {
		/* top-bottom resize */
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
		/* side-by-side resize */
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

/* ./src/xcb_util.c */



static uint64_t
get_time_millis(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

static int16_t
get_cursor_axis(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return -1;
	}

	int16_t x = p_reply->root_x;
	_FREE_(p_reply);

	return x;
}

static void
log_children(xcb_conn_t *conn, xcb_window_t root_window)
{
	xcb_query_tree_cookie_t tree_cookie = xcb_query_tree(conn, root_window);
	xcb_query_tree_reply_t *tree_reply =
		xcb_query_tree_reply(conn, tree_cookie, NULL);
	if (tree_reply == NULL) {
		_LOG_(ERROR, "failed to query tree reply");
		return;
	}

	_LOG_(DEBUG, "children of root window:");
	xcb_window_t *children	   = xcb_query_tree_children(tree_reply);
	const int	  num_children = xcb_query_tree_children_length(tree_reply);
	for (int i = 0; i < num_children; ++i) {
		xcb_icccm_get_text_property_reply_t t_reply;
		xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(conn, children[i]);
		uint8_t wr = xcb_icccm_get_wm_name_reply(conn, cn, &t_reply, NULL);
		if (wr == 1) {
			_LOG_(DEBUG, "child %d: %s", i + 1, t_reply.name);
			xcb_icccm_get_text_property_reply_wipe(&t_reply);
		} else {
			_LOG_(DEBUG, "failed to get window name for child %d", i + 1);
		}
	}

	_FREE_(tree_reply);
}

#define WINDOW_X (XCB_CONFIG_WINDOW_X)
#define WINDOW_Y (XCB_CONFIG_WINDOW_Y)
#define WINDOW_W (XCB_CONFIG_WINDOW_WIDTH)
#define WINDOW_H (XCB_CONFIG_WINDOW_HEIGHT)
#define MOVE	 (WINDOW_X | WINDOW_Y)
#define RESIZE	 (WINDOW_W | WINDOW_H)

/* caller must free */
static char *
win_name(xcb_window_t win)
{
	xcb_icccm_get_text_property_reply_t t_reply;
	xcb_get_property_cookie_t cn = xcb_icccm_get_wm_name(wm->connection, win);
	const uint8_t			  wr =
		xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
	if (wr != 1)
		return NULL;

	char *str = (char *)malloc(t_reply.name_len + 1);
	if (str == NULL)
		return NULL;

	strncpy(str, (char *)t_reply.name, t_reply.name_len);
	str[t_reply.name_len] = '\0';
	xcb_icccm_get_text_property_reply_wipe(&t_reply);

	return str;
}

static int
check_window_map_state(xcb_window_t win, win_map_state_t state)
{
	xcb_get_window_attributes_cookie_t attr_cookie =
		xcb_get_window_attributes(wm->connection, win);
	xcb_get_window_attributes_reply_t *attr =
		xcb_get_window_attributes_reply(wm->connection, attr_cookie, NULL);

	if (attr == NULL) {
		return 0;
	}

	if (state == WIN_MAP_STATE_ANY) {
		_FREE_(attr);
		return 1;
	}

	int matched = (attr->map_state == (uint8_t)state);
	_FREE_(attr);
	return matched;
}

static void
window_above(xcb_window_t win1, xcb_window_t win2)
{
	if (win1 == wm->root_window || win2 == XCB_NONE)
		return;

	if (win2 == wm->root_window)
		return;

	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_ABOVE};
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

static void
window_below(xcb_window_t win1, xcb_window_t win2)
{
	if (win2 == XCB_NONE) {
		return;
	}
	uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t values[] = {win2, XCB_STACK_MODE_BELOW};
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win1, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win2,
			  err->error_code);
		_FREE_(err);
	}
}

static void
lower_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_BELOW};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

static void
raise_window(xcb_window_t win)
{
	uint32_t	 values[] = {XCB_STACK_MODE_ABOVE};
	uint16_t	 mask	  = XCB_CONFIG_WINDOW_STACK_MODE;
	xcb_cookie_t c =
		xcb_configure_window_checked(wm->connection, win, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "in stacking window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
	}
}

static xcb_get_geometry_reply_t *
get_geometry(xcb_window_t win, xcb_conn_t *conn)
{
	xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(conn, win);
	xcb_error_t				 *err;
	xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(conn, gc, &err);
	if (err) {
		_LOG_(ERROR,
			  "error getting geometry for window %u: %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return NULL;
	}

	if (gr == NULL) {
		_LOG_(ERROR, "failed to get geometry for window %u", win);
		return NULL;
	}
	return gr;
}

static int
resize_window(xcb_window_t win, uint16_t width, uint16_t height)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t v[] = {width, height};
	xcb_cookie_t   c =
		xcb_configure_window_checked(wm->connection, win, RESIZE, v);

	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "error resizing window (ID %u): %s",
			  win,
			  strerror(err->error_code));
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
move_window(xcb_window_t win, int16_t x, int16_t y)
{
	if (win == 0 || win == XCB_NONE) {
		return 0;
	}

	const uint32_t v[] = {x, y};
	xcb_cookie_t c = xcb_configure_window_checked(wm->connection, win, MOVE, v);
	xcb_error_t *err = xcb_request_check(wm->connection, c);

	if (err) {
		_LOG_(ERROR, "error moving window (ID %u): %d", win, err->error_code);
		_FREE_(err);
		return -1;
	}

	return 0;
}

static int
send_configure_notify(xcb_window_t win, rectangle_t r, uint16_t bw)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	xcb_configure_notify_event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.response_type	 = XCB_CONFIGURE_NOTIFY;
	ev.event			 = win;
	ev.window			 = win;
	ev.above_sibling	 = XCB_NONE;
	ev.x				 = r.x;
	ev.y				 = r.y;
	ev.width			 = r.width;
	ev.height			 = r.height;
	ev.border_width		 = bw;
	ev.override_redirect = false;

	xcb_cookie_t c	 = xcb_send_event_checked(wm->connection,
											  false,
											  win,
											  XCB_EVENT_MASK_STRUCTURE_NOTIFY,
											  (const char *)&ev);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "error sending configure notify for window %d: error code %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
apply_window_geometry(xcb_window_t win, rectangle_t r, uint16_t bw)
{
	if (win == 0 || win == XCB_NONE)
		return 0;

	const uint32_t v[] = {r.x, r.y, r.width, r.height};
	xcb_cookie_t   c =
		xcb_configure_window_checked(wm->connection, win, MOVE | RESIZE, v);
	xcb_error_t *err = xcb_request_check(wm->connection, c);
	if (err) {
		_LOG_(ERROR,
			  "error configuring window (ID %u): %d",
			  win,
			  err->error_code);
		_FREE_(err);
		return -1;
	}

	return send_configure_notify(win, r, bw);
}

static int
change_border_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		bcolor,
				   uint32_t		bwidth,
				   bool			stack)
{

	uint32_t bpx_width = XCB_CW_BORDER_PIXEL;
	uint32_t b_width   = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t stack_	   = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t input	   = XCB_INPUT_FOCUS_PARENT;

	if (change_window_attr(conn, win, bpx_width, &bcolor) != 0) {
		return -1;
	}

	if (configure_window(conn, win, b_width, &bwidth) != 0) {
		return -1;
	}

	if (stack) {
		const uint16_t arg[1] = {XCB_STACK_MODE_ABOVE};
		if (configure_window(conn, win, stack_, arg) != 0) {
			return -1;
		}

		if (set_input_focus(conn, input, win, XCB_CURRENT_TIME) != 0) {
			return -1;
		}
	}

	xcb_flush(conn);
	return 0;
}

static int
change_window_attr(xcb_conn_t  *conn,
				   xcb_window_t win,
				   uint32_t		attr,
				   const void  *val)
{
	xcb_cookie_t attr_cookie =
		xcb_change_window_attributes_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, attr_cookie);
	if (err) {
		_LOG_(ERROR,
			  "failed to change window attributes: error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
configure_window(xcb_conn_t	 *conn,
				 xcb_window_t win,
				 uint16_t	  attr,
				 const void	 *val)
{
	xcb_cookie_t c	 = xcb_configure_window_checked(conn, win, attr, val);
	xcb_error_t *err = xcb_request_check(conn, c);
	if (err) {
		_LOG_(ERROR,
			  "failed to configure window : error code %d",
			  err->error_code);
		_FREE_(err);
		return -1;
	}
	return 0;
}

static int
set_input_focus(xcb_conn_t	   *conn,
				uint8_t			revert_to,
				xcb_window_t	win,
				xcb_timestamp_t time)
{
	/* if window is viewable before attempting to set focus */
	if (!check_window_map_state(win, WIN_MAP_STATE_VIEWABLE)) {
		return -1;
	}

	xcb_cookie_t c	 = xcb_set_input_focus_checked(conn, revert_to, win, time);
	xcb_error_t *err = xcb_request_check(conn, c);
	if (err) {
		char *n = win_name(win);
		_LOG_(ERROR,
			  "failed to set input focus for win %d name %s : error code %d "
			  "error message %s",
			  win,
			  n,
			  err->error_code,
			  strerror(err->error_code));
		_FREE_(err);
		_FREE_(n);
		return -1;
	}
	return 0;
}

static xcb_window_t
get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, win);
	xcb_query_pointer_reply_t *p_reply =
		xcb_query_pointer_reply(conn, p_cookie, NULL);

	if (p_reply == NULL) {
		_LOG_(ERROR, "failed to query pointer position");
		return XCB_NONE;
	}

	xcb_window_t x = p_reply->child;
	_FREE_(p_reply);

	return x;
}

static void
grab_pointer(xcb_window_t win, bool wants_events)
{
	xcb_grab_pointer_reply_t *r;
	xcb_grab_pointer_cookie_t c = xcb_grab_pointer(wm->connection,
												   wants_events,
												   win,
												   XCB_NONE,
												   XCB_GRAB_MODE_SYNC,
												   XCB_GRAB_MODE_ASYNC,
												   XCB_NONE,
												   XCB_NONE,
												   XCB_CURRENT_TIME);
	if ((r = xcb_grab_pointer_reply(wm->connection, c, NULL))) {
		if (r->status != XCB_GRAB_STATUS_SUCCESS)
			_LOG_(WARNING, "cannot grab the pointer");
	}
	_FREE_(r);
}

static void
ungrab_pointer(void)
{
	xcb_ungrab_pointer(wm->connection, XCB_CURRENT_TIME);
}

static xcb_atom_t
get_atom(char *atom_name, xcb_conn_t *conn)
{
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_atom_t				 atom;
	xcb_intern_atom_reply_t *rep;

	atom_cookie =
		xcb_intern_atom(conn, 0, (uint16_t)strlen(atom_name), atom_name);
	rep = xcb_intern_atom_reply(conn, atom_cookie, NULL);
	if (NULL != rep) {
		atom = rep->atom;
		_FREE_(rep);
		return atom;
	}
	return 0;
}

static bool
window_exists(xcb_conn_t *conn, xcb_window_t win)
{
	xcb_query_tree_cookie_t c		   = xcb_query_tree(conn, win);
	xcb_query_tree_reply_t *tree_reply = xcb_query_tree_reply(conn, c, NULL);

	if (tree_reply == NULL) {
		return false;
	}

	_FREE_(tree_reply);
	return true;
}

/* ./src/view.c */



static leaf_visibility_t
_leaf_visibility_(desktop_t *d, node_t *leaf)
{
	/* normal layouts map tiled leaves.
	 * monocle/deck hide most tiled leaves, but floating is always shown. */
	if (!leaf || IS_INTERNAL(leaf) || !leaf->client)
		return LEAF_IGNORED;

	if (IS_FLOATING(leaf->client))
		return LEAF_VISIBLE_FLOATING;

	switch (d->layout) {
	case DEFAULT:
	case MASTER:
	case STACK:
	case GRID:
	case THREE_COL: return LEAF_VISIBLE_TILED;

	case MONOCLE: {
		/* one tiled node, picked from logical_focus */
		node_t *focus = d->logical_focus;
		if (!focus || IS_FLOATING(focus->client))
			focus = pick_desktop_focus(d);
		return (leaf == focus) ? LEAF_VISIBLE_TILED : LEAF_HIDDEN_TILED;
	}

	case DECK: {
		if (leaf->is_master)
			return LEAF_VISIBLE_TILED;
		/* master + one node from the deck side */
		node_t *focus = d->logical_focus;
		if (!focus || IS_FLOATING(focus->client) || focus->is_master)
			focus = pick_deck_focus(d);
		return (leaf == focus) ? LEAF_VISIBLE_TILED : LEAF_HIDDEN_TILED;
	}
	}

	return LEAF_VISIBLE_TILED;
}

/* BFS renderer */
static int
_render_tree_view_(desktop_t *d)
{
	if (!d || is_tree_empty(d->tree))
		return 0;

	queue_t *q = create_queue();
	if (!q)
		return -1;

	enqueue(q, d->tree);
	int rc = 0;

	while (q->front) {
		node_t *cur = dequeue(q);

		if (IS_INTERNAL(cur) || !cur->client) {
			if (cur->first_child)
				enqueue(q, cur->first_child);
			if (cur->second_child)
				enqueue(q, cur->second_child);
			continue;
		}

		if (IS_FULLSCREEN(cur->client)) {
			/* map before resize; some clients ignore the reverse order */
			if (set_desktop_visibility(cur->client->window, true) != 0) {
				rc = -1;
				break;
			}
			if (_handle_fullscreen_window(cur->client->window) != 0) {
				rc = -1;
				break;
			}
			continue;
		}

		leaf_visibility_t vis = _leaf_visibility_(d, cur);
		switch (vis) {
		case LEAF_VISIBLE_TILED:
		case LEAF_VISIBLE_FLOATING:
			if (set_desktop_visibility(cur->client->window, true) != 0) {
				rc = -1;
				goto done;
			}
			if (tile(cur) != 0) {
				rc = -1;
				goto done;
			}
			break;
		case LEAF_HIDDEN_TILED:
			if (set_desktop_visibility(cur->client->window, false) != 0) {
				rc = -1;
				goto done;
			}
			break;
		case LEAF_IGNORED: break;
		}
	}

done:
	free_queue(q);
	return rc;
}

static void
_focus_node_(desktop_t *d, node_t *n)
{
	if (!d || !n)
		return;
	d->logical_focus = n;
	d->last_focused	 = n->client ? n->client->window : XCB_NONE;
	n->is_focused	 = true;
	update_focus(d, n);
}

static int
_focus_input_(desktop_t *d, node_t *n)
{
	if (!d || !n || !n->client)
		return 0;
	/* win_focus sets border colour and X input focus; it does not raise */
	return win_focus(n->client->window, true);
}

static node_t *
_pick_focus_(desktop_t *d)
{
	if (!d)
		return NULL;
	if (d->layout == DECK)
		return pick_deck_focus(d);
	return pick_desktop_focus(d);
}

static int
_render_view_(desktop_t *d)
{
	if (!d)
		return 0;
	return _render_tree_view_(d);
}

static void
_flush_view_(desktop_t *d)
{
	(void)d;
	restack();
	xcb_flush(wm->connection);
}

/* ./src/zwm.c */



static wm_t *
init_wm(void)
{
	int i, default_screen;
	wm = (wm_t *)malloc(sizeof(wm_t));
	if (wm == NULL) {
		_LOG_(ERROR, "failed to malloc for window manager");
		return NULL;
	}

	wm->connection = xcb_connect(NULL, &default_screen);
	if (xcb_connection_has_error(wm->connection) > 0) {
		_LOG_(ERROR, "error: Unable to open X connection");
		_FREE_(wm);
		return NULL;
	}

	const xcb_setup_t	 *setup = xcb_get_setup(wm->connection);
	xcb_screen_iterator_t iter	= xcb_setup_roots_iterator(setup);
	for (i = 0; i < default_screen; ++i) {
		xcb_screen_next(&iter);
	}
	wm->screen				= iter.data;
	wm->root_window			= iter.data->root;
	wm->screen_nbr			= default_screen;
	wm->split_type			= DYNAMIC_TYPE;
	const uint32_t mask		= XCB_CW_EVENT_MASK;
	const uint32_t values[] = {ROOT_EVENT_MASK};

	/* register events */
	xcb_cookie_t   cookie	= xcb_change_window_attributes_checked(
		wm->connection, wm->root_window, mask, values);
	xcb_error_t *err = xcb_request_check(wm->connection, cookie);
	if (err) {
		_LOG_(ERROR,
			  "error registering for substructure redirection "
			  "events on window "
			  "%u: %d",
			  wm->root_window,
			  err->error_code);
		_FREE_(wm);
		_FREE_(err);
		return NULL;
	}

	meta_window				 = xcb_generate_id(wm->connection);
	xcb_connection_t *dpy	 = wm->connection;
	uint8_t			  depth	 = XCB_COPY_FROM_PARENT;
	xcb_window_t	  mw	 = meta_window;
	xcb_window_t	  rw	 = wm->root_window;
	uint32_t		  m		 = XCB_NONE;
	xcb_visualid_t	  visual = XCB_COPY_FROM_PARENT;
	uint16_t		  class	 = XCB_WINDOW_CLASS_INPUT_ONLY;

	xcb_create_window(
		dpy, depth, mw, rw, -1, -1, 1, 1, 0, class, visual, m, NULL);
	xcb_icccm_set_wm_class(dpy, mw, sizeof(WM_NAME), WM_NAME);
	return wm;
}

/* setup wm
 * monitors come first, then desktops, then EWMH.
 */
static bool
setup_wm(void)
{
	if (wm == NULL)
		return false;

	if (!setup_monitors()) {
		_LOG_(ERROR, "error while setting up monitors");
		return false;
	}

	if (!setup_desktops()) {
		_LOG_(ERROR, "error while setting up desktops");
		return false;
	}

	if (!setup_ewmh()) {
		_LOG_(ERROR, "error while setting up ewmh");
		return false;
	}
	/* load_cursors(); */
	/* set_cursor(CURSOR_POINTER); */
	/* init_pointer(); */

	return true;
}

static void
parse_args(int argc, char **argv)
{
	char *c = NULL;
	if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-run") == 0) {
		if (argc >= 2) {
			c = argv[2];
		} else {
			_LOG_(ERROR, "missing argument after -r/--run");
		}
	}
	exec_process(&((arg_t){.argc = 1, .cmd = (char *[]){c}}));
}

static void
signal_handler(int sig)
{
	should_shutdown = 1;
}

static void
cleanup(int sig)
{
	if (wm != NULL) {
		if (wm->connection != NULL) {
			xcb_disconnect(wm->connection);
		}
		if (wm->ewmh != NULL) {
			xcb_ewmh_connection_wipe(wm->ewmh);
			free(wm->ewmh);
		}
		_FREE_(wm);
	}
	free_keys();
	free_rules();
	cleanup_strut_windows();
	free_monitors(); /* frees desktops and trees as well */
	_LOG_(INFO, "ZWM exits with signal number %d", sig);
	/* uncommenting the following line *exit(sig)* prevents the os
	 * from generating a core dump file when zwm crashes */
	/* exit(sig); */
}

int
main(int argc, char **argv)
{

	/* if loading the config file went sideways, we use the default values,
	 * and default keys */
	if (load_config(&conf) != 0) {
		_LOG_(ERROR, "error while loading config -> using default macros");
		conf.active_border_color  = ACTIVE_BORDER_COLOR;
		conf.normal_border_color  = NORMAL_BORDER_COLOR;
		conf.border_width		  = BORDER_WIDTH;
		conf.window_gap			  = W_GAP;
		conf.focus_follow_pointer = FOCUS_FOLLOW_POINTER;
		conf.focus_follow_spawn	  = FOCUS_FOLLOW_SPAWN;
		conf.virtual_desktops	  = NUMBER_OF_DESKTOPS;
		conf.restore_last_focus	  = RESTORE_LAST_FOCUS;
	}

	wm = init_wm();
	if (wm == 0x00) {
		_LOG_(ERROR, "failed to initialize window manager");
		exit(EXIT_FAILURE);
	}

	if (!setup_wm()) {
		_LOG_(ERROR, "failed to setup window manager");
		exit(EXIT_FAILURE);
	}

	if (argc >= 2) {
		parse_args(argc, argv);
	}

	/* do not wait for mapping event. Grab the keys as soon as zwm starts */
	if (grab_keys(wm->connection, wm->root_window) != 0) {
		_LOG_(ERROR, "cannot grab keys");
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGABRT, signal_handler);

	event_loop(wm);
	cleanup(0);

	return 0;
}
