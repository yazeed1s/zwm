#include "tree.h"
#include "logger.h"
#include "type.h"
#include "zwm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb_icccm.h>

#define MAX(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })

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
        return 0;
    }

    if (current_node->node_type != INTERNAL_NODE && current_node->client != NULL) {
        if (tile(wm, current_node) != 0) {
            return -1;
        }
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

    return 1 + MAX(level_first_child, level_second_child);
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

    if (current_node->node_type != ROOT_NODE) {
        current_node->node_type = INTERNAL_NODE;
    }

    if (current_node->client->window == win) {
        if (!has_first_child(current_node)) {
            // client_t *nc = create_client(current_node->client->window,
            // XCB_ATOM_WINDOW, wm->connection); if (nc == NULL) {
            //     log_message(ERROR, "faild to allocate client for child");
            //     return;
            // }
            current_node->first_child = create_node(current_node->client);
            if (current_node->first_child == NULL) {
                return;
            }
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
            current_node->second_child = create_node(new_client);
            if (current_node->second_child == NULL) {
                return;
            }
            current_node->second_child->parent = current_node;
            rectangle_t r                      = {0};
            if (current_node->rectangle.width >= current_node->rectangle.height) {
                r.x      = (int16_t
                )(current_node->rectangle.x + current_node->rectangle.width / 2 + W_GAP);
                r.y      = current_node->rectangle.y;
                r.width  = current_node->rectangle.width / 2 - W_GAP / 2 - W_GAP;
                r.height = current_node->rectangle.height;
            } else {
                r.x      = current_node->rectangle.x;
                r.y      = (int16_t
                )(current_node->rectangle.height / 2 + W_GAP + current_node->rectangle.y);
                r.width  = current_node->rectangle.width;
                r.height = current_node->rectangle.height / 2 - W_GAP;
            }
            current_node->second_child->rectangle = r;
            current_node->second_child->node_type = EXTERNAL_NODE;
        }
        //     insert_under_cursor(current_node->second_child, new_client, win);
        // }
    } else {
        log_message(DEBUG, "window id bring no match");
        return;
    }
}

node_t *find_node_by_window_id(node_t *root, xcb_window_t win) {
    if (root == NULL) {
        return NULL;
    }

    if (root->client != NULL && root->client->window == win) {
        return root;
    }

    node_t *left_result = find_node_by_window_id(root->first_child, win);
    if (left_result != NULL) {
        return left_result;
    }

    node_t *right_result = find_node_by_window_id(root->second_child, win);
    if (right_result != NULL) {
        return right_result;
    }

    return NULL;
}

void free_tree(node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->client != NULL) {
        free(root->client);
        root->client = NULL;
    }

    node_t *f = root->first_child;
    free_tree(f);
    root->first_child = NULL;

    node_t *s = root->second_child;
    free_tree(s);
    root->second_child = NULL;

    free(root);
}

void resize_subtree(node_t *parent) {
    if (parent == NULL) {
        return;
    }

    rectangle_t r, r2 = {0};

    if (parent->rectangle.width >= parent->rectangle.height) {
        r.x      = parent->rectangle.x;
        r.y      = parent->rectangle.y;
        r.width  = parent->rectangle.width / 2 - W_GAP / 2;
        r.height = parent->rectangle.height;
    } else {
        r.x      = parent->rectangle.x;
        r.y      = parent->rectangle.y;
        r.width  = parent->rectangle.width;
        r.height = parent->rectangle.height / 2 - W_GAP;
    }

    if (parent->rectangle.width >= parent->rectangle.height) {
        r2.x      = (int16_t)(parent->rectangle.x + parent->rectangle.width / 2 + W_GAP);
        r2.y      = parent->rectangle.y;
        r2.width  = parent->rectangle.width / 2 - W_GAP / 2 - W_GAP;
        r2.height = parent->rectangle.height;
    } else {
        r2.x      = parent->rectangle.x;
        r2.y      = (int16_t)(parent->rectangle.height / 2 + W_GAP + parent->rectangle.y);
        r2.width  = parent->rectangle.width;
        r2.height = parent->rectangle.height / 2 - W_GAP;
    }

    parent->first_child->rectangle  = r;
    parent->second_child->rectangle = r2;

    if (parent->first_child->node_type == INTERNAL_NODE) {
        resize_subtree(parent->first_child);
    }

    if (parent->second_child->node_type == INTERNAL_NODE) {
        resize_subtree(parent->second_child);
    }
}

bool has_sibling(const node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return false;
    }

    const node_t *parent = node->parent;

    return (parent->first_child != NULL && parent->second_child != NULL);
}

bool has_internal_sibling(const node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return false;
    }

    const node_t *parent = node->parent;

    return (parent->first_child != NULL && parent->second_child != NULL) &&
           ((parent->first_child->node_type == INTERNAL_NODE) ||
            (parent->second_child->node_type == INTERNAL_NODE));
}

bool is_sibling_external(const node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return false;
    }

    node_t *parent = node->parent;
    const node_t *sibling =
        (parent->first_child == node) ? parent->second_child : parent->first_child;

    return (sibling != NULL && sibling->node_type == EXTERNAL_NODE);
}

node_t *get_external_sibling(const node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return NULL;
    }

    const node_t *parent = node->parent;
    node_t *sibling =
        (parent->first_child == node) ? parent->second_child : parent->first_child;

    return (sibling != NULL && sibling->node_type == EXTERNAL_NODE) ? sibling : NULL;
}

bool is_sibling_internal(const node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return false;
    }

    node_t *parent = node->parent;
    const node_t *sibling =
        (parent->first_child == node) ? parent->second_child : parent->first_child;

    return (sibling != NULL && sibling->node_type == INTERNAL_NODE);
}

node_t *get_internal_sibling(node_t *node) {
    if (node == NULL || node->parent == NULL) {
        return NULL;
    }

    node_t *parent = node->parent;
    node_t *sibling =
        (parent->first_child == node) ? parent->second_child : parent->first_child;

    return (sibling != NULL && sibling->node_type == INTERNAL_NODE) ? sibling : NULL;
}

node_t *get_sibling(node_t *node, node_type_t type) {
    if (node == NULL || node->parent == NULL) {
        return NULL;
    }

    node_t *parent = node->parent;
    node_t *sibling =
        (parent->first_child == node) ? parent->second_child : parent->first_child;

    switch (type) {
    case INTERNAL_NODE: {
        return (sibling != NULL && sibling->node_type == INTERNAL_NODE) ? sibling : NULL;
    }
    case EXTERNAL_NODE: {
        return (sibling != NULL && sibling->node_type == EXTERNAL_NODE) ? sibling : NULL;
    }
    case ROOT_NODE: break;
    }
    return NULL;
}

bool has_external_children(const node_t *parent) {
    return (parent->first_child != NULL && parent->first_child->node_type == EXTERNAL_NODE
           ) &&
           (parent->second_child != NULL && parent->second_child->node_type == EXTERNAL_NODE
           );
}

node_t *find_tree_root(node_t *node) {
    if (node->node_type == ROOT_NODE) {
        return node;
    }
    return find_tree_root(node->parent);
}

bool has_single_external_child(const node_t *parent) {
    if (parent == NULL) return false;

    return ((parent->first_child != NULL && parent->second_child != NULL) &&
            (parent->first_child->node_type == EXTERNAL_NODE &&
             parent->second_child->node_type != EXTERNAL_NODE)) ||
           ((parent->first_child != NULL && parent->second_child != NULL) &&
            (parent->second_child->node_type == EXTERNAL_NODE &&
             parent->first_child->node_type != EXTERNAL_NODE));
}

client_t *find_client_by_window_id(node_t *root, xcb_window_t win) {
    if (root == NULL) {
        return NULL;
    }

    if (root->client != NULL && root->client->window == win) {
        return root->client;
    }

    node_t *left_result = find_node_by_window_id(root->first_child, win);
    if (left_result == NULL) {
        return NULL;
    }

    if (left_result->client != NULL) {
        return left_result->client;
    }

    node_t *right_result = find_node_by_window_id(root->second_child, win);
    if (right_result == NULL) {
        return NULL;
    }

    if (right_result->client != NULL) {
        return right_result->client;
    }

    return NULL;
}

int delete_node_with_external_sibling(node_t *node) {
    /* node to delete = N, internal node = I, external node = E
     *         I
     *    	 /   \
     *     N||E   N||E
     *
     * logic:
     * just delete N and replace E with I and give it full I's rectangle
     */
    log_message(DEBUG, "parent of node has two external children");
    node_t *external_node = NULL;
    if (is_sibling_external(node)) {
        external_node = get_external_sibling(node);
    }

    if (external_node == NULL) {
        log_message(ERROR, "external node is null");
        return -1;
    }

    // if I has no parent
    if (node->parent->parent == NULL) {
        node->parent->node_type    = ROOT_NODE;
        node->parent->client       = external_node->client;
        node->parent->first_child  = NULL;
        node->parent->second_child = NULL;
    } else { // if I has a prent
        /*
         *         I
         *    	 /   \
         *   	E     I
         *    	 	/   \
         *   	   N     E
         */
        node_t *grandparent = node->parent->parent;
        if (grandparent->first_child == node->parent) {
            grandparent->first_child->node_type    = EXTERNAL_NODE;
            grandparent->first_child->client       = external_node->client;
            grandparent->first_child->first_child  = NULL;
            grandparent->first_child->second_child = NULL;
        } else {
            grandparent->second_child->node_type    = EXTERNAL_NODE;
            grandparent->second_child->client       = external_node->client;
            grandparent->second_child->first_child  = NULL;
            grandparent->second_child->second_child = NULL;
        }
    }
    free(external_node);
    external_node = NULL;
    free(node->client);
    node->client = NULL;
    free(node);
    node = NULL;
    return 0;
}

int delete_node_with_internal_sibling(node_t *node, desktop_t *d) {
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
     * logic:
     * if N in left subtree, delete it and replace its parent with the parent of
     * N's parent right subtree if N in right subtree,delete it and replace its
     * parent with the parent of N's parent left subtree basically IN and N are
     * deleted, and IE takes IN's place and rectangle and IE's children are
     * resized
     */
    node_t *internal_sibling = NULL;
    // if IN has no parent
    if (node->parent->parent == NULL) {
        log_message(DEBUG, "node's parent's parent is null");
        if (is_sibling_internal(node)) {
            internal_sibling = get_internal_sibling(node);
        }

        if (internal_sibling == NULL) {
            log_message(ERROR, "intertal node is null");
            return -1;
        }

        internal_sibling->rectangle = node->parent->rectangle;
        internal_sibling->parent    = NULL;
        internal_sibling->node_type = ROOT_NODE;
        /* if (wm->root == node->parent) { */
        /*     node->parent = NULL; */
        /*     if (wm->root->first_child == node) { */
        /*         wm->root->first_child                  =
         * internal_sibling->first_child; */
        /*         internal_sibling->first_child->parent  = wm->root; */
        /*         wm->root->second_child                 =
         * internal_sibling->second_child; */
        /*         internal_sibling->second_child->parent = wm->root; */
        /*     } else { */
        /*         wm->root->second_child                 =
         * internal_sibling->first_child; */
        /*         internal_sibling->first_child->parent  = wm->root; */
        /*         wm->root->first_child                  =
         * internal_sibling->second_child; */
        /*         internal_sibling->second_child->parent = wm->root; */
        /*     } */
        /* } */
        if (d->tree == node->parent) {
            node->parent = NULL;
            if (d->tree->first_child == node) {
                d->tree->first_child                   = internal_sibling->first_child;
                internal_sibling->first_child->parent  = d->tree;
                d->tree->second_child                  = internal_sibling->second_child;
                internal_sibling->second_child->parent = d->tree;
            } else {
                d->tree->second_child                  = internal_sibling->first_child;
                internal_sibling->first_child->parent  = d->tree;
                d->tree->first_child                   = internal_sibling->second_child;
                internal_sibling->second_child->parent = d->tree;
            }
        }

        free(internal_sibling);
        internal_sibling = NULL;
        free(node->client);
        node->client = NULL;
        free(node);
        node = NULL;
        /* resize_subtree(wm->root); */
        resize_subtree(d->tree);
        return 0;
    } else { // if IN has a parent
        /*            ...
         *              \
         *              IN
         *       	  /   \
         *           N     IE
         *                 / \
         *               E1   E2
         */
        log_message(DEBUG, "node's parent's parent is null");
        if (is_sibling_internal(node)) {
            internal_sibling = get_internal_sibling(node);
        } else {
            log_message(ERROR, "intertal node is null");
            return -1;
        }
        internal_sibling->parent = NULL;
        if (node->parent->first_child == node) {
            node->parent->first_child              = internal_sibling->first_child;
            internal_sibling->first_child->parent  = node->parent;
            node->parent->second_child             = internal_sibling->second_child;
            internal_sibling->second_child->parent = node->parent;
        } else {
            node->parent->second_child             = internal_sibling->first_child;
            internal_sibling->first_child->parent  = node->parent;
            node->parent->first_child              = internal_sibling->second_child;
            internal_sibling->second_child->parent = node->parent;
        }
        free(internal_sibling);
        internal_sibling = NULL;
        free(node->client);
        node->client = NULL;
        resize_subtree(node->parent);
        free(node);
        node = NULL;
        return 0;
    }
}

void delete_node(node_t *node, desktop_t *d) {
    if (node == NULL) {
        log_message(DEBUG, "node to be deleted is null");
        return;
    }

    if (node->node_type == INTERNAL_NODE) {
        log_message(
            DEBUG,
            "node to be deleted is not an external node type: %d",
            node->node_type
        );
        return;
    }

    if (is_parent_null(node) && node != d->tree) {
        log_message(DEBUG, "parent of node is null");
        return;
    }

    /* if (is_parent_null(node) && node != wm->root) { */
    /*     log_message(DEBUG, "parent of node is null"); */
    /*     return; */
    /* } */

    // node_t *parent = node->parent;

    // early check if only single node/client is mapped to the screen
    if (node == d->tree) {
        free(node->client);
        node->client = NULL;
        free(node);
        node    = NULL;
        d->tree = NULL;
        return;
    }

    /*
     * case 1: node's parent has two external children
     *         I
     *    	 /   \
     *   	E     E
     */
    // if (has_external_children(parent)) {
    if (is_sibling_external(node)) {
        xcb_window_t w = node->client->window;
        if (delete_node_with_external_sibling(node) != 0) {
            log_message(ERROR, "cannot delete node with id: %d", w);
            return;
        }
        return;
    }

    /*
     * case 2: node's parent has one internal child
     * in other words, node has an internal sibling
     *         I
     *    	 /   \
     *   	E     I
     *             \
     *            rest...
     */
    if (is_sibling_internal(node)) {
        xcb_window_t w = node->client->window;
        if (delete_node_with_internal_sibling(node, d) != 0) {
            log_message(ERROR, "cannot delete node with id: %d", w);
            return;
        }
        return;
    }
}

bool has_first_child(const node_t *parent) {
    return parent->first_child != NULL;
}

bool has_second_child(const node_t *parent) {
    return parent->second_child != NULL;
}

bool is_internal(const node_t *node) {
    return node->node_type == INTERNAL_NODE;
}

bool is_external(const node_t *node) {
    return node->node_type == EXTERNAL_NODE;
}

bool is_tree_empty(const node_t *root) {
    return root == NULL;
}

bool is_parent_null(const node_t *node) {
    return node->parent == NULL;
}

bool is_parent_internal(const node_t *node) {
    return node->parent->node_type == INTERNAL_NODE;
}

void log_tree_nodes(node_t *node, wm_t *wm) {
    if (node != NULL) {
        if (node->client != NULL) {
            xcb_icccm_get_text_property_reply_t t_reply;
            xcb_get_property_cookie_t           cn =
                xcb_icccm_get_wm_name(wm->connection, node->client->window);
            uint8_t wr = xcb_icccm_get_wm_name_reply(wm->connection, cn, &t_reply, NULL);
            char    name[256];
            if (wr == 1) {
                snprintf(name, sizeof(name), "%s", t_reply.name);
                xcb_icccm_get_text_property_reply_wipe(&t_reply);
            }
            log_message(
                DEBUG,
                "Node Type: %d, Client Window ID: %u, name: %s",
                node->node_type,
                node->client->window,
                name
            );
        } else {
            log_message(DEBUG, "Node Type: %d", node->node_type);
        }
        log_tree_nodes(node->first_child, wm);
        log_tree_nodes(node->second_child, wm);
    }
}

int hide_windows(node_t *cn, wm_t *wm) {
    if (cn == NULL) {
        return 0;
    }

    if (cn->node_type != INTERNAL_NODE && cn->client != NULL) {
        if (hide_window(wm, cn->client->window) != 0) {
            return -1;
        }
    }

    hide_windows(cn->first_child, wm);
    hide_windows(cn->second_child, wm);

    return 0;
}

int show_windows(node_t *cn, wm_t *wm) {
    if (cn == NULL) {
        return 0;
    }

    if (cn->node_type != INTERNAL_NODE && cn->client != NULL) {
        if (show_window(wm, cn->client->window) != 0) {
            return -1;
        }
    }

    show_windows(cn->first_child, wm);
    show_windows(cn->second_child, wm);

    return 0;
}

bool client_exist(node_t *cn, xcb_window_t win) {
    if (cn == NULL) {
        return false;
    }

    if (cn->client != NULL) {
        log_message(DEBUG, "Checking node with win ID: %d", cn->client->window);
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

bool in_left_subtree(node_t *first_left_child, node_t *n) {
    if (first_left_child == NULL) {
        return false;
    }

    if (first_left_child == n || first_left_child->first_child == n ||
        first_left_child->second_child == n) {
        return true;
    }

    if (in_left_subtree(first_left_child->first_child, n)) {
        return true;
    }

    if (in_left_subtree(first_left_child->second_child, n)) {
        return true;
    }

    return false;
}

bool in_right_subtree(node_t *first_right_child, node_t *n) {
    if (first_right_child == NULL) {
        return false;
    }

    if (first_right_child == n || first_right_child->first_child == n ||
        first_right_child->second_child == n) {
        return true;
    }

    if (in_right_subtree(first_right_child->first_child, n)) {
        return true;
    }

    if (in_right_subtree(first_right_child->second_child, n)) {
        return true;
    }

    return false;
}

void horizontal_resize(node_t *n, resize_t t) {
    const int16_t px               = 5;
    direction_t   grow_direction   = NONE;
    direction_t   shrink_direction = NONE;

    node_t *root = find_tree_root(n);
    assert(root->node_type == ROOT_NODE);

    if (in_left_subtree(root->first_child, n)) {
        grow_direction   = RIGHT;
        shrink_direction = LEFT;
    } else if (in_right_subtree(root->second_child, n)) {
        grow_direction   = LEFT;
        shrink_direction = RIGHT;
    }
    log_message(INFO, "dir == %d", grow_direction);

    if (n->parent == NULL || n->node_type == ROOT_NODE) {
        return;
    }

    /*
     * case 1: node's parent is the root.
     * I = Internal node
     * E = external node
     *
     * I's type is ROOT_NODE
     * Node to delete is E.
     * logic: grow E's width by 5 pixels
     * then move E's sibling x by 5 pixles and shrink its width by 5 pixels
     * E's sibling can be External or Internal -> if internal, resize its children
     *         I
     *    	 /   \
     *   	E     E/I
     *             \
     *            rest...
     */
    if (n->parent->node_type == ROOT_NODE) {
        node_t *s = NULL;
        if (is_sibling_external(n)) {
            s = get_external_sibling(n);
            if (!s) {
                return;
            }
            if (t == GROW) {
                n->rectangle.width += px;
                s->rectangle.width -= px;
                grow_direction == RIGHT ? (s->rectangle.x += px) : (n->rectangle.x -= px);
                return;
            } else {
                // shrink
                n->rectangle.width -= px;
                s->rectangle.width += px;
                (shrink_direction == LEFT) ? (s->rectangle.x -= px) : (n->rectangle.x += px);
                return;
            }
        } else {
            // sibling is internal
            s = get_internal_sibling(n);
            if (!s) {
                return;
            }
            if (t == GROW) {
                n->rectangle.width += px;
                s->rectangle.width -= px;
                grow_direction == RIGHT ? (s->rectangle.x += px) : (n->rectangle.x -= px);
                resize_subtree(s);
                return;
            } else {
                n->rectangle.width -= px;
                s->rectangle.width += px;
                (shrink_direction == LEFT) ? (s->rectangle.x -= px) : (n->rectangle.x += px);
                resize_subtree(s);
                return;
            }
        }
    }
    /*
     * case 2: node's parent is not the root (it is INTERNAL_NODE).
     * I = Internal node
     * E = external node
     *
     * I's type is INTERNAL_NODE
     * Node to expand is E.
     * logic: grow the whole subtree(rectangle)'s width in E's side by 5 pixels
     * then move the opposite subtree(rectangle)'s x by 5 pixles and shrink its width
     * by 5 pixels
     *         I
     *    	 /   \
     *   	I     E/I
     *    /   \     \
     *   E    E/I   rest...
     */
    if (n->parent->node_type == INTERNAL_NODE) {
        if (in_left_subtree(root->first_child, n)) {
            log_message(INFO, "node is in left subtree");
            if (root->first_child == NULL || root->first_child->node_type != INTERNAL_NODE) {
                return;
            }
            (t == GROW) ? (root->first_child->rectangle.width += px)
                        : (root->first_child->rectangle.width -= px);
            (t == GROW) ? (root->second_child->rectangle.width -= px)
                        : (root->second_child->rectangle.width += px);
            (t == GROW) ? (root->second_child->rectangle.x += px)
                        : (root->second_child->rectangle.x -= px);

            if (root->second_child != NULL &&
                root->second_child->node_type == EXTERNAL_NODE) {
                resize_subtree(root->first_child);
                return;
            } else {
                resize_subtree(root->first_child);
                resize_subtree(root->second_child);
                return;
            }
        } else {
            log_message(INFO, "node is in right subtree");
            if (root->second_child == NULL ||
                root->second_child->node_type != INTERNAL_NODE) {
                return;
            }
            (t == GROW) ? (root->second_child->rectangle.width += px)
                        : (root->second_child->rectangle.width -= px);
            (t == GROW) ? (root->second_child->rectangle.x -= px)
                        : (root->second_child->rectangle.x += px);
            (t == GROW) ? (root->first_child->rectangle.width -= px)
                        : (root->first_child->rectangle.width += px);

            if (root->first_child != NULL && root->first_child->node_type == EXTERNAL_NODE) {
                resize_subtree(root->second_child);
                return;
            } else {
                resize_subtree(root->first_child);
                resize_subtree(root->second_child);
                return;
            }
        }
    }
}
