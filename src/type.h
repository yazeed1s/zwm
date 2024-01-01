#ifndef ZWM_TYPES_H
#define ZWM_TYPES_H
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>

typedef enum { HORIZONTAL_TYPE __attribute__((unused)), VERTICAL_TYPE } split_type_t;

typedef enum { HORIZONTAL_FLIP __attribute__((unused)), VERTICAL_FLIP __attribute__((unused)) } flip_t;

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
    int16_t  x;
    int16_t  y;
    uint16_t width;
    uint16_t height;
} rectangle_t;

typedef struct {
    xcb_window_t window;
    xcb_atom_t   type;
    uint8_t      border_width;
    bool         is_focused;
    posxy_t      position_info;
} client_t;

typedef enum { ROOT_NODE, INTERNAL_NODE, EXTERNAL_NODE } node_type_t;

/*
        I         ROOT (root is also an INTERNAL NODE, unless it is leaf)
      /   \
     I     I      INTERNAL NODES
    /     / \
   E     E   E    EXTERNAL NODES (or leaves)

 I, if parent of E = screen sections/partitions in which windows can be mapped (displayed).
 E = windows in every screen partition.
 windows are basically the leaves of a full binary tree.
 E nodes, on the screen, evenly share the width & height of their I parent,
 and the I's x & y as well.

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
*/

typedef struct node_t node_t;
struct node_t {
    uint32_t     id;
    node_t      *parent;
    node_t      *first_child;
    node_t      *second_child;
    node_type_t  node_type;
    split_type_t split_type;
    rectangle_t  rectangle;
    client_t    *client;
    bool         is_focused;
    bool         is_fullscreen;
};

typedef struct {
    uint8_t    id;
    bool       is_focused;
    bool       is_full;
    client_t **clients;
    uint8_t    clients_n;
} desktop_t;

typedef struct {
    xcb_connection_t *connection;
    xcb_screen_t     *screen;
    xcb_window_t      root_window;
    split_type_t      split_type;
    desktop_t       **desktops;
    node_t           *root;
    uint8_t           max_number_of_desktops;
    uint8_t           number_of_desktops;
} wm_t;

#endif // ZWM_TYPES_H