#ifndef ZWM_ZWM_H
#define ZWM_ZWM_H

#include "type.h"
#include <xcb/xcb_icccm.h>

// clang-format off
extern xcb_window_t       wbar;
xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_conn_t *c);
xcb_atom_t                get_atom(char *atom_name, xcb_conn_t *con);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *cn);
client_t                 *find_client_by_window(xcb_window_t win);
wm_t                     *init_wm();
bool                      window_exists(xcb_conn_t *conn, xcb_window_t win);
void                      init_clients();
void                      exec_process(const char *command);
void                      add_client(client_t *new_client);
void                      free_clients();
int16_t                   get_cursor_axis(xcb_conn_t *conn, xcb_window_t window);
int                       handle_first_window(client_t *client, wm_t *wm, desktop_t *d);
int                       handle_subsequent_window(client_t *client, wm_t *wm, desktop_t *d);
int                       set_active_window_name(wm_t *w, xcb_window_t win);
int                       kill_window(xcb_window_t win, wm_t *wm);
int                       hide_window(wm_t *wm, xcb_window_t win);
int                       get_focused_desktop_idx(wm_t *w);
int                       set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state, wm_t *w);
int                       show_window(wm_t *wm, xcb_window_t win);
int                       switch_desktop(wm_t *w, uint32_t nd);
int                       resize_window(wm_t *wm, xcb_window_t win, uint16_t width, uint16_t height);
int                       move_window(wm_t *wm, xcb_window_t win, int16_t x, int16_t y);
int                       handle_map_request(xcb_window_t win, wm_t *wm);
int                       handle_enter_notify(wm_t *w, xcb_enter_notify_event_t *ev);
int                       tile(wm_t *wm, node_t *node);
int                       handle_leave_notify(wm_t *w, xcb_window_t win);
int                       change_border_attr(xcb_conn_t *, client_t *, uint32_t, uint32_t, bool);
int                       change_window_attr(xcb_conn_t *, xcb_window_t, uint32_t, const void *);
int                       configure_window(xcb_conn_t *, xcb_window_t, uint16_t, const void *);
int                       set_input_focus(xcb_conn_t *, uint8_t, xcb_window_t, xcb_timestamp_t );
int                       handle_xcb_error(xcb_conn_t *, xcb_void_cookie_t, const char *);
#endif // ZWM_ZWM_H
