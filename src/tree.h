#ifndef ZWM_TREE_H
#define ZWM_TREE_H

#include "type.h"

node_t *create_node(client_t *c);
node_t *init_root();
node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id);
node_t *find_tree_root(node_t *node);
node_t *get_sibling(node_t *node, node_type_t type);
node_t *get_external_sibling(node_t *node);
node_t *get_internal_sibling(node_t *node);
int     display_tree(node_t *current_node, wm_t *wm);
int     get_tree_level(node_t *current_node);
bool    is_tree_empty(node_t *root);
bool    is_parent_null(node_t *node);
bool    has_sibling(node_t *node);
bool    has_internal_sibling(node_t *node);
bool    is_sibling_external(node_t *node);
bool    is_sibling_internal(node_t *node);
bool    has_external_children(node_t *parent);
bool    is_parent_internal(node_t *node);
bool    has_single_external_child(node_t *parent);
bool    has_first_child(node_t *parent);
bool    has_second_child(node_t *parent);
bool    is_internal(node_t *node);
bool    is_external(node_t *node);
void    resize_subtree(node_t *parent);
void    free_tree(node_t *root);
void    delete_node(node_t *node, wm_t *wm);
void    insert_under_cursor(node_t *current_node, client_t *new_client, xcb_window_t win);
void    log_tree_nodes(node_t *node);

#endif // ZWM_TREE_H
