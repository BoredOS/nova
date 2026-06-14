// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_toolbar.h"
#include "ntk_button.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_TOOLBAR_MAX_ITEMS 32

typedef struct {
    NtkWidget *item;
    bool       is_separator;
} NtkToolBarItem;

typedef struct {
    NtkToolBarItem items[NTK_TOOLBAR_MAX_ITEMS];
    int            item_count;
    int            spacing;
} NtkToolBarInstance;

static void toolbar_paint(NtkWidget *w, NtkPainter *p);
static void toolbar_layout(NtkWidget *w);
static NtkSize toolbar_preferred_size(NtkWidget *w);

static const NtkWidgetClass toolbar_class = {
    .type_name      = "NtkToolBar",
    .paint          = toolbar_paint,
    .layout         = toolbar_layout,
    .preferred_size = toolbar_preferred_size,
    .handle_event   = NULL,
    .destroy        = NULL
};

NtkWidget* ntk_toolbar_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &toolbar_class, sizeof(NtkToolBarInstance));
    if (!w) return NULL;

    NtkToolBarInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->spacing = 2;

    return w;
}

void ntk_toolbar_add_button(NtkWidget *tb, NtkWidget *btn) {
    NtkToolBarInstance *inst = ntk_widget_get_instance_data(tb);
    if (!inst || inst->item_count >= NTK_TOOLBAR_MAX_ITEMS) return;

    ntk_widget_add_child(tb, btn);
    
    NtkToolBarItem *item = &inst->items[inst->item_count++];
    item->item = btn;
    item->is_separator = false;

    toolbar_layout(tb);
    ntk_widget_repaint(tb);
}

void ntk_toolbar_add_separator(NtkWidget *tb) {
    NtkToolBarInstance *inst = ntk_widget_get_instance_data(tb);
    if (!inst || inst->item_count >= NTK_TOOLBAR_MAX_ITEMS) return;

    NtkToolBarItem *item = &inst->items[inst->item_count++];
    item->item = NULL;
    item->is_separator = true;

    toolbar_layout(tb);
    ntk_widget_repaint(tb);
}

NtkWidget* ntk_tool_button_new(const char *label, NtkPixmap *icon) {
    NtkWidget *btn = ntk_button_new_with_icon(label, icon, NULL);
    if (btn) {
        ntk_button_set_flat(btn, true);
    }
    return btn;
}
static void toolbar_paint(NtkWidget *w, NtkPainter *p) {
    NtkToolBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_TOOLBAR_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);

    for (int i = 0; i < inst->item_count; i++) {
        NtkToolBarItem *item = &inst->items[i];
        if (item->is_separator) {
            NtkRect prev_geom = ntk_widget_get_geometry(inst->items[i-1].item);
            int x = r.x + (i > 0 ? prev_geom.x + prev_geom.width + inst->spacing : 4);
            ntk_painter_set_color(p, dark);
            ntk_painter_draw_line(p, x, r.y + 4, x, r.y + r.height - 4);
            ntk_painter_set_color(p, light);
            ntk_painter_draw_line(p, x + 1, r.y + 4, x + 1, r.y + r.height - 4);
        } else if (item->item && ntk_widget_is_visible(item->item)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(item->item);
            if (klass && klass->paint) klass->paint(item->item, p);
        }
    }
}

static void toolbar_layout(NtkWidget *w) {
    NtkToolBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int curr_x = 4;
    int border = 2;

    for (int i = 0; i < inst->item_count; i++) {
        NtkToolBarItem *item = &inst->items[i];
        if (item->is_separator) {
            curr_x += 6; 
        } else if (item->item) {
            NtkSize pref = ntk_widget_get_preferred_size(item->item);
            int item_w = pref.width > 0 ? pref.width : 24;
            int item_h = geom.height - 2 * border;

            ntk_widget_set_geometry(item->item, NTK_RECT(geom.x + curr_x, geom.y + border, item_w, item_h));
            curr_x += item_w + inst->spacing;

            const NtkWidgetClass *klass = ntk_widget_get_class(item->item);
            if (klass && klass->layout) klass->layout(item->item);
        }
    }
}

static NtkSize toolbar_preferred_size(NtkWidget *w) {
    NtkToolBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkStyle *style = ntk_widget_get_style(w);

    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_TOOLBAR_HEIGHT);
    if (height <= 0) height = 28;

    int width = 8;
    for (int i = 0; i < inst->item_count; i++) {
        NtkToolBarItem *item = &inst->items[i];
        if (item->is_separator) {
            width += 6;
        } else if (item->item) {
            NtkSize pref = ntk_widget_get_preferred_size(item->item);
            width += (pref.width > 0 ? pref.width : 24) + inst->spacing;
        }
    }

    return NTK_SIZE(width, height);
}
