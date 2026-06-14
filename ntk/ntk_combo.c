// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_combo.h"
#include "ntk_style.h"
#include "ntk_window.h"
#include "ntk_app.h"
#include <stdlib.h>
#include <string.h>

#define NTK_COMBO_MAX_ITEMS 32
#define NTK_COMBO_ITEM_H    16

typedef struct {
    char *items[NTK_COMBO_MAX_ITEMS];
    int   item_count;
    int   selected_index;
    bool  popup_open;
    int   hover_index;
    NtkWidget *popup_win;
} NtkComboBoxInstance;

static void combo_paint(NtkWidget *w, NtkPainter *p);
static bool combo_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize combo_preferred_size(NtkWidget *w);
static void combo_destroy(NtkWidget *w);

static const NtkWidgetClass combo_class = {
    .type_name      = "NtkComboBox",
    .paint          = combo_paint,
    .layout         = NULL,
    .preferred_size = combo_preferred_size,
    .handle_event   = combo_handle_event,
    .destroy        = combo_destroy
};

typedef struct {
    NtkWidget *combo;
} NtkComboPopupInstance;

static void combo_popup_paint(NtkWidget *w, NtkPainter *p) {
    NtkComboPopupInstance *inst = ntk_widget_get_instance_data(w);
    NtkWidget *combo = inst->combo;
    NtkComboBoxInstance *combo_inst = ntk_widget_get_instance_data(combo);
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
    for (int i = 0; i < combo_inst->item_count; i++) {
        NtkRect item_r = NTK_RECT(r.x + 2, r.y + 2 + i * NTK_COMBO_ITEM_H, r.width - 4, NTK_COMBO_ITEM_H);
        
        bool hovered = (i == combo_inst->hover_index);
        bool selected = (i == combo_inst->selected_index);
        
        if (hovered) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(p, item_r);
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
        } else if (selected) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_HOVER));
            ntk_painter_fill_rect(p, item_r);
            ntk_painter_set_color(p, text_col);
        } else {
            ntk_painter_set_color(p, text_col);
        }

        int item_pad_y = (NTK_COMBO_ITEM_H - ntk_font_get_line_height(font)) / 2;
        ntk_painter_draw_text(p, combo_inst->items[i], item_r.x + pad_x, item_r.y + item_pad_y);
    }
}

static bool combo_popup_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkComboPopupInstance *inst = ntk_widget_get_instance_data(w);
    NtkWidget *combo = inst->combo;
    NtkComboBoxInstance *combo_inst = ntk_widget_get_instance_data(combo);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_MOVE) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= 2 && p.y <= geom.height - 2) {
            int old_hover = combo_inst->hover_index;
            combo_inst->hover_index = (p.y - 2) / NTK_COMBO_ITEM_H;
            if (combo_inst->hover_index >= combo_inst->item_count) {
                combo_inst->hover_index = combo_inst->item_count - 1;
            }
            if (old_hover != combo_inst->hover_index) {
                ntk_widget_repaint(w);
            }
        } else {
            if (combo_inst->hover_index != -1) {
                combo_inst->hover_index = -1;
                ntk_widget_repaint(w);
            }
        }
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= 2 && p.y <= geom.height - 2) {
            int clicked_idx = (p.y - 2) / NTK_COMBO_ITEM_H;
            if (clicked_idx >= 0 && clicked_idx < combo_inst->item_count) {
                ntk_combo_box_set_selected(combo, clicked_idx);
            }
            ntk_app_set_active_popup(NULL);
            ntk_window_close(ntk_widget_get_parent(w));
            return true;
        }
    }
    return false;
}

static const NtkWidgetClass combo_popup_class = {
    .type_name      = "NtkComboPopup",
    .paint          = combo_popup_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = combo_popup_handle_event,
    .destroy        = NULL
};

NtkWidget* ntk_combo_popup_new(NtkWidget *parent, NtkWidget *combo) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &combo_popup_class, sizeof(NtkComboPopupInstance));
    if (!w) return NULL;
    NtkComboPopupInstance *inst = ntk_widget_get_instance_data(w);
    inst->combo = combo;
    return w;
}

static void on_combo_popup_closed(NtkWidget *win, void *userdata) {
    NtkWidget *combo = userdata;
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(combo);
    inst->popup_open = false;
    inst->popup_win = NULL;
    ntk_widget_repaint(combo);
}

NtkWidget* ntk_combo_box_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &combo_class, sizeof(NtkComboBoxInstance));
    if (!w) return NULL;

    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->selected_index = -1;
    inst->popup_open = false;
    inst->hover_index = -1;
    inst->popup_win = NULL;

    return w;
}

void ntk_combo_box_append(NtkWidget *w, const char *text) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->item_count < NTK_COMBO_MAX_ITEMS && text) {
        inst->items[inst->item_count++] = strdup(text);
        if (inst->selected_index < 0) {
            inst->selected_index = 0;
        }
        ntk_widget_repaint(w);
    }
}

void ntk_combo_box_set_selected(NtkWidget *w, int index) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && index >= 0 && index < inst->item_count) {
        if (inst->selected_index != index) {
            inst->selected_index = index;
            ntk_widget_emit(w, "selection-changed", &inst->selected_index);
            ntk_widget_repaint(w);
        }
    }
}

int ntk_combo_box_get_selected(NtkWidget *w) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->selected_index : -1;
}

const char* ntk_combo_box_get_item(NtkWidget *w, int index) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && index >= 0 && index < inst->item_count) {
        return inst->items[index];
    }
    return NULL;
}

int ntk_combo_box_get_count(NtkWidget *w) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->item_count : 0;
}

static void combo_paint(NtkWidget *w, NtkPainter *p) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
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
    int btn_w = 16;
    NtkRect btn_r = NTK_RECT(r.x + r.width - btn_w - 2, r.y + 2, btn_w, r.height - 4);
    ntk_painter_set_color(p, btn_bg);
    ntk_painter_fill_rect(p, btn_r);
    if (inst->popup_open) ntk_painter_draw_bevel_sunken(p, btn_r, light, dark);
    else ntk_painter_draw_bevel_raised(p, btn_r, light, dark);
    ntk_painter_set_color(p, text_col);
    int cx = btn_r.x + btn_r.width / 2;
    int cy = btn_r.y + btn_r.height / 2;
    if (inst->popup_open) { cx += 1; cy += 1; }
    ntk_painter_draw_line(p, cx - 3, cy - 1, cx, cy + 2);
    ntk_painter_draw_line(p, cx, cy + 2, cx + 3, cy - 1);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    int pad_x = 4;
    int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;
    if (pad_y < 2) pad_y = 2;

    if (inst->selected_index >= 0 && inst->selected_index < inst->item_count) {
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, inst->items[inst->selected_index], r.x + pad_x, r.y + pad_y);
    }
}

static bool combo_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 0 && p.x <= geom.width && p.y >= 0 && p.y <= geom.height) {
            if (inst->popup_open) {
                if (inst->popup_win) {
                    ntk_app_set_active_popup(NULL);
                    ntk_window_close(inst->popup_win);
                }
            } else {
                inst->popup_open = true;
                inst->hover_index = -1;

                int win_x = 0, win_y = 0;
                ntk_window_get_screen_position(w, &win_x, &win_y);
                NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));

                int pop_x = win_x + origin.x;
                int pop_y = win_y + origin.y + geom.height;
                int pop_w = geom.width;
                int pop_h = inst->item_count * NTK_COMBO_ITEM_H + 4;

                inst->popup_win = ntk_window_new_popup(w, pop_x, pop_y, pop_w, pop_h);
                if (inst->popup_win) {
                    NtkWidget *pop_content = ntk_combo_popup_new(inst->popup_win, w);
                    ntk_window_set_content(inst->popup_win, pop_content);
                    ntk_widget_show(inst->popup_win);
                    ntk_widget_connect(inst->popup_win, "closed", on_combo_popup_closed, w);
                    ntk_app_set_active_popup(inst->popup_win);
                }
            }
            ntk_widget_repaint(w);
            return true;
        }
    }
    return false;
}

static NtkSize combo_preferred_size(NtkWidget *w) {
    NtkStyle *style = ntk_widget_get_style(w);
    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_ENTRY_HEIGHT);
    if (height <= 0) height = 22;

    return NTK_SIZE(120, height);
}

static void combo_destroy(NtkWidget *w) {
    NtkComboBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst->popup_win) {
        ntk_window_close(inst->popup_win);
    }
    for (int i = 0; i < inst->item_count; i++) {
        free(inst->items[i]);
    }
}
