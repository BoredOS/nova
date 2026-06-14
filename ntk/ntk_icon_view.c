// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_icon_view.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_ICON_VIEW_MAX_ITEMS 32

typedef struct {
    char      *label;
    NtkPixmap *icon;
} NtkIconViewItem;

typedef struct {
    NtkIconViewItem items[NTK_ICON_VIEW_MAX_ITEMS];
    int             item_count;
    int             selected_index;
    int             item_width;
    int             item_height;
} NtkIconViewInstance;

static void icon_view_paint(NtkWidget *w, NtkPainter *p);
static bool icon_view_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize icon_view_preferred_size(NtkWidget *w);
static void icon_view_destroy(NtkWidget *w);

static const NtkWidgetClass icon_view_class = {
    .type_name      = "NtkIconView",
    .paint          = icon_view_paint,
    .layout         = NULL,
    .preferred_size = icon_view_preferred_size,
    .handle_event   = icon_view_handle_event,
    .destroy        = icon_view_destroy
};

NtkWidget* ntk_icon_view_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &icon_view_class, sizeof(NtkIconViewInstance));
    if (!w) return NULL;

    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->selected_index = -1;
    inst->item_width = 64;
    inst->item_height = 64;

    return w;
}

void ntk_icon_view_append(NtkWidget *w, const char *label, NtkPixmap *icon) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->item_count < NTK_ICON_VIEW_MAX_ITEMS) {
        NtkIconViewItem *item = &inst->items[inst->item_count++];
        item->label = label ? strdup(label) : strdup("");
        item->icon = icon;
        if (inst->selected_index < 0) {
            inst->selected_index = 0;
        }
        ntk_widget_repaint(w);
    }
}

void ntk_icon_view_set_selected(NtkWidget *w, int index) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && index >= 0 && index < inst->item_count) {
        if (inst->selected_index != index) {
            inst->selected_index = index;
            ntk_widget_emit(w, "selection-changed", &inst->selected_index);
            ntk_widget_repaint(w);
        }
    }
}

int ntk_icon_view_get_selected(NtkWidget *w) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->selected_index : -1;
}
static void icon_view_paint(NtkWidget *w, NtkPainter *p) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    ntk_painter_draw_bevel_sunken(p, r, light, dark);

    if (inst->item_count == 0) return;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    int margin = 8;
    int cols = (geom.width - 2 * margin) / inst->item_width;
    if (cols <= 0) cols = 1;

    for (int i = 0; i < inst->item_count; i++) {
        NtkIconViewItem *item = &inst->items[i];
        
        int row = i / cols;
        int col = i % cols;
        
        int item_x = r.x + margin + col * inst->item_width;
        int item_y = r.y + margin + row * inst->item_height;
        if (item_y + inst->item_height > r.y + r.height - margin) break; 

        NtkRect item_r = NTK_RECT(item_x, item_y, inst->item_width, inst->item_height);
        
        bool selected = (i == inst->selected_index);
        if (selected) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(p, item_r);
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
        } else {
            ntk_painter_set_color(p, text_col);
        }
        int icon_w = item->icon ? ntk_pixmap_get_width(item->icon) : 32;
        int icon_h = item->icon ? ntk_pixmap_get_height(item->icon) : 32;
        int ix = item_r.x + (item_r.width - icon_w) / 2;
        int iy = item_r.y + 4;
        
        if (item->icon) {
            ntk_painter_draw_pixmap(p, item->icon, ix, iy);
        } else {
            NtkRect ph = NTK_RECT(ix, iy, icon_w, icon_h);
            ntk_painter_draw_rect(p, ph);
        }
        if (item->label) {
            NtkSize ts = ntk_font_measure_text(font, item->label);
            int tx = item_r.x + (item_r.width - ts.width) / 2;
            int ty = item_r.y + 4 + icon_h + 4;
            ntk_painter_draw_text(p, item->label, tx, ty);
        }
    }
}

static bool icon_view_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        int margin = 8;
        int cols = (geom.width - 2 * margin) / inst->item_width;
        if (cols <= 0) cols = 1;

        int clicked_col = (p.x - margin) / inst->item_width;
        int clicked_row = (p.y - margin) / inst->item_height;

        if (clicked_col >= 0 && clicked_col < cols && clicked_row >= 0) {
            int idx = clicked_row * cols + clicked_col;
            if (idx >= 0 && idx < inst->item_count) {
                ntk_icon_view_set_selected(w, idx);
                return true;
            }
        }
    }
    return false;
}

static NtkSize icon_view_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(200, 150);
}

static void icon_view_destroy(NtkWidget *w) {
    NtkIconViewInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->item_count; i++) {
        free(inst->items[i].label);
        if (inst->items[i].icon) ntk_pixmap_destroy(inst->items[i].icon);
    }
}
