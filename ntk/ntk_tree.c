// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_tree.h"
#include "ntk_style.h"

#include <stdlib.h>
#include <string.h>

#define NTK_TREE_ROW_HEIGHT 20
#define NTK_TREE_INDENT 16

struct NtkTreeNode {
    char *text;
    bool expanded;
    bool may_have_children;
    void *user_data;
    NtkPixmap *icon;
    NtkTreeNode *parent;
    NtkTreeNode **children;
    int child_count;
    int child_capacity;
};

typedef struct {
    NtkTreeNode root;
    NtkTreeNode *selected_node;
} NtkTreeViewInstance;

static void tree_paint(NtkWidget *widget, NtkPainter *painter);
static bool tree_handle_event(NtkWidget *widget, NtkEvent *event);
static NtkSize tree_preferred_size(NtkWidget *widget);
static void tree_destroy(NtkWidget *widget);

static const NtkWidgetClass tree_class = {
    .type_name = "NtkTreeView",
    .paint = tree_paint,
    .layout = NULL,
    .preferred_size = tree_preferred_size,
    .handle_event = tree_handle_event,
    .destroy = tree_destroy,
};

static void tree_node_destroy(NtkTreeNode *node) {
    if (!node) return;

    for (int i = 0; i < node->child_count; ++i) {
        tree_node_destroy(node->children[i]);
    }

    free(node->children);
    free(node->text);
    if (node->icon) ntk_pixmap_destroy(node->icon);
    if (node->parent) free(node);
}

static int tree_visible_node_count(const NtkTreeNode *node) {
    int count = node->parent ? 1 : 0;
    if (node->expanded) {
        for (int i = 0; i < node->child_count; ++i) {
            count += tree_visible_node_count(node->children[i]);
        }
    }
    return count;
}

static void tree_paint_node(NtkWidget *widget,
                            NtkPainter *painter,
                            NtkTreeNode *node,
                            int depth,
                            int *row) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    NtkStyle *style = ntk_widget_get_style(widget);

    if (node->parent) {
        NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
        NtkRect geometry = ntk_widget_get_geometry(widget);
        int row_y = origin.y + 2 + *row * NTK_TREE_ROW_HEIGHT;
        int toggle_x = origin.x + 4 + depth * NTK_TREE_INDENT;
        NtkRect row_rect = NTK_RECT(origin.x + 2, row_y,
                                    geometry.width - 4, NTK_TREE_ROW_HEIGHT);

        if (view->selected_node == node) {
            ntk_painter_set_color(painter,
                                  ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(painter, row_rect);
            ntk_painter_set_color(painter,
                                  ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
        } else {
            ntk_painter_set_color(painter,
                                  ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY));
        }

        if (node->may_have_children || node->child_count > 0) {
            NtkRect toggle_rect = NTK_RECT(toggle_x, row_y + 4, 11, 11);
            ntk_painter_draw_rect(painter, toggle_rect);
            ntk_painter_draw_line(painter, toggle_x + 2, row_y + 9,
                                  toggle_x + 9, row_y + 9);
            if (!node->expanded) {
                ntk_painter_draw_line(painter, toggle_x + 5, row_y + 6,
                                      toggle_x + 5, row_y + 12);
            }
        }

        int text_x = toggle_x + 15;
        if (node->icon) {
            ntk_painter_draw_pixmap(painter, node->icon, text_x, row_y + 2);
            text_x += 18;
        }
        ntk_painter_draw_text(painter, node->text ? node->text : "", text_x, row_y + 2);

        ++*row;
        ++depth;
    }

    if (node->expanded) {
        for (int i = 0; i < node->child_count; ++i) {
            tree_paint_node(widget, painter, node->children[i], depth, row);
        }
    }
}

static NtkTreeNode *tree_hit_test(NtkTreeNode *node,
                                  int depth,
                                  int wanted_row,
                                  int *current_row,
                                  int mouse_x,
                                  bool *toggle_hit) {
    if (node->parent) {
        if (*current_row == wanted_row) {
            int toggle_x = 4 + depth * NTK_TREE_INDENT;
            *toggle_hit = mouse_x >= toggle_x && mouse_x < toggle_x + 15;
            return node;
        }
        ++*current_row;
        ++depth;
    }

    if (node->expanded) {
        for (int i = 0; i < node->child_count; ++i) {
            NtkTreeNode *hit = tree_hit_test(node->children[i], depth, wanted_row,
                                             current_row, mouse_x, toggle_hit);
            if (hit) return hit;
        }
    }
    return NULL;
}

static void tree_paint(NtkWidget *widget, NtkPainter *painter) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    NtkStyle *style = ntk_widget_get_style(widget);
    NtkRect geometry = ntk_widget_get_geometry(widget);
    NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
    NtkRect bounds = NTK_RECT(origin.x, origin.y, geometry.width, geometry.height);

    ntk_painter_set_color(painter,
                          ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG));
    ntk_painter_fill_rect(painter, bounds);
    ntk_painter_draw_bevel_sunken(
        painter, bounds,
        ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT),
        ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK));

    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(painter, font);
    ntk_painter_set_clip_rect(painter,
                              NTK_RECT(origin.x + 2, origin.y + 2,
                                       geometry.width - 4, geometry.height - 4));

    int row = 0;
    tree_paint_node(widget, painter, &view->root, 0, &row);
    ntk_painter_clear_clip(painter);
}

static bool tree_handle_event(NtkWidget *widget, NtkEvent *event) {
    if (event->type != NTK_EVENT_MOUSE_PRESS ||
        event->mouse_button != NTK_MOUSE_BUTTON_LEFT) {
        return false;
    }

    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    int wanted_row = (event->mouse_pos.y - 2) / NTK_TREE_ROW_HEIGHT;
    int current_row = 0;
    bool toggle_hit = false;
    NtkTreeNode *node = tree_hit_test(&view->root, 0, wanted_row, &current_row,
                                      event->mouse_pos.x, &toggle_hit);
    if (!node) return false;

    if (toggle_hit && (node->may_have_children || node->child_count > 0)) {
        view->selected_node = node;
        node->expanded = !node->expanded;
        ntk_widget_emit(widget, node->expanded ? "expanded" : "collapsed", node);
    } else {
        view->selected_node = node;
        ntk_widget_emit(widget, "selection-changed", node);
    }

    ntk_widget_repaint(widget);
    return true;
}

static NtkSize tree_preferred_size(NtkWidget *widget) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    return NTK_SIZE(180,
                    tree_visible_node_count(&view->root) * NTK_TREE_ROW_HEIGHT + 4);
}

static void tree_destroy(NtkWidget *widget) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    tree_node_destroy(&view->root);
}

NtkWidget *ntk_tree_view_new(NtkWidget *parent) {
    NtkWidget *widget = ntk_widget_new_with_class(parent, &tree_class,
                                                   sizeof(NtkTreeViewInstance));
    if (!widget) return NULL;

    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    view->root.text = strdup("Root");
    view->root.expanded = true;
    return widget;
}

NtkTreeNode *ntk_tree_view_get_root(NtkWidget *widget) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    return view ? &view->root : NULL;
}

NtkTreeNode *ntk_tree_node_add_child(NtkTreeNode *parent, const char *text) {
    if (!parent) return NULL;

    if (parent->child_count == parent->child_capacity) {
        int new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 8;
        NtkTreeNode **children = realloc(
            parent->children, (size_t)new_capacity * sizeof(*children));
        if (!children) return NULL;
        parent->children = children;
        parent->child_capacity = new_capacity;
    }

    NtkTreeNode *node = calloc(1, sizeof(*node));
    if (!node) return NULL;
    node->text = strdup(text ? text : "");
    if (!node->text) {
        free(node);
        return NULL;
    }

    node->parent = parent;
    parent->children[parent->child_count++] = node;
    parent->may_have_children = true;
    return node;
}

void ntk_tree_node_clear_children(NtkTreeNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; ++i) tree_node_destroy(node->children[i]);
    free(node->children);
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
}

void ntk_tree_node_set_text(NtkTreeNode *node, const char *text) {
    if (!node) return;
    char *copy = strdup(text ? text : "");
    if (!copy) return;
    free(node->text);
    node->text = copy;
}

const char *ntk_tree_node_get_text(NtkTreeNode *node) {
    return node && node->text ? node->text : "";
}

void ntk_tree_node_set_icon(NtkTreeNode *node, NtkPixmap *icon) {
    if (!node) return;
    if (node->icon) ntk_pixmap_destroy(node->icon);
    node->icon = icon;
}

NtkPixmap *ntk_tree_node_get_icon(NtkTreeNode *node) {
    return node ? node->icon : NULL;
}

void ntk_tree_node_set_user_data(NtkTreeNode *node, void *data) {
    if (node) node->user_data = data;
}

void *ntk_tree_node_get_user_data(NtkTreeNode *node) {
    return node ? node->user_data : NULL;
}

void ntk_tree_node_set_has_children(NtkTreeNode *node, bool has_children) {
    if (node) node->may_have_children = has_children;
}

bool ntk_tree_node_has_children(NtkTreeNode *node) {
    return node && (node->may_have_children || node->child_count > 0);
}

void ntk_tree_node_set_expanded(NtkTreeNode *node, bool expanded) {
    if (node) node->expanded = expanded;
}

bool ntk_tree_node_is_expanded(NtkTreeNode *node) {
    return node && node->expanded;
}

void ntk_tree_view_set_selected(NtkWidget *widget, NtkTreeNode *node) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    if (!view) return;
    view->selected_node = node;
    ntk_widget_repaint(widget);
}

NtkTreeNode *ntk_tree_view_get_selected(NtkWidget *widget) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    return view ? view->selected_node : NULL;
}

int ntk_tree_view_get_visible_height(NtkWidget *widget) {
    NtkTreeViewInstance *view = ntk_widget_get_instance_data(widget);
    return view ? tree_visible_node_count(&view->root) * NTK_TREE_ROW_HEIGHT + 4 : 0;
}
