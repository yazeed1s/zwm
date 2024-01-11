#ifndef ZWM_TREE_H
#define ZWM_TREE_H

#include "type.h"

// // void    insert_under_cursor(node_t *c, client_t *nc, wm_t *wm, xcb_window_t w, direction_t d);
node_t *create_node(client_t *c);
node_t *init_root();
node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id);
void    insert_under_cursor(node_t *current_node, client_t *new_client, xcb_window_t win);
int     get_tree_level(node_t *current_node);
int     display_tree(node_t *current_node, wm_t *wm);
void    delete_node(node_t *node);
void    free_tree(node_t *root);
void    log_tree_nodes(node_t *node);
void    set_rectangle(node_t *node, rectangle_t rec);
bool    is_parent_null(node_t *node);
bool    has_sibling(node_t *node);
void    set_parent(node_t *child, node_t *parent);
void    set_left_child(node_t *internal_node, node_t *left_child);
void    set_right_child(node_t *internal_node, node_t *right_child);
void    set_node_type(node_t *node, node_type_t type);
bool    has_first_child(node_t *parent);
bool    has_second_child(node_t *parent);
bool    is_internal(node_t *node);
bool    is_external(node_t *node);
bool    is_tree_empty(node_t *root);
#endif // ZWM_TREE_H
