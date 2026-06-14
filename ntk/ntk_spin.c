// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_spin.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int  value;
    int  min_val;
    int  max_val;
    bool up_pressed;
    bool down_pressed;
} NtkSpinBoxInstance;

static void spin_paint(NtkWidget *w, NtkPainter *p);
static bool spin_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize spin_preferred_size(NtkWidget *w);

static const NtkWidgetClass spin_class = {
    .type_name      = "NtkSpinBox",
    .paint          = spin_paint,
    .layout         = NULL,
    .preferred_size = spin_preferred_size,
    .handle_event   = spin_handle_event,
    .destroy        = NULL
};

NtkWidget* ntk_spin_box_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &spin_class, sizeof(NtkSpinBoxInstance));
    if (!w) return NULL;

    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->min_val = 0;
    inst->max_val = 100;
    inst->value = 0;
    inst->up_pressed = false;
    inst->down_pressed = false;

    return w;
}

void ntk_spin_box_set_range(NtkWidget *w, int min_val, int max_val) {
    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->min_val = min_val;
        inst->max_val = max_val > min_val ? max_val : min_val;
        if (inst->value < inst->min_val) inst->value = inst->min_val;
        if (inst->value > inst->max_val) inst->value = inst->max_val;
        ntk_widget_repaint(w);
    }
}

void ntk_spin_box_set_value(NtkWidget *w, int value) {
    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        if (value < inst->min_val) value = inst->min_val;
        if (value > inst->max_val) value = inst->max_val;
        if (inst->value != value) {
            inst->value = value;
            ntk_widget_emit(w, "value-changed", &inst->value);
            ntk_widget_repaint(w);
        }
    }
}

int ntk_spin_box_get_value(NtkWidget *w) {
    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->value : 0;
}
static void spin_paint(NtkWidget *w, NtkPainter *p) {
    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor btn_bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    ntk_painter_draw_bevel_sunken(p, r, light, dark);

    int btn_w = 14;
    int btn_h = (r.height - 4) / 2;

    NtkRect up_btn = NTK_RECT(r.x + r.width - btn_w - 2, r.y + 2, btn_w, btn_h);
    NtkRect down_btn = NTK_RECT(r.x + r.width - btn_w - 2, r.y + 2 + btn_h, btn_w, btn_h);
    ntk_painter_set_color(p, btn_bg);
    ntk_painter_fill_rect(p, up_btn);
    if (inst->up_pressed) ntk_painter_draw_bevel_sunken(p, up_btn, light, dark);
    else ntk_painter_draw_bevel_raised(p, up_btn, light, dark);
    ntk_painter_fill_rect(p, down_btn);
    if (inst->down_pressed) ntk_painter_draw_bevel_sunken(p, down_btn, light, dark);
    else ntk_painter_draw_bevel_raised(p, down_btn, light, dark);
    ntk_painter_set_color(p, text_col);
    int cx = up_btn.x + up_btn.width / 2;
    int cy = up_btn.y + up_btn.height / 2;
    if (inst->up_pressed) { cx += 1; cy += 1; }
    ntk_painter_draw_line(p, cx - 3, cy + 1, cx, cy - 2);
    ntk_painter_draw_line(p, cx, cy - 2, cx + 3, cy + 1);
    cx = down_btn.x + down_btn.width / 2;
    cy = down_btn.y + down_btn.height / 2;
    if (inst->down_pressed) { cx += 1; cy += 1; }
    ntk_painter_draw_line(p, cx - 3, cy - 2, cx, cy + 1);
    ntk_painter_draw_line(p, cx, cy + 1, cx + 3, cy - 2);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d", inst->value);

    int pad_x = 4;
    int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;
    if (pad_y < 2) pad_y = 2;

    ntk_painter_draw_text(p, val_str, r.x + pad_x, r.y + pad_y);
}

static bool spin_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkSpinBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int btn_w = 14;
    int btn_h = (geom.height - 4) / 2;

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= geom.width - btn_w - 2 && p.x <= geom.width - 2) {
            if (p.y >= 2 && p.y <= 2 + btn_h) {
                inst->up_pressed = true;
                ntk_spin_box_set_value(w, inst->value + 1);
            } else if (p.y >= 2 + btn_h && p.y <= 2 + 2 * btn_h) {
                inst->down_pressed = true;
                ntk_spin_box_set_value(w, inst->value - 1);
            }
            ntk_widget_repaint(w);
            return true;
        }
    } else if (e->type == NTK_EVENT_MOUSE_RELEASE) {
        inst->up_pressed = false;
        inst->down_pressed = false;
        ntk_widget_repaint(w);
        return true;
    }
    return false;
}

static NtkSize spin_preferred_size(NtkWidget *w) {
    NtkStyle *style = ntk_widget_get_style(w);
    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_ENTRY_HEIGHT);
    if (height <= 0) height = 22;

    return NTK_SIZE(80, height);
}
