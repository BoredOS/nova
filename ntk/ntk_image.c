// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_image.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    NtkPixmap   *pixmap;
    NtkScaleMode scale_mode;
} NtkImageInstance;

static void image_paint(NtkWidget *w, NtkPainter *p);
static NtkSize image_preferred_size(NtkWidget *w);
static void image_destroy(NtkWidget *w);

static const NtkWidgetClass image_class = {
    .type_name      = "NtkImage",
    .paint          = image_paint,
    .layout         = NULL,
    .preferred_size = image_preferred_size,
    .handle_event   = NULL,
    .destroy        = image_destroy
};

NtkWidget* ntk_image_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &image_class, sizeof(NtkImageInstance));
    if (!w) return NULL;

    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    inst->pixmap = NULL;
    inst->scale_mode = NTK_SCALE_NONE;

    return w;
}

NtkWidget* ntk_image_new_from_file(const char *path, NtkWidget *parent) {
    NtkWidget *w = ntk_image_new(parent);
    if (w && path) {
        NtkPixmap *pm = ntk_pixmap_new_from_file(path);
        if (pm) {
            ntk_image_set_pixmap(w, pm);
        }
    }
    return w;
}

void ntk_image_set_pixmap(NtkWidget *w, NtkPixmap *pm) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        if (inst->pixmap) ntk_pixmap_destroy(inst->pixmap);
        inst->pixmap = pm; 
        ntk_widget_repaint(w);
    }
}

NtkPixmap* ntk_image_get_pixmap(NtkWidget *w) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->pixmap : NULL;
}

void ntk_image_set_scale_mode(NtkWidget *w, NtkScaleMode mode) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        inst->scale_mode = mode;
        ntk_widget_repaint(w);
    }
}

NtkScaleMode ntk_image_get_scale_mode(NtkWidget *w) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->scale_mode : NTK_SCALE_NONE;
}
static void image_paint(NtkWidget *w, NtkPainter *p) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst || !inst->pixmap) return;

    NtkRect geom = ntk_widget_get_geometry(w);
    NtkPoint origin = ntk_widget_map_to_global(w, NTK_POINT(0, 0));
    NtkRect r = NTK_RECT(origin.x, origin.y, geom.width, geom.height);

    int img_w = ntk_pixmap_get_width(inst->pixmap);
    int img_h = ntk_pixmap_get_height(inst->pixmap);
    if (img_w <= 0 || img_h <= 0 || r.width <= 0 || r.height <= 0) return;

    if (inst->scale_mode == NTK_SCALE_NONE) {
        int x = r.x + (r.width - img_w) / 2;
        int y = r.y + (r.height - img_h) / 2;
        ntk_painter_draw_pixmap(p, inst->pixmap, x, y);
    } else if (inst->scale_mode == NTK_SCALE_STRETCH) {
        ntk_painter_draw_pixmap_scaled(p, inst->pixmap, r);
    } else if (inst->scale_mode == NTK_SCALE_FIT) {
        float scale_x = (float)r.width / (float)img_w;
        float scale_y = (float)r.height / (float)img_h;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;
        int new_w = (int)(img_w * scale);
        int new_h = (int)(img_h * scale);
        if (new_w <= 0) new_w = 1;
        if (new_h <= 0) new_h = 1;
        int x = r.x + (r.width - new_w) / 2;
        int y = r.y + (r.height - new_h) / 2;
        ntk_painter_draw_pixmap_scaled(p, inst->pixmap, NTK_RECT(x, y, new_w, new_h));
    } else if (inst->scale_mode == NTK_SCALE_FILL) {
        float scale_x = (float)r.width / (float)img_w;
        float scale_y = (float)r.height / (float)img_h;
        float scale = (scale_x > scale_y) ? scale_x : scale_y;
        int new_w = (int)(img_w * scale);
        int new_h = (int)(img_h * scale);
        if (new_w <= 0) new_w = 1;
        if (new_h <= 0) new_h = 1;
        int x = r.x + (r.width - new_w) / 2;
        int y = r.y + (r.height - new_h) / 2;
        ntk_painter_draw_pixmap_scaled(p, inst->pixmap, NTK_RECT(x, y, new_w, new_h));
    }
}

static NtkSize image_preferred_size(NtkWidget *w) {
    NtkSize min_sz = ntk_widget_get_min_size(w);
    if (min_sz.width > 0 && min_sz.height > 0) {
        return min_sz;
    }
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->pixmap) {
        return NTK_SIZE(ntk_pixmap_get_width(inst->pixmap), ntk_pixmap_get_height(inst->pixmap));
    }
    return NTK_SIZE(32, 32);
}

static void image_destroy(NtkWidget *w) {
    NtkImageInstance *inst = ntk_widget_get_instance_data(w);
    if (inst->pixmap) ntk_pixmap_destroy(inst->pixmap);
}
