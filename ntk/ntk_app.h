// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_APP_H
#define NTK_APP_H

#include "ntk_types.h"

// Lifecycle
NtkApp*     ntk_app_new(void);
void        ntk_app_destroy(NtkApp *app);
int         ntk_app_run(NtkApp *app);
void        ntk_app_quit(NtkApp *app);

// Global settings
void        ntk_app_set_clipboard(const char *text);
char*       ntk_app_get_clipboard(void);

// Timers
uint32_t    ntk_app_set_timer(uint32_t interval_ms, NtkTimerCallback cb, void *userdata);
void        ntk_app_kill_timer(uint32_t timer_id);

// Internal helper accessors
int         ntk_app_get_fd(void);
void        ntk_app_register_window(NtkWidget *win);
void        ntk_app_unregister_window(NtkWidget *win);
void        ntk_app_run_modal(NtkWidget *modal_win, bool *done_flag);
void        ntk_app_set_active_popup(NtkWidget *popup);
NtkWidget*  ntk_app_get_active_popup(void);
void        ntk_app_draw_windows(void);
void        ntk_app_set_custom_poll_fd(int fd, void (*cb)(int, void*), void *data);

#endif // NTK_APP_H
