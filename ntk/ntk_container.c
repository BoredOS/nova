// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_container.h"
#include <stdlib.h>

static void container_paint(NtkWidget *w, NtkPainter *p) {
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->paint) {
                klass->paint(child, p);
            }
        }
    }
}

static void container_layout(NtkWidget *w) {
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child) {
            const NtkWidgetClass *klass = ntk_widget_get_class(child);
            if (klass && klass->layout) {
                klass->layout(child);
            }
        }
    }
}

static NtkSize container_preferred_size(NtkWidget *w) {
    int count = ntk_widget_get_child_count(w);
    int max_w = 0;
    int max_h = 0;
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            NtkSize pref = ntk_widget_get_preferred_size(child);
            NtkPoint pos = ntk_widget_get_position(child);
            if (pos.x + pref.width > max_w) max_w = pos.x + pref.width;
            if (pos.y + pref.height > max_h) max_h = pos.y + pref.height;
        }
    }
    return NTK_SIZE(max_w, max_h);
}

static bool container_handle_event(NtkWidget *w, NtkEvent *e) {
    (void)w;
    (void)e;
    return false;
}

static void container_destroy(NtkWidget *w) {
    (void)w;
}

static const NtkWidgetClass container_class = {
    .type_name      = "NtkContainer",
    .paint          = container_paint,
    .layout         = container_layout,
    .preferred_size = container_preferred_size,
    .handle_event   = container_handle_event,
    .destroy        = container_destroy
};

NtkWidget* ntk_container_new(NtkWidget *parent) {
    return ntk_widget_new_with_class(parent, &container_class, 0);
}

void ntk_container_layout_children(NtkWidget *w) {
    container_layout(w);
}
