// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_label.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char    *text;
    NtkAlign align;
    NtkColor color;
} NtkLabelInstance;
static void label_paint(NtkWidget *w, NtkPainter *p);
static NtkSize label_preferred_size(NtkWidget *w);
static void label_destroy(NtkWidget *w);

static const NtkWidgetClass label_class = {
    .type_name      = "NtkLabel",
    .paint          = label_paint,
    .layout         = NULL,
    .preferred_size = label_preferred_size,
    .handle_event   = NULL,
    .destroy        = label_destroy
};

NtkWidget* ntk_label_new(const char *text, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &label_class, sizeof(NtkLabelInstance));
    if (!w) return NULL;

    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    inst->text = text ? strdup(text) : NULL;
    inst->align = NTK_ALIGN_START;
    inst->color = 0;

    return w;
}

void ntk_label_set_text(NtkWidget *w, const char *text) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        free(inst->text);
        inst->text = text ? strdup(text) : NULL;
        ntk_widget_repaint(w);
    }
}

const char* ntk_label_get_text(NtkWidget *w) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->text : NULL;
}

void ntk_label_set_alignment(NtkWidget *w, NtkAlign align) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->align = align;
        ntk_widget_repaint(w);
    }
}

void ntk_label_set_color(NtkWidget *w, NtkColor color) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->color = color;
        ntk_widget_repaint(w);
    }
}
static void label_paint(NtkWidget *w, NtkPainter *p) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst->text || strlen(inst->text) == 0) return;

    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor text_col = inst->color;
    if (text_col == 0) {
        text_col = ntk_widget_is_enabled(w) ? 
            ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY) : 
            ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED);
    }

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);
    ntk_painter_set_color(p, text_col);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    ntk_painter_draw_text_rect(p, inst->text, r, inst->align);
}

static NtkSize label_preferred_size(NtkWidget *w) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst->text || strlen(inst->text) == 0) return NTK_SIZE(0, 0);

    NtkStyle *style = ntk_widget_get_style(w);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    if (!font) return NTK_SIZE(10, 10);

    return ntk_font_measure_text(font, inst->text);
}

static void label_destroy(NtkWidget *w) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->text);
}
