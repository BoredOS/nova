// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_WIDGET_H
#define NTK_WIDGET_H
#include "ntk_types.h"
#include "ntk_event.h"
#include "ntk_style.h"
#include "ntk_painter.h"
typedef struct NtkWidgetClass {
    const char* type_name;
    void    (*paint)(NtkWidget *w, NtkPainter *p);
    void    (*layout)(NtkWidget *w);
    NtkSize (*preferred_size)(NtkWidget *w);
    bool    (*handle_event)(NtkWidget *w, NtkEvent *e);
    void    (*destroy)(NtkWidget *w);
} NtkWidgetClass;
NtkWidget*  ntk_widget_new(NtkWidget *parent);
void        ntk_widget_destroy(NtkWidget *w);
NtkWidget*  ntk_widget_new_with_class(NtkWidget *parent, const NtkWidgetClass *klass, size_t instance_size);
void        ntk_widget_set_parent(NtkWidget *w, NtkWidget *parent);
NtkWidget*  ntk_widget_get_parent(NtkWidget *w);
NtkWidget** ntk_widget_get_children(NtkWidget *w, int *count_out);
void        ntk_widget_add_child(NtkWidget *parent, NtkWidget *child);
void        ntk_widget_remove_child(NtkWidget *parent, NtkWidget *child);
void        ntk_widget_reorder_child(NtkWidget *parent, NtkWidget *child, int index);
int         ntk_widget_get_child_index(NtkWidget *parent, NtkWidget *child);
int         ntk_widget_get_child_count(NtkWidget *w);
NtkWidget*  ntk_widget_get_child_at(NtkWidget *w, int index);
void        ntk_widget_show(NtkWidget *w);
void        ntk_widget_hide(NtkWidget *w);
bool        ntk_widget_is_visible(NtkWidget *w);
void        ntk_widget_set_enabled(NtkWidget *w, bool enabled);
bool        ntk_widget_is_enabled(NtkWidget *w);
void        ntk_widget_set_focus(NtkWidget *w);
bool        ntk_widget_has_focus(NtkWidget *w);
void        ntk_widget_raise(NtkWidget *w);
void        ntk_widget_lower(NtkWidget *w);
void        ntk_widget_set_geometry(NtkWidget *w, NtkRect rect);
NtkRect     ntk_widget_get_geometry(NtkWidget *w);
void        ntk_widget_set_position(NtkWidget *w, int x, int y);
void        ntk_widget_set_size(NtkWidget *w, int width, int height);
NtkPoint    ntk_widget_get_position(NtkWidget *w);
NtkSize     ntk_widget_get_size(NtkWidget *w);
void        ntk_widget_set_min_size(NtkWidget *w, NtkSize size);
void        ntk_widget_set_max_size(NtkWidget *w, NtkSize size);
NtkSize     ntk_widget_get_min_size(NtkWidget *w);
NtkSize     ntk_widget_get_max_size(NtkWidget *w);
NtkSize     ntk_widget_get_preferred_size(NtkWidget *w);
void        ntk_widget_resize_to_preferred(NtkWidget *w);
void        ntk_widget_set_margin(NtkWidget *w, NtkInsets margin);
NtkInsets   ntk_widget_get_margin(NtkWidget *w);
void        ntk_widget_set_padding(NtkWidget *w, NtkInsets padding);
NtkInsets   ntk_widget_get_padding(NtkWidget *w);
void        ntk_widget_set_cursor(NtkWidget *w, NtkCursor cursor);
NtkCursor   ntk_widget_get_cursor(NtkWidget *w);
void        ntk_widget_set_tooltip(NtkWidget *w, const char *text);
const char* ntk_widget_get_tooltip(NtkWidget *w);
void        ntk_widget_set_style(NtkWidget *w, NtkStyle *style);
NtkStyle*   ntk_widget_get_style(NtkWidget *w);
void        ntk_widget_set_style_class(NtkWidget *w, const char *class_name);
void        ntk_widget_repaint(NtkWidget *w);
void        ntk_widget_repaint_rect(NtkWidget *w, NtkRect rect);
NtkPoint    ntk_widget_map_to_global(NtkWidget *w, NtkPoint local);
NtkPoint    ntk_widget_map_from_global(NtkWidget *w, NtkPoint global);
NtkPoint    ntk_widget_map_to_widget(NtkWidget *src, NtkWidget *dst, NtkPoint p);
void        ntk_widget_set_user_data(NtkWidget *w, const char *key, void *data);
void*       ntk_widget_get_user_data(NtkWidget *w, const char *key);
void        ntk_widget_connect(NtkWidget *w, const char *signal, NtkCallback cb, void *userdata);
void        ntk_widget_disconnect(NtkWidget *w, const char *signal, NtkCallback cb);
void        ntk_widget_emit(NtkWidget *w, const char *signal, void *event_data);
void*               ntk_widget_get_instance_data(NtkWidget *w);
const NtkWidgetClass* ntk_widget_get_class(NtkWidget *w);
uint32_t*           ntk_widget_get_pixel_buffer(NtkWidget *w);

#endif // NTK_WIDGET_H
