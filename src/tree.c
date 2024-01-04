//
// Created by yaz on 12/31/23.
//

#include "tree.h"
#include "logger.h"
#include "type.h"
#include "zwm.h"
#include <stdio.h>
#include <stdlib.h>

node_t *create_node(client_t *c) {
    if (c == 0x00) {
        return NULL;
    }
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

node_t *create_internal_node() {
    node_t *internal_node = (node_t *)malloc(sizeof(node_t));
    if (internal_node == NULL) {
        return NULL;
    }
    internal_node->node_type    = INTERNAL_NODE;
    internal_node->first_child  = NULL;
    internal_node->second_child = NULL;
    internal_node->parent       = NULL;
    return internal_node;
}

void display_tree(node_t *current_node, wm_t *wm) {
    if (current_node != NULL) {
        display_tree(current_node->first_child, wm);
        if (current_node->node_type == EXTERNAL_NODE) {
            tile(wm, current_node);
        }
        display_tree(current_node->second_child, wm);
    }
}

int get_tree_level(node_t *current_node) {
    if (current_node == NULL) {
        return 0;
    }

    int level_first_child  = get_tree_level(current_node->first_child);
    int level_second_child = get_tree_level(current_node->second_child);

    return 1 + ((level_first_child > level_second_child) ? level_first_child : level_second_child);
}

void insert_under_cursor(node_t *current_node, client_t *new_client, wm_t *wm, xcb_window_t win) {
    if (current_node == NULL) {
        fprintf(stderr, "Invalid node for window: %u\n", new_client->window);
        log_message(ERROR, "current_node is null, window id %d", current_node->client->window);
        return;
    }
    log_message(DEBUG, "current->window: %d, passed window: %d", current_node->client->window, win);
    if (current_node->client->window == win) {
        if (current_node != NULL) {
            current_node->node_type = INTERNAL_NODE;
        }
        if (current_node->first_child == NULL) { // leaf node, insert here
            current_node->first_child         = create_node(current_node->client);
            current_node->first_child->parent = current_node;
            if (current_node->rectangle.width >= current_node->rectangle.height) {
                rectangle_t r1                       = {.x      = current_node->rectangle.x,
                                                        .y      = current_node->rectangle.y,
                                                        .width  = current_node->rectangle.width / 2 - W_GAP / 2,
                                                        .height = current_node->rectangle.height};
                current_node->first_child->rectangle = r1;
                current_node->first_child->node_type = EXTERNAL_NODE;
            } else {
                log_message(DEBUG, "parent 2 child rectangle: X: %d, Y: %d, Width: %u, Height: %u",
                            current_node->rectangle.x, current_node->rectangle.y, current_node->rectangle.width,
                            current_node->rectangle.height);
                rectangle_t r1                       = {.x      = current_node->rectangle.x,
                                                        .y      = current_node->rectangle.y,
                                                        .width  = current_node->rectangle.width,
                                                        .height = current_node->rectangle.height / 2 - W_GAP};
                current_node->first_child->rectangle = r1;
                current_node->first_child->node_type = EXTERNAL_NODE;
            }
            // return 0;
        } else {
            insert_under_cursor(current_node->first_child, new_client, wm, win);
        }
        if (current_node->second_child == NULL) { // insert as second child
            // current_node->node_type               = INTERNAL_NODE;
            current_node->second_child            = create_node(new_client);
            current_node->second_child->parent    = current_node;
            current_node->second_child->node_type = EXTERNAL_NODE;
            if (current_node->rectangle.width >= current_node->rectangle.height) {
                rectangle_t r2 = {.x      = current_node->rectangle.x + current_node->rectangle.width / 2 + W_GAP,
                                  .y      = current_node->rectangle.y,
                                  .width  = current_node->rectangle.width / 2 - W_GAP / 2 - W_GAP,
                                  .height = current_node->rectangle.height};
                set_rectangle(current_node->second_child, r2);
            } else {
                log_message(DEBUG, "parent 2 child rectangle: X: %d, Y: %d, Width: %u, Height: %u",
                            current_node->rectangle.x, current_node->rectangle.y, current_node->rectangle.width,
                            current_node->rectangle.height);
                rectangle_t r2 = {.x      = current_node->rectangle.x,
                                  .y      = current_node->rectangle.height / 2 + W_GAP + current_node->rectangle.y,
                                  .width  = current_node->rectangle.width,
                                  .height = current_node->rectangle.height / 2 - W_GAP};
                set_rectangle(current_node->second_child, r2);
            }

            log_message(DEBUG, "1st -- child rectangle: X: %d, Y: %d, Width: %u, Height: %u",
                        current_node->first_child->rectangle.x, current_node->first_child->rectangle.y,
                        current_node->first_child->rectangle.width, current_node->first_child->rectangle.height);
            log_message(DEBUG, "2st -- child rectangle: X: %d, Y: %d, Width: %u, Height: %u",
                        current_node->second_child->rectangle.x, current_node->second_child->rectangle.y,
                        current_node->second_child->rectangle.width, current_node->second_child->rectangle.height);
            return;
        } else {
            insert_under_cursor(current_node->second_child, new_client, wm, win);
        }
    }
    return;
}

bool is_tree_empty(node_t *root) {
    return root == NULL;
}

void set_parent(node_t *child, node_t *parent) {
    if (child == NULL) {
        return;
    }
    child->parent = parent;
}

bool is_parent_null(node_t *node) {
    return node->parent == NULL;
}

void set_rectangle(node_t *node, rectangle_t rec) {
    if (node != NULL) {
        node->rectangle = rec;
    }
}

void set_left_child(node_t *internal_node, node_t *left_child) {
    if (internal_node != NULL) {
        internal_node->first_child = left_child;
        if (left_child != NULL) {
            left_child->parent = internal_node;
        }
    }
}

void set_right_child(node_t *internal_node, node_t *right_child) {
    if (internal_node != NULL) {
        internal_node->second_child = right_child;
        if (right_child != NULL) {
            right_child->parent = internal_node;
        }
    }
}

void set_node_type(node_t *node, node_type_t type) {
    if (node != NULL) {
        node->node_type = type;
    }
}

node_t *find_node_by_window_id(node_t *root, xcb_window_t window_id) {
    if (root == NULL) {
        return NULL;
    }
    if (root->client != NULL && root->client->window == window_id) {
        return root;
    }
    node_t *left_result = find_node_by_window_id(root->first_child, window_id);
    if (left_result != NULL) {
        return left_result;
    }
    node_t *right_result = find_node_by_window_id(root->second_child, window_id);
    if (right_result != NULL) {
        return right_result;
    }
    return NULL;
}

void free_tree(node_t *root) {
    if (root == NULL) {
        return;
    }
    free_tree(root->first_child);
    free_tree(root->second_child);
    if (root->client != NULL) {
        free(root->client);
    }
    free(root);
}

bool has_sibling(node_t *node) {
    node_t *parent = node->parent;
    return parent->first_child != NULL || parent->second_child != NULL;
}

bool has_first_child(node_t *parent) {
    return parent->first_child != NULL;
}
bool has_second_child(node_t *parent) {
    return parent->second_child != NULL;
}

bool is_internal(node_t *node) {
    return node->second_child == NULL || node->first_child == NULL;
}
bool is_external(node_t *node) {
    return node->second_child == NULL && node->first_child == NULL;
}