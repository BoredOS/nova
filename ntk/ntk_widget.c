// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_widget.h"
#include "ntk_event.h"
#include <stdlib.h>
#include <string.h>

#define NTK_MAX_CHILDREN     256
#define NTK_MAX_SIGNALS      16
#define NTK_MAX_USER_DATA    16
#define NTK_SIGNAL_NAME_LEN  32

typedef struct {
    char        signal[NTK_SIGNAL_NAME_LEN];
    NtkCallback cb;
    void       *userdata;
} NtkSignalSlot;

typedef struct {
    char  key[32];
    void *data;
} NtkUserDataEntry;

typedef struct {
    NtkMouseCallback    mouse_cb;     void *mouse_ud;
    NtkKeyCallback      key_cb;       void *key_ud;
    NtkScrollCallback   scroll_cb;    void *scroll_ud;
    NtkFocusCallback    focus_cb;     void *focus_ud;
    NtkResizeCallback   resize_cb;    void *resize_ud;
    NtkCallback         enter_cb;     void *enter_ud;
    NtkCallback         leave_cb;     void *leave_ud;
    NtkDragCallback     drag_cb;      void *drag_ud;
    NtkDropCallback     drop_cb;      void *drop_ud;
} NtkEventHandlers;

struct NtkWidget {
    const NtkWidgetClass *klass;
    NtkWidget  *parent;
    NtkWidget  *children[NTK_MAX_CHILDREN];
    int         child_count;
    bool        visible;
    bool        enabled;
    bool        focused;
    NtkCursor   cursor;
    NtkRect     geometry;
    NtkSize     min_size;
    NtkSize     max_size;
    NtkInsets   margin;
    NtkInsets   padding;
    NtkStyle   *style;
    char       *style_class;
    char       *tooltip;
    NtkSignalSlot signals[NTK_MAX_SIGNALS];
    int           signal_count;
    NtkUserDataEntry user_data[NTK_MAX_USER_DATA];
    int              user_data_count;
    NtkEventHandlers handlers;
    bool        needs_repaint;
    NtkRect     dirty_rect;
};

static void default_paint(NtkWidget *w, NtkPainter *p) { (void)w; (void)p; }
static void default_layout(NtkWidget *w) { (void)w; }
static NtkSize default_preferred_size(NtkWidget *w) { (void)w; return NTK_SIZE(0, 0); }
static bool default_handle_event(NtkWidget *w, NtkEvent *e) {
    if (!w || !e) return false;
    switch (e->type) {
        case NTK_EVENT_MOUSE_PRESS:
        case NTK_EVENT_MOUSE_RELEASE:
        case NTK_EVENT_MOUSE_MOVE:
            if (w->handlers.mouse_cb) {
                w->handlers.mouse_cb(w, e, w->handlers.mouse_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_KEY_PRESS:
        case NTK_EVENT_KEY_RELEASE:
            if (w->handlers.key_cb) {
                w->handlers.key_cb(w, e, w->handlers.key_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_SCROLL:
            if (w->handlers.scroll_cb) {
                w->handlers.scroll_cb(w, e, w->handlers.scroll_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_FOCUS_IN:
            if (w->handlers.focus_cb) {
                w->handlers.focus_cb(w, true, w->handlers.focus_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_FOCUS_OUT:
            if (w->handlers.focus_cb) {
                w->handlers.focus_cb(w, false, w->handlers.focus_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_RESIZE:
            if (w->handlers.resize_cb) {
                w->handlers.resize_cb(w, e->width, e->height, w->handlers.resize_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_MOUSE_ENTER:
            if (w->handlers.enter_cb) {
                w->handlers.enter_cb(w, w->handlers.enter_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_MOUSE_LEAVE:
            if (w->handlers.leave_cb) {
                w->handlers.leave_cb(w, w->handlers.leave_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_DRAG:
            if (w->handlers.drag_cb) {
                w->handlers.drag_cb(w, e, w->handlers.drag_ud);
                return e->accepted;
            }
            break;
        case NTK_EVENT_DROP:
            if (w->handlers.drop_cb) {
                w->handlers.drop_cb(w, e, w->handlers.drop_ud);
                return e->accepted;
            }
            break;
        default:
            break;
    }
    return false;
}
static void default_destroy(NtkWidget *w) { (void)w; }

static const NtkWidgetClass default_widget_class = {
    .type_name      = "NtkWidget",
    .paint          = default_paint,
    .layout         = default_layout,
    .preferred_size = default_preferred_size,
    .handle_event   = default_handle_event,
    .destroy        = default_destroy
};

NtkWidget* ntk_widget_new_with_class(NtkWidget *parent, const NtkWidgetClass *klass, size_t instance_size) {
    size_t total = sizeof(NtkWidget) + instance_size;
    NtkWidget *w = calloc(1, total);
    if (!w) return NULL;

    w->klass   = klass ? klass : &default_widget_class;
    w->visible = true;
    w->enabled = true;
    w->cursor  = NTK_CURSOR_DEFAULT;
    w->needs_repaint = true;

    if (parent) {
        ntk_widget_add_child(parent, w);
    }

    return w;
}

NtkWidget* ntk_widget_new(NtkWidget *parent) {
    return ntk_widget_new_with_class(parent, &default_widget_class, 0);
}

void ntk_widget_destroy(NtkWidget *w) {
    if (!w) return;

    extern void ntk_app_notify_widget_destroyed(NtkWidget *w);
    ntk_app_notify_widget_destroyed(w);
    if (w->parent) {
        ntk_widget_remove_child(w->parent, w);
    }
    while (w->child_count > 0) {
        ntk_widget_destroy(w->children[w->child_count - 1]);
    }
    if (w->klass && w->klass->destroy) {
        w->klass->destroy(w);
    }

    if (w->style) ntk_style_destroy(w->style);
    free(w->style_class);
    free(w->tooltip);

    free(w);
}

void ntk_widget_set_parent(NtkWidget *w, NtkWidget *parent) {
    if (!w || w->parent == parent) return;

    if (w->parent) {
        ntk_widget_remove_child(w->parent, w);
    }

    if (parent) {
        ntk_widget_add_child(parent, w);
    }
}

NtkWidget* ntk_widget_get_parent(NtkWidget *w) {
    return w ? w->parent : NULL;
}

NtkWidget** ntk_widget_get_children(NtkWidget *w, int *count_out) {
    if (!w || w->child_count == 0) {
        if (count_out) *count_out = 0;
        return NULL;
    }

    NtkWidget **result = malloc((size_t)w->child_count * sizeof(NtkWidget *));
    if (!result) {
        if (count_out) *count_out = 0;
        return NULL;
    }

    memcpy(result, w->children, (size_t)w->child_count * sizeof(NtkWidget *));
    if (count_out) *count_out = w->child_count;
    return result;
}

void ntk_widget_add_child(NtkWidget *parent, NtkWidget *child) {
    if (!parent || !child || parent->child_count >= NTK_MAX_CHILDREN) return;
    if (child->parent == parent) return;

    if (child->parent) {
        ntk_widget_remove_child(child->parent, child);
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void ntk_widget_remove_child(NtkWidget *parent, NtkWidget *child) {
    if (!parent || !child) return;

    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            for (int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent = NULL;
            return;
        }
    }
}

void ntk_widget_reorder_child(NtkWidget *parent, NtkWidget *child, int index) {
    if (!parent || !child) return;

    int current = -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) { current = i; break; }
    }
    if (current < 0) return;
    if (index < 0) index = 0;
    if (index >= parent->child_count) index = parent->child_count - 1;
    if (current == index) return;

    for (int i = current; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }

    for (int i = parent->child_count - 1; i > index; i--) {
        parent->children[i] = parent->children[i - 1];
    }
    parent->children[index] = child;
}

int ntk_widget_get_child_index(NtkWidget *parent, NtkWidget *child) {
    if (!parent || !child) return -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) return i;
    }
    return -1;
}

int ntk_widget_get_child_count(NtkWidget *w) {
    return w ? w->child_count : 0;
}

NtkWidget* ntk_widget_get_child_at(NtkWidget *w, int index) {
    if (!w || index < 0 || index >= w->child_count) return NULL;
    return w->children[index];
}
void ntk_widget_show(NtkWidget *w) {
    if (w) { w->visible = true; w->needs_repaint = true; }
}

void ntk_widget_hide(NtkWidget *w) {
    if (w) { w->visible = false; w->needs_repaint = true; }
}

bool ntk_widget_is_visible(NtkWidget *w) {
    return w ? w->visible : false;
}

void ntk_widget_set_enabled(NtkWidget *w, bool enabled) {
    if (w) { w->enabled = enabled; w->needs_repaint = true; }
}

bool ntk_widget_is_enabled(NtkWidget *w) {
    return w ? w->enabled : false;
}

void ntk_widget_set_focus(NtkWidget *w) {
    if (w) w->focused = true;
}

bool ntk_widget_has_focus(NtkWidget *w) {
    return w ? w->focused : false;
}

void ntk_widget_raise(NtkWidget *w) {
    if (!w || !w->parent) return;
    ntk_widget_reorder_child(w->parent, w, w->parent->child_count - 1);
}

void ntk_widget_lower(NtkWidget *w) {
    if (!w || !w->parent) return;
    ntk_widget_reorder_child(w->parent, w, 0);
}
void ntk_widget_set_geometry(NtkWidget *w, NtkRect rect) {
    if (w) { w->geometry = rect; w->needs_repaint = true; }
}

NtkRect ntk_widget_get_geometry(NtkWidget *w) {
    return w ? w->geometry : NTK_RECT_ZERO;
}

void ntk_widget_set_position(NtkWidget *w, int x, int y) {
    if (w) { w->geometry.x = x; w->geometry.y = y; }
}

void ntk_widget_set_size(NtkWidget *w, int width, int height) {
    if (w) { w->geometry.width = width; w->geometry.height = height; w->needs_repaint = true; }
}

NtkPoint ntk_widget_get_position(NtkWidget *w) {
    return w ? NTK_POINT(w->geometry.x, w->geometry.y) : NTK_POINT_ZERO;
}

NtkSize ntk_widget_get_size(NtkWidget *w) {
    return w ? NTK_SIZE(w->geometry.width, w->geometry.height) : NTK_SIZE_ZERO;
}

void ntk_widget_set_min_size(NtkWidget *w, NtkSize size) {
    if (w) w->min_size = size;
}

void ntk_widget_set_max_size(NtkWidget *w, NtkSize size) {
    if (w) w->max_size = size;
}

NtkSize ntk_widget_get_min_size(NtkWidget *w) {
    return w ? w->min_size : NTK_SIZE_ZERO;
}

NtkSize ntk_widget_get_max_size(NtkWidget *w) {
    return w ? w->max_size : NTK_SIZE_ZERO;
}

NtkSize ntk_widget_get_preferred_size(NtkWidget *w) {
    if (!w || !w->klass || !w->klass->preferred_size) return NTK_SIZE_ZERO;
    return w->klass->preferred_size(w);
}

void ntk_widget_resize_to_preferred(NtkWidget *w) {
    NtkSize pref = ntk_widget_get_preferred_size(w);
    if (pref.width > 0 && pref.height > 0) {
        ntk_widget_set_size(w, pref.width, pref.height);
    }
}
void ntk_widget_set_margin(NtkWidget *w, NtkInsets margin) { if (w) w->margin = margin; }
NtkInsets ntk_widget_get_margin(NtkWidget *w) { return w ? w->margin : NTK_INSETS_ZERO; }
void ntk_widget_set_padding(NtkWidget *w, NtkInsets padding) { if (w) w->padding = padding; }
NtkInsets ntk_widget_get_padding(NtkWidget *w) { return w ? w->padding : NTK_INSETS_ZERO; }
void ntk_widget_set_cursor(NtkWidget *w, NtkCursor cursor) { if (w) w->cursor = cursor; }
NtkCursor ntk_widget_get_cursor(NtkWidget *w) { return w ? w->cursor : NTK_CURSOR_DEFAULT; }

void ntk_widget_set_tooltip(NtkWidget *w, const char *text) {
    if (!w) return;
    free(w->tooltip);
    w->tooltip = text ? strdup(text) : NULL;
}

const char* ntk_widget_get_tooltip(NtkWidget *w) {
    return w ? w->tooltip : NULL;
}

void ntk_widget_set_style(NtkWidget *w, NtkStyle *style) {
    if (!w) return;
    if (w->style) ntk_style_destroy(w->style);
    w->style = style; // sinks ownership
    w->needs_repaint = true;
}

NtkStyle* ntk_widget_get_style(NtkWidget *w) {
    if (!w) return NULL;
    if (w->style) return w->style;
    return ntk_style_get_global();
}

void ntk_widget_set_style_class(NtkWidget *w, const char *class_name) {
    if (!w) return;
    free(w->style_class);
    w->style_class = class_name ? strdup(class_name) : NULL;
}
void ntk_widget_repaint(NtkWidget *w) {
    if (w) w->needs_repaint = true;
}

void ntk_widget_repaint_rect(NtkWidget *w, NtkRect rect) {
    if (!w) return;
    w->needs_repaint = true;
    w->dirty_rect = rect;
}
NtkPoint ntk_widget_map_to_global(NtkWidget *w, NtkPoint local) {
    NtkPoint result = local;
    if (w) {
        result.x += w->geometry.x;
        result.y += w->geometry.y;
    }
    return result;
}

NtkPoint ntk_widget_map_from_global(NtkWidget *w, NtkPoint global) {
    NtkPoint result = global;
    if (w) {
        result.x -= w->geometry.x;
        result.y -= w->geometry.y;
    }
    return result;
}

NtkPoint ntk_widget_map_to_widget(NtkWidget *src, NtkWidget *dst, NtkPoint p) {
    NtkPoint global = ntk_widget_map_to_global(src, p);
    return ntk_widget_map_from_global(dst, global);
}
void ntk_widget_set_user_data(NtkWidget *w, const char *key, void *data) {
    if (!w || !key) return;
    for (int i = 0; i < w->user_data_count; i++) {
        if (strcmp(w->user_data[i].key, key) == 0) {
            w->user_data[i].data = data;
            return;
        }
    }
    if (w->user_data_count < NTK_MAX_USER_DATA) {
        strncpy(w->user_data[w->user_data_count].key, key, 31);
        w->user_data[w->user_data_count].key[31] = '\0';
        w->user_data[w->user_data_count].data = data;
        w->user_data_count++;
    }
}

void* ntk_widget_get_user_data(NtkWidget *w, const char *key) {
    if (!w || !key) return NULL;
    for (int i = 0; i < w->user_data_count; i++) {
        if (strcmp(w->user_data[i].key, key) == 0) {
            return w->user_data[i].data;
        }
    }
    return NULL;
}
void ntk_widget_connect(NtkWidget *w, const char *signal, NtkCallback cb, void *userdata) {
    if (!w || !signal || !cb || w->signal_count >= NTK_MAX_SIGNALS) return;

    NtkSignalSlot *slot = &w->signals[w->signal_count++];
    strncpy(slot->signal, signal, NTK_SIGNAL_NAME_LEN - 1);
    slot->signal[NTK_SIGNAL_NAME_LEN - 1] = '\0';
    slot->cb       = cb;
    slot->userdata = userdata;
}

void ntk_widget_disconnect(NtkWidget *w, const char *signal, NtkCallback cb) {
    if (!w || !signal || !cb) return;

    for (int i = 0; i < w->signal_count; i++) {
        if (strcmp(w->signals[i].signal, signal) == 0 && w->signals[i].cb == cb) {
            for (int j = i; j < w->signal_count - 1; j++) {
                w->signals[j] = w->signals[j + 1];
            }
            w->signal_count--;
            return;
        }
    }
}

void ntk_widget_emit(NtkWidget *w, const char *signal, void *event_data) {
    if (!w || !signal) return;
    (void)event_data; 

    for (int i = 0; i < w->signal_count; i++) {
        if (strcmp(w->signals[i].signal, signal) == 0) {
            w->signals[i].cb(w, w->signals[i].userdata);
        }
    }
}
void* ntk_widget_get_instance_data(NtkWidget *w) {
    if (!w) return NULL;
    return (void *)((char *)w + sizeof(NtkWidget));
}

const NtkWidgetClass* ntk_widget_get_class(NtkWidget *w) {
    return w ? w->klass : NULL;
}

uint32_t* ntk_widget_get_pixel_buffer(NtkWidget *w) {
    NtkWidget *cur = w;
    while (cur) {
        const NtkWidgetClass *klass = ntk_widget_get_class(cur);
        if (klass && strcmp(klass->type_name, "NtkWindow") == 0) {
            extern uint32_t* ntk_window_get_pixels_internal(NtkWidget *w);
            return ntk_window_get_pixels_internal(cur);
        }
        cur = cur->parent;
    }
    return NULL;
}
void ntk_event_set_mouse_handler(NtkWidget *w, NtkMouseCallback cb, void *userdata) {
    if (w) { w->handlers.mouse_cb = cb; w->handlers.mouse_ud = userdata; }
}

void ntk_event_set_key_handler(NtkWidget *w, NtkKeyCallback cb, void *userdata) {
    if (w) { w->handlers.key_cb = cb; w->handlers.key_ud = userdata; }
}

void ntk_event_set_scroll_handler(NtkWidget *w, NtkScrollCallback cb, void *userdata) {
    if (w) { w->handlers.scroll_cb = cb; w->handlers.scroll_ud = userdata; }
}

void ntk_event_set_focus_handler(NtkWidget *w, NtkFocusCallback cb, void *userdata) {
    if (w) { w->handlers.focus_cb = cb; w->handlers.focus_ud = userdata; }
}

void ntk_event_set_resize_handler(NtkWidget *w, NtkResizeCallback cb, void *userdata) {
    if (w) { w->handlers.resize_cb = cb; w->handlers.resize_ud = userdata; }
}

void ntk_event_set_enter_handler(NtkWidget *w, NtkCallback cb, void *userdata) {
    if (w) { w->handlers.enter_cb = cb; w->handlers.enter_ud = userdata; }
}

void ntk_event_set_leave_handler(NtkWidget *w, NtkCallback cb, void *userdata) {
    if (w) { w->handlers.leave_cb = cb; w->handlers.leave_ud = userdata; }
}

void ntk_event_set_drag_handler(NtkWidget *w, NtkDragCallback cb, void *userdata) {
    if (w) { w->handlers.drag_cb = cb; w->handlers.drag_ud = userdata; }
}

void ntk_event_set_drop_handler(NtkWidget *w, NtkDropCallback cb, void *userdata) {
    if (w) { w->handlers.drop_cb = cb; w->handlers.drop_ud = userdata; }
}

bool ntk_event_propagates(NtkEvent *e) {
    return e ? e->propagates : false;
}

void ntk_event_stop_propagation(NtkEvent *e) {
    if (e) e->propagates = false;
}

NtkWidget* ntk_event_get_target(NtkEvent *e) {
    return e ? e->target : NULL;
}

NtkPoint ntk_event_get_mouse_position(NtkEvent *e) {
    return e ? e->mouse_pos : NTK_POINT_ZERO;
}

NtkMouseButton ntk_event_get_mouse_button(NtkEvent *e) {
    return e ? e->mouse_button : NTK_MOUSE_BUTTON_NONE;
}

NtkKeyCode ntk_event_get_key_code(NtkEvent *e) {
    return e ? e->key_code : 0;
}

uint32_t ntk_event_get_modifiers(NtkEvent *e) {
    return e ? e->modifiers : 0;
}
