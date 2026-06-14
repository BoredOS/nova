// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_tree.h"
#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>

#define NTK_TREE_MAX_CHILDREN 16
#define NTK_TREE_ROW_H        16

struct NtkTreeNode {
    char        *text;
    bool         expanded;
    NtkTreeNode *parent;
    NtkTreeNode *children[NTK_TREE_MAX_CHILDREN];
    int          child_count;
};

typedef struct {
    NtkTreeNode  root;
    NtkTreeNode *selected_node;
} NtkTreeViewInstance;
static void tree_paint(NtkWidget *w, NtkPainter *p);
static bool tree_handle_event(NtkWidget *w, NtkEvent *e);
static NtkSize tree_preferred_size(NtkWidget *w);
static void tree_destroy(NtkWidget *w);

static const NtkWidgetClass tree_class = {
    .type_name      = "NtkTreeView",
    .paint          = tree_paint,
    .layout         = NULL,
    .preferred_size = tree_preferred_size,
    .handle_event   = tree_handle_event,
    .destroy        = tree_destroy
};

NtkWidget* ntk_tree_view_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &tree_class, sizeof(NtkTreeViewInstance));
    if (!w) return NULL;

    NtkTreeViewInstance *inst = ntk_widget_get_instance_data(w);
    inst->root.text = strdup("Root");
    inst->root.expanded = true;
    inst->root.parent = NULL;
    inst->root.child_count = 0;
    inst->selected_node = NULL;

    return w;
}

NtkTreeNode* ntk_tree_view_get_root(NtkWidget *tv) {
    NtkTreeViewInstance *inst = ntk_widget_get_instance_data(tv);
    return inst ? &inst->root : NULL;
}

NtkTreeNode* ntk_tree_node_add_child(NtkTreeNode *parent, const char *text) {
    if (!parent || parent->child_count >= NTK_TREE_MAX_CHILDREN) return NULL;

    NtkTreeNode *child = calloc(1, sizeof(NtkTreeNode));
    if (!child) return NULL;

    child->text = text ? strdup(text) : strdup("");
    child->expanded = false;
    child->parent = parent;
    child->child_count = 0;

    parent->children[parent->child_count++] = child;

    return child;
}

void ntk_tree_node_set_text(NtkTreeNode *node, const char *text) {
    if (node) {
        free(node->text);
        node->text = text ? strdup(text) : strdup("");
    }
}

const char* ntk_tree_node_get_text(NtkTreeNode *node) {
    return node ? node->text : "";
}

void ntk_tree_node_set_expanded(NtkTreeNode *node, bool expanded) {
    if (node) {
        node->expanded = expanded;
    }
}

bool ntk_tree_node_is_expanded(NtkTreeNode *node) {
    return node ? node->expanded : false;
}
static void free_node(NtkTreeNode *node) {
    if (!node) return;
    free(node->text);
    for (int i = 0; i < node->child_count; i++) {
        free_node(node->children[i]);
    }
}

static bool is_last_child(NtkTreeNode *n) {
    if (!n || !n->parent) return true;
    return n->parent->children[n->parent->child_count - 1] == n;
}

static int traverse_nodes(NtkPainter *p, NtkTreeNode *node, NtkFont *font, NtkStyle *style, NtkColor text_col,
                          NtkColor light, NtkColor dark, int x, int y, int width, int indent,
                          NtkTreeNode *selected, int mouse_y, NtkTreeNode **hit_node, bool *toggle_clicked, int mouse_x) {
    if (!node) return y;
    bool is_root = (node->parent == NULL);
    if (!is_root) {
        int row_y = y;
        NtkRect toggle_r = NTK_RECT(x + 12 + indent, row_y + 2, 12, 12);
        
        if (p) {
            int level = indent / 16;
            NtkTreeNode *ancestors[32];
            int anc_count = 0;
            NtkTreeNode *curr = node;
            while (curr && curr->parent) {
                if (anc_count < 32) {
                    ancestors[anc_count++] = curr;
                }
                curr = curr->parent;
            }
            
            for (int j = 0; j <= level; j++) {
                int col_x = x + 12 + j * 16 - 10;
                NtkTreeNode *anc = ancestors[anc_count - 1 - j];
                bool last = is_last_child(anc);
                
                if (j == level) {
                    int line_h = last ? 8 : NTK_TREE_ROW_H;
                    ntk_painter_set_color(p, dark);
                    ntk_painter_draw_line(p, col_x, row_y, col_x, row_y + line_h);
                    
                    int line_end_x = (node->child_count > 0) ? (x + 12 + indent) : (x + 12 + indent + 6);
                    ntk_painter_draw_line(p, col_x, row_y + 8, line_end_x, row_y + 8);
                } else {
                    if (!last) {
                        ntk_painter_set_color(p, dark);
                        ntk_painter_draw_line(p, col_x, row_y, col_x, row_y + NTK_TREE_ROW_H);
                    }
                }
            }
        }

        if (node->child_count > 0) {
            if (p) {
                ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG));
                ntk_painter_fill_rect(p, toggle_r);
                ntk_painter_draw_bevel_raised(p, toggle_r, light, dark);
                ntk_painter_set_color(p, text_col);
                ntk_painter_draw_line(p, toggle_r.x + 3, toggle_r.y + 6, toggle_r.x + 9, toggle_r.y + 6); // Horizontal line
                if (!node->expanded) {
                    ntk_painter_draw_line(p, toggle_r.x + 6, toggle_r.y + 3, toggle_r.x + 6, toggle_r.y + 9); // Vertical line
                }
            }

            if (hit_node && mouse_y >= row_y && mouse_y < row_y + NTK_TREE_ROW_H) {
                if (mouse_x >= toggle_r.x && mouse_x <= toggle_r.x + toggle_r.width) {
                    *hit_node = node;
                    *toggle_clicked = true;
                }
            }
        }

        NtkRect label_r = NTK_RECT(x + 12 + indent + 16, row_y, width - (12 + indent + 16), NTK_TREE_ROW_H);
        
        if (p) {
            bool is_sel = (node == selected);
            if (is_sel) {
                ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
                ntk_painter_fill_rect(p, label_r);
                ntk_painter_set_color(p, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
            } else {
                ntk_painter_set_color(p, text_col);
            }

            int pad_y = (NTK_TREE_ROW_H - ntk_font_get_line_height(font)) / 2;
            ntk_painter_draw_text(p, node->text, label_r.x + 2, label_r.y + pad_y);
        }

        if (hit_node && !(*toggle_clicked) && mouse_y >= row_y && mouse_y < row_y + NTK_TREE_ROW_H) {
            if (mouse_x >= label_r.x && mouse_x <= label_r.x + label_r.width) {
                *hit_node = node;
                *toggle_clicked = false;
            }
        }

        y += NTK_TREE_ROW_H;
        indent += 16;
    }

    if (node->expanded) {
        for (int i = 0; i < node->child_count; i++) {
            y = traverse_nodes(p, node->children[i], font, style, text_col, light, dark, x, y, width, indent, selected, mouse_y, hit_node, toggle_clicked, mouse_x);
        }
    }

    return y;
}
static void tree_paint(NtkWidget *w, NtkPainter *p) {
    NtkTreeViewInstance *inst = ntk_widget_get_instance_data(w);
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
    ntk_painter_set_clip_rect(p, NTK_RECT(r.x + 2, r.y + 2, r.width - 4, r.height - 4));

    traverse_nodes(p, &inst->root, font, style, text_col, light, dark, r.x + 2, r.y + 2, r.width - 4, 0, inst->selected_node, 0, NULL, NULL, 0);

    ntk_painter_clear_clip(p);
}

static bool tree_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkTreeViewInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS && e->mouse_button == NTK_MOUSE_BUTTON_LEFT) {
        NtkPoint p = e->mouse_pos;
        NtkTreeNode *hit_node = NULL;
        bool toggle_clicked = false;
        NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
        traverse_nodes(NULL, &inst->root, font, style, 0, 0, 0, 2, 2, geom.width - 4, 0, inst->selected_node, p.y, &hit_node, &toggle_clicked, p.x);

        if (hit_node) {
            if (toggle_clicked) {
                hit_node->expanded = !hit_node->expanded;
            } else {
                inst->selected_node = hit_node;
                ntk_widget_emit(w, "selection-changed", hit_node);
            }
            ntk_widget_repaint(w);
            return true;
        }
    }
    return false;
}

static NtkSize tree_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(150, 100);
}

static void tree_destroy(NtkWidget *w) {
    NtkTreeViewInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->root.child_count; i++) {
        free_node(inst->root.children[i]);
    }
}
