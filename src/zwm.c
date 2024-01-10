#include "zwm.h"
#include "logger.h"
#include "tree.h"
#include "type.h"
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#ifdef DEBUGGING
#define DEBUG(x)       fprintf(stderr, "%s\n", x);
#define DEBUGP(x, ...) fprintf(stderr, x, ##__VA_ARGS__);
#else
#define DEBUG(x)
#define DEBUGP(x, ...)
#endif

// handy xcb masks for common operations
#define XCB_MOVE_RESIZE \
    (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define XCB_MOVE                 (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)
#define XCB_RESIZE               (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define SUBSTRUCTURE_REDIRECTION (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define CLIENT_EVENT_MASK                                                                         \
    (XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_ENTER_WINDOW | \
     XCB_EVENT_MASK_LEAVE_WINDOW | XCB_KEY_PRESS | XCB_KEY_RELEASE)
#define ROOT_EVENT_MASK                                                                         \
    (SUBSTRUCTURE_REDIRECTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_BUTTON_PRESS | \
     XCB_EVENT_MASK_FOCUS_CHANGE)

#define MAX_N_DESKTOPS 5
#define ALT_MASK       XCB_MOD_MASK_1
#define SUPER_MASK     XCB_MOD_MASK_4
#define SHIFT_MASK     XCB_MOD_MASK_SHIFT
#define CTRL_MASK      XCB_MOD_MASK_CONTROL

// TODO: trigger functions on matches to this array
// use function pointers
// {trigger key, action key, function to execute, args if needed}
// https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
static _key_t keys[] = {
  //{SUPER_MASK, XK_Return, (function_ptr)handle_map_request}, // TODO: open terminal
    {SUPER_MASK, XK_w, (function_ptr)kill_window}  // TODO: kill window
  // TODO: rest of keys
};

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
    client_t *c = (client_t *)malloc(sizeof(client_t));
    if (c == 0x00) {
        fprintf(stderr, "Failed to calloc for client\n");
        exit(EXIT_FAILURE);
    }
    c->window       = win;
    c->type         = wtype;
    c->border_width = -1;
    // xcb_get_geometry_reply_t *g = get_geometry(c->window, cn);
    // c->position_info.previous_x = c->position_info.current_x = g->x;
    // c->position_info.previous_y = c->position_info.current_y = g->y;
    uint32_t             mask     = XCB_CW_EVENT_MASK;
    uint32_t             values[] = {CLIENT_EVENT_MASK};
    xcb_void_cookie_t    cookie   = xcb_change_window_attributes_checked(cn, c->window, mask, values);
    xcb_generic_error_t *error    = xcb_request_check(cn, cookie);
    // free(g);
    // g = NULL;
    if (error) {
        log_message(
            ERROR,
            "Error setting window attributes for client %u: %d\n",
            c->window,
            error->error_code
        );
        free(error);
        exit(EXIT_FAILURE);
    }
    return c;
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

    w->screen                 = iter.data;
    w->root_window            = w->screen->root;
    w->desktops               = NULL;
    w->root                   = NULL;
    w->max_number_of_desktops = MAX_N_DESKTOPS;
    w->number_of_desktops     = 0;
    w->split_type             = VERTICAL_TYPE;
    uint32_t mask             = XCB_CW_EVENT_MASK;
    uint32_t values[]         = {ROOT_EVENT_MASK};

    xcb_void_cookie_t cookie =
        xcb_change_window_attributes_checked(w->connection, w->root_window, mask, values);
    xcb_generic_error_t *error = xcb_request_check(w->connection, cookie);
    if (error) {
        log_message(
            ERROR,
            "Error registering for substructure redirection events on "
            "window %u: %d\n",
            w->root_window,
            error->error_code
        );
        free(w);
        w = NULL;
        free(error);
        exit(EXIT_FAILURE);
    }

    return w;
}

int8_t resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height) {
    if (window == 0 || window == XCB_NONE) return 0;
   
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
    if (window == 0 || window == XCB_NONE) return 0;
   
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

int8_t change_border_attributes(
    xcb_connection_t *conn, client_t *client, uint32_t b_color, uint32_t b_width, bool stack
) {
    if (client == NULL) {
        log_message(ERROR, "Invalid client provided");
        return -1;
    }

    client->is_focused = true;

    if (change_window_attributes(conn, client->window, XCB_CW_BORDER_PIXEL, &b_color) != 0) {
        return -1;
    }

    if (configure_window(conn, client->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &b_width) != 0) {
        return -1;
    }

    if (stack) {
        uint16_t arg[1] = {XCB_STACK_MODE_ABOVE};
        if (configure_window(conn, client->window, XCB_CONFIG_WINDOW_STACK_MODE, arg) != 0) {
            return -1;
        }
    }

    if (set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->window, XCB_CURRENT_TIME) != 0) {
        return -1;
    }

    xcb_flush(conn);
    return 0;
}

int8_t change_window_attributes(
    xcb_connection_t *conn, xcb_window_t window, uint32_t attribute, const void *value
) {
    xcb_void_cookie_t attr_cookie = xcb_change_window_attributes_checked(conn, window, attribute, value);
    return handle_xcb_error(conn, attr_cookie, "Failed to change window attributes");
}

int8_t configure_window(xcb_connection_t *conn, xcb_window_t window, uint16_t attribute, const void *value) {
    xcb_void_cookie_t config_cookie = xcb_configure_window_checked(conn, window, attribute, value);
    return handle_xcb_error(conn, config_cookie, "Failed to configure window");
}

int8_t set_input_focus(xcb_connection_t *conn, uint8_t revert_to, xcb_window_t window, xcb_timestamp_t time) {
    xcb_void_cookie_t focus_cookie = xcb_set_input_focus_checked(conn, revert_to, window, time);
    return handle_xcb_error(conn, focus_cookie, "Failed to set input focus");
}

int8_t handle_xcb_error(xcb_connection_t *conn, xcb_void_cookie_t cookie, const char *error_message) {
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL) {
        log_message(ERROR, "%s: error code %d", error_message, error->error_code);
        free(error);
        return -1;
    }
    return 0;
}

int8_t tile(wm_t *wm, node_t *node) {
    if (node == NULL) return 0;

    uint16_t width  = node->rectangle.width;
    uint16_t height = node->rectangle.height;
    int16_t  x      = node->rectangle.x;
    int16_t  y      = node->rectangle.y;
    if (resize_window(wm, node->client->window, width, height) != 0 ||
        move_window(wm, node->client->window, x, y) != 0) {
        return -1;
    }

    xcb_void_cookie_t    cookie = xcb_map_window_checked(wm->connection, node->client->window);
    xcb_generic_error_t *error  = xcb_request_check(wm->connection, cookie);
    if (error != NULL) {
        log_message(
            ERROR,
            "Errror in mapping window %d: error code %d",
            node->client->window,
            error->error_code
        );
        free(error);
        return -1;
    }
    xcb_flush(wm->connection);
    return 0;
}

int get_keysym(xcb_keycode_t keycode, xcb_connection_t *con) {
    xcb_key_symbols_t *keysyms;
    xcb_keysym_t       keysym = 0;

    if (!(keysyms = xcb_key_symbols_alloc(con))) {
        return -1;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
    xcb_key_symbols_free(keysyms);

    return keysym;
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

int8_t kill_window(xcb_connection_t *conn, xcb_window_t win, node_t *root, wm_t *wm) {
    if (win == 0) return 0;
    log_message(DEBUG, "before inner kill");
    node_t *n = find_node_by_window_id(root, win);
    if (n == NULL) return 0;

    xcb_void_cookie_t    cookie = xcb_unmap_window_checked(conn, n->client->window);
    xcb_generic_error_t *error  = xcb_request_check(wm->connection, cookie);
    if (error != NULL) {
        log_message(
            ERROR,
            "Errror in unmapping window %d: error code %d",
            n->client->window,
            error->error_code
        );
        free(error);
        return -1;
    }
    delete_node(n);

    if (display_tree(root, wm) != 0) return -1;
    log_message(DEBUG, "after inner kill");
    return 0;
}

int8_t handle_first_window(client_t *new_client, wm_t *wm) {
    wm->root         = init_root();
    wm->root->client = new_client;

    rectangle_t r = {
        .x      = 5,
        .y      = 5,
        .width  = wm->screen->width_in_pixels - W_GAP,
        .height = wm->screen->height_in_pixels - W_GAP
    };

    set_rectangle(wm->root, r);

    if (tile(wm, wm->root) != 0) {
        log_message(ERROR, "cannot display root node with window id %d", wm->root->client->window);
        return -1;
    }

    return 0;
}

int8_t handle_subsequent_window(client_t *new_client, wm_t *wm) {
    xcb_window_t wi = get_window_under_cursor(wm->connection, wm->root_window);
    if (wi == wm->root_window || wi == 0) return 0; // don't attempt to split root_window

    node_t *n = find_node_by_window_id(wm->root, wi);
    if (n == NULL || n->client == NULL) {
        log_message(ERROR, "cannot find node with window id %d", wi);
        return -1;
    }
    insert_under_cursor(n, new_client, wm, wi);
    if (display_tree(wm->root, wm) != 0) return -1;
    log_tree_nodes(wm->root);
    return 0;
}

int8_t handle_map_request(xcb_window_t win, wm_t *wm) {
    client_t *new_client = create_client(win, XCB_ATOM_WINDOW, wm->connection);

    if (new_client == NULL) {
        log_message(ERROR, "Failed to allocate client for window: %u\n", win);
        return -1;
    }

    if (is_tree_empty(wm->root)) {
        return handle_first_window(new_client, wm);
    }

    return handle_subsequent_window(new_client, wm);
}

int8_t handle_enter_notify(xcb_connection_t *conn, xcb_window_t win, node_t *root) {
    node_t *n = find_node_by_window_id(root, win);

    if (n == NULL) return -1;

    if (change_border_attributes(conn, n->client, 0x83a598, 1, true) != 0) {
        return -1;
    }
    return 0;
}

int8_t handle_leave_notify(xcb_connection_t *conn, xcb_window_t win, node_t *root) {
    node_t *n = find_node_by_window_id(root, win);

    if (n == NULL) return 0;

    if (change_border_attributes(conn, n->client, 0x000000, 0, false) != 0) {
        log_message(ERROR, "Failed to change border attr for window %d\n", n->client->window);
        return -1;
    }

    return 0;
}

int main() {
    wm_t *zwm = init_wm();
    if (zwm == 0x00) {
        fprintf(stderr, "Failed to initialize window manager\n");
        exit(EXIT_FAILURE);
    }

    xcb_flush(zwm->connection);

    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(zwm->connection))) {
        switch (event->response_type & ~0x80) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *map_request = (xcb_map_request_event_t *)event;
            if (handle_map_request(map_request->window, zwm) != 0) {
                log_message(ERROR, "Failed to handle MAP_REQUEST for window %d\n", map_request->window);
                exit(EXIT_FAILURE);
            }
            break;
        }
        case XCB_UNMAP_NOTIFY: {
            __attribute__((unused)) xcb_unmap_notify_event_t *unmap_notify =
                (xcb_unmap_notify_event_t *)event;
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            __attribute__((unused)) xcb_client_message_event_t *client_message =
                (xcb_client_message_event_t *)event;
            break;
        }
        case XCB_CONFIGURE_REQUEST: {
            __attribute__((unused)) xcb_configure_request_event_t *config_request =
                (xcb_configure_request_event_t *)event;
            break;
        }
        case XCB_CONFIGURE_NOTIFY: {
            __attribute__((unused)) xcb_configure_notify_event_t *config_notify =
                (xcb_configure_notify_event_t *)event;
            break;
        }
        case XCB_PROPERTY_NOTIFY: {
            __attribute__((unused)) xcb_property_notify_event_t *property_notify =
                (xcb_property_notify_event_t *)event;
            break;
        }
        case XCB_ENTER_NOTIFY: {
            xcb_enter_notify_event_t *enter_event = (xcb_enter_notify_event_t *)event;
            if (handle_enter_notify(zwm->connection, enter_event->event, zwm->root) != 0) {
                log_message(ERROR, "Failed to handle XCB_ENTER_NOTIFY for window %d\n", enter_event->event);
                exit(EXIT_FAILURE);
            }
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            xcb_leave_notify_event_t *leave_event = (xcb_leave_notify_event_t *)event;
            if (handle_leave_notify(zwm->connection, leave_event->event, zwm->root) != 0) {
                log_message(ERROR, "Failed to handle XCB_LEAVE_NOTIFY for window %d\n", leave_event->event);
                exit(EXIT_FAILURE);
            };
            break;
        }
        case XCB_MOTION_NOTIFY: {
            __attribute__((unused)) xcb_motion_notify_event_t *motion_notify =
                (xcb_motion_notify_event_t *)event;
            break;
        }
        case XCB_BUTTON_PRESS: {
            __attribute__((unused)) xcb_button_press_event_t *button_press =
                (xcb_button_press_event_t *)event;
            break;
        }
        case XCB_BUTTON_RELEASE: {
            __attribute__((unused)) xcb_button_release_event_t *button_release =
                (xcb_button_release_event_t *)event;
            break;
        }
        case XCB_KEY_PRESS: {
            xcb_key_press_event_t *key_press = (xcb_key_press_event_t *)event;
            xcb_keysym_t           k         = get_keysym(key_press->detail, zwm->connection);
            // log_message(DEBUG, "Key pressed: %u", k);
            if (k == XK_w) {
                log_message(DEBUG, "before kill %u", k);
                kill_window(zwm->connection, key_press->event, zwm->root, zwm);
                log_message(DEBUG, "after kill %u", k);
            }
            // for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            //     if (keys[i].keysym == k) {
            //         keys[i].fptr(zwm->connection, key_press->event, zwm->root, zwm);
            //         break;
            //     }
            // }
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
            __attribute__((unused)) xcb_mapping_notify_event_t *mapping_notify =
                (xcb_mapping_notify_event_t *)event;
            break;
        }
        default: break;
        }
        free(event);
    }
    xcb_disconnect(zwm->connection);
    free_tree(zwm->root);
    zwm->root = NULL;
    free(zwm);
    zwm = NULL;
    return 0;
}