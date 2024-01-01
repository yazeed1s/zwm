#ifndef ZWM_TREE_H
#define ZWM_TREE_H

#include "type.h"

node_t *create_node(client_t *c);
node_t *init_root();
bool    is_parent_null(node_t *node);
void    set_rectangle(node_t *node, rectangle_t rec);
void    set_parent(node_t *child, node_t *parent);
void    set_left_child(node_t *internal_node, node_t *left_child);
void    set_right_child(node_t *internal_node, node_t *right_child);
void    set_node_type(node_t *node, node_type_t type);
node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id);
void    free_tree(node_t *root);
#endif // ZWM_TREE_H
