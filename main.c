#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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
#define XCB_MOVE_RESIZE              (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define XCB_MOVE                     (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)
#define XCB_RESIZE                   (XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT)
#define XCB_SUBSTRUCTURE_REDIRECTION (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define W_GAP                        10
#define CLIENT_CAP                   4
#define MAX_N_DESKTOPS               5

typedef enum { HORIZONTAL_TYPE, VERTICAL_TYPE } split_type_t;

typedef enum { HORIZONTAL_FLIP, VERTICAL_FLIP } flip_t;

typedef struct {
    uint32_t previous_x, previous_y;
    uint32_t current_x, current_y;
} posxy_t;

typedef struct {
    xcb_window_t window;
    xcb_atom_t   type;
    posxy_t      position_info;
    uint32_t     border_width;
    bool         is_focused;
    bool         is_fullscreen;
    bool         is_horizontally_split; // not sure if this will be needed
    bool         is_vertically_split;   // same here
} client_t;

typedef struct {
    bool       is_focused;
    bool       is_full; // number of clients per desktop will be limited to N;
    client_t **clients;
    int        clients_n;
} desktop_t;

typedef struct {
    xcb_connection_t *connection;
    xcb_screen_t     *screen;
    xcb_window_t      root_window;
    split_type_t      split_type;
    desktop_t       **desktops;
    uint8_t           max_number_of_desktops;
    uint8_t           number_of_desktops;
} wm_t;

client_t                **clients; // for now this is global
int                       clients_n = 0;
xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn);
void                      init_clients();
void                      add_client(client_t *new_client);
void                      free_clients();
wm_t                     *init_wm();
int                       resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height);
int                       move_window(wm_t *wm, xcb_window_t window, uint16_t x, uint16_t y);
int                       handle_map_request(xcb_window_t win, wm_t *wm);

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
    client_t *c = calloc(1, sizeof(client_t));
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
    return c;
}

void init_clients() {
    clients = malloc(CLIENT_CAP * sizeof(client_t *));
    if (clients == NULL) {
        fprintf(stderr, "Failed to malloc for clients\n");
        exit(EXIT_FAILURE);
    }
}

void add_client(client_t *new_client) {
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
    wm_t *w = malloc(sizeof(wm_t));
    if (w == 0x00) {
        fprintf(stderr, "Failed to malloc for window manager\n");
        exit(EXIT_FAILURE);
    }

    w->connection = xcb_connect(NULL, &default_screen);
    if (xcb_connection_has_error(w->connection) > 0) {
        fprintf(stderr, "Error: Unable to open the X connection\n");
        free(w);
        exit(EXIT_FAILURE);
    }

    const xcb_setup_t    *setup = xcb_get_setup(w->connection);
    xcb_screen_iterator_t iter  = xcb_setup_roots_iterator(setup);
    for (i = 0; i < default_screen; ++i) { xcb_screen_next(&iter); }
    w->screen                  = iter.data;
    w->root_window             = w->screen->root;
    w->desktops                = NULL;
    w->max_number_of_desktops  = MAX_N_DESKTOPS;
    w->number_of_desktops      = 0;
    w->split_type              = VERTICAL_TYPE;
    uint32_t          mask     = XCB_CW_EVENT_MASK;
    uint32_t          values[] = {XCB_SUBSTRUCTURE_REDIRECTION};
    xcb_void_cookie_t cookie   = xcb_change_window_attributes_checked(w->connection, w->root_window, mask, values);

    xcb_generic_error_t *error = xcb_request_check(w->connection, cookie);
    if (error) {
        fprintf(stderr, "Error registering for substructure redirection events: %d\n", error->error_code);
        free(w);
        free(error);
        exit(EXIT_FAILURE);
    }
    return w;
}

// void create_frame(wm *wm, xcb_window_t window) {
//     // xcb_get_geometry_reply_t *frame_geometry = get_geometry(wm->root_window, wm->connection);
//     // xcb_window_t frame              = xcb_generate_id(wm->connection);
//     //    uint32_t frame_value_list[] = {
//     //        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_RESIZE | XCB_MOVE,
//     //    };
//     // xcb_create_window(wm->connection, XCB_COPY_FROM_PARENT, frame, wm->root_window, 100, 100,
//     frame_geometry->width -
//     // 300,
//     //                   frame_geometry->height - 300, 10, // border width
//     //                   XCB_WINDOW_CLASS_INPUT_OUTPUT, wm->screen->root_visual, XCB_CW_EVENT_MASK,
//     frame_value_list);
//     // xcb_reparent_window(wm->connection, window, frame, 0, 0);
//     xcb_configure_window(wm->connection, window, XCB_RESIZE,
//                          (const uint32_t[]){wm->screen->width_in_pixels - W_GAP, wm->screen->height_in_pixels -
//                          W_GAP});
//     xcb_configure_window(wm->connection, window, XCB_MOVE, (const uint32_t[]){10, 20});
//     // xcb_map_window(wm->connection, frame);
//     xcb_map_window(wm->connection, window);
//
//     xcb_set_input_focus(wm->connection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
//     // free(frame_geometry);
//     xcb_flush(wm->connection);
// }

int resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height) {
    const uint32_t       values[] = {width, height};
    xcb_void_cookie_t    cookie   = xcb_configure_window_checked(wm->connection, window, XCB_RESIZE, values);
    xcb_generic_error_t *error    = xcb_request_check(wm->connection, cookie);
    if (error) {
        fprintf(stderr, "Error resizing window: %d\n", error->error_code);
        free(error);
        return -1;
    }
    return 0;
}

int move_window(wm_t *wm, xcb_window_t window, uint16_t x, uint16_t y) {
    const uint32_t       values[] = {x, y};
    xcb_void_cookie_t    cookie   = xcb_configure_window_checked(wm->connection, window, XCB_MOVE, values);
    xcb_generic_error_t *error    = xcb_request_check(wm->connection, cookie);
    if (error) {
        fprintf(stderr, "Error moving window: %d\n", error->error_code);
        free(error);
        return -1;
    }
    return 0;
}

int handle_map_request(xcb_window_t win, wm_t *wm) {
    printf("Received MapRequest event for window: %u\n", win);
    client_t *new_client = create_client(win, XCB_ATOM_WINDOW, wm->connection);
    if (new_client == 0x00) {
        fprintf(stderr, "Failed to create client for window: %u\n", win);
        return -1;
    }
    add_client(new_client);

    uint16_t width  = wm->screen->width_in_pixels - W_GAP;
    uint16_t height = wm->screen->height_in_pixels - W_GAP;
    // 1920x1200 --> x (horizontal) = 1920 pixels,  y (vertical) = 1200 pixels
    // TODO: make the resizing/moving of newly created windows(clients) dynamic
    // this is ugly
    if (clients_n == 1) {
        if (resize_window(wm, new_client->window, width, height) != 0 ||
            move_window(wm, new_client->window, W_GAP / 2, W_GAP / 2) != 0) {
            return -1;
        }
    } else if (clients_n == 2) {
        if (resize_window(wm, clients[0]->window, width / 2 - W_GAP / 2, height) != 0 ||
            move_window(wm, clients[0]->window, W_GAP / 2, W_GAP / 2) != 0) {
            return -1;
        }
        if (resize_window(wm, new_client->window, width / 2 - W_GAP / 2, height) != 0 ||
            move_window(wm, new_client->window, width / 2 + W_GAP / 2, W_GAP / 2) != 0) {
            return -1;
        }
    } else if (clients_n == 3) {
        if (resize_window(wm, clients[1]->window, width / 2 - W_GAP / 2, height / 2 - W_GAP / 2) != 0 ||
            move_window(wm, clients[1]->window, width / 2 + W_GAP / 2, W_GAP / 2) != 0) {
            return -1;
        }
        if (resize_window(wm, new_client->window, width / 2 - W_GAP / 2, height / 2 - W_GAP / 2) != 0 ||
            move_window(wm, new_client->window, width / 2 + W_GAP / 2, height / 2 + W_GAP / 2) != 0) {
            return -1;
        }
    } else if (clients_n == 4) {
        if (resize_window(wm, clients[0]->window, width / 2 - W_GAP / 2, height / 2 - W_GAP / 2) != 0 ||
            move_window(wm, clients[0]->window, W_GAP / 2, W_GAP / 2) != 0) {
            return -1;
        }
        if (resize_window(wm, new_client->window, width / 2 - W_GAP / 2, height / 2 - W_GAP / 2) != 0 ||
            move_window(wm, new_client->window, W_GAP / 2, height / 2 + W_GAP / 2) != 0) {
            return -1;
        }
    }
    //    for (int i = 0; i < clients_n; ++i) {
    //        uint16_t x = i * width + W_GAP / 2;
    //        uint16_t y = W_GAP / 2;
    //        uint16_t w = width - W_GAP;
    //        uint16_t h = height - W_GAP;
    //        if (clients_n % 2 == 0) {
    //            y = i % 2 == 0 ? W_GAP / 2 : height / 2 + W_GAP / 2;
    //            h = height / 2 - W_GAP / 2;
    //        }
    //
    //        if (resize_window(wm, clients[i]->window, w, h) != 0 || move_window(wm, clients[i]->window, x, y) != 0) {
    //            return -1;
    //        }
    //    }
    xcb_set_input_focus(wm->connection, XCB_INPUT_FOCUS_POINTER_ROOT, new_client->window, XCB_CURRENT_TIME);
    xcb_map_window(wm->connection, new_client->window);
    xcb_flush(wm->connection);
    return 0;
}

int main() {
    init_clients();
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
                fprintf(stderr, "Failed to handle MapRequest\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case XCB_DESTROY_NOTIFY: printf("Window Destroyed!\n"); break;
        default: break;
        }
        free(event);
    }
    xcb_disconnect(zwm->connection);
    free_clients();
    free(zwm);
    zwm = NULL;
    return 0;
}