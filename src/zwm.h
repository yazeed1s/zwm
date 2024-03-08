#ifndef ZWM_ZWM_H
#define ZWM_ZWM_H

#include "type.h"
#include <xcb/xcb_icccm.h>

xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_connection_t *c);
xcb_atom_t                get_atom(char *atom_name, wm_t *w);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_connection_t *cn);
client_t                 *find_client_by_window(xcb_window_t win);
wm_t                     *init_wm();
void                      init_clients();
void                      exec_process(const char *command);
void                      add_client(client_t *new_client);
void                      free_clients();
int16_t                   get_cursor_axis(xcb_connection_t *conn, xcb_window_t window);
int                       handle_first_window(client_t *client, wm_t *wm, int idx);
int                       handle_subsequent_window(client_t *client, wm_t *wm, int idx);
int                       set_active_window_name(wm_t *w, xcb_window_t win);
int                       kill_window(xcb_window_t win, wm_t *wm);
int                       hide_window(wm_t *wm, xcb_window_t win);
int                       get_focused_desktop_idx(wm_t *w);
int                       set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state, wm_t *w);
int                       show_window(wm_t *wm, xcb_window_t win);
int                       switch_desktop(wm_t *w, uint32_t nd);
int                       resize_window(wm_t *wm, xcb_window_t win, uint16_t width, uint16_t height);
int                       move_window(wm_t *wm, xcb_window_t win, uint16_t x, uint16_t y);
int                       handle_map_request(xcb_window_t win, wm_t *wm);
int                       handle_enter_notify(wm_t *w, xcb_window_t win);
int                       tile(wm_t *wm, node_t *node);
int                       handle_leave_notify(wm_t *w, xcb_window_t win);
int change_border_attributes(xcb_connection_t *cn, client_t *cl, uint32_t b_c, uint32_t b_w, bool stack);
int change_window_attributes(xcb_connection_t *cn, xcb_window_t win, uint32_t attr, const void *);
int configure_window(xcb_connection_t *cn, xcb_window_t win, uint16_t attr, const void *);
int set_input_focus(xcb_connection_t *cn, uint8_t r, xcb_window_t win, xcb_timestamp_t t);
int handle_xcb_error(xcb_connection_t *cn, xcb_void_cookie_t c, const char *err);
#endif // ZWM_ZWM_H
