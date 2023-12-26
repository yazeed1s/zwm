#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/* compile with -DDEBUGGING for debugging output */
#ifdef DEBUGGING
#define DEBUG(x)       fprintf(stderr, "%s\n", x);
#define DEBUGP(x, ...) fprintf(stderr, x, ##__VA_ARGS__);
#else
#define DEBUG(x)
#define DEBUGP(x, ...)
#endif

// handy xcb masks for common operations
#define XCB_MOVE_RESIZE XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
#define XCB_MOVE        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
#define XCB_RESIZE      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
// window gap
#define W_GAP           30

typedef struct {
    int previous_x, previous_y;
    int current_x, current_y;
} posxy_t;

typedef struct {
    xcb_window_t window;
    xcb_atom_t   type;
    posxy_t      position_info;
    int          border_width;
} client;

typedef struct {
    xcb_connection_t *connection;
    xcb_screen_t     *screen;
    xcb_window_t      root_window;
} wm;

static inline xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c) {
    xcb_get_geometry_cookie_t gc = xcb_get_geometry(c, win);
    xcb_get_geometry_reply_t *r  = xcb_get_geometry_reply(c, gc, NULL);
    if (!r) {
        fprintf(stderr, "Failed to get geometry for window %u\n", win);
        exit(EXIT_FAILURE);
    }

    printf("gm depth %hhu\n", r->depth);
    printf("gm x %hd\n", r->x);
    printf("gm y %hd\n", r->y);
    printf("gm width %hd\n", r->width);
    printf("gm height %hd\n", r->height);
    return r;
}

client *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn) {
    client *c = calloc(1, sizeof(client));
    if (c == NULL) {
        fprintf(stderr, "Failed to calloc for client\n");
        exit(EXIT_FAILURE);
    }

    c->window       = win;
    c->type         = wtype;
    c->border_width = -1; /* default: use global border width */

    xcb_get_geometry_reply_t *g = get_geometry(c->window, cn);
    c->position_info.previous_x = c->position_info.current_x = g->x;
    c->position_info.previous_y = c->position_info.current_y = g->y;
    free(g);

    return c;
}

wm *init_wm() {
    int i, default_screen;
    wm *w = malloc(sizeof(wm));
    if (w == NULL) {
        fprintf(stderr, "Failed to malloc for window manager\n");
        exit(EXIT_FAILURE);
    }

    w->connection = xcb_connect(NULL, &default_screen);
    if (xcb_connection_has_error(w->connection) > 0) {
        fprintf(stderr, "Error: Unable to open the X connection\n");
        exit(EXIT_FAILURE);
    }

    const xcb_setup_t    *setup = xcb_get_setup(w->connection);
    xcb_screen_iterator_t iter  = xcb_setup_roots_iterator(setup);
    for (i = 0; i < default_screen; ++i) {
        xcb_screen_next(&iter);
    }
    w->screen      = iter.data;
    w->root_window = w->screen->root;

    printf("\n");
    printf("Informations of screen %u:\n", w->screen->root);
    printf("  width.........: %d\n", w->screen->width_in_pixels);
    printf("  height........: %d\n", w->screen->height_in_pixels);
    printf("  white pixel...: %u\n", w->screen->white_pixel);
    printf("  black pixel...: %u\n", w->screen->black_pixel);
    printf("\n");
    // register for substructure redirection events on the root window
    uint32_t mask     = XCB_CW_EVENT_MASK;
    uint32_t values[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    xcb_change_window_attributes(w->connection, w->root_window, mask, values);

    return w;
}

void create_frame(wm *wm, xcb_window_t window) {
    // xcb_get_geometry_reply_t *frame_geometry = get_geometry(wm->root_window, wm->connection);
    // xcb_window_t frame              = xcb_generate_id(wm->connection);
    uint32_t frame_value_list[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_RESIZE | XCB_MOVE,
    };
    // xcb_create_window(wm->connection, XCB_COPY_FROM_PARENT, frame, wm->root_window, 100, 100, frame_geometry->width -
    // 300,
    //                   frame_geometry->height - 300, 10, // border width
    //                   XCB_WINDOW_CLASS_INPUT_OUTPUT, wm->screen->root_visual, XCB_CW_EVENT_MASK, frame_value_list);
    // xcb_reparent_window(wm->connection, window, frame, 0, 0);
    xcb_configure_window(wm->connection, window, XCB_RESIZE,
                         (const uint32_t[]){wm->screen->width_in_pixels - W_GAP, wm->screen->height_in_pixels - W_GAP});
    xcb_configure_window(wm->connection, window, XCB_MOVE, (const uint32_t[]){10, 20});
    // xcb_map_window(wm->connection, frame);
    xcb_map_window(wm->connection, window);

    xcb_set_input_focus(wm->connection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
    // free(frame_geometry);
    xcb_flush(wm->connection);
}

void handle_map_request(xcb_window_t win, wm *w) {
    printf("Received MapRequest event for window: %u\n", win);
    create_frame(w, win);
}

int main() {
    wm *zwm = init_wm();
    xcb_flush(zwm->connection);
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(zwm->connection))) {
        switch (event->response_type & ~0x80) {
        case XCB_MAP_REQUEST: {
            xcb_map_request_event_t *map_request = (xcb_map_request_event_t *)event;
            handle_map_request(map_request->window, zwm);
            break;
        }
        case XCB_DESTROY_NOTIFY:
            printf("Window Destroyed!\n");
            break;
        default:
            break;
        }
        free(event);
    }

    xcb_disconnect(zwm->connection);
    free(zwm);

    return 0;
}
