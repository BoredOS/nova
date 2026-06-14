// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_progress.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    float value;
    char *text;
} NtkProgressBarInstance;

static void progress_paint(NtkWidget *w, NtkPainter *p);
static NtkSize progress_preferred_size(NtkWidget *w);
static void progress_destroy(NtkWidget *w);

static const NtkWidgetClass progress_class = {
    .type_name      = "NtkProgressBar",
    .paint          = progress_paint,
    .layout         = NULL,
    .preferred_size = progress_preferred_size,
    .handle_event   = NULL,
    .destroy        = progress_destroy
};

NtkWidget* ntk_progress_bar_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &progress_class, sizeof(NtkProgressBarInstance));
    if (!w) return NULL;

    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    inst->value = 0.0f;
    inst->text = NULL;

    return w;
}

void ntk_progress_bar_set_value(NtkWidget *w, float value) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        if (inst->value != value) {
            inst->value = value;
            ntk_widget_repaint(w);
        }
    }
}

float ntk_progress_bar_get_value(NtkWidget *w) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->value : 0.0f;
}

void ntk_progress_bar_set_text(NtkWidget *w, const char *text) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        free(inst->text);
        inst->text = text ? strdup(text) : NULL;
        ntk_widget_repaint(w);
    }
}

const char* ntk_progress_bar_get_text(NtkWidget *w) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->text : NULL;
}
static void progress_paint(NtkWidget *w, NtkPainter *p) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor fill_col = ntk_style_get_color(style, NTK_STYLE_ROLE_ACCENT);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    ntk_painter_draw_bevel_sunken(p, r, light, dark);
    int border = 2;
    NtkRect inner = NTK_RECT(r.x + border, r.y + border, r.width - 2 * border, r.height - 2 * border);
    int fill_w = (int)(inst->value * inner.width);
    if (fill_w > 0) {
        NtkRect fill_r = NTK_RECT(inner.x, inner.y, fill_w, inner.height);
        
        NtkGradient *g = ntk_style_get_gradient(style, NTK_STYLE_GRADIENT_PROGRESS_FILL);
        if (g) {
            ntk_painter_draw_gradient(p, g, fill_r);
        } else {
            ntk_painter_set_color(p, fill_col);
            ntk_painter_fill_rect(p, fill_r);
        }
    }

    char pct_buf[32];
    const char *overlay = inst->text;
    if (!overlay) {
        snprintf(pct_buf, sizeof(pct_buf), "%d%%", (int)(inst->value * 100));
        overlay = pct_buf;
    }

    if (overlay && strlen(overlay) > 0) {
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, font);
        
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text_rect(p, overlay, inner, NTK_ALIGN_CENTER);
    }
}

static NtkSize progress_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(150, 18);
}

static void progress_destroy(NtkWidget *w) {
    NtkProgressBarInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->text);
}
