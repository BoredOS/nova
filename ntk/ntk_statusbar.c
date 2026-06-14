// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_statusbar.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_STATUSBAR_MAX_MESSAGES 8

typedef struct {
    char *messages[NTK_STATUSBAR_MAX_MESSAGES];
    int   message_count;
} NtkStatusBarInstance;

static void statusbar_paint(NtkWidget *w, NtkPainter *p);
static NtkSize statusbar_preferred_size(NtkWidget *w);
static void statusbar_destroy(NtkWidget *w);

static const NtkWidgetClass statusbar_class = {
    .type_name      = "NtkStatusBar",
    .paint          = statusbar_paint,
    .layout         = NULL,
    .preferred_size = statusbar_preferred_size,
    .handle_event   = NULL,
    .destroy        = statusbar_destroy
};

NtkWidget* ntk_statusbar_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &statusbar_class, sizeof(NtkStatusBarInstance));
    if (!w) return NULL;

    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(w);
    inst->message_count = 0;

    return w;
}

void ntk_statusbar_push(NtkWidget *sb, const char *text) {
    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(sb);
    if (inst && inst->message_count < NTK_STATUSBAR_MAX_MESSAGES) {
        inst->messages[inst->message_count++] = text ? strdup(text) : strdup("");
        ntk_widget_repaint(sb);
    }
}

void ntk_statusbar_pop(NtkWidget *sb) {
    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(sb);
    if (inst && inst->message_count > 0) {
        free(inst->messages[--inst->message_count]);
        ntk_widget_repaint(sb);
    }
}

void ntk_statusbar_set_text(NtkWidget *sb, const char *text) {
    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(sb);
    if (inst) {
        while (inst->message_count > 0) {
            free(inst->messages[--inst->message_count]);
        }
        ntk_statusbar_push(sb, text);
    }
}
static void statusbar_paint(NtkWidget *w, NtkPainter *p) {
    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);
    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_STATUSBAR_BG);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    NtkRect status_r = NTK_RECT(r.x + 4, r.y + 2, r.width - 8, r.height - 4);
    ntk_painter_draw_bevel_sunken(p, status_r, light, dark);

    if (inst->message_count > 0) {
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_STATUS_FONT);
        if (!font) font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        ntk_painter_set_font(p, font);
        ntk_painter_set_color(p, text_col);

        int pad_x = 6;
        int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;
        if (pad_y < 2) pad_y = 2;

        ntk_painter_draw_text(p, inst->messages[inst->message_count - 1], status_r.x + pad_x, r.y + pad_y);
    }
}

static NtkSize statusbar_preferred_size(NtkWidget *w) {
    NtkStyle *style = ntk_widget_get_style(w);
    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_STATUSBAR_HEIGHT);
    if (height <= 0) height = 20;

    return NTK_SIZE(100, height);
}

static void statusbar_destroy(NtkWidget *w) {
    NtkStatusBarInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->message_count; i++) {
        free(inst->messages[i]);
    }
}
