#ifndef ZWM_ZWM_H
#define ZWM_ZWM_H

#include "type.h"

xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn);
void                      init_clients();
void                      add_client(client_t *new_client);
void                      free_clients();
wm_t                     *init_wm();
int8_t                    resize_window(wm_t *wm, xcb_window_t window, uint16_t width, uint16_t height);
int8_t                    move_window(wm_t *wm, xcb_window_t window, uint16_t x, uint16_t y);
int16_t                   get_cursor_axis(xcb_connection_t *conn, xcb_window_t window);
int8_t                    handle_map_request(xcb_window_t win, wm_t *wm);
int8_t                    handle_enter_notify(xcb_connection_t *conn, xcb_window_t win, node_t *root);
client_t                 *find_client_by_window(xcb_window_t win);
int8_t change_border_attributes(xcb_connection_t *conn, client_t *client, uint32_t b_color, uint32_t b_width,
                                bool stack);
int8_t tile(wm_t *wm, node_t *node);
int8_t handle_leave_notify(xcb_connection_t *conn, xcb_window_t win);
#endif // ZWM_ZWM_H
