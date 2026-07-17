// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_menu.h"
#include "ntk_style.h"
#include "ntk_window.h"
#include "ntk_app.h"
#include <stdlib.h>
#include <string.h>

#define NTK_MENU_BAR_MAX_MENUS 8
#define NTK_MENU_MAX_ITEMS     32
#define NTK_MENU_ITEM_H        24
#define NTK_MENU_SEPARATOR_H   8
#define NTK_MENU_ICON_SIZE     16
#define NTK_MENU_ICON_PAD      6
#define NTK_MENU_ARROW_W       8
#define NTK_MENU_H_PAD         6
#define NTK_MENU_MIN_W         160

typedef struct {
    NtkWidget *menu;
    char      *label;
} NtkMenuBarItem;

typedef struct {
    NtkMenuBarItem items[NTK_MENU_BAR_MAX_MENUS];
    int            item_count;
    int            active_index;
} NtkMenuBarInstance;

typedef struct {
    NtkWidget *item;
    bool       is_separator;
} NtkMenuItemData;

typedef struct {
    NtkMenuItemData items[NTK_MENU_MAX_ITEMS];
    int             item_count;
    int             hover_index;
    int             popup_x;
    int             popup_y;
    bool            is_popped_up;
    int             computed_width;
    NtkWidget      *popup_win;
    NtkWidget      *original_parent;
} NtkMenuInstance;

typedef struct {
    char      *label;
    NtkPixmap *icon;
    NtkWidget *submenu;
    bool       checked;
} NtkMenuItemInstance;
static void menu_bar_paint(NtkWidget *w, NtkPainter *p);
static bool menu_bar_handle_event(NtkWidget *w, NtkEvent *e);

static const NtkWidgetClass menu_bar_class = {
    .type_name      = "NtkMenuBar",
    .paint          = menu_bar_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = menu_bar_handle_event,
    .destroy        = NULL
};
static void menu_paint(NtkWidget *w, NtkPainter *p);
static bool menu_handle_event(NtkWidget *w, NtkEvent *e);

static const NtkWidgetClass menu_class = {
    .type_name      = "NtkMenu",
    .paint          = menu_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = menu_handle_event,
    .destroy        = NULL
};
static void menu_item_paint(NtkWidget *w, NtkPainter *p);

static const NtkWidgetClass menu_item_class = {
    .type_name      = "NtkMenuItem",
    .paint          = menu_item_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = NULL,
    .destroy        = NULL
};
static int menu_compute_height(NtkMenuInstance *inst) {
    int h = 4; 
    for (int i = 0; i < inst->item_count; i++) {
        h += inst->items[i].is_separator ? NTK_MENU_SEPARATOR_H : NTK_MENU_ITEM_H;
    }
    return h;
}

static int menu_compute_width(NtkMenuInstance *inst, NtkStyle *style) {
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_MENU_FONT);
    if (!font) font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);

    int max_w = NTK_MENU_MIN_W;
    for (int i = 0; i < inst->item_count; i++) {
        if (inst->items[i].is_separator || !inst->items[i].item) continue;

        NtkMenuItemInstance *item_inst = ntk_widget_get_instance_data(inst->items[i].item);
        if (!item_inst || !item_inst->label) continue;

        int w = NTK_MENU_H_PAD + NTK_MENU_ICON_SIZE + NTK_MENU_ICON_PAD; 
        if (font) {
            NtkSize ts = ntk_font_measure_text(font, item_inst->label);
            w += ts.width;
        } else {
            w += (int)strlen(item_inst->label) * 8;
        }
        w += NTK_MENU_ICON_PAD;
        if (item_inst->submenu) {
            w += NTK_MENU_ARROW_W + NTK_MENU_H_PAD; 
        }
        w += NTK_MENU_H_PAD; 
        if (w > max_w) max_w = w;
    }
    return max_w + 4;
}
static int menu_item_y_offset(NtkMenuInstance *inst, int idx) {
    int y = 2;
    for (int i = 0; i < idx && i < inst->item_count; i++) {
        y += inst->items[i].is_separator ? NTK_MENU_SEPARATOR_H : NTK_MENU_ITEM_H;
    }
    return y;
}

static void menu_close_submenus(NtkMenuInstance *inst) {
    for (int i = 0; i < inst->item_count; i++) {
        if (!inst->items[i].item || inst->items[i].is_separator) continue;
        NtkMenuItemInstance *item_inst = ntk_widget_get_instance_data(inst->items[i].item);
        if (item_inst && item_inst->submenu) {
            NtkMenuInstance *sub_inst = ntk_widget_get_instance_data(item_inst->submenu);
            if (sub_inst && sub_inst->is_popped_up) {
                menu_close_submenus(sub_inst);
                sub_inst->is_popped_up = false;
                if (sub_inst->popup_win) {
                    NtkWidget *pop = sub_inst->popup_win;
                    sub_inst->popup_win = NULL;
                    ntk_window_close(pop);
                }
            }
        }
    }
}

NtkWidget* ntk_menu_bar_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &menu_bar_class, sizeof(NtkMenuBarInstance));
    if (!w) return NULL;

    NtkMenuBarInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->active_index = -1;

    return w;
}

void ntk_menu_bar_add_menu(NtkWidget *mb, NtkWidget *menu, const char *label) {
    NtkMenuBarInstance *inst = ntk_widget_get_instance_data(mb);
    if (!inst || inst->item_count >= NTK_MENU_BAR_MAX_MENUS) return;

    ntk_widget_add_child(mb, menu);
    
    NtkMenuBarItem *item = &inst->items[inst->item_count++];
    item->menu = menu;
    item->label = label ? strdup(label) : strdup("");

    ntk_widget_hide(menu);
}

static void menu_bar_paint(NtkWidget *w, NtkPainter *p) {
    NtkMenuBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_MENUBAR_BG);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_MENU_FONT);
    if (!font) font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    int curr_x = r.x + 8;
    int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;

    for (int i = 0; i < inst->item_count; i++) {
        NtkSize ts = ntk_font_measure_text(font, inst->items[i].label);
        NtkRect tab = NTK_RECT(curr_x - 4, r.y + 1, ts.width + 8, r.height - 2);

        bool active = (i == inst->active_index);
        if (active) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_ACTIVE));
            ntk_painter_fill_rect(p, tab);
            ntk_painter_draw_bevel_sunken(p, tab, ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT), ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK));
        }

        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, inst->items[i].label, curr_x, r.y + pad_y);

        if (inst->items[i].label && strlen(inst->items[i].label) > 0) {
            char first_char[2] = { inst->items[i].label[0], '\0' };
            NtkSize fc_sz = ntk_font_measure_text(font, first_char);
            int ascent = ntk_font_get_ascent(font);
            int ul_y = r.y + pad_y + ascent + 2;
            ntk_painter_draw_line(p, curr_x, ul_y, curr_x + fc_sz.width - 2, ul_y);
        }

        curr_x += ts.width + 12;
    }
}

static bool menu_bar_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkMenuBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_MENU_FONT);
        if (!font) font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);

        int curr_x = 8;
        for (int i = 0; i < inst->item_count; i++) {
            NtkSize ts = ntk_font_measure_text(font, inst->items[i].label);
            if (p.x >= curr_x - 4 && p.x <= curr_x + ts.width + 4) {
                int old_active = inst->active_index;
                if (old_active == i) {
                    if (inst->items[i].menu) {
                        ntk_menu_close(inst->items[i].menu);
                    }
                    inst->active_index = -1;
                } else {
                    if (old_active != -1 && inst->items[old_active].menu) {
                        ntk_menu_close(inst->items[old_active].menu);
                    }
                    inst->active_index = i;
                    if (inst->items[i].menu) {
                        NtkWidget *menu = inst->items[i].menu;
                        ntk_widget_show(menu);
                        ntk_menu_popup(menu, curr_x - 4, geom.y + geom.height);
                    }
                }
                ntk_widget_repaint(w);
                return true;
            }
            curr_x += ts.width + 12;
        }

        if (inst->active_index != -1) {
            if (inst->items[inst->active_index].menu) {
                ntk_menu_close(inst->items[inst->active_index].menu);
            }
            inst->active_index = -1;
            ntk_widget_repaint(w);
            return true;
        }
    }
    return false;
}
NtkWidget* ntk_menu_new(void) {
    NtkWidget *w = ntk_widget_new_with_class(NULL, &menu_class, sizeof(NtkMenuInstance));
    if (!w) return NULL;

    NtkMenuInstance *inst = ntk_widget_get_instance_data(w);
    inst->item_count = 0;
    inst->hover_index = -1;
    inst->is_popped_up = false;
    inst->computed_width = NTK_MENU_MIN_W;
    inst->popup_win = NULL;
    inst->original_parent = NULL;

    return w;
}

void ntk_menu_add_item(NtkWidget *m, NtkWidget *item) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(m);
    if (!inst || inst->item_count >= NTK_MENU_MAX_ITEMS) return;

    ntk_widget_add_child(m, item);
    
    NtkMenuItemData *data = &inst->items[inst->item_count++];
    data->item = item;
    data->is_separator = false;
}

void ntk_menu_add_separator(NtkWidget *m) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(m);
    if (!inst || inst->item_count >= NTK_MENU_MAX_ITEMS) return;

    NtkMenuItemData *data = &inst->items[inst->item_count++];
    data->item = NULL;
    data->is_separator = true;
}

static void on_menu_popup_closed(NtkWidget *win, void *userdata) {
    NtkWidget *menu = userdata;
    NtkMenuInstance *inst = ntk_widget_get_instance_data(menu);
    inst->is_popped_up = false;
    inst->popup_win = NULL;

    ntk_window_detach_content(win);

    if (inst->original_parent) {
        ntk_widget_add_child(inst->original_parent, menu);

        NtkWidget *oparent = inst->original_parent;
        const NtkWidgetClass *klass = ntk_widget_get_class(oparent);
        if (klass && strcmp(klass->type_name, "NtkMenuBar") == 0) {
            NtkMenuBarInstance *mb_inst = ntk_widget_get_instance_data(oparent);
            mb_inst->active_index = -1;
            ntk_widget_repaint(oparent);
        }
    }
    ntk_widget_hide(menu);
}

void ntk_menu_popup(NtkWidget *m, int x, int y) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(m);
    if (inst && !inst->popup_win) {
        inst->popup_x = x;
        inst->popup_y = y;
        inst->is_popped_up = true;
        inst->hover_index = -1;

        NtkStyle *style = ntk_widget_get_style(m);
        int width = menu_compute_width(inst, style);
        int height = menu_compute_height(inst);
        inst->computed_width = width;

        int win_x = 0, win_y = 0;
        ntk_window_get_screen_position(m, &win_x, &win_y);
        
        int pop_x = win_x + x;
        int pop_y = win_y + y;

        inst->original_parent = ntk_widget_get_parent(m);
        inst->popup_win = ntk_window_new_popup(NULL, pop_x, pop_y, width, height);
        if (inst->popup_win) {
            ntk_window_set_content(inst->popup_win, m);
            ntk_widget_show(inst->popup_win);
            ntk_widget_connect(inst->popup_win, "closed", on_menu_popup_closed, m);
            ntk_app_set_active_popup(inst->popup_win);
        }
    }
}

void ntk_menu_close(NtkWidget *m) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(m);
    if (inst) {
        menu_close_submenus(inst);
        inst->is_popped_up = false;
        inst->hover_index = -1;
        if (inst->popup_win) {
            NtkWidget *pop = inst->popup_win;
            inst->popup_win = NULL;
            ntk_window_close(pop);
        }
        ntk_widget_hide(m);
    }
}

static void menu_paint(NtkWidget *w, NtkPainter *p) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst->is_popped_up) return;

    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_MENU_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    ntk_painter_draw_bevel_raised(p, r, light, dark);
    int item_y = r.y + 2;
    for (int i = 0; i < inst->item_count; i++) {
        NtkMenuItemData *data = &inst->items[i];

        if (data->is_separator) {
            int mid_y = item_y + NTK_MENU_SEPARATOR_H / 2;
            ntk_painter_set_color(p, dark);
            ntk_painter_draw_line(p, r.x + 3, mid_y, r.x + r.width - 3, mid_y);
            ntk_painter_set_color(p, light);
            ntk_painter_draw_line(p, r.x + 3, mid_y + 1, r.x + r.width - 3, mid_y + 1);
            item_y += NTK_MENU_SEPARATOR_H;
        } else if (data->item) {
            NtkRect item_r = NTK_RECT(r.x + 2, item_y, r.width - 4, NTK_MENU_ITEM_H);

            bool hovered = (i == inst->hover_index);
            if (hovered) {
                ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_MENU_HIGHLIGHT));
                ntk_painter_fill_rect(p, item_r);
            }

            ntk_widget_set_geometry(data->item, NTK_RECT(geom.x + 2, geom.y + (item_y - r.y), geom.width - 4, NTK_MENU_ITEM_H));
            
            const NtkWidgetClass *klass = ntk_widget_get_class(data->item);
            if (klass && klass->paint) klass->paint(data->item, p);

            item_y += NTK_MENU_ITEM_H;
        }
    }
}

static int menu_hit_test(NtkMenuInstance *inst, int local_y) {
    int y = 2;
    for (int i = 0; i < inst->item_count; i++) {
        int item_h = inst->items[i].is_separator ? NTK_MENU_SEPARATOR_H : NTK_MENU_ITEM_H;
        if (local_y >= y && local_y < y + item_h) {
            return inst->items[i].is_separator ? -1 : i;
        }
        y += item_h;
    }
    return -1;
}

static void menu_open_submenu_for_item(NtkWidget *menu, NtkMenuInstance *inst, int idx) {
    if (idx < 0 || idx >= inst->item_count) return;
    NtkMenuItemData *data = &inst->items[idx];
    if (!data->item || data->is_separator) return;

    NtkMenuItemInstance *item_inst = ntk_widget_get_instance_data(data->item);
    if (!item_inst || !item_inst->submenu) return;

    NtkRect geom = ntk_widget_get_geometry(menu);
    int item_y_off = menu_item_y_offset(inst, idx);
    
    int sub_x = geom.x + geom.width - 2;
    int sub_y = geom.y + item_y_off;

    ntk_widget_show(item_inst->submenu);
    ntk_menu_popup(item_inst->submenu, sub_x, sub_y);
}

static bool menu_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkMenuInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= 2 && p.y <= geom.height - 2) {
            int idx = menu_hit_test(inst, p.y);
            if (idx >= 0 && idx < inst->item_count) {
                NtkMenuItemData *data = &inst->items[idx];
                if (data->item && !data->is_separator) {
                    NtkMenuItemInstance *item_inst = ntk_widget_get_instance_data(data->item);
                    
                    if (!ntk_widget_is_enabled(data->item)) return true;
                    if (item_inst && item_inst->submenu) {
                        menu_close_submenus(inst);
                        menu_open_submenu_for_item(w, inst, idx);
                        ntk_widget_repaint(w);
                        return true;
                    }

                    ntk_widget_emit(data->item, "triggered", NULL);
                    
                    NtkWidget *cur = w;
                    while (cur) {
                        NtkMenuInstance *cur_inst = ntk_widget_get_instance_data(cur);
                        if (cur_inst && cur_inst->popup_win) {
                            ntk_menu_close(cur);
                        }
                        cur = cur_inst ? cur_inst->original_parent : NULL;
                    }

                    NtkWidget *parent = inst->original_parent;
                    if (parent && strcmp(ntk_widget_get_class(parent)->type_name, "NtkMenuBar") == 0) {
                        NtkMenuBarInstance *mb_inst = ntk_widget_get_instance_data(parent);
                        mb_inst->active_index = -1;
                        ntk_widget_repaint(parent);
                    }
                    ntk_app_set_active_popup(NULL);
                    return true;
                }
            }
        }
    } else if (e->type == NTK_EVENT_MOUSE_MOVE) {
        NtkPoint p = e->mouse_pos;
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= 2 && p.y <= geom.height - 2) {
            int old = inst->hover_index;
            int new_idx = menu_hit_test(inst, p.y);
            inst->hover_index = new_idx;

            if (old != inst->hover_index) {
                menu_close_submenus(inst);
                if (new_idx >= 0) {
                    menu_open_submenu_for_item(w, inst, new_idx);
                }
                ntk_widget_repaint(w);
            }
        } else {
            if (inst->hover_index != -1) {
                inst->hover_index = -1;
                ntk_widget_repaint(w);
            }
        }
        return true;
    }
    return false;
}

NtkWidget* ntk_menu_item_new(const char *label) {
    NtkWidget *w = ntk_widget_new_with_class(NULL, &menu_item_class, sizeof(NtkMenuItemInstance));
    if (!w) return NULL;

    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(w);
    inst->label = label ? strdup(label) : NULL;
    inst->icon = NULL;
    inst->submenu = NULL;

    return w;
}

NtkWidget* ntk_menu_item_new_with_icon(const char *label, NtkPixmap *icon) {
    NtkWidget *w = ntk_menu_item_new(label);
    if (w) {
        NtkMenuItemInstance *inst = ntk_widget_get_instance_data(w);
        inst->icon = icon;
    }
    return w;
}

void ntk_menu_item_set_submenu(NtkWidget *item, NtkWidget *submenu) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(item);
    if (inst) {
        inst->submenu = submenu;
        if (submenu) {
            ntk_widget_hide(submenu);
        }
    }
}

bool ntk_menu_item_has_submenu(NtkWidget *item) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(item);
    return inst && inst->submenu != NULL;
}

NtkWidget* ntk_menu_item_get_submenu(NtkWidget *item) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(item);
    return inst ? inst->submenu : NULL;
}

void ntk_menu_item_set_checked(NtkWidget *item, bool checked) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(item);
    if (!inst) return;

    inst->checked = checked;
    ntk_widget_repaint(item);
}

bool ntk_menu_item_is_checked(NtkWidget *item) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(item);
    return inst && inst->checked;
}

static void menu_item_paint(NtkWidget *w, NtkPainter *p) {
    NtkMenuItemInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_MENU_FONT);
    if (!font) font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    NtkWidget *menu = ntk_widget_get_parent(w);
    bool hovered = false;
    if (menu) {
        NtkMenuInstance *m_inst = ntk_widget_get_instance_data(menu);
        if (m_inst) {
            for (int i = 0; i < m_inst->item_count; i++) {
                if (m_inst->items[i].item == w) {
                    hovered = (i == m_inst->hover_index);
                    break;
                }
            }
        }
    }

    NtkColor text_col = !ntk_widget_is_enabled(w) ?
        ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED) : hovered ?
        ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT) : 
        ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    int curr_x = r.x + NTK_MENU_H_PAD;
    int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;

    if (inst->checked) {
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_line(p, curr_x + 2, r.y + 12, curr_x + 6, r.y + 16);
        ntk_painter_draw_line(p, curr_x + 6, r.y + 16, curr_x + 13, r.y + 7);
    } else if (inst->icon) {
        int ih = ntk_pixmap_get_height(inst->icon);
        int iy = r.y + (r.height - ih) / 2;
        ntk_painter_draw_pixmap(p, inst->icon, curr_x, iy);
    }
    curr_x += NTK_MENU_ICON_SIZE + NTK_MENU_ICON_PAD;

    if (inst->label) {
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, inst->label, curr_x, r.y + pad_y);
    }

    if (inst->submenu) {
        int arrow_x = r.x + r.width - NTK_MENU_H_PAD - NTK_MENU_ARROW_W;
        int arrow_cy = r.y + r.height / 2;
        
        NtkPoint arrow_pts[3];
        arrow_pts[0] = NTK_POINT(arrow_x, arrow_cy - 4);
        arrow_pts[1] = NTK_POINT(arrow_x + 6, arrow_cy);
        arrow_pts[2] = NTK_POINT(arrow_x, arrow_cy + 4);
        
        ntk_painter_set_color(p, text_col);
        ntk_painter_fill_polygon(p, arrow_pts, 3);
    }
}
