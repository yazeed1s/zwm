#include "zwm.h"
#include "logger.h"
#include "tree.h"
#include "type.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#ifdef DEBUGGING
#define DEBUG(x)       fprintf(stderr, "%s\n", x);
#define DEBUGP(x, ...) fprintf(stderr, x, ##__VA_ARGS__);
#else
#define DEBUG(x)
#define DEBUGP(x, ...)
#endif

// handy xcb masks for common operations
#define XCB_MOVE_RESIZE          (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define XCB_MOVE                 (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)
#define XCB_RESIZE               (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define SUBSTRUCTURE_REDIRECTION (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define CLIENT_EVENT_MASK                                                                         \
    (XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW | \
     XCB_EVENT_MASK_LEAVE_WINDOW)
#define ROOT_EVENT_MASK                                                                         \
    (SUBSTRUCTURE_REDIRECTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS | \
     XCB_EVENT_MASK_FOCUS_CHANGE)

#define W_GAP          10
#define CLIENT_CAP     4
#define MAX_N_DESKTOPS 5
#define NUM_COLUMNS    2
#define NUM_ROWS       2

// https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
const uint16_t keycodes[] = {
    0x0077, // 'w'
    0x006c, // 'l'
    0x0068, // 'h'
    0x0073, // 's'
    0x0066, // 'f'
    0x0053, // 'S'
    0x004c, // 'L'
    0x0048, // 'H'
    0x0056, // 'V'
    0xff0d, // return
    0xffeb, // super
};

client_t                **clients; // for now this is global
int                       clients_n = 0;
xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn);
void                      init_clients();
void                      add_client(client_t *new_client);
void                      free_clients();
wm_t                     *init_wm();
int8_t                    resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height);
int8_t                    move_window(wm_t *wm, xcb_window_t window, uint16_t x, uint16_t y);
int8_t                    handle_map_request(xcb_window_t win, wm_t *wm);

xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c) {
    xcb_get_geometry_cookie_t gc = xcb_get_geometry_unchecked(c, win);
    xcb_generic_error_t      *error;
    xcb_get_geometry_reply_t *gr = xcb_get_geometry_reply(c, gc, &error);
    if (error) {
        fprintf(stderr, "Error getting geometry for window %u: %d\n", win, error->error_code);
        free(error);
        exit(EXIT_FAILURE);
    }
    if (!gr) {
        fprintf(stderr, "Failed to get geometry for window %u\n", win);
        exit(EXIT_FAILURE);
    }
    return gr;
}

client_t *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn) {
    if (clients_n == CLIENT_CAP) {
        fprintf(stderr, "Client capacity exceeded\n");
        exit(EXIT_FAILURE);
    }
    client_t *c = (client_t *)malloc(sizeof(client_t));
    if (c == 0x00) {
        fprintf(stderr, "Failed to calloc for client\n");
        exit(EXIT_FAILURE);
    }
    c->window                   = win;
    c->type                     = wtype;
    c->border_width             = -1;
    xcb_get_geometry_reply_t *g = get_geometry(c->window, cn);
    c->position_info.previous_x = c->position_info.current_x = g->x;
    c->position_info.previous_y = c->position_info.current_y = g->y;
    free(g);
    g                             = NULL;
    uint32_t             mask     = XCB_CW_EVENT_MASK;
    uint32_t             values[] = {CLIENT_EVENT_MASK};
    xcb_void_cookie_t    cookie   = xcb_change_window_attributes_checked(cn, c->window, mask, values);
    xcb_generic_error_t *error    = xcb_request_check(cn, cookie);
    if (error) {
        log_message(ERROR, "Error setting window attributes for client %u: %d\n", c->window, error->error_code);
        free(error);
        exit(EXIT_FAILURE);
    }
    return c;
}

void init_clients() {
    clients = (client_t **)malloc(CLIENT_CAP * sizeof(client_t *));
    if (clients == NULL) {
        fprintf(stderr, "Failed to malloc for clients\n");
        exit(EXIT_FAILURE);
    }
}

void add_client(client_t *new_client) {
    log_window_id(new_client->window, "Adding client to the list");
    if (clients_n == CLIENT_CAP) {
        fprintf(stderr, "Client capacity exceeded\n");
        exit(EXIT_FAILURE);
    }
    clients[clients_n++] = new_client;
}

void free_clients() {
    for (int i = 0; i < clients_n; ++i) {
        free(clients[i]);
        clients[i] = NULL;
    }
    free(clients);
    clients = NULL;
}

wm_t *init_wm() {
    int   i, default_screen;
    wm_t *w = (wm_t *)malloc(sizeof(wm_t));
    if (w == 0x00) {
        fprintf(stderr, "Failed to malloc for window manager\n");
        exit(EXIT_FAILURE);
    }

    w->connection = xcb_connect(NULL, &default_screen);
    if (xcb_connection_has_error(w->connection) > 0) {
        fprintf(stderr, "Error: Unable to open the X connection\n");
        free(w);
        w = NULL;
        exit(EXIT_FAILURE);
    }

    const xcb_setup_t    *setup = xcb_get_setup(w->connection);
    xcb_screen_iterator_t iter  = xcb_setup_roots_iterator(setup);
    for (i = 0; i < default_screen; ++i) {
        xcb_screen_next(&iter);
    }
    w->screen                     = iter.data;
    w->root_window                = w->screen->root;
    w->desktops                   = NULL;
    w->root                       = NULL;
    w->max_number_of_desktops     = MAX_N_DESKTOPS;
    w->number_of_desktops         = 0;
    w->split_type                 = VERTICAL_TYPE;
    uint32_t             mask     = XCB_CW_EVENT_MASK;
    uint32_t             values[] = {ROOT_EVENT_MASK};
    xcb_void_cookie_t    cookie   = xcb_change_window_attributes_checked(w->connection, w->root_window, mask, values);
    xcb_generic_error_t *error    = xcb_request_check(w->connection, cookie);
    if (error) {
        log_message(ERROR, "Error registering for substructure redirection events on window %u: %d\n", w->root_window,
                    error->error_code);
        free(w);
        w = NULL;
        free(error);
        exit(EXIT_FAILURE);
    }
    return w;
}

int8_t resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height) {
    const uint32_t       values[] = {width, height};
    xcb_void_cookie_t    cookie   = xcb_configure_window_checked(wm->connection, window, XCB_RESIZE, values);
    xcb_generic_error_t *error    = xcb_request_check(wm->connection, cookie);
    if (error) {
        log_message(ERROR, "Error resizing window (ID %u): %d", window, error->error_code);
        free(error);
        return -1;
    }
    return 0;
}

int8_t move_window(wm_t *wm, xcb_window_t window, uint16_t x, uint16_t y) {
    const uint32_t       values[] = {x, y};
    xcb_void_cookie_t    cookie   = xcb_configure_window_checked(wm->connection, window, XCB_MOVE, values);
    xcb_generic_error_t *error    = xcb_request_check(wm->connection, cookie);
    if (error) {
        log_message(ERROR, "Error moving window (ID %u): %d", window, error->error_code);
        free(error);
        return -1;
    }
    return 0;
}

__attribute__((unused)) xcb_get_geometry_reply_t *get_prev_client_geometry(int i, xcb_connection_t *c) {
    if (i == 0) return NULL;
    return get_geometry(clients[i - 1]->window, c);
}

__attribute__((unused)) void write_client_info_to_file(xcb_get_geometry_reply_t *g, int i, xcb_window_t win) {
    FILE *file = fopen("./client_info.txt", "a");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file for writing\n");
        return;
    }
    fprintf(file, "-------------------------------------------\n");
    fprintf(file, "Client %d Information:\n", i);
    fprintf(file, "Window ID: %u\n", win);
    fprintf(file, "Position: x = %u, y = %u\n", g->x, g->y);
    fprintf(file, "Size: width = %u, height = %u\n", g->width, g->height);
    fprintf(file, "-------------------------------------------\n");
    free(g);
    fclose(file);
}

client_t *find_client_by_window(xcb_window_t win) {
    for (int i = 0; i < clients_n; i++) {
        if (clients[i]->window == win) {
            return clients[i];
        }
    }
    return NULL;
}

int8_t change_border_attributes(xcb_connection_t *conn, client_t *client, uint32_t b_color, uint32_t b_width,
                                bool stack) {
    if (client == NULL) {
        log_message(ERROR, "Invalid client provided");
        return -1;
    }
    client->is_focused = true;
    xcb_void_cookie_t attr_cookie =
        xcb_change_window_attributes_checked(conn, client->window, XCB_CW_BORDER_PIXEL, &b_color);
    xcb_generic_error_t *error = xcb_request_check(conn, attr_cookie);
    if (error != NULL) {
        log_message(ERROR, "Failed to change window attributes: error code %d", error->error_code);
        free(error);
        return -1;
    }
    xcb_void_cookie_t width_cookie =
        xcb_configure_window_checked(conn, client->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &b_width);
    error = xcb_request_check(conn, width_cookie);
    if (error != NULL) {
        log_message(ERROR, "Failed to configure window border width: error code %d", error->error_code);
        free(error);
        return -1;
    }
    if (stack) {
        uint16_t          arg[1] = {XCB_STACK_MODE_ABOVE};
        xcb_void_cookie_t stack_cookie =
            xcb_configure_window_checked(conn, client->window, XCB_CONFIG_WINDOW_STACK_MODE, arg);
        error = xcb_request_check(conn, stack_cookie);
        if (error != NULL) {
            log_message(ERROR, "Failed to configure window stacking mode: error code %d", error->error_code);
            free(error);
            return -1;
        }
    }
    xcb_void_cookie_t focus_cookie =
        xcb_set_input_focus_checked(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->window, XCB_CURRENT_TIME);
    error = xcb_request_check(conn, focus_cookie);
    if (error != NULL) {
        log_message(ERROR, "Failed to set input focus: error code %d", error->error_code);
        free(error);
        return -1;
    }
    for (int j = 0; j < clients_n; j++) {
        if (clients[j] != client) {
            clients[j]->is_focused = false;
        }
    }
    xcb_flush(conn);
    return 0;
}

int8_t tile(wm_t *wm, node_t *node) {
    uint16_t width  = node->rectangle.width;
    uint16_t height = node->rectangle.height;
    int16_t  x      = node->rectangle.x;
    int16_t  y      = node->rectangle.y;
    if (resize_window(wm, node->client->window, width, height) != 0 ||
        move_window(wm, node->client->window, x, y) != 0) {
        return -1;
    }
    xcb_map_window(wm->connection, node->client->window);
    xcb_flush(wm->connection);
    return 0;
}

int16_t get_cursor_axis(xcb_connection_t *conn, xcb_window_t window) {
    xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, window);
    xcb_query_pointer_reply_t *p_reply  = xcb_query_pointer_reply(conn, p_cookie, NULL);
    if (!p_reply) {
        fprintf(stderr, "Failed to query pointer position\n");
        return -1;
    }
    log_message(DEBUG, "cursor: X: %d, Y: %d", p_reply->root_x, p_reply->root_y);
    int16_t x = p_reply->root_x;
    free(p_reply);
    return x;
}

xcb_window_t get_window_under_cursor(xcb_connection_t *conn, xcb_window_t window) {
    xcb_query_pointer_cookie_t p_cookie = xcb_query_pointer(conn, window);
    xcb_query_pointer_reply_t *p_reply  = xcb_query_pointer_reply(conn, p_cookie, NULL);
    if (!p_reply) {
        fprintf(stderr, "Failed to query pointer position\n");
        return -1;
    }
    xcb_window_t x = p_reply->child;
    free(p_reply);
    return x;
}

// rectangle_t *create_rectangle(){
//     rectangle_t *r = (rectangle_t*)malloc(sizeof(rectangle_t));
//     r->width = r->height = r->y = r->x = 0;
//     return r;
// }

int8_t handle_map_request(xcb_window_t win, wm_t *wm) {
    client_t *new_client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
    if (new_client == NULL) {
        fprintf(stderr, "Failed to create client for window: %u\n", win);
        return -1;
    }
    if (is_tree_empty(wm->root)) {
        // the very first window
        wm->root         = init_root();
        wm->root->client = new_client;
        rectangle_t r    = {.x      = 5,
                            .y      = 5,
                            .width  = wm->screen->width_in_pixels - W_GAP,
                            .height = wm->screen->height_in_pixels - W_GAP};
        set_rectangle(wm->root, r);
        log_message(DEBUG, "Initialized root rectangle: X: %d, Y: %d, Width: %u, Height: %u", wm->root->rectangle.x,
                    wm->root->rectangle.y, wm->root->rectangle.width, wm->root->rectangle.height);
        if (tile(wm, wm->root) != 0) {
            log_message(ERROR, "cannot display root node with window id %d", wm->root->client->window);
            return -1;
        }
        return 0;
    }
    xcb_window_t wi = get_window_under_cursor(wm->connection, wm->root_window);
    if (wi == wm->root_window || wi == 0) return 0; // dont attempt to split root_window
    node_t *n = find_node_by_window_id(wm->root, wi);
    if (n == NULL) {
        log_message(ERROR, "cannot find node with window id %d", wi);
        return -1;
    }
    insert_under_cursor(n, new_client, wm, wi);
    if (display_tree(wm->root, wm) != 0) return -1;
    return 0;
}

int8_t handle_enter_notify(xcb_connection_t *conn, xcb_window_t win, node_t *root) {
    node_t *n = find_node_by_window_id(root, win);
    if (change_border_attributes(conn, n->client, 0x83a598, 1, true) != 0) {
        return -1;
    }
    return 0;
}

int8_t handle_leave_notify(xcb_connection_t *conn, xcb_window_t win, node_t *root) {
    node_t *n = find_node_by_window_id(root, win);
    if (change_border_attributes(conn, n->client, 0x000000, 0, false) != 0) {
        return -1;
    }
    return 0;
}

int main() {
    // init_clients();
    wm_t *zwm = init_wm();
    if (zwm == 0x00) {
        fprintf(stderr, "Failed to initialize window manager\n");
        exit(EXIT_FAILURE);
    }
    xcb_flush(zwm->connection);
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(zwm->connection))) {
        // log_message(INFO, "Event type: %u", event->response_type & ~0x80);
        switch (event->response_type & ~0x80) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *map_request = (xcb_map_request_event_t *)event;
            // log_window_id(map_request->window, "Initial window id");
            if (handle_map_request(map_request->window, zwm) != 0) {
                fprintf(stderr, "Failed to handle MAP_REQUEST\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            __attribute__((unused)) xcb_unmap_notify_event_t *unmap_notify = (xcb_unmap_notify_event_t *)event;
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            __attribute__((unused)) xcb_client_message_event_t *client_message = (xcb_client_message_event_t *)event;
            break;
        }
        case XCB_CONFIGURE_REQUEST: {
            __attribute__((unused)) xcb_configure_request_event_t *config_request =
                (xcb_configure_request_event_t *)event;
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            __attribute__((unused)) xcb_configure_notify_event_t *config_notify = (xcb_configure_notify_event_t *)event;
            break;
        }
        case XCB_PROPERTY_NOTIFY: {
            __attribute__((unused)) xcb_property_notify_event_t *property_notify = (xcb_property_notify_event_t *)event;
            break;
        }
        case XCB_ENTER_NOTIFY: {
            xcb_enter_notify_event_t *enter_event = (xcb_enter_notify_event_t *)event;
            if (handle_enter_notify(zwm->connection, enter_event->event, zwm->root) != 0) {
                fprintf(stderr, "Failed to handle XCB_ENTER_NOTIFY\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            xcb_leave_notify_event_t *leave_event = (xcb_leave_notify_event_t *)event;
            handle_leave_notify(zwm->connection, leave_event->event, zwm->root);
            break;
        }
        case XCB_MOTION_NOTIFY: {
            __attribute__((unused)) xcb_motion_notify_event_t *motion_notify = (xcb_motion_notify_event_t *)event;
            break;
        }
        case XCB_BUTTON_PRESS: {
            __attribute__((unused)) xcb_button_press_event_t *button_press = (xcb_button_press_event_t *)event;
            break;
        }
        case XCB_BUTTON_RELEASE: {
            __attribute__((unused)) xcb_button_release_event_t *button_release = (xcb_button_release_event_t *)event;
            break;
        }
        case XCB_KEY_PRESS: {
            __attribute__((unused)) xcb_key_press_event_t *key_press = (xcb_key_press_event_t *)event;
            break;
        }
        case XCB_KEY_RELEASE: {
            __attribute__((unused)) xcb_key_release_event_t *key_release = (xcb_key_release_event_t *)event;
            break;
        }
        case XCB_FOCUS_IN: {
            __attribute__((unused)) xcb_focus_in_event_t *focus_in_event = (xcb_focus_in_event_t *)event;
            break;
        }
        case XCB_FOCUS_OUT: {
            __attribute__((unused)) xcb_focus_out_event_t *focus_out_event = (xcb_focus_out_event_t *)event;
            break;
        }
        case XCB_MAPPING_NOTIFY: {
            __attribute__((unused)) xcb_mapping_notify_event_t *mapping_notify = (xcb_mapping_notify_event_t *)event;
            break;
        }
        default: break;
        }
        free(event);
    }
    xcb_disconnect(zwm->connection);
    // free_clients();
    free_tree(zwm->root);
    zwm->root = NULL;
    free(zwm);
    zwm = NULL;
    return 0;
}