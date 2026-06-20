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
typedef struct {
    const char *start;
    int len;
} WrappedLine;

static NtkSize measure_substring(NtkFont *font, const char *start, int len) {
    char *tmp = malloc(len + 1);
    if (!tmp) return NTK_SIZE(0, 0);
    memcpy(tmp, start, len);
    tmp[len] = '\0';
    NtkSize sz = ntk_font_measure_text(font, tmp);
    free(tmp);
    return sz;
}

static int wrap_text(NtkFont *font, const char *text, int max_width, WrappedLine *lines, int max_lines) {
    if (!text || max_width <= 20) {
        lines[0].start = text;
        lines[0].len = text ? strlen(text) : 0;
        return 1;
    }

    int line_count = 0;
    const char *ptr = text;
    const char *line_start = text;

    while (*ptr && line_count < max_lines) {
        if (*ptr == '\n') {
            lines[line_count].start = line_start;
            lines[line_count].len = ptr - line_start;
            line_count++;
            ptr++;
            line_start = ptr;
            continue;
        }

        if (ptr == line_start && *ptr == ' ') {
            while (*ptr == ' ' && *ptr != '\n') {
                ptr++;
            }
            line_start = ptr;
        }

        const char *word_start = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\n') {
            ptr++;
        }
        int word_len = ptr - word_start;

        int line_len_with_word = ptr - line_start;
        NtkSize sz = measure_substring(font, line_start, line_len_with_word);

        if (sz.width > max_width) {
            if (word_start > line_start) {
                const char *wrap_end = word_start;
                if (wrap_end > line_start && *(wrap_end - 1) == ' ') {
                    wrap_end--;
                }
                lines[line_count].start = line_start;
                lines[line_count].len = wrap_end - line_start;
                line_count++;
                line_start = word_start;
            } else {
                lines[line_count].start = line_start;
                lines[line_count].len = word_len;
                line_count++;
                line_start = ptr;
            }
        }

        if (*ptr == ' ') {
            ptr++;
        }
    }

    if (line_start && *line_start && line_count < max_lines) {
        lines[line_count].start = line_start;
        lines[line_count].len = strlen(line_start);
        line_count++;
    }

    if (line_count == 0) {
        lines[0].start = "";
        lines[0].len = 0;
        return 1;
    }

    return line_count;
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

    WrappedLine lines[64];
    int line_count = wrap_text(font, inst->text, geom.width, lines, 64);
    int line_height = ntk_font_get_line_height(font);
    int total_text_height = line_count * line_height;

    int start_y = origin.y;
    if (geom.height > total_text_height) {
        start_y += (geom.height - total_text_height) / 2;
    }

    NtkRect saved_clip = ntk_painter_get_clip_rect(p);
    bool had_clip = ntk_painter_has_clip(p);
    ntk_painter_set_clip_rect(p, NTK_RECT(origin.x, origin.y, geom.width, geom.height));

    for (int i = 0; i < line_count; i++) {
        char *line_str = malloc(lines[i].len + 1);
        if (line_str) {
            memcpy(line_str, lines[i].start, lines[i].len);
            line_str[lines[i].len] = '\0';

            NtkSize line_sz = ntk_font_measure_text(font, line_str);
            int x = origin.x;
            if (inst->align == NTK_ALIGN_CENTER) {
                x += (geom.width - line_sz.width) / 2;
            } else if (inst->align == NTK_ALIGN_END) {
                x += geom.width - line_sz.width;
            }

            ntk_painter_draw_text(p, line_str, x, start_y + i * line_height);
            free(line_str);
        }
    }

    if (had_clip) {
        ntk_painter_set_clip_rect(p, saved_clip);
    } else {
        ntk_painter_clear_clip(p);
    }
}

static NtkSize label_preferred_size(NtkWidget *w) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst->text || strlen(inst->text) == 0) return NTK_SIZE(0, 0);

    NtkStyle *style = ntk_widget_get_style(w);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    if (!font) return NTK_SIZE(10, 10);

    NtkRect geom = ntk_widget_get_geometry(w);
    int wrap_width = geom.width;

    if (wrap_width <= 20) {
        NtkWidget *parent = ntk_widget_get_parent(w);
        if (parent) {
            NtkRect parent_geom = ntk_widget_get_geometry(parent);
            wrap_width = parent_geom.width;
        }
    }

    if (wrap_width <= 20) {
        wrap_width = 400;
    }

    WrappedLine lines[64];
    int line_count = wrap_text(font, inst->text, wrap_width, lines, 64);
    int line_height = ntk_font_get_line_height(font);

    int max_w = 0;
    for (int i = 0; i < line_count; i++) {
        char *line_str = malloc(lines[i].len + 1);
        if (line_str) {
            memcpy(line_str, lines[i].start, lines[i].len);
            line_str[lines[i].len] = '\0';
            NtkSize line_sz = ntk_font_measure_text(font, line_str);
            if (line_sz.width > max_w) {
                max_w = line_sz.width;
            }
            free(line_str);
        }
    }

    return NTK_SIZE(max_w, line_count * line_height);
}

static void label_destroy(NtkWidget *w) {
    NtkLabelInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->text);
}
