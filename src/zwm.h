#ifndef ZWM_ZWM_H
#define ZWM_ZWM_H

#include "type.h"
#include <xcb/xcb_icccm.h>
#include <xcb/xproto.h>

// clang-format off
extern config_t 		  conf;
extern wm_t 			  *wm;
xcb_get_geometry_reply_t *get_geometry(xcb_window_t win, xcb_conn_t *c);
xcb_atom_t                get_atom(char *atom_name, xcb_conn_t *con);
client_t                 *create_client(xcb_window_t win, xcb_atom_t wtype, xcb_conn_t *cn);
client_t                 *find_client_by_window(xcb_window_t win);
wm_t                     *init_wm();
xcb_window_t 			  get_window_under_cursor(xcb_conn_t *conn, xcb_window_t win);
bool                      window_exists(xcb_conn_t *conn, xcb_window_t win);
void                      init_clients();
void                      add_client(client_t *new_client);
void                      free_clients();
void 					  raise_window(xcb_window_t win);
void 		              lower_window(xcb_window_t win);
int16_t                   get_cursor_axis(xcb_conn_t *conn, xcb_window_t window);
int                       exec_process(const arg_t *arg);
int 					  layout_handler(const arg_t *arg);
int  					  set_fullscreen(node_t *n, bool flag);
int  					  set_fullscreen_wrapper();
int 					  flip_node_wrapper();
int 					  win_focus(xcb_window_t win, bool set_focus);
int                       handle_first_window(client_t *client, desktop_t *d);
int                       handle_subsequent_window(client_t *client, desktop_t *d);
int                       set_active_window_name(xcb_window_t win);
int                       kill_window(xcb_window_t win);
int                       hide_window(xcb_window_t win);
int                       get_focused_desktop_idx();
int                       set_window_state(xcb_window_t win, xcb_icccm_wm_state_t state);
int                       show_window(xcb_window_t win);
int 				      switch_desktop_wrapper(const arg_t *arg);
int 				      switch_desktop(const int nd);
int 					  close_or_kill_wrapper();
int 					  close_or_kill(xcb_window_t win);
int 					  horizontal_resize_wrapper(const arg_t *arg);
int 					  traverse_stack_wrapper(const arg_t *arg);
int                       resize_window(xcb_window_t win, uint16_t width, uint16_t height);
int                       move_window(xcb_window_t win, int16_t x, int16_t y);
int                       handle_map_request(xcb_map_request_event_t *ev);
int                       handle_enter_notify(const xcb_enter_notify_event_t *ev);
int                       tile(node_t *node);
int 					  set_focus(node_t *n, bool flag);
int                       handle_leave_notify(const xcb_leave_notify_event_t *ev);
int                       change_border_attr(xcb_conn_t *, xcb_window_t, uint32_t, uint32_t, bool);
int                       change_window_attr(xcb_conn_t *, xcb_window_t, uint32_t, const void *);
int                       configure_window(xcb_conn_t *, xcb_window_t, uint16_t, const void *);
int                       set_input_focus(xcb_conn_t *, uint8_t, xcb_window_t, xcb_timestamp_t );
int                       handle_xcb_error(xcb_conn_t *, xcb_void_cookie_t, const char *);
int 					  swap_node_wrapper();
#endif // ZWM_ZWM_H
