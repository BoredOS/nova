// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_box.h"
#include <stdlib.h>
#include <string.h>

#define NTK_BOX_MAX_CHILDREN 32

typedef struct {
    NtkWidget *child;
    bool       expand;
    bool       fill;
    int        padding;
    bool       pack_end;
} NtkBoxChild;

typedef struct {
    NtkOrientation orientation;
    int            spacing;
    bool           homogeneous;
    NtkBoxChild    children[NTK_BOX_MAX_CHILDREN];
    int            child_count;
} NtkBoxInstance;

static void box_paint(NtkWidget *w, NtkPainter *p);
static void box_layout(NtkWidget *w);
static NtkSize box_preferred_size(NtkWidget *w);
static void box_destroy(NtkWidget *w);

static const NtkWidgetClass box_class = {
    .type_name      = "NtkBox",
    .paint          = box_paint,
    .layout         = box_layout,
    .preferred_size = box_preferred_size,
    .handle_event   = NULL,
    .destroy        = box_destroy
};

NtkWidget* ntk_box_new(NtkOrientation orientation, NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &box_class, sizeof(NtkBoxInstance));
    if (!w) return NULL;

    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    inst->orientation = orientation;
    inst->spacing = 4;
    inst->homogeneous = false;

    return w;
}

void ntk_box_set_spacing(NtkWidget *w, int spacing) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->spacing = spacing;
        ntk_widget_repaint(w);
    }
}

int ntk_box_get_spacing(NtkWidget *w) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->spacing : 0;
}

void ntk_box_set_homogeneous(NtkWidget *w, bool homogeneous) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->homogeneous = homogeneous;
        ntk_widget_repaint(w);
    }
}

bool ntk_box_is_homogeneous(NtkWidget *w) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->homogeneous : false;
}

void ntk_box_pack_start(NtkWidget *box, NtkWidget *child, bool expand, bool fill, int padding) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(box);
    if (!inst || inst->child_count >= NTK_BOX_MAX_CHILDREN) return;

    ntk_widget_add_child(box, child);

    NtkBoxChild *c = &inst->children[inst->child_count++];
    c->child = child;
    c->expand = expand;
    c->fill = fill;
    c->padding = padding;
    c->pack_end = false;

    ntk_widget_repaint(box);
}

void ntk_box_pack_end(NtkWidget *box, NtkWidget *child, bool expand, bool fill, int padding) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(box);
    if (!inst || inst->child_count >= NTK_BOX_MAX_CHILDREN) return;

    ntk_widget_add_child(box, child);

    NtkBoxChild *c = &inst->children[inst->child_count++];
    c->child = child;
    c->expand = expand;
    c->fill = fill;
    c->padding = padding;
    c->pack_end = true;

    ntk_widget_repaint(box);
}
static void box_paint(NtkWidget *w, NtkPainter *p) {
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->paint) klass->paint(child, p);
        }
    }
}

static void box_layout(NtkWidget *w) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    int visible_count = 0;
    int expand_count = 0;
    int fixed_size = 0;

    for (int i = 0; i < inst->child_count; i++) {
        NtkBoxChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            visible_count++;
            if (c->expand) expand_count++;
            
            NtkSize pref = ntk_widget_get_preferred_size(c->child);
            int size = (inst->orientation == NTK_HORIZONTAL) ? pref.width : pref.height;
            fixed_size += size + 2 * c->padding;
        }
    }

    if (visible_count == 0) return;

    int total_space = (inst->orientation == NTK_HORIZONTAL) ? geom.width : geom.height;
    int spacing_total = (visible_count - 1) * inst->spacing;
    int extra_space = total_space - fixed_size - spacing_total;
    if (extra_space < 0) extra_space = 0;

    int homogeneous_size = 0;
    if (inst->homogeneous) {
        homogeneous_size = (total_space - spacing_total) / visible_count;
    }

    int start_pos = 0;
    int end_pos = total_space;

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < inst->child_count; i++) {
            NtkBoxChild *c = &inst->children[i];
            if (!c->child || !ntk_widget_is_visible(c->child)) continue;
            
            bool is_end = c->pack_end;
            if ((pass == 0 && is_end) || (pass == 1 && !is_end)) continue;

            NtkSize pref = ntk_widget_get_preferred_size(c->child);
            int child_size = 0;

            if (inst->homogeneous) {
                child_size = homogeneous_size;
            } else {
                int base_size = (inst->orientation == NTK_HORIZONTAL) ? pref.width : pref.height;
                child_size = base_size + 2 * c->padding;
                if (c->expand && expand_count > 0) {
                    child_size += extra_space / expand_count;
                }
            }

            if (inst->orientation == NTK_HORIZONTAL) {
                int cx = 0;
                int cw = child_size;
                int cy = 0;
                int ch = geom.height;

                if (!c->fill) {
                    cw = pref.width;
                }

                if (is_end) {
                    end_pos -= child_size;
                    cx = end_pos + c->padding;
                    end_pos -= inst->spacing;
                } else {
                    cx = start_pos + c->padding;
                    start_pos += child_size + inst->spacing;
                }

                ntk_widget_set_geometry(c->child, NTK_RECT(geom.x + cx, geom.y + cy, cw, ch));
            } else {
                int cx = 0;
                int cw = geom.width;
                int cy = 0;
                int ch = child_size;

                if (!c->fill) {
                    ch = pref.height;
                }

                if (is_end) {
                    end_pos -= child_size;
                    cy = end_pos + c->padding;
                    end_pos -= inst->spacing;
                } else {
                    cy = start_pos + c->padding;
                    start_pos += child_size + inst->spacing;
                }

                ntk_widget_set_geometry(c->child, NTK_RECT(geom.x + cx, geom.y + cy, cw, ch));
            }

            const NtkWidgetClass *klass = ntk_widget_get_class(c->child);
            if (klass && klass->layout) klass->layout(c->child);
        }
    }
}

static NtkSize box_preferred_size(NtkWidget *w) {
    NtkBoxInstance *inst = ntk_widget_get_instance_data(w);
    int total_w = 0;
    int total_h = 0;
    int visible_count = 0;

    for (int i = 0; i < inst->child_count; i++) {
        NtkBoxChild *c = &inst->children[i];
        if (c->child && ntk_widget_is_visible(c->child)) {
            visible_count++;
            NtkSize pref = ntk_widget_get_preferred_size(c->child);
            if (inst->orientation == NTK_HORIZONTAL) {
                total_w += pref.width + 2 * c->padding;
                if (pref.height > total_h) total_h = pref.height;
            } else {
                total_h += pref.height + 2 * c->padding;
                if (pref.width > total_w) total_w = pref.width;
            }
        }
    }

    if (visible_count > 0) {
        if (inst->orientation == NTK_HORIZONTAL) {
            total_w += (visible_count - 1) * inst->spacing;
        } else {
            total_h += (visible_count - 1) * inst->spacing;
        }
    }

    return NTK_SIZE(total_w, total_h);
}

static void box_destroy(NtkWidget *w) {
    (void)w;
}
