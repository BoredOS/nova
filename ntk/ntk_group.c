// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_group.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    bool  checkable;
    bool  checked;
} NtkGroupBoxInstance;
static void group_paint(NtkWidget *w, NtkPainter *p);
static void group_layout(NtkWidget *w);
static NtkSize group_preferred_size(NtkWidget *w);
static bool group_handle_event(NtkWidget *w, NtkEvent *e);
static void group_destroy(NtkWidget *w);

static const NtkWidgetClass group_class = {
    .type_name      = "NtkGroupBox",
    .paint          = group_paint,
    .layout         = group_layout,
    .preferred_size = group_preferred_size,
    .handle_event   = group_handle_event,
    .destroy        = group_destroy
};

static void update_enabled_state(NtkWidget *w) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    bool enabled = !inst->checkable || inst->checked;
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child) {
            ntk_widget_set_enabled(child, enabled);
        }
    }
}

NtkWidget* ntk_group_box_new(const char *title, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &group_class, sizeof(NtkGroupBoxInstance));
    if (!w) return NULL;

    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->title = title ? strdup(title) : NULL;
    inst->checkable = false;
    inst->checked = true;

    return w;
}

void ntk_group_box_set_title(NtkWidget *w, const char *title) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        free(inst->title);
        inst->title = title ? strdup(title) : NULL;
        ntk_widget_repaint(w);
    }
}

const char* ntk_group_box_get_title(NtkWidget *w) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->title : NULL;
}

void ntk_group_box_set_checkable(NtkWidget *w, bool checkable) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->checkable = checkable;
        update_enabled_state(w);
        ntk_widget_repaint(w);
    }
}

bool ntk_group_box_is_checkable(NtkWidget *w) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->checkable : false;
}

void ntk_group_box_set_checked(NtkWidget *w, bool checked) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->checked = checked;
        update_enabled_state(w);
        ntk_widget_repaint(w);
    }
}

bool ntk_group_box_is_checked(NtkWidget *w) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->checked : false;
}
static void group_paint(NtkWidget *w, NtkPainter *p) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    int top_offset = 8;
    NtkRect frame = NTK_RECT(r.x, r.y + top_offset, r.width, r.height - top_offset);
    ntk_painter_draw_bevel_sunken(p, frame, light, dark);
    if (inst->title && strlen(inst->title) > 0) {
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, font);

        int text_x = r.x + 12;
        int text_y = r.y;

        int cb_size = 12;
        if (inst->checkable) {
            NtkRect cb_r = NTK_RECT(text_x, text_y, cb_size, cb_size);
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG));
            ntk_painter_fill_rect(p, cb_r);
            ntk_painter_draw_bevel_sunken(p, cb_r, light, dark);
            
            if (inst->checked) {
                ntk_painter_set_color(p, text_col);
                ntk_painter_draw_line(p, cb_r.x + 2, cb_r.y + 5, cb_r.x + 5, cb_r.y + 8);
                ntk_painter_draw_line(p, cb_r.x + 5, cb_r.y + 8, cb_r.x + 9, cb_r.y + 3);
            }
            text_x += cb_size + 6;
        }

        NtkSize text_size = ntk_font_measure_text(font, inst->title);
        NtkRect clear_r = NTK_RECT(r.x + 8, r.y + top_offset - 2, text_x - (r.x + 8) + text_size.width + 4, 4);
        ntk_painter_set_color(p, bg);
        ntk_painter_fill_rect(p, clear_r);
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, inst->title, text_x, text_y);
    }
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->paint) klass->paint(child, p);
        }
    }
}

static void group_layout(NtkWidget *w) {
    NtkRect geom = ntk_widget_get_geometry(w);
    int top_margin = 20;
    int border = 8;

    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child) {
            ntk_widget_set_geometry(child, NTK_RECT(geom.x + border, geom.y + top_margin, geom.width - 2 * border, geom.height - top_margin - border));
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->layout) klass->layout(child);
        }
    }
}

static NtkSize group_preferred_size(NtkWidget *w) {
    int max_w = 100;
    int max_h = 50;

    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            NtkSize pref = ntk_widget_get_preferred_size(child);
            if (pref.width > max_w) max_w = pref.width;
            if (pref.height > max_h) max_h = pref.height;
        }
    }

    return NTK_SIZE(max_w + 16, max_h + 28);
}

static bool group_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && inst->checkable) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 12 && p.x <= 36 && p.y >= 0 && p.y <= 16) {
            ntk_group_box_set_checked(w, !inst->checked);
            return true;
        }
    }
    return false;
}

static void group_destroy(NtkWidget *w) {
    NtkGroupBoxInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->title);
}
