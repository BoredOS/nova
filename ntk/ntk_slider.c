// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_slider.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    NtkOrientation orientation;
    int            value;
    int            min_val;
    int            max_val;
    bool           dragging;
} NtkSliderInstance;

static void slider_paint(NtkWidget *w, NtkPainter *p);
static bool slider_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize slider_preferred_size(NtkWidget *w);

static const NtkWidgetClass slider_class = {
    .type_name      = "NtkSlider",
    .paint          = slider_paint,
    .layout         = NULL,
    .preferred_size = slider_preferred_size,
    .handle_event   = slider_handle_event,
    .destroy        = NULL
};

NtkWidget* ntk_slider_new(NtkOrientation orientation, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &slider_class, sizeof(NtkSliderInstance));
    if (!w) return NULL;

    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    inst->orientation = orientation;
    inst->min_val = 0;
    inst->max_val = 100;
    inst->value = 0;
    inst->dragging = false;

    return w;
}

void ntk_slider_set_range(NtkWidget *w, int min_val, int max_val) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->min_val = min_val;
        inst->max_val = max_val > min_val ? max_val : min_val;
        if (inst->value < inst->min_val) inst->value = inst->min_val;
        if (inst->value > inst->max_val) inst->value = inst->max_val;
        ntk_widget_repaint(w);
    }
}

void ntk_slider_set_value(NtkWidget *w, int value) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
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

int ntk_slider_get_value(NtkWidget *w) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->value : 0;
}
static void slider_paint(NtkWidget *w, NtkPainter *p) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    int range = inst->max_val - inst->min_val;
    if (range <= 0) range = 1;

    int thumb_w = 10;
    int thumb_h = 16;
    if (inst->orientation == NTK_VERTICAL) {
        thumb_w = 16;
        thumb_h = 10;
    }
    NtkRect track;
    if (inst->orientation == NTK_HORIZONTAL) {
        int track_h = 4;
        int track_y = r.y + (r.height - track_h) / 2;
        track = NTK_RECT(r.x + thumb_w / 2, track_y, r.width - thumb_w, track_h);
    } else {
        int track_w = 4;
        int track_x = r.x + (r.width - track_w) / 2;
        track = NTK_RECT(track_x, r.y + thumb_h / 2, track_w, r.height - thumb_h);
    }
    ntk_painter_draw_bevel_sunken(p, track, light, dark);
    NtkRect thumb;
    if (inst->orientation == NTK_HORIZONTAL) {
        int travel = r.width - thumb_w;
        int pos = ((inst->value - inst->min_val) * travel) / range;
        thumb = NTK_RECT(r.x + pos, r.y + (r.height - thumb_h) / 2, thumb_w, thumb_h);
    } else {
        int travel = r.height - thumb_h;
        int pos = ((inst->value - inst->min_val) * travel) / range;
        thumb = NTK_RECT(r.x + (r.width - thumb_w) / 2, r.y + travel - pos, thumb_w, thumb_h);
    }
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, thumb);
    ntk_painter_draw_bevel_raised(p, thumb, light, dark);
}

static bool slider_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    int range = inst->max_val - inst->min_val;

    int thumb_size = (inst->orientation == NTK_HORIZONTAL) ? 10 : 16;

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        inst->dragging = true;
    }

    if (e->type == NTK_EVENT_MOUSE_RELEASE) {
        inst->dragging = false;
        return true;
    }

    if (inst->dragging && (e->type == NTK_EVENT_MOUSE_MOVE || e->type == NTK_EVENT_MOUSE_PRESS)) {
        NtkPoint p = e->mouse_pos;
        if (inst->orientation == NTK_HORIZONTAL) {
            int travel = geom.width - thumb_size;
            int offset = p.x - thumb_size / 2;
            if (offset < 0) offset = 0;
            if (offset > travel) offset = travel;
            int value = inst->min_val + (offset * range + travel / 2) / travel;
            ntk_slider_set_value(w, value);
        } else {
            int travel = geom.height - thumb_size;
            int offset = travel - (p.y - thumb_size / 2);
            if (offset < 0) offset = 0;
            if (offset > travel) offset = travel;
            int value = inst->min_val + (offset * range + travel / 2) / travel;
            ntk_slider_set_value(w, value);
        }
        return true;
    }

    return false;
}

static NtkSize slider_preferred_size(NtkWidget *w) {
    NtkSliderInstance *inst = ntk_widget_get_instance_data(w);
    if (inst->orientation == NTK_HORIZONTAL) {
        return NTK_SIZE(100, 20);
    } else {
        return NTK_SIZE(20, 100);
    }
}
