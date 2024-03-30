#ifndef ZWM_TREE_H
#define ZWM_TREE_H

#include "type.h"

node_t	 *create_node(client_t *c);
node_t	 *init_root();
node_t	 *find_node_by_window_id(node_t *root, xcb_window_t window_id);
client_t *find_client_by_window_id(node_t *root, xcb_window_t window_id);
void	  unlink_node(node_t *node, desktop_t *d);
int		  render_tree(node_t *current_node, wm_t *wm);
int		  get_tree_level(node_t *current_node);
int		  hide_windows(node_t *tree, wm_t *w);
int		  show_windows(node_t *tree, wm_t *w);
bool	  is_tree_empty(const node_t *root);
bool	  is_parent_null(const node_t *node);
bool	  client_exist(node_t *cn, xcb_window_t id);
bool	  has_sibling(const node_t *node);
bool	  has_internal_sibling(const node_t *node);
bool	  is_sibling_external(const node_t *node);
bool	  is_sibling_internal(const node_t *node);
bool	  has_external_children(const node_t *parent);
bool	  is_parent_internal(const node_t *node);
bool	  has_single_external_child(const node_t *parent);
bool	  has_first_child(const node_t *parent);
bool	  has_second_child(const node_t *parent);
bool	  is_internal(const node_t *node);
bool	  is_external(const node_t *node);
void	  resize_subtree(node_t *parent);
void	  free_tree(node_t *root);
void	  horizontal_resize(node_t *n, resize_t t);
void	  delete_node(node_t *node, desktop_t *d);
void	  insert_node(node_t *current_node, node_t *new_node);
void	  log_tree_nodes(node_t *node, wm_t *w);
void	  transfer_node(node_t *, desktop_t *, wm_t *);

#endif // ZWM_TREE_H
