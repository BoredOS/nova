// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_canvas.h"
#include <stdlib.h>

typedef struct {
    NtkDrawCallback draw_cb;
    void            *userdata;
} NtkCanvasInstance;

static void canvas_paint(NtkWidget *w, NtkPainter *p) {
    NtkCanvasInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->draw_cb) {
        inst->draw_cb((NtkCanvas*)w, p, inst->userdata);
    }
}

static NtkSize canvas_preferred_size(NtkWidget *w) {
    (void)w;
    return NTK_SIZE(100, 100);
}

static const NtkWidgetClass canvas_class = {
    .type_name      = "NtkCanvas",
    .paint          = canvas_paint,
    .layout         = NULL,
    .preferred_size = canvas_preferred_size,
    .handle_event   = NULL,
    .destroy        = NULL
};

NtkWidget* ntk_canvas_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &canvas_class, sizeof(NtkCanvasInstance));
    if (!w) return NULL;

    NtkCanvasInstance *inst = ntk_widget_get_instance_data(w);
    inst->draw_cb = NULL;
    inst->userdata = NULL;

    return w;
}

void ntk_canvas_set_draw_callback(NtkWidget *w, NtkDrawCallback cb, void *userdata) {
    NtkCanvasInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->draw_cb = cb;
        inst->userdata = userdata;
        ntk_widget_repaint(w);
    }
}
