// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_list.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_LIST_MAX_ITEMS 64
#define NTK_LIST_ITEM_H    16

typedef struct {
    char *items[NTK_LIST_MAX_ITEMS];
    int   item_count;
    int   selected_index;
} NtkListBoxInstance;

static void list_paint(NtkWidget *w, NtkPainter *p);
static bool list_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize list_preferred_size(NtkWidget *w);
static void list_destroy(NtkWidget *w);

static const NtkWidgetClass list_class = {
    .type_name      = "NtkListBox",
    .paint          = list_paint,
    .layout         = NULL,
    .preferred_size = list_preferred_size,
    .handle_event   = list_handle_event,
    .destroy        = list_destroy
};

NtkWidget* ntk_list_box_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &list_class, sizeof(NtkListBoxInstance));
    if (!w) return NULL;

    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->selected_index = -1;

    return w;
}

void ntk_list_box_append(NtkWidget *w, const char *text) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->item_count < NTK_LIST_MAX_ITEMS && text) {
        inst->items[inst->item_count++] = strdup(text);
        if (inst->selected_index < 0) {
            inst->selected_index = 0;
        }
        ntk_widget_repaint(w);
    }
}

void ntk_list_box_set_selected(NtkWidget *w, int index) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && index >= 0 && index < inst->item_count) {
        if (inst->selected_index != index) {
            inst->selected_index = index;
            ntk_widget_emit(w, "selection-changed", &inst->selected_index);
            ntk_widget_repaint(w);
        }
    }
}

int ntk_list_box_get_selected(NtkWidget *w) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->selected_index : -1;
}

const char* ntk_list_box_get_item(NtkWidget *w, int index) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && index >= 0 && index < inst->item_count) {
        return inst->items[index];
    }
    return NULL;
}

int ntk_list_box_get_count(NtkWidget *w) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->item_count : 0;
}
static void list_paint(NtkWidget *w, NtkPainter *p) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
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

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    int pad_x = 4;
    int line_h = NTK_LIST_ITEM_H;

    for (int i = 0; i < inst->item_count; i++) {
        int item_y = r.y + 2 + i * line_h;
        if (item_y + line_h > r.y + r.height - 2) break; 

        NtkRect item_r = NTK_RECT(r.x + 2, item_y, r.width - 4, line_h);
        
        bool selected = (i == inst->selected_index);
        if (selected) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(p, item_r);
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
        } else {
            ntk_painter_set_color(p, text_col);
        }

        int pad_y = (line_h - ntk_font_get_line_height(font)) / 2;
        ntk_painter_draw_text(p, inst->items[i], item_r.x + pad_x, item_r.y + pad_y);
    }
}

static bool list_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= 2 && p.y <= geom.height - 2) {
            int clicked_idx = (p.y - 2) / NTK_LIST_ITEM_H;
            if (clicked_idx >= 0 && clicked_idx < inst->item_count) {
                ntk_list_box_set_selected(w, clicked_idx);
                return true;
            }
        }
    }
    return false;
}

static NtkSize list_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(100, 100);
}

static void list_destroy(NtkWidget *w) {
    NtkListBoxInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->item_count; i++) {
        free(inst->items[i]);
    }
}
