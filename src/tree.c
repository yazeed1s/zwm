//
// Created by yaz on 12/31/23.
//

#include "type.h"
#include <stdlib.h>

node_t *create_node(client_t *c) {
    if (c == 0x00) { return NULL; }
    node_t *node       = (node_t *)malloc(sizeof(node_t));
    node->client       = c;
    node->id           = 1;
    node->parent       = NULL;
    node->first_child  = NULL;
    node->second_child = NULL;
    return node;
}

node_t *init_root() {
    node_t *node       = (node_t *)malloc(sizeof(node_t));
    node->client       = NULL;
    node->id           = 1;
    node->parent       = NULL;
    node->first_child  = NULL;
    node->second_child = NULL;
    return node;
}

void insert(node_t *node) {
    if (node->parent == NULL) {
        //
        node_t *parent = create_node(NULL);
        node->parent   = parent;
    }
}

void set_parent(node_t *child, node_t *parent) {
    if (child == NULL) { return; }
    child->parent = parent;
}

bool is_parent_null(node_t *node) {
    return node->parent == NULL;
}

void set_rectangle(node_t *node, rectangle_t rec) {
    if (node != NULL) { node->rectangle = rec; }
}

void set_left_child(node_t *internal_node, node_t *left_child) {
    if (internal_node != NULL) {
        internal_node->first_child = left_child;
        if (left_child != NULL) { left_child->parent = internal_node; }
    }
}

void set_right_child(node_t *internal_node, node_t *right_child) {
    if (internal_node != NULL) {
        internal_node->second_child = right_child;
        if (right_child != NULL) { right_child->parent = internal_node; }
    }
}

void set_node_type(node_t *node, node_type_t type) {
    if (node != NULL) { node->node_type = type; }
}

node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id) {
    if (root == NULL) { return NULL; }

    if (root->client != NULL && root->client->window == window_id) { return root; }

    node_t *left_result = find_node_by_window_id(root->first_child, window_id);
    if (left_result != NULL) { return left_result; }

    node_t *right_result = find_node_by_window_id(root->second_child, window_id);
    return right_result;
}

void free_tree(node_t *root) {
    if (root == NULL) { return; }
    free_tree(root->first_child);
    free_tree(root->second_child);
    if (root->client != NULL) { free(root->client); }
    free(root);
}
