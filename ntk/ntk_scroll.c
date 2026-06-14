// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_scroll.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    NtkOrientation orientation;
    int            value;
    int            min_val;
    int            max_val;
    int            page_size;
    bool           dragging;
    int            drag_start_val;
    NtkPoint       drag_start_pos;
    bool           btn1_pressed;
    bool           btn2_pressed;
} NtkScrollBarInstance;

static void scrollbar_paint(NtkWidget *w, NtkPainter *p);
static bool scrollbar_handle_event(NtkWidget *w, NtkEvent *e);

static const NtkWidgetClass scrollbar_class = {
    .type_name      = "NtkScrollBar",
    .paint          = scrollbar_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = scrollbar_handle_event,
    .destroy        = NULL
};

NtkWidget* ntk_scrollbar_new(NtkOrientation orientation, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &scrollbar_class, sizeof(NtkScrollBarInstance));
    if (!w) return NULL;

    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(w);
    inst->orientation = orientation;
    inst->min_val = 0;
    inst->max_val = 100;
    inst->page_size = 10;
    inst->value = 0;

    return w;
}

void ntk_scrollbar_set_range(NtkWidget *sb, int min_val, int max_val, int page_size) {
    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(sb);
    if (inst) {
        inst->min_val = min_val;
        inst->max_val = max_val > min_val ? max_val : min_val;
        inst->page_size = page_size > 0 ? page_size : 1;
        if (inst->value < inst->min_val) inst->value = inst->min_val;
        if (inst->value > inst->max_val) inst->value = inst->max_val;
        ntk_widget_repaint(sb);
    }
}

void ntk_scrollbar_set_value(NtkWidget *sb, int value) {
    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(sb);
    if (inst) {
        if (value < inst->min_val) value = inst->min_val;
        if (value > inst->max_val) value = inst->max_val;
        if (inst->value != value) {
            inst->value = value;
            ntk_widget_emit(sb, "value-changed", &inst->value);
            ntk_widget_repaint(sb);
        }
    }
}

int ntk_scrollbar_get_value(NtkWidget *sb) {
    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(sb);
    return inst ? inst->value : 0;
}

static void scrollbar_paint(NtkWidget *w, NtkPainter *p) {
    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_SCROLLBAR_BG);
    NtkColor thumb_col = ntk_style_get_color(style, NTK_STYLE_ROLE_SCROLLBAR_THUMB);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    // Draw track
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);

    int btn_size = (inst->orientation == NTK_VERTICAL) ? r.width : r.height;
    if (btn_size < 12) btn_size = 12;

    NtkRect btn1, btn2;
    if (inst->orientation == NTK_VERTICAL) {
        btn1 = NTK_RECT(r.x, r.y, r.width, btn_size);
        btn2 = NTK_RECT(r.x, r.y + r.height - btn_size, r.width, btn_size);
    } else {
        btn1 = NTK_RECT(r.x, r.y, btn_size, r.height);
        btn2 = NTK_RECT(r.x + r.width - btn_size, r.y, btn_size, r.height);
    }

    ntk_painter_set_color(p, thumb_col);
    ntk_painter_fill_rect(p, btn1);
    if (inst->btn1_pressed) ntk_painter_draw_bevel_sunken(p, btn1, light, dark);
    else ntk_painter_draw_bevel_raised(p, btn1, light, dark);
    ntk_painter_fill_rect(p, btn2);
    if (inst->btn2_pressed) ntk_painter_draw_bevel_sunken(p, btn2, light, dark);
    else ntk_painter_draw_bevel_raised(p, btn2, light, dark);
    int range = inst->max_val - inst->min_val;
    if (range <= 0) return;

    int track_len = (inst->orientation == NTK_VERTICAL) ? r.height - 2 * btn_size : r.width - 2 * btn_size;
    if (track_len <= 10) return;

    int thumb_len = (inst->page_size * track_len) / (range + inst->page_size);
    if (thumb_len < 12) thumb_len = 12;

    int thumb_pos = ((inst->value - inst->min_val) * (track_len - thumb_len)) / range;

    NtkRect thumb;
    if (inst->orientation == NTK_VERTICAL) {
        thumb = NTK_RECT(r.x + 1, r.y + btn_size + thumb_pos, r.width - 2, thumb_len);
    } else {
        thumb = NTK_RECT(r.x + btn_size + thumb_pos, r.y + 1, thumb_len, r.height - 2);
    }

    ntk_painter_set_color(p, thumb_col);
    ntk_painter_fill_rect(p, thumb);
    ntk_painter_draw_bevel_raised(p, thumb, light, dark);
}

static bool scrollbar_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkScrollBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    int btn_size = (inst->orientation == NTK_VERTICAL) ? geom.width : geom.height;
    if (btn_size < 12) btn_size = 12;

    int range = inst->max_val - inst->min_val;

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        int mouse_coord = (inst->orientation == NTK_VERTICAL) ? p.y : p.x;
        int max_coord = (inst->orientation == NTK_VERTICAL) ? geom.height : geom.width;

        if (mouse_coord < btn_size) {
            inst->btn1_pressed = true;
            ntk_scrollbar_set_value(w, inst->value - 1);
        } else if (mouse_coord > max_coord - btn_size) {
            inst->btn2_pressed = true;
            ntk_scrollbar_set_value(w, inst->value + 1);
        } else {
            int track_len = max_coord - 2 * btn_size;
            int thumb_len = (inst->page_size * track_len) / (range + inst->page_size);
            if (thumb_len < 12) thumb_len = 12;
            int thumb_pos = ((inst->value - inst->min_val) * (track_len - thumb_len)) / range;

            int thumb_start = btn_size + thumb_pos;
            if (mouse_coord >= thumb_start && mouse_coord <= thumb_start + thumb_len) {
                inst->dragging = true;
                inst->drag_start_val = inst->value;
                inst->drag_start_pos = e->mouse_pos;
            } else {
                if (mouse_coord < thumb_start) {
                    ntk_scrollbar_set_value(w, inst->value - inst->page_size);
                } else {
                    ntk_scrollbar_set_value(w, inst->value + inst->page_size);
                }
            }
        }
        ntk_widget_repaint(w);
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_RELEASE) {
        inst->btn1_pressed = false;
        inst->btn2_pressed = false;
        inst->dragging = false;
        ntk_widget_repaint(w);
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_MOVE && inst->dragging) {
        int mouse_delta = (inst->orientation == NTK_VERTICAL) ? (e->mouse_pos.y - inst->drag_start_pos.y) : (e->mouse_pos.x - inst->drag_start_pos.x);
        int max_coord = (inst->orientation == NTK_VERTICAL) ? geom.height : geom.width;
        int track_len = max_coord - 2 * btn_size;
        int thumb_len = (inst->page_size * track_len) / (range + inst->page_size);
        if (thumb_len < 12) thumb_len = 12;

        int scrollable_len = track_len - thumb_len;
        if (scrollable_len > 0) {
            int val_delta = (mouse_delta * range) / scrollable_len;
            ntk_scrollbar_set_value(w, inst->drag_start_val + val_delta);
        }
        return true;
    }
    return false;
}
static void viewport_paint(NtkWidget *w, NtkPainter *p);

static const NtkWidgetClass viewport_class = {
    .type_name      = "NtkViewport",
    .paint          = viewport_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = NULL,
    .destroy        = NULL
};

NtkWidget* ntk_viewport_new(NtkWidget *parent) {
    return ntk_widget_new_with_class(parent, &viewport_class, 0);
}

static void viewport_paint(NtkWidget *w, NtkPainter *p) {
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));

    NtkRect clip = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    ntk_painter_set_clip_rect(p, clip);

    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->paint) klass->paint(child, p);
        }
    }

    ntk_painter_clear_clip(p);
}
typedef struct {
    NtkWidget      *viewport;
    NtkWidget      *content;
    NtkWidget      *v_scrollbar;
    NtkWidget      *h_scrollbar;
    NtkScrollPolicy h_policy;
    NtkScrollPolicy v_policy;
    int            offset_x;
    int            offset_y;
} NtkScrollAreaInstance;

static void scroll_area_paint(NtkWidget *w, NtkPainter *p);
static void scroll_area_layout(NtkWidget *w);
static NtkSize scroll_area_preferred_size(NtkWidget *w);
static void scroll_area_destroy(NtkWidget *w);

static const NtkWidgetClass scroll_area_class = {
    .type_name      = "NtkScrollArea",
    .paint          = scroll_area_paint,
    .layout         = scroll_area_layout,
    .preferred_size = scroll_area_preferred_size,
    .handle_event   = NULL,
    .destroy        = scroll_area_destroy
};

static void on_scroll_value_changed(NtkWidget *sb, void *userdata) {
    NtkWidget *scroll = (NtkWidget *)userdata;
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);

    int val = ntk_scrollbar_get_value(sb);
    if (sb == inst->v_scrollbar) {
        inst->offset_y = val;
    } else {
        inst->offset_x = val;
    }
    scroll_area_layout(scroll);
    ntk_widget_repaint(scroll);
}

NtkWidget* ntk_scroll_area_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &scroll_area_class, sizeof(NtkScrollAreaInstance));
    if (!w) return NULL;

    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(w);
    inst->h_policy = NTK_SCROLL_AUTO;
    inst->v_policy = NTK_SCROLL_AUTO;
    inst->viewport = ntk_viewport_new(w);
    inst->v_scrollbar = ntk_scrollbar_new(NTK_VERTICAL, w);
    inst->h_scrollbar = ntk_scrollbar_new(NTK_HORIZONTAL, w);

    ntk_widget_connect(inst->v_scrollbar, "value-changed", on_scroll_value_changed, w);
    ntk_widget_connect(inst->h_scrollbar, "value-changed", on_scroll_value_changed, w);

    return w;
}

void ntk_scroll_area_set_content(NtkWidget *scroll, NtkWidget *content) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);
    if (inst->content) {
        ntk_widget_remove_child(inst->viewport, inst->content);
        ntk_widget_destroy(inst->content);
    }
    inst->content = content;
    if (content) {
        ntk_widget_add_child(inst->viewport, content);
    }
    scroll_area_layout(scroll);
    ntk_widget_repaint(scroll);
}

NtkWidget* ntk_scroll_area_get_content(NtkWidget *scroll) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);
    return inst->content;
}

void ntk_scroll_area_set_policy(NtkWidget *scroll, NtkScrollPolicy h_policy, NtkScrollPolicy v_policy) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);
    if (inst) {
        inst->h_policy = h_policy;
        inst->v_policy = v_policy;
        scroll_area_layout(scroll);
        ntk_widget_repaint(scroll);
    }
}

void ntk_scroll_area_scroll_to(NtkWidget *scroll, int x, int y) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);
    if (inst) {
        ntk_scrollbar_set_value(inst->h_scrollbar, x);
        ntk_scrollbar_set_value(inst->v_scrollbar, y);
    }
}

void ntk_scroll_area_get_offsets(NtkWidget *scroll, int *x_out, int *y_out) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(scroll);
    if (inst) {
        if (x_out) *x_out = inst->offset_x;
        if (y_out) *y_out = inst->offset_y;
    }
}

static void scroll_area_paint(NtkWidget *w, NtkPainter *p) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(w);

    if (inst->viewport && ntk_widget_is_visible(inst->viewport)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->viewport);
        if (klass && klass->paint) klass->paint(inst->viewport, p);
    }
    if (inst->v_scrollbar && ntk_widget_is_visible(inst->v_scrollbar)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->v_scrollbar);
        if (klass && klass->paint) klass->paint(inst->v_scrollbar, p);
    }
    if (inst->h_scrollbar && ntk_widget_is_visible(inst->h_scrollbar)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->h_scrollbar);
        if (klass && klass->paint) klass->paint(inst->h_scrollbar, p);
    }
}

static void scroll_area_layout(NtkWidget *w) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    NtkSize content_pref = inst->content ? ntk_widget_get_preferred_size(inst->content) : NTK_SIZE_ZERO;

    int sb_width = 16;

    bool need_v = (inst->v_policy == NTK_SCROLL_ALWAYS) || 
                  (inst->v_policy == NTK_SCROLL_AUTO && content_pref.height > geom.height);
    bool need_h = (inst->h_policy == NTK_SCROLL_ALWAYS) || 
                  (inst->h_policy == NTK_SCROLL_AUTO && content_pref.width > geom.width);

    if (need_v && !need_h && inst->h_policy == NTK_SCROLL_AUTO) {
        if (content_pref.width > geom.width - sb_width) need_h = true;
    }
    if (need_h && !need_v && inst->v_policy == NTK_SCROLL_AUTO) {
        if (content_pref.height > geom.height - sb_width) need_v = true;
    }

    int vp_w = geom.width - (need_v ? sb_width : 0);
    int vp_h = geom.height - (need_h ? sb_width : 0);

    ntk_widget_set_geometry(inst->viewport, NTK_RECT(geom.x, geom.y, vp_w, vp_h));

    if (need_v) {
        ntk_widget_show(inst->v_scrollbar);
        ntk_widget_set_geometry(inst->v_scrollbar, NTK_RECT(geom.x + vp_w, geom.y, sb_width, vp_h));
        ntk_scrollbar_set_range(inst->v_scrollbar, 0, content_pref.height - vp_h, vp_h);
    } else {
        ntk_widget_hide(inst->v_scrollbar);
        inst->offset_y = 0;
    }

    if (need_h) {
        ntk_widget_show(inst->h_scrollbar);
        ntk_widget_set_geometry(inst->h_scrollbar, NTK_RECT(geom.x, geom.y + vp_h, vp_w, sb_width));
        ntk_scrollbar_set_range(inst->h_scrollbar, 0, content_pref.width - vp_w, vp_w);
    } else {
        ntk_widget_hide(inst->h_scrollbar);
        inst->offset_x = 0;
    }

    if (inst->content) {
        int cw = content_pref.width > vp_w ? content_pref.width : vp_w;
        int ch = content_pref.height > vp_h ? content_pref.height : vp_h;
        ntk_widget_set_geometry(inst->content, NTK_RECT(geom.x - inst->offset_x, geom.y - inst->offset_y, cw, ch));

        const NtkWidgetClass *klass = ntk_widget_get_class(inst->content);
        if (klass && klass->layout) klass->layout(inst->content);
    }
}

static NtkSize scroll_area_preferred_size(NtkWidget *w) {
    NtkScrollAreaInstance *inst = ntk_widget_get_instance_data(w);
    if (inst->content) {
        return ntk_widget_get_preferred_size(inst->content);
    }
    return NTK_SIZE(100, 100);
}

static void scroll_area_destroy(NtkWidget *w) {
    (void)w;
}
