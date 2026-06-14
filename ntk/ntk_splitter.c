// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_splitter.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    NtkOrientation orientation;
    NtkWidget     *w1;
    NtkWidget     *w2;
    int            position;
    int            handle_width;
    bool           dragging;
    int            drag_start_pos;
    int            drag_start_offset;
} NtkSplitterInstance;

static void splitter_paint(NtkWidget *w, NtkPainter *p);
static void splitter_layout(NtkWidget *w);
static NtkSize splitter_preferred_size(NtkWidget *w);
static bool splitter_handle_event(NtkWidget *w, NtkEvent *e);

static const NtkWidgetClass splitter_class = {
    .type_name      = "NtkSplitter",
    .paint          = splitter_paint,
    .layout         = splitter_layout,
    .preferred_size = splitter_preferred_size,
    .handle_event   = splitter_handle_event,
    .destroy        = NULL
};

NtkWidget* ntk_splitter_new(NtkOrientation orientation, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &splitter_class, sizeof(NtkSplitterInstance));
    if (!w) return NULL;

    NtkSplitterInstance *inst = ntk_widget_get_instance_data(w);
    inst->orientation = orientation;
    inst->position = 150;
    inst->handle_width = 5;
    inst->dragging = false;

    return w;
}

void ntk_splitter_set_widgets(NtkWidget *splitter, NtkWidget *w1, NtkWidget *w2) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(splitter);
    if (!inst) return;

    if (inst->w1) {
        ntk_widget_remove_child(splitter, inst->w1);
        ntk_widget_destroy(inst->w1);
    }
    if (inst->w2) {
        ntk_widget_remove_child(splitter, inst->w2);
        ntk_widget_destroy(inst->w2);
    }

    inst->w1 = w1;
    inst->w2 = w2;

    if (w1) ntk_widget_add_child(splitter, w1);
    if (w2) ntk_widget_add_child(splitter, w2);

    splitter_layout(splitter);
    ntk_widget_repaint(splitter);
}

void ntk_splitter_set_position(NtkWidget *splitter, int position) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(splitter);
    if (inst) {
        if (position < 20) position = 20;
        inst->position = position;
        splitter_layout(splitter);
        ntk_widget_repaint(splitter);
    }
}

int ntk_splitter_get_position(NtkWidget *splitter) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(splitter);
    return inst ? inst->position : 0;
}
static void splitter_paint(NtkWidget *w, NtkPainter *p) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    if (inst->w1 && ntk_widget_is_visible(inst->w1)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->w1);
        if (klass && klass->paint) klass->paint(inst->w1, p);
    }
    if (inst->w2 && ntk_widget_is_visible(inst->w2)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->w2);
        if (klass && klass->paint) klass->paint(inst->w2, p);
    }

    NtkRect handle;
    if (inst->orientation == NTK_HORIZONTAL) {
        handle = NTK_RECT(r.x + inst->position, r.y, inst->handle_width, r.height);
    } else {
        handle = NTK_RECT(r.x, r.y + inst->position, r.width, inst->handle_width);
    }

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, handle);
    ntk_painter_draw_bevel_raised(p, handle, light, dark);
}

static void splitter_layout(NtkWidget *w) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int limit = (inst->orientation == NTK_HORIZONTAL) ? geom.width : geom.height;
    if (inst->position > limit - 20) inst->position = limit - 20;
    if (inst->position < 20) inst->position = 20;

    if (inst->orientation == NTK_HORIZONTAL) {
        if (inst->w1) {
            ntk_widget_set_geometry(inst->w1, NTK_RECT(geom.x, geom.y, inst->position, geom.height));
            const NtkWidgetClass *klass = ntk_widget_get_class(inst->w1);
            if (klass && klass->layout) klass->layout(inst->w1);
        }
        if (inst->w2) {
            int x = geom.x + inst->position + inst->handle_width;
            int width = geom.width - inst->position - inst->handle_width;
            ntk_widget_set_geometry(inst->w2, NTK_RECT(x, geom.y, width, geom.height));
            const NtkWidgetClass *klass = ntk_widget_get_class(inst->w2);
            if (klass && klass->layout) klass->layout(inst->w2);
        }
    } else {
        if (inst->w1) {
            ntk_widget_set_geometry(inst->w1, NTK_RECT(geom.x, geom.y, geom.width, inst->position));
            const NtkWidgetClass *klass = ntk_widget_get_class(inst->w1);
            if (klass && klass->layout) klass->layout(inst->w1);
        }
        if (inst->w2) {
            int y = geom.y + inst->position + inst->handle_width;
            int height = geom.height - inst->position - inst->handle_width;
            ntk_widget_set_geometry(inst->w2, NTK_RECT(geom.x, y, geom.width, height));
            const NtkWidgetClass *klass = ntk_widget_get_class(inst->w2);
            if (klass && klass->layout) klass->layout(inst->w2);
        }
    }
}

static NtkSize splitter_preferred_size(NtkWidget *w) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(w);
    NtkSize s1 = inst->w1 ? ntk_widget_get_preferred_size(inst->w1) : NTK_SIZE_ZERO;
    NtkSize s2 = inst->w2 ? ntk_widget_get_preferred_size(inst->w2) : NTK_SIZE_ZERO;

    if (inst->orientation == NTK_HORIZONTAL) {
        int width = s1.width + s2.width + inst->handle_width;
        int height = s1.height > s2.height ? s1.height : s2.height;
        return NTK_SIZE(width, height);
    } else {
        int height = s1.height + s2.height + inst->handle_width;
        int width = s1.width > s2.width ? s1.width : s2.width;
        return NTK_SIZE(width, height);
    }
}

static bool splitter_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkSplitterInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        int coord = (inst->orientation == NTK_HORIZONTAL) ? p.x : p.y;
        if (coord >= inst->position && coord <= inst->position + inst->handle_width) {
            inst->dragging = true;
            inst->drag_start_pos = coord;
            inst->drag_start_offset = inst->position;
            return true;
        }
    } else if (e->type == NTK_EVENT_MOUSE_RELEASE) {
        if (inst->dragging) {
            inst->dragging = false;
            return true;
        }
    } else if (e->type == NTK_EVENT_MOUSE_MOVE && inst->dragging) {
        NtkPoint p = e->mouse_pos;
        int coord = (inst->orientation == NTK_HORIZONTAL) ? p.x : p.y;
        int delta = coord - inst->drag_start_pos;
        ntk_splitter_set_position(w, inst->drag_start_offset + delta);
        return true;
    }
    return false;
}
