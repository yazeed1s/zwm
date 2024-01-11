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
    node_t *node = (node_t *)malloc(sizeof(node_t));
    if (node == 0x00) {
        return NULL;
    }
    node->client       = c;
    node->id           = 1;
    node->parent       = NULL;
    node->first_child  = NULL;
    node->second_child = NULL;
    return node;
}

node_t *init_root() {
    node_t *node = (node_t *)malloc(sizeof(node_t));
    if (node == 0x00) {
        return NULL;
    }
    node->client       = NULL;
    node->id           = 1;
    node->parent       = NULL;
    node->first_child  = NULL;
    node->second_child = NULL;
    node->node_type    = ROOT_NODE;
    return node;
}

int display_tree(node_t *current_node, wm_t *wm) {

    if (current_node == NULL) {
        log_message(DEBUG, "early returns in display_tree()");
        return 0;
    }

    if (current_node->node_type == EXTERNAL_NODE) {
        if (tile(wm, current_node) != 0) return -1;
    }

    display_tree(current_node->first_child, wm);
    display_tree(current_node->second_child, wm);

    return 0;
}

int get_tree_level(node_t *current_node) {
    if (current_node == NULL) {
        return 0;
    }

    int level_first_child  = get_tree_level(current_node->first_child);
    int level_second_child = get_tree_level(current_node->second_child);

    return 1 + ((level_first_child > level_second_child) ? level_first_child : level_second_child);
}

void insert_under_cursor(node_t *current_node, client_t *new_client, xcb_window_t win) {
    if (current_node == NULL) {
        log_message(ERROR, "current_node is null");
        return;
    }

    if (current_node->client == NULL) {
        log_message(ERROR, "client is null in current_node");
        return;
    }

    current_node->node_type = INTERNAL_NODE;

    if (current_node->client->window == win) {
        if (!has_first_child(current_node)) {
            // client_t *nc = create_client(current_node->client->window, XCB_ATOM_WINDOW, wm->connection);
            // if (nc == NULL) {
            //     log_message(ERROR, "faild to allocate client for child");
            //     return;
            // }
            current_node->first_child         = create_node(current_node->client);
            current_node->first_child->parent = current_node;
            rectangle_t r                     = {0};
            if (current_node->rectangle.width >= current_node->rectangle.height) {
                r.x      = current_node->rectangle.x;
                r.y      = current_node->rectangle.y;
                r.width  = current_node->rectangle.width / 2 - W_GAP / 2;
                r.height = current_node->rectangle.height;
            } else {
                r.x      = current_node->rectangle.x;
                r.y      = current_node->rectangle.y;
                r.width  = current_node->rectangle.width;
                r.height = current_node->rectangle.height / 2 - W_GAP;
            }
            current_node->first_child->rectangle = r;
            current_node->first_child->node_type = EXTERNAL_NODE;
            // free(current_node->client);
            current_node->client = NULL;
        }
        // else {
        //     insert_under_cursor(current_node->first_child, new_client, win);
        // }
        if (!has_second_child(current_node)) {
            current_node->second_child         = create_node(new_client);
            current_node->second_child->parent = current_node;
            rectangle_t r                      = {0};
            if (current_node->rectangle.width >= current_node->rectangle.height) {
                r.x      = current_node->rectangle.x + current_node->rectangle.width / 2 + W_GAP;
                r.y      = current_node->rectangle.y;
                r.width  = current_node->rectangle.width / 2 - W_GAP / 2 - W_GAP;
                r.height = current_node->rectangle.height;
            } else {
                r.x      = current_node->rectangle.x;
                r.y      = current_node->rectangle.height / 2 + W_GAP + current_node->rectangle.y;
                r.width  = current_node->rectangle.width;
                r.height = current_node->rectangle.height / 2 - W_GAP;
            }
            current_node->second_child->rectangle = r;
            current_node->second_child->node_type = EXTERNAL_NODE;
        }
        // else {
        //     insert_under_cursor(current_node->second_child, new_client, win);
        // }
    } else {
        log_message(DEBUG, "window id bring no match");
        return;
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
    if (node == NULL) {
        return;
    }
    node->rectangle = rec;
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

    node_t *f = root->first_child;
    node_t *s = root->second_child;

    if (root->client != NULL) {
        free(root->client);
    }

    free(root);
    free_tree(f);
    free_tree(s);
}

void delete_node(node_t *node) {
    if (node == NULL || node->node_type != EXTERNAL_NODE) {
        log_message(
            DEBUG,
            "node to be deleted is either null or not an external node type: %d",
            node->node_type
        );
        return;
    }
    if (is_parent_null(node)) return;
    node_t *parent = node->parent;
    if (parent->first_child == node) {
        log_message(
            DEBUG,
            "node to be deleted: X: %d, Y: %d, Width: %u, Height: %u",
            parent->first_child->rectangle.x,
            parent->first_child->rectangle.y,
            parent->first_child->rectangle.width,
            parent->first_child->rectangle.height
        );
        if (parent->second_child != NULL) {
            log_message(
                DEBUG,
                "brother of node to be deleted before resize: X: %d, Y: %d, Width: %u, Height: %u",
                parent->second_child->rectangle.x,
                parent->second_child->rectangle.y,
                parent->second_child->rectangle.width,
                parent->second_child->rectangle.height
            );
            parent->second_child->rectangle = parent->rectangle;
            log_message(
                DEBUG,
                "brother of node to be deleted after resize: X: %d, Y: %d, Width: %u, Height: %u",
                parent->second_child->rectangle.x,
                parent->second_child->rectangle.y,
                parent->second_child->rectangle.width,
                parent->second_child->rectangle.height
            );
            free(parent->first_child->client);
            free(parent->first_child);
            parent->first_child = NULL;
        }
    } else if (parent->second_child == node) {
        log_message(
            DEBUG,
            "node to be deleted: X: %d, Y: %d, Width: %u, Height: %u",
            parent->second_child->rectangle.x,
            parent->second_child->rectangle.y,
            parent->second_child->rectangle.width,
            parent->second_child->rectangle.height
        );
        if (parent->first_child != NULL) {
            log_message(
                DEBUG,
                "brother of node to be deleted before resize: X: %d, Y: %d, Width: %u, Height: %u",
                parent->first_child->rectangle.x,
                parent->first_child->rectangle.y,
                parent->first_child->rectangle.width,
                parent->first_child->rectangle.height
            );
            parent->first_child->rectangle = parent->rectangle;
            log_message(
                DEBUG,
                "brother of node to be deleted after resize: X: %d, Y: %d, Width: %u, Height: %u",
                parent->first_child->rectangle.x,
                parent->first_child->rectangle.y,
                parent->first_child->rectangle.width,
                parent->first_child->rectangle.height
            );
            free(parent->second_child->client);
            free(parent->second_child);
            parent->second_child = NULL;
        }
    }
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
    return node->second_child != NULL || node->first_child != NULL || node->node_type == INTERNAL_NODE;
}

bool is_external(node_t *node) {
    return node->second_child == NULL && node->first_child == NULL && node->node_type == EXTERNAL_NODE;
}

void log_tree_nodes(node_t *node) {
    if (node != NULL) {
        if (node->client != NULL) {
            log_message(DEBUG, "Node Type: %d, Client Window ID: %u", node->node_type, node->client->window);
        } else {
            log_message(DEBUG, "Node Type: %d", node->node_type);
        }
        log_tree_nodes(node->first_child);
        log_tree_nodes(node->second_child);
    }
}