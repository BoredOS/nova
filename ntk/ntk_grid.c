// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_grid.h"
#include <stdlib.h>
#include <string.h>

#define NTK_GRID_MAX_CHILDREN 32
#define NTK_GRID_MAX_COLS     10
#define NTK_GRID_MAX_ROWS     10

typedef struct {
    NtkWidget *child;
    int        left;
    int        top;
    int        width;
    int        height;
} NtkGridChild;

typedef struct {
    int          col_spacing;
    int          row_spacing;
    bool         col_homogeneous;
    bool         row_homogeneous;
    NtkGridChild children[NTK_GRID_MAX_CHILDREN];
    int          child_count;
} NtkGridInstance;

static void grid_paint(NtkWidget *w, NtkPainter *p);
static void grid_layout(NtkWidget *w);
static NtkSize grid_preferred_size(NtkWidget *w);
static void grid_destroy(NtkWidget *w);

static const NtkWidgetClass grid_class = {
    .type_name      = "NtkGrid",
    .paint          = grid_paint,
    .layout         = grid_layout,
    .preferred_size = grid_preferred_size,
    .handle_event   = NULL,
    .destroy        = grid_destroy
};

NtkWidget* ntk_grid_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &grid_class, sizeof(NtkGridInstance));
    if (!w) return NULL;

    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    inst->col_spacing = 4;
    inst->row_spacing = 4;
    inst->col_homogeneous = false;
    inst->row_homogeneous = false;

    return w;
}

void ntk_grid_set_column_spacing(NtkWidget *w, int spacing) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->col_spacing = spacing;
        ntk_widget_repaint(w);
    }
}

void ntk_grid_set_row_spacing(NtkWidget *w, int spacing) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->row_spacing = spacing;
        ntk_widget_repaint(w);
    }
}

void ntk_grid_set_column_homogeneous(NtkWidget *w, bool homogeneous) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->col_homogeneous = homogeneous;
        ntk_widget_repaint(w);
    }
}

void ntk_grid_set_row_homogeneous(NtkWidget *w, bool homogeneous) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->row_homogeneous = homogeneous;
        ntk_widget_repaint(w);
    }
}

void ntk_grid_attach(NtkWidget *grid, NtkWidget *child, int left, int top, int width, int height) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(grid);
    if (!inst || inst->child_count >= NTK_GRID_MAX_CHILDREN) return;
    if (left < 0 || left >= NTK_GRID_MAX_COLS) return;
    if (top < 0 || top >= NTK_GRID_MAX_ROWS) return;

    ntk_widget_add_child(grid, child);

    NtkGridChild *c = &inst->children[inst->child_count++];
    c->child = child;
    c->left = left;
    c->top = top;
    c->width = width > 0 ? width : 1;
    c->height = height > 0 ? height : 1;

    ntk_widget_repaint(grid);
}
static void grid_paint(NtkWidget *w, NtkPainter *p) {
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->paint) klass->paint(child, p);
        }
    }
}

static void grid_layout(NtkWidget *w) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int max_col = 0;
    int max_row = 0;
    for (int i = 0; i < inst->child_count; i++) {
        NtkGridChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            int right = c->left + c->width - 1;
            int bottom = c->top + c->height - 1;
            if (right > max_col) max_col = right;
            if (bottom > max_row) max_row = bottom;
        }
    }

    int num_cols = max_col + 1;
    int num_rows = max_row + 1;
    if (num_cols == 0 || num_rows == 0) return;

    int col_widths[NTK_GRID_MAX_COLS] = {0};
    int row_heights[NTK_GRID_MAX_ROWS] = {0};
    for (int i = 0; i < inst->child_count; i++) {
        NtkGridChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            NtkSize pref = ntk_widget_get_preferred_size(c->child);
            if (c->width == 1) {
                if (pref.width > col_widths[c->left]) col_widths[c->left] = pref.width;
            }
            if (c->height == 1) {
                if (pref.height > row_heights[c->top]) row_heights[c->top] = pref.height;
            }
        }
    }
    if (inst->col_homogeneous) {
        int spacing_w = (num_cols - 1) * inst->col_spacing;
        int target_w = (geom.width - spacing_w) / num_cols;
        for (int i = 0; i < num_cols; i++) col_widths[i] = target_w;
    }
    if (inst->row_homogeneous) {
        int spacing_h = (num_rows - 1) * inst->row_spacing;
        int target_h = (geom.height - spacing_h) / num_rows;
        for (int i = 0; i < num_rows; i++) row_heights[i] = target_h;
    }

    if (!inst->col_homogeneous) {
        int sum_w = 0;
        for (int i = 0; i < num_cols; i++) sum_w += col_widths[i];
        int spacing_w = (num_cols - 1) * inst->col_spacing;
        int diff = geom.width - spacing_w - sum_w;
        if (diff > 0) {
            for (int i = 0; i < num_cols; i++) col_widths[i] += diff / num_cols;
        }
    }

    if (!inst->row_homogeneous) {
        int sum_h = 0;
        for (int i = 0; i < num_rows; i++) sum_h += row_heights[i];
        int spacing_h = (num_rows - 1) * inst->row_spacing;
        int diff = geom.height - spacing_h - sum_h;
        if (diff > 0) {
            for (int i = 0; i < num_rows; i++) row_heights[i] += diff / num_rows;
        }
    }

    int col_x[NTK_GRID_MAX_COLS] = {0};
    int row_y[NTK_GRID_MAX_ROWS] = {0};

    int cx = 0;
    for (int i = 0; i < num_cols; i++) {
        col_x[i] = cx;
        cx += col_widths[i] + inst->col_spacing;
    }

    int cy = 0;
    for (int i = 0; i < num_rows; i++) {
        row_y[i] = cy;
        cy += row_heights[i] + inst->row_spacing;
    }

    for (int i = 0; i < inst->child_count; i++) {
        NtkGridChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            int width_pixels = 0;
            for (int w_idx = 0; w_idx < c->width; w_idx++) {
                if (c->left + w_idx < num_cols) {
                    width_pixels += col_widths[c->left + w_idx];
                }
            }
            if (c->width > 1) {
                width_pixels += (c->width - 1) * inst->col_spacing;
            }

            int height_pixels = 0;
            for (int h_idx = 0; h_idx < c->height; h_idx++) {
                if (c->top + h_idx < num_rows) {
                    height_pixels += row_heights[c->top + h_idx];
                }
            }
            if (c->height > 1) {
                height_pixels += (c->height - 1) * inst->row_spacing;
            }

            int x = geom.x + col_x[c->left];
            int y = geom.y + row_y[c->top];

            ntk_widget_set_geometry(c->child, NTK_RECT(x, y, width_pixels, height_pixels));

            const NtkWidgetClass *klass = ntk_widget_get_class(c->child);
            if (klass && klass->layout) klass->layout(c->child);
        }
    }
}

static NtkSize grid_preferred_size(NtkWidget *w) {
    NtkGridInstance *inst = ntk_widget_get_instance_data(w);
    int max_col = 0;
    int max_row = 0;

    for (int i = 0; i < inst->child_count; i++) {
        NtkGridChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            int right = c->left + c->width - 1;
            int bottom = c->top + c->height - 1;
            if (right > max_col) max_col = right;
            if (bottom > max_row) max_row = bottom;
        }
    }

    int num_cols = max_col + 1;
    int num_rows = max_row + 1;
    if (num_cols == 0 || num_rows == 0) return NTK_SIZE_ZERO;

    int total_w = 0;
    for (int col = 0; col < num_cols; col++) {
        int col_max = 0;
        for (int i = 0; i < inst->child_count; i++) {
            NtkGridChild *c = &inst->children[i];
            if (c->child && ntk_widget_is_visible(c->child) && c->left == col && c->width == 1) {
                NtkSize pref = ntk_widget_get_preferred_size(c->child);
                if (pref.width > col_max) col_max = pref.width;
            }
        }
        total_w += col_max;
    }
    total_w += (num_cols - 1) * inst->col_spacing;

    int total_h = 0;
    for (int row = 0; row < num_rows; row++) {
        int row_max = 0;
        for (int i = 0; i < inst->child_count; i++) {
            NtkGridChild *c = &inst->children[i];
            if (c->child && ntk_widget_is_visible(c->child) && c->top == row && c->height == 1) {
                NtkSize pref = ntk_widget_get_preferred_size(c->child);
                if (pref.height > row_max) row_max = pref.height;
            }
        }
        total_h += row_max;
    }
    total_h += (num_rows - 1) * inst->row_spacing;

    return NTK_SIZE(total_w, total_h);
}

static void grid_destroy(NtkWidget *w) {
    (void)w;
}
