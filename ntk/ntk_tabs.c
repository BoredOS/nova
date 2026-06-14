// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_tabs.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_TABS_MAX_PAGES 16

typedef struct {
    NtkWidget *page;
    char      *label;
} NtkTabPageData;

typedef struct {
    NtkTabPageData pages[NTK_TABS_MAX_PAGES];
    int            page_count;
    int            current_index;
    NtkTabPosition position;
} NtkTabWidgetInstance;

static void tabs_paint(NtkWidget *w, NtkPainter *p);
static void tabs_layout(NtkWidget *w);
static NtkSize tabs_preferred_size(NtkWidget *w);
static bool tabs_handle_event(NtkWidget *w, NtkEvent *e);
static void tabs_destroy(NtkWidget *w);

static const NtkWidgetClass tabs_class = {
    .type_name      = "NtkTabWidget",
    .paint          = tabs_paint,
    .layout         = tabs_layout,
    .preferred_size = tabs_preferred_size,
    .handle_event   = tabs_handle_event,
    .destroy        = tabs_destroy
};

NtkWidget* ntk_tab_widget_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &tabs_class, sizeof(NtkTabWidgetInstance));
    if (!w) return NULL;

    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    inst->page_count = 0;
    inst->current_index = -1;
    inst->position = NTK_TAB_TOP;

    return w;
}

void ntk_tab_widget_add_page(NtkWidget *tw, NtkWidget *page, const char *label) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    if (!inst || inst->page_count >= NTK_TABS_MAX_PAGES) return;

    ntk_widget_add_child(tw, page);

    NtkTabPageData *p = &inst->pages[inst->page_count++];
    p->page = page;
    p->label = label ? strdup(label) : strdup("");
    if (inst->page_count == 1) {
        inst->current_index = 0;
        ntk_widget_show(page);
    } else {
        ntk_widget_hide(page);
    }

    tabs_layout(tw);
    ntk_widget_repaint(tw);
}

void ntk_tab_widget_remove_page(NtkWidget *tw, int index) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    if (!inst || index < 0 || index >= inst->page_count) return;

    NtkWidget *page = inst->pages[index].page;
    free(inst->pages[index].label);
    for (int i = index; i < inst->page_count - 1; i++) {
        inst->pages[i] = inst->pages[i + 1];
    }
    inst->page_count--;
    ntk_widget_remove_child(tw, page);
    ntk_widget_destroy(page);
    if (inst->current_index >= inst->page_count) {
        inst->current_index = inst->page_count - 1;
    }
    if (inst->current_index >= 0) {
        ntk_widget_show(inst->pages[inst->current_index].page);
    } else {
        inst->current_index = -1;
    }

    tabs_layout(tw);
    ntk_widget_repaint(tw);
}

void ntk_tab_widget_set_current(NtkWidget *tw, int index) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    if (!inst || index < 0 || index >= inst->page_count) return;

    if (inst->current_index != index) {
        if (inst->current_index >= 0) {
            ntk_widget_hide(inst->pages[inst->current_index].page);
        }
        inst->current_index = index;
        ntk_widget_show(inst->pages[index].page);
        
        tabs_layout(tw);
        ntk_widget_repaint(tw);
    }
}

int ntk_tab_widget_get_current(NtkWidget *tw) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    return inst ? inst->current_index : -1;
}

int ntk_tab_widget_get_count(NtkWidget *tw) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    return inst ? inst->page_count : 0;
}

NtkWidget* ntk_tab_widget_get_page(NtkWidget *tw, int index) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(tw);
    if (inst && index >= 0 && index < inst->page_count) {
        return inst->pages[index].page;
    }
    return NULL;
}
static void tabs_paint(NtkWidget *w, NtkPainter *p) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    int tab_h = 24;
    NtkRect content_frame = NTK_RECT(r.x, r.y + tab_h, r.width, r.height - tab_h);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, content_frame);
    ntk_painter_draw_bevel_raised(p, content_frame, light, dark);

    if (inst->page_count == 0) return;
    int tab_w = r.width / inst->page_count;
    if (tab_w < 50) tab_w = 50;

    for (int i = 0; i < inst->page_count; i++) {
        NtkRect tab_r = NTK_RECT(r.x + i * tab_w, r.y, tab_w, tab_h);
        
        bool active = (i == inst->current_index);
        if (active) {
            tab_r.y -= 2;
            tab_r.height += 2;
        }

        ntk_painter_set_color(p, bg);
        ntk_painter_fill_rect(p, tab_r);
        ntk_painter_draw_bevel_raised(p, tab_r, light, dark);

        if (active) {
            ntk_painter_set_color(p, bg);
            ntk_painter_fill_rect(p, NTK_RECT(tab_r.x + 2, tab_r.y + tab_r.height - 2, tab_r.width - 4, 3));
        }
        NtkFont *f = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, f);
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text_rect(p, inst->pages[i].label, tab_r, NTK_ALIGN_CENTER);
    }

    if (inst->current_index >= 0) {
        NtkWidget *page = inst->pages[inst->current_index].page;
        if (page && ntk_widget_is_visible(page)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(page);
            if (klass && klass->paint) klass->paint(page, p);
        }
    }
}

static void tabs_layout(NtkWidget *w) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int tab_h = 24;
    int border = 4; 

    if (inst->current_index >= 0) {
        NtkWidget *page = inst->pages[inst->current_index].page;
        if (page) {
            ntk_widget_set_geometry(page, NTK_RECT(geom.x + border, geom.y + tab_h + border, geom.width - 2 * border, geom.height - tab_h - 2 * border));
            
            const NtkWidgetClass *klass = ntk_widget_get_class(page);
            if (klass && klass->layout) klass->layout(page);
        }
    }
}

static NtkSize tabs_preferred_size(NtkWidget *w) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    int max_w = 150;
    int max_h = 100;

    for (int i = 0; i < inst->page_count; i++) {
        NtkSize pref = ntk_widget_get_preferred_size(inst->pages[i].page);
        if (pref.width > max_w) max_w = pref.width;
        if (pref.height > max_h) max_h = pref.height;
    }

    return NTK_SIZE(max_w + 8, max_h + 24 + 8);
}

static bool tabs_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        NtkPoint p = e->mouse_pos;
        int tab_h = 24;

        if (p.y >= 0 && p.y <= tab_h && inst->page_count > 0) {
            int tab_w = geom.width / inst->page_count;
            if (tab_w < 50) tab_w = 50;

            int clicked_idx = p.x / tab_w;
            if (clicked_idx >= 0 && clicked_idx < inst->page_count) {
                ntk_tab_widget_set_current(w, clicked_idx);
                return true;
            }
        }
    }
    return false;
}

static void tabs_destroy(NtkWidget *w) {
    NtkTabWidgetInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->page_count; i++) {
        free(inst->pages[i].label);
    }
}
