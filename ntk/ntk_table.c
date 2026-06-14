// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_table.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_TABLE_MAX_COLS  8
#define NTK_TABLE_MAX_ROWS  32
#define NTK_TABLE_HEADER_H  20
#define NTK_TABLE_ROW_H     16

typedef struct {
    int   col_count;
    char *headers[NTK_TABLE_MAX_COLS];
    int   col_widths[NTK_TABLE_MAX_COLS];
    
    int   row_count;
    char *cells[NTK_TABLE_MAX_ROWS][NTK_TABLE_MAX_COLS];
    
    int   selected_row;
} NtkTableViewInstance;

static void table_paint(NtkWidget *w, NtkPainter *p);
static bool table_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize table_preferred_size(NtkWidget *w);
static void table_destroy(NtkWidget *w);

static const NtkWidgetClass table_class = {
    .type_name      = "NtkTableView",
    .paint          = table_paint,
    .layout         = NULL,
    .preferred_size = table_preferred_size,
    .handle_event   = table_handle_event,
    .destroy        = table_destroy
};

NtkWidget* ntk_table_view_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &table_class, sizeof(NtkTableViewInstance));
    if (!w) return NULL;

    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    inst->col_count = 0;
    inst->row_count = 0;
    inst->selected_row = -1;

    return w;
}

void ntk_table_view_set_columns(NtkWidget *w, int cols, const char **headers, int *widths) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst || cols < 0) return;
    if (cols > NTK_TABLE_MAX_COLS) cols = NTK_TABLE_MAX_COLS;

    for (int i = 0; i < inst->col_count; i++) {
        free(inst->headers[i]);
        inst->headers[i] = NULL;
    }

    inst->col_count = cols;
    for (int i = 0; i < cols; i++) {
        inst->headers[i] = headers[i] ? strdup(headers[i]) : strdup("");
        inst->col_widths[i] = widths ? widths[i] : 60;
    }
    ntk_widget_repaint(w);
}

void ntk_table_view_set_cell(NtkWidget *w, int row, int col, const char *text) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (row < 0 || row >= NTK_TABLE_MAX_ROWS) return;
    if (col < 0 || col >= NTK_TABLE_MAX_COLS) return;

    free(inst->cells[row][col]);
    inst->cells[row][col] = text ? strdup(text) : NULL;
    
    if (row >= inst->row_count) {
        inst->row_count = row + 1;
    }
    ntk_widget_repaint(w);
}

const char* ntk_table_view_get_cell(NtkWidget *w, int row, int col) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && row >= 0 && row < NTK_TABLE_MAX_ROWS && col >= 0 && col < NTK_TABLE_MAX_COLS) {
        return inst->cells[row][col];
    }
    return NULL;
}

void ntk_table_view_set_row_count(NtkWidget *w, int count) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst || count < 0) return;
    if (count > NTK_TABLE_MAX_ROWS) count = NTK_TABLE_MAX_ROWS;

    for (int r = count; r < inst->row_count; r++) {
        for (int c = 0; c < inst->col_count; c++) {
            free(inst->cells[r][c]);
            inst->cells[r][c] = NULL;
        }
    }
    inst->row_count = count;
    if (inst->selected_row >= count) {
        inst->selected_row = -1;
    }
    ntk_widget_repaint(w);
}

static void table_paint(NtkWidget *w, NtkPainter *p) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG);
    NtkColor btn_bg = ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG);
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
    int curr_x = r.x + 2;
    for (int i = 0; i < inst->col_count; i++) {
        NtkRect hdr_r = NTK_RECT(curr_x, r.y + 2, inst->col_widths[i], NTK_TABLE_HEADER_H);
        
        ntk_painter_set_color(p, btn_bg);
        ntk_painter_fill_rect(p, hdr_r);
        ntk_painter_draw_bevel_raised(p, hdr_r, light, dark);

        if (inst->headers[i]) {
            int pad_y = (NTK_TABLE_HEADER_H - ntk_font_get_line_height(font)) / 2;
            ntk_painter_set_color(p, text_col);
            ntk_painter_draw_text(p, inst->headers[i], hdr_r.x + 4, hdr_r.y + pad_y);
        }
        curr_x += inst->col_widths[i];
    }
    ntk_painter_set_clip_rect(p, NTK_RECT(r.x + 2, r.y + 2 + NTK_TABLE_HEADER_H, r.width - 4, r.height - 4 - NTK_TABLE_HEADER_H));
    for (int row = 0; row < inst->row_count; row++) {
        int row_y = r.y + 2 + NTK_TABLE_HEADER_H + row * NTK_TABLE_ROW_H;
        if (row_y + NTK_TABLE_ROW_H > r.y + r.height - 2) break;

        NtkRect row_r = NTK_RECT(r.x + 2, row_y, r.width - 4, NTK_TABLE_ROW_H);
        
        bool selected = (row == inst->selected_row);
        if (selected) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(p, row_r);
        } else if (row % 2 == 1) {
            ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG_HOVER));
            ntk_painter_fill_rect(p, row_r);
        }

        curr_x = r.x + 2;
        for (int col = 0; col < inst->col_count; col++) {
            NtkRect cell_r = NTK_RECT(curr_x, row_y, inst->col_widths[col], NTK_TABLE_ROW_H);
            
            if (inst->cells[row][col]) {
                if (selected) {
                    ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
                } else {
                    ntk_painter_set_color(p, text_col);
                }
                int pad_y = (NTK_TABLE_ROW_H - ntk_font_get_line_height(font)) / 2;
                ntk_painter_draw_text(p, inst->cells[row][col], cell_r.x + 4, cell_r.y + pad_y);
            }
            curr_x += inst->col_widths[col];
        }
    }

    ntk_painter_clear_clip(p);
}

static bool table_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        int row_area_top = 2 + NTK_TABLE_HEADER_H;
        
        if (p.x >= 2 && p.x <= geom.width - 2 && p.y >= row_area_top && p.y <= geom.height - 2) {
            int clicked_row = (p.y - row_area_top) / NTK_TABLE_ROW_H;
            if (clicked_row >= 0 && clicked_row < inst->row_count) {
                inst->selected_row = clicked_row;
                ntk_widget_emit(w, "selection-changed", &inst->selected_row);
                ntk_widget_repaint(w);
                return true;
            }
        }
    }
    return false;
}

static NtkSize table_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(200, 100);
}

static void table_destroy(NtkWidget *w) {
    NtkTableViewInstance *inst = ntk_widget_get_instance_data(w);
        for (int i = 0; i < inst->col_count; i++) {
        free(inst->headers[i]);
    }
        for (int r = 0; r < NTK_TABLE_MAX_ROWS; r++) {
        for (int c = 0; c < NTK_TABLE_MAX_COLS; c++) {
            free(inst->cells[r][c]);
        }
    }
}
