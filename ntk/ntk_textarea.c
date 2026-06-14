// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_textarea.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    int   text_len;
    int   text_capacity;
} NtkTextAreaInstance;
static void textarea_paint(NtkWidget *w, NtkPainter *p);
static NtkSize textarea_preferred_size(NtkWidget *w);
static void textarea_destroy(NtkWidget *w);

static const NtkWidgetClass textarea_class = {
    .type_name      = "NtkTextArea",
    .paint          = textarea_paint,
    .layout         = NULL,
    .preferred_size = textarea_preferred_size,
    .handle_event   = NULL,
    .destroy        = textarea_destroy
};

NtkWidget* ntk_text_area_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &textarea_class, sizeof(NtkTextAreaInstance));
    if (!w) return NULL;

    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    inst->text_capacity = 1024;
    inst->text = malloc((size_t)inst->text_capacity);
    if (inst->text) inst->text[0] = '\0';
    inst->text_len = 0;

    ntk_widget_set_cursor(w, NTK_CURSOR_IBEAM);

    return w;
}

void ntk_text_area_set_text(NtkWidget *w, const char *text) {
    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        int len = text ? (int)strlen(text) : 0;
        if (len >= inst->text_capacity) {
            inst->text_capacity = len + 256;
            inst->text = realloc(inst->text, (size_t)inst->text_capacity);
        }
        if (inst->text) {
            strcpy(inst->text, text ? text : "");
            inst->text_len = len;
            ntk_widget_repaint(w);
        }
    }
}

const char* ntk_text_area_get_text(NtkWidget *w) {
    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->text : "";
}

void ntk_text_area_append(NtkWidget *w, const char *text) {
    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && text) {
        int len = (int)strlen(text);
        if (inst->text_len + len >= inst->text_capacity) {
            inst->text_capacity = inst->text_len + len + 512;
            inst->text = realloc(inst->text, (size_t)inst->text_capacity);
        }
        if (inst->text) {
            strcat(inst->text, text);
            inst->text_len += len;
            ntk_widget_repaint(w);
        }
    }
}
static void textarea_paint(NtkWidget *w, NtkPainter *p) {
    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = 0xFFFFFFFF;
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor text_col = ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY);

    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);
    ntk_painter_set_color(p, bg);
    ntk_painter_fill_rect(p, r);
    ntk_painter_draw_bevel_sunken(p, r, light, dark);

    if (!inst->text || inst->text_len == 0) return;

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(p, font);
    ntk_painter_set_color(p, text_col);

    int pad_x = 4;
    int pad_y = 4;
    int line_h = ntk_font_get_line_height(font);
    char *text_copy = strdup(inst->text);
    if (!text_copy) return;

    char *line = strtok(text_copy, "\n");
    int curr_y = r.y + pad_y;

    while (line) {
        if (curr_y + line_h > r.y + r.height - pad_y) break; 
        ntk_painter_draw_text(p, line, r.x + pad_x, curr_y);
        curr_y += line_h;
        line = strtok(NULL, "\n");
    }

    free(text_copy);
}

static NtkSize textarea_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(200, 100);
}

static void textarea_destroy(NtkWidget *w) {
    NtkTextAreaInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->text);
}
