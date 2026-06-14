// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_WINDOW_H
#define NTK_WINDOW_H

#include "ntk_widget.h"
#include "ntk_pixmap.h"
NtkWidget*  ntk_window_new(const char *title, int width, int height);
NtkWidget*  ntk_window_new_dialog(NtkWidget *parent, const char *title, int width, int height);
NtkWidget*  ntk_window_new_popup(NtkWidget *parent, int x, int y, int width, int height);
void        ntk_window_get_screen_position(NtkWidget *win, int *x_out, int *y_out);
void        ntk_window_set_title(NtkWidget *w, const char *title);
const char* ntk_window_get_title(NtkWidget *w);
void        ntk_window_set_icon(NtkWidget *w, NtkPixmap *icon);
void        ntk_window_set_icon_path(NtkWidget *w, const char *icon_path);
NtkPixmap*  ntk_window_get_icon(NtkWidget *w);
void        ntk_window_set_content(NtkWidget *w, NtkWidget *content);
NtkWidget*  ntk_window_get_content(NtkWidget *w);
void        ntk_window_detach_content(NtkWidget *w);
void        ntk_window_set_resizable(NtkWidget *w, bool resizable);
bool        ntk_window_is_resizable(NtkWidget *w);
void        ntk_window_set_minimizable(NtkWidget *w, bool minimizable);
void        ntk_window_set_maximizable(NtkWidget *w, bool maximizable);
void        ntk_window_set_closable(NtkWidget *w, bool closable);
void        ntk_window_set_frameless(NtkWidget *w, bool frameless);
void        ntk_window_set_always_on_top(NtkWidget *w, bool on_top);
void        ntk_window_set_opacity(NtkWidget *w, float opacity);
void        ntk_window_minimize(NtkWidget *w);
void        ntk_window_maximize(NtkWidget *w);
void        ntk_window_restore(NtkWidget *w);
void        ntk_window_close(NtkWidget *w);
void        ntk_window_center_on_screen(NtkWidget *w);
void        ntk_window_center_on_parent(NtkWidget *w);
void        ntk_window_set_menubar(NtkWidget *w, NtkWidget *menubar);
void        ntk_window_set_toolbar(NtkWidget *w, NtkWidget *toolbar);
void        ntk_window_set_statusbar(NtkWidget *w, NtkWidget *statusbar);
NtkWidget*  ntk_window_get_menubar(NtkWidget *w);
NtkWidget*  ntk_window_get_toolbar(NtkWidget *w);
NtkWidget*  ntk_window_get_statusbar(NtkWidget *w);

#endif // NTK_WINDOW_H
