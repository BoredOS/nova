// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_entry.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char text[256];
    char placeholder[128];
    bool read_only;
    bool password;
    int  cursor_pos;
} NtkTextEntryInstance;
static void entry_paint(NtkWidget *w, NtkPainter *p);
static bool entry_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize entry_preferred_size(NtkWidget *w);

static const NtkWidgetClass entry_class = {
    .type_name      = "NtkTextEntry",
    .paint          = entry_paint,
    .layout         = NULL,
    .preferred_size = entry_preferred_size,
    .handle_event   = entry_handle_event,
    .destroy        = NULL
};
NtkWidget* ntk_text_entry_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &entry_class, sizeof(NtkTextEntryInstance));
    if (!w) return NULL;

    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    memset(inst->text, 0, 256);
    memset(inst->placeholder, 0, 128);
    inst->read_only = false;
    inst->password = false;
    inst->cursor_pos = 0;
    ntk_widget_set_cursor(w, NTK_CURSOR_IBEAM);

    return w;
}

void ntk_text_entry_set_text(NtkWidget *w, const char *text) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        strncpy(inst->text, text ? text : "", 255);
        inst->text[255] = '\0';
        inst->cursor_pos = (int)strlen(inst->text);
        ntk_widget_emit(w, "text-changed", inst->text);
        ntk_widget_repaint(w);
    }
}

const char* ntk_text_entry_get_text(NtkWidget *w) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->text : "";
}

void ntk_text_entry_set_placeholder(NtkWidget *w, const char *text) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        strncpy(inst->placeholder, text ? text : "", 127);
        inst->placeholder[127] = '\0';
        ntk_widget_repaint(w);
    }
}

const char* ntk_text_entry_get_placeholder(NtkWidget *w) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->placeholder : "";
}

void ntk_text_entry_set_read_only(NtkWidget *w, bool read_only) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->read_only = read_only;
    }
}

bool ntk_text_entry_is_read_only(NtkWidget *w) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->read_only : false;
}

void ntk_text_entry_set_password_mode(NtkWidget *w, bool password) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->password = password;
        ntk_widget_repaint(w);
    }
}

bool ntk_text_entry_is_password_mode(NtkWidget *w) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->password : false;
}
static void entry_paint(NtkWidget *w, NtkPainter *p) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);
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
    int pad_y = (r.height - ntk_font_get_line_height(font)) / 2;
    if (pad_y < 2) pad_y = 2;
    char draw_buf[256] = {0};
    bool empty = (strlen(inst->text) == 0);

    if (empty) {
        if (strlen(inst->placeholder) > 0) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_DISABLED));
            ntk_painter_draw_text(p, inst->placeholder, r.x + pad_x, r.y + pad_y);
        }
    } else {
        if (inst->password) {
            size_t len = strlen(inst->text);
            for (size_t i = 0; i < len && i < 255; i++) draw_buf[i] = '*';
            draw_buf[len] = '\0';
        } else {
            strcpy(draw_buf, inst->text);
        }

        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_text(p, draw_buf, r.x + pad_x, r.y + pad_y);
    }
    if (ntk_widget_has_focus(w)) {
        int cur_x = r.x + pad_x;
        if (inst->cursor_pos > 0 && !empty) {
            char sub[256];
            strncpy(sub, draw_buf, (size_t)inst->cursor_pos);
            sub[inst->cursor_pos] = '\0';
            NtkSize sub_sz = ntk_font_measure_text(font, sub);
            cur_x += sub_sz.width;
        }

        int cur_h = ntk_font_get_line_height(font);
        ntk_painter_set_color(p, text_col);
        ntk_painter_draw_line(p, cur_x, r.y + pad_y, cur_x, r.y + pad_y + cur_h);
    }
}

static bool entry_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkTextEntryInstance *inst = ntk_widget_get_instance_data(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        ntk_widget_set_focus(w);
        ntk_widget_repaint(w);
        return true;
    }

    if (ntk_widget_has_focus(w) && e->type == NTK_EVENT_KEY_PRESS) {
        if (inst->read_only) return false;
        size_t len = strlen(inst->text);
        if (e->key_code == 39) { 
            if (inst->cursor_pos > 0) {
                for (int i = inst->cursor_pos - 1; i < (int)len; i++) {
                    inst->text[i] = inst->text[i + 1];
                }
                inst->cursor_pos--;
                ntk_widget_emit(w, "text-changed", inst->text);
                ntk_widget_repaint(w);
            }
            return true;
        } else if (e->key_code == 37) {
            ntk_widget_emit(w, "activated", inst->text);
            return true;
        } else if (e->key_code == 42) {
            if (inst->cursor_pos > 0) {
                inst->cursor_pos--;
                ntk_widget_repaint(w);
            }
            return true;
        } else if (e->key_code == 43) { 
            if (inst->cursor_pos < (int)len) {
                inst->cursor_pos++;
                ntk_widget_repaint(w);
            }
            return true;
        } else if (strlen(e->text) > 0) {
            char c = e->text[0];
            if (c >= 32 && c <= 126 && len < 254) {
                for (int i = (int)len; i >= inst->cursor_pos; i--) {
                    inst->text[i + 1] = inst->text[i];
                }
                inst->text[inst->cursor_pos] = c;
                inst->cursor_pos++;
                ntk_widget_emit(w, "text-changed", inst->text);
                ntk_widget_repaint(w);
                return true;
            }
        }
    }
    return false;
}

static NtkSize entry_preferred_size(NtkWidget *w) {
    NtkStyle *style = ntk_widget_get_style(w);
    int height = ntk_style_get_metric(style, NTK_STYLE_METRIC_ENTRY_HEIGHT);
    if (height <= 0) height = 22;

    return NTK_SIZE(120, height);
}
