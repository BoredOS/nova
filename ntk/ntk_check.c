// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_check.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_RADIO_GROUP_MAX_BUTTONS 16

struct NtkRadioGroup {
    NtkWidget *buttons[NTK_RADIO_GROUP_MAX_BUTTONS];
    int        button_count;
};

typedef struct {
    char         *label;
    NtkCheckState state;
    bool          tristate;
} NtkCheckBoxInstance;

typedef struct {
    char          *label;
    bool           selected;
    NtkRadioGroup *group;
} NtkRadioButtonInstance;

static void checkbox_paint(NtkWidget *w, NtkPainter *p);
static bool checkbox_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize checkbox_preferred_size(NtkWidget *w);
static void checkbox_destroy(NtkWidget *w);

static const NtkWidgetClass checkbox_class = {
    .type_name      = "NtkCheckBox",
    .paint          = checkbox_paint,
    .layout         = NULL,
    .preferred_size = checkbox_preferred_size,
    .handle_event   = checkbox_handle_event,
    .destroy        = checkbox_destroy
};
static void radio_paint(NtkWidget *w, NtkPainter *p);
static bool radio_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize radio_preferred_size(NtkWidget *w);
static void radio_destroy(NtkWidget *w);

static const NtkWidgetClass radio_class = {
    .type_name      = "NtkRadioButton",
    .paint          = radio_paint,
    .layout         = NULL,
    .preferred_size = radio_preferred_size,
    .handle_event   = radio_handle_event,
    .destroy        = radio_destroy
};
NtkWidget* ntk_checkbox_new(const char *label, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &checkbox_class, sizeof(NtkCheckBoxInstance));
    if (!w) return NULL;

    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->label = label ? strdup(label) : NULL;
    inst->state = NTK_CHECK_UNCHECKED;
    inst->tristate = false;

    return w;
}

void ntk_checkbox_set_checked(NtkWidget *w, bool checked) {
    ntk_checkbox_set_state(w, checked ? NTK_CHECK_CHECKED : NTK_CHECK_UNCHECKED);
}

bool ntk_checkbox_is_checked(NtkWidget *w) {
    return ntk_checkbox_get_state(w) == NTK_CHECK_CHECKED;
}

void ntk_checkbox_set_tristate(NtkWidget *w, bool tristate) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->tristate = tristate;
    }
}

bool ntk_checkbox_is_tristate(NtkWidget *w) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->tristate : false;
}

void ntk_checkbox_set_state(NtkWidget *w, NtkCheckState state) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->state != state) {
        inst->state = state;
        ntk_widget_emit(w, "state-changed", &inst->state);
        ntk_widget_repaint(w);
    }
}

NtkCheckState ntk_checkbox_get_state(NtkWidget *w) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->state : NTK_CHECK_UNCHECKED;
}

static void checkbox_paint(NtkWidget *w, NtkPainter *p) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);
    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    int box_size = 12;
    int box_y = r.y + (r.height - box_size) / 2;
    NtkRect box_r = NTK_RECT(r.x, box_y, box_size, box_size);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, box_r);
    ntk_painter_draw_bevel_sunken(p, box_r, light, dark);
    if (inst->state == NTK_CHECK_CHECKED) {
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_line(p, box_r.x + 2, box_r.y + 5, box_r.x + 5, box_r.y + 8);
        ntk_painter_draw_line(p, box_r.x + 5, box_r.y + 8, box_r.x + 9, box_r.y + 3);
    } else if (inst->state == NTK_CHECK_INDETERMINATE) {
        NtkRect ind_r = NTK_RECT(box_r.x + 3, box_r.y + 3, box_size - 6, box_size - 6);
        ntk_painter_set_color(p, dark);
        ntk_painter_fill_rect(p, ind_r);
    }
    if (inst->label) {
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, font);
        
        if (!ntk_widget_is_enabled(w)) {
            text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED);
        }
        
        ntk_painter_set_color(p, text_col);
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        int text_y = r.y + (r.height - ts.height) / 2;
        ntk_painter_draw_text(p, inst->label, r.x + box_size + 6, text_y);
    }
}

static bool checkbox_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (!ntk_widget_is_enabled(w)) return false;

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        if (inst->tristate) {
            NtkCheckState next = (inst->state + 1) % 3;
            ntk_checkbox_set_state(w, next);
        } else {
            bool checked = (inst->state == NTK_CHECK_CHECKED);
            ntk_checkbox_set_checked(w, !checked);
        }
        return true;
    }
    return false;
}

static NtkSize checkbox_preferred_size(NtkWidget *w) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkStyle *style = ntk_widget_get_style(w);

    int width = 12 + 6;
    int height = 16;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    if (inst->label && font) {
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        width += ts.width;
        if (ts.height + 4 > height) height = ts.height + 4;
    }

    return NTK_SIZE(width, height);
}

static void checkbox_destroy(NtkWidget *w) {
    NtkCheckBoxInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->label);
}
NtkWidget* ntk_radio_button_new(const char *label, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &radio_class, sizeof(NtkRadioButtonInstance));
    if (!w) return NULL;

    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    inst->label = label ? strdup(label) : NULL;
    inst->selected = false;
    inst->group = NULL;

    return w;
}

void ntk_radio_button_set_selected(NtkWidget *w, bool selected) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->selected != selected) {
        inst->selected = selected;
        if (selected && inst->group) {
            for (int i = 0; i < inst->group->button_count; i++) {
                NtkWidget *other = inst->group->buttons[i];
                if (other != w) {
                    NtkRadioButtonInstance *oinst = ntk_widget_get_instance_data(other);
                    if (oinst && oinst->selected) {
                        oinst->selected = false;
                        ntk_widget_emit(other, "toggled", &oinst->selected);
                        ntk_widget_repaint(other);
                    }
                }
            }
        }
        ntk_widget_emit(w, "toggled", &inst->selected);
        ntk_widget_repaint(w);
    }
}

bool ntk_radio_button_is_selected(NtkWidget *w) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->selected : false;
}

static void radio_paint(NtkWidget *w, NtkPainter *p) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    int size = 12;
    int y_pos = r.y + (r.height - size) / 2;
    int cx = r.x + size / 2;
    int cy = y_pos + size / 2;
    int radius = size / 2 - 1;
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_circle(p, cx, cy, radius);
    ntk_painter_set_color(p, dark);
    ntk_painter_draw_circle(p, cx, cy, radius);
    ntk_painter_set_color(p, light);
    ntk_painter_draw_circle(p, cx, cy, radius + 1);
    if (inst->selected) {
        int dot_r = radius - 2;
        if (dot_r < 1) dot_r = 1;
        ntk_painter_set_color(p, text_col);
        ntk_painter_fill_circle(p, cx, cy, dot_r);
    }

    if (inst->label) {
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, font);
        
        if (!ntk_widget_is_enabled(w)) {
            text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED);
        }
        
        ntk_painter_set_color(p, text_col);
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        int text_y = r.y + (r.height - ts.height) / 2;
        ntk_painter_draw_text(p, inst->label, r.x + size + 6, text_y);
    }
}

static bool radio_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    if (!ntk_widget_is_enabled(w)) return false;

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        if (!inst->selected) {
            ntk_radio_button_set_selected(w, true);
        }
        return true;
    }
    return false;
}

static NtkSize radio_preferred_size(NtkWidget *w) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    NtkStyle *style = ntk_widget_get_style(w);

    int width = 12 + 6;
    int height = 16;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    if (inst->label && font) {
        NtkSize ts = ntk_font_measure_text(font, inst->label);
        width += ts.width;
        if (ts.height + 4 > height) height = ts.height + 4;
    }

    return NTK_SIZE(width, height);
}

static void radio_destroy(NtkWidget *w) {
    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->label);
}
NtkRadioGroup* ntk_radio_group_new(void) {
    return calloc(1, sizeof(NtkRadioGroup));
}

void ntk_radio_group_destroy(NtkRadioGroup *g) {
    free(g);
}

void ntk_radio_group_add(NtkRadioGroup *g, NtkWidget *rb) {
    if (!g || !rb || g->button_count >= NTK_RADIO_GROUP_MAX_BUTTONS) return;
    g->buttons[g->button_count++] = rb;

    NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(rb);
    if (inst) {
        inst->group = g;
    }
}

void ntk_radio_group_set_selected(NtkRadioGroup *g, int index) {
    if (g && index >= 0 && index < g->button_count) {
        ntk_radio_button_set_selected(g->buttons[index], true);
    }
}

int ntk_radio_group_get_selected(NtkRadioGroup *g) {
    if (!g) return -1;
    for (int i = 0; i < g->button_count; i++) {
        NtkRadioButtonInstance *inst = ntk_widget_get_instance_data(g->buttons[i]);
        if (inst && inst->selected) {
            return i;
        }
    }
    return -1;
}
