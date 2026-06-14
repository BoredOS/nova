// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_button.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char      *label;
    NtkPixmap *icon;
    bool       flat;
    bool       pressed;
    bool       hover;
    bool       is_toggle;
    bool       active;
} NtkButtonInstance;

static void button_paint(NtkWidget *w, NtkPainter *p);
static bool button_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize button_preferred_size(NtkWidget *w);
static void button_destroy(NtkWidget *w);

static const NtkWidgetClass button_class = {
    .type_name      = "NtkButton",
    .paint          = button_paint,
    .layout         = NULL,
    .preferred_size = button_preferred_size,
    .handle_event   = button_handle_event,
    .destroy        = button_destroy
};

NtkWidget* ntk_button_new(const char *label, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &button_class, sizeof(NtkButtonInstance));
    if (!w) return NULL;

    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    inst->label = label ? strdup(label) : NULL;
    inst->flat = false;
    inst->is_toggle = false;

    return w;
}

NtkWidget* ntk_button_new_with_icon(const char *label, NtkPixmap *icon, NtkWidget *parent) {
    NtkWidget *w = ntk_button_new(label, parent);
    if (w) {
        ntk_button_set_icon(w, icon);
    }
    return w;
}

NtkWidget* ntk_toggle_button_new(const char *label, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &button_class, sizeof(NtkButtonInstance));
    if (!w) return NULL;

    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    inst->label = label ? strdup(label) : NULL;
    inst->flat = false;
    inst->is_toggle = true;
    inst->active = false;

    return w;
}

void ntk_button_set_label(NtkWidget *w, const char *label) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        free(inst->label);
        inst->label = label ? strdup(label) : NULL;
        ntk_widget_repaint(w);
    }
}

const char* ntk_button_get_label(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->label : NULL;
}

void ntk_button_set_icon(NtkWidget *w, NtkPixmap *icon) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        if (inst->icon) ntk_pixmap_destroy(inst->icon);
        inst->icon = icon; 
        ntk_widget_repaint(w);
    }
}

NtkPixmap* ntk_button_get_icon(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->icon : NULL;
}

void ntk_button_set_flat(NtkWidget *w, bool flat) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->flat = flat;
        ntk_widget_repaint(w);
    }
}

bool ntk_button_is_flat(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->flat : false;
}

void ntk_toggle_button_set_active(NtkWidget *w, bool active) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->is_toggle) {
        if (inst->active != active) {
            inst->active = active;
            ntk_widget_emit(w, "toggled", &inst->active);
            ntk_widget_repaint(w);
        }
    }
}

bool ntk_toggle_button_is_active(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    return inst && inst->is_toggle ? inst->active : false;
}

static void button_paint(NtkWidget *w, NtkPainter *p) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    bool draw_bevel = !inst->flat || inst->hover || inst->pressed || (inst->is_toggle && inst->active);
    bool is_sunken = inst->pressed || (inst->is_toggle && inst->active);

    if (inst->pressed) {
        bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_ACTIVE);
    } else if (inst->hover) {
        bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_HOVER);
    } else if (!ntk_widget_is_enabled(w)) {
        bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_DISABLED);
        text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED);
    }

    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);

    if (draw_bevel) {
        if (is_sunken) {
            ntk_painter_draw_bevel_sunken(p, r, light, dark);
        } else {
            ntk_painter_draw_bevel_raised(p, r, light, dark);
        }
    }

    int content_w = 0;
    int text_w = 0, text_h = 0;
    int icon_w = 0, icon_h = 0;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);

    if (inst->label && strlen(inst->label) > 0) {
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        text_w = ts.width;
        text_h = ts.height;
        content_w += text_w;
    }

    if (inst->icon) {
        icon_w = ntk_pixmap_get_width(inst->icon);
        icon_h = ntk_pixmap_get_height(inst->icon);
        content_w += icon_w;
        if (text_w > 0) content_w += 6; 
    }

    int cx = r.x + (r.width - content_w) / 2;
    if (is_sunken) {
        cx += 1;
    }

    if (inst->icon) {
        int cy = r.y + (r.height - icon_h) / 2;
        if (is_sunken) cy += 1;
        ntk_painter_draw_pixmap(p, inst->icon, cx, cy);
        cx += icon_w + 6;
    }

    if (inst->label && strlen(inst->label) > 0) {
        int cy = r.y + (r.height - text_h) / 2;
        if (is_sunken) cy += 1;
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, inst->label, cx, cy);
    }
}

static bool button_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (!ntk_widget_is_enabled(w)) return false;

    if (e->type == NTK_EVENT_MOUSE_ENTER) {
        inst->hover = true;
        ntk_widget_repaint(w);
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_LEAVE) {
        inst->hover = false;
        if (inst->pressed) {
            inst->pressed = false;
            ntk_widget_emit(w, "released", NULL);
        }
        ntk_widget_repaint(w);
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        inst->pressed = true;
        ntk_widget_emit(w, "pressed", NULL);
        ntk_widget_repaint(w);
        return true;
    } else if (e->type == NTK_EVENT_MOUSE_RELEASE && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        if (inst->pressed) {
            inst->pressed = false;
            ntk_widget_emit(w, "released", NULL);
            if (inst->is_toggle) {
                inst->active = !inst->active;
                ntk_widget_emit(w, "toggled", &inst->active);
            }
            ntk_widget_emit(w, "clicked", NULL);
            ntk_widget_repaint(w);
            return true;
        }
    }
    return false;
}

static NtkSize button_preferred_size(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    NtkStyle *style = ntk_widget_get_style(w);

    int width = 24;
    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_BUTTON_HEIGHT);
    if (height <= 0) height = 24;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    if (inst->label && font) {
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        width += ts.width;
        if (ts.height + 12 > height) height = ts.height + 12;
    }

    if (inst->icon) {
        int iw = ntk_pixmap_get_width(inst->icon);
        int ih = ntk_pixmap_get_height(inst->icon);
        width += iw;
        if (inst->label) width += 6;
        if (ih + 10 > height) height = ih + 10;
    }

    return NTK_SIZE(width, height);
}

static void button_destroy(NtkWidget *w) {
    NtkButtonInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->label);
    if (inst->icon) ntk_pixmap_destroy(inst->icon);
}
