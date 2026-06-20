// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_window.h"
#include "ntk_app.h"
#include "ntk_nova.h"
#include "ntk_style.h"
#include "libnovaproto/novaproto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

typedef struct {
    char      *title;
    NtkPixmap *icon;
    NtkWidget *content;
    NtkWidget *menubar;
    NtkWidget *toolbar;
    NtkWidget *statusbar;
    uint32_t   surface_id;
    char       shm_path[128];
    uint32_t  *pixels;
    int        width;
    int        height;
    bool       resizable;
    bool       minimizable;
    bool       maximizable;
    bool       closable;
    bool       frameless;
    bool       always_on_top;
    float      opacity;
} NtkWindowInstance;

static void window_paint(NtkWidget *w, NtkPainter *p);
static void window_layout(NtkWidget *w);
static NtkSize window_preferred_size(NtkWidget *w);
static bool window_handle_event(NtkWidget *w, NtkEvent *e);
static void window_destroy(NtkWidget *w);

static const NtkWidgetClass window_class = {
    .type_name      = "NtkWindow",
    .paint          = window_paint,
    .layout         = window_layout,
    .preferred_size = window_preferred_size,
    .handle_event   = window_handle_event,
    .destroy        = window_destroy
};

NtkWidget* ntk_window_new(const char *title, int width, int height) {
    int fd = ntk_app_get_fd();
    if (fd < 0) return NULL;

    NtkWidget *w = ntk_widget_new_with_class(NULL, &window_class, sizeof(NtkWindowInstance));
    if (!w) return NULL;

    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->width = width;
    inst->height = height;
    inst->title = title ? strdup(title) : NULL;
    inst->resizable = true;
    inst->minimizable = true;
    inst->maximizable = true;
    inst->closable = true;
    inst->opacity = 1.0f;

    uint32_t surf_id;
    char shm_path[128];
    if (ntk_nova_create_surface(fd, width, height, 2, 0, &surf_id, shm_path) < 0) {
        free(inst->title);
        free(w);
        return NULL;
    }

    inst->surface_id = surf_id;
    strcpy(inst->shm_path, shm_path);

    int sfd = open(shm_path, O_RDWR);
    if (sfd >= 0) {
        inst->pixels = mmap(NULL, (size_t)(width * height * 4), PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
        close(sfd);
        if (inst->pixels == MAP_FAILED) inst->pixels = NULL;
    }

    if (!inst->pixels) {
        ntk_nova_destroy_surface(fd, surf_id);
        free(inst->title);
        free(w);
        return NULL;
    }

    ntk_widget_set_geometry(w, NTK_RECT(0, 0, width, height));

    if (title) {
        ntk_nova_set_title(fd, surf_id, title);
    }

    ntk_app_register_window(w);
    return w;
}

NtkWidget* ntk_window_new_dialog(NtkWidget *parent, const char *title, int width, int height) {
    int fd = ntk_app_get_fd();
    if (fd < 0) return NULL;

    NtkWidget *w = ntk_widget_new_with_class(parent, &window_class, sizeof(NtkWindowInstance));
    if (!w) return NULL;

    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->width = width;
    inst->height = height;
    inst->title = title ? strdup(title) : NULL;
    inst->resizable = false;
    inst->minimizable = false;
    inst->maximizable = false;
    inst->closable = true;
    inst->opacity = 1.0f;

    uint32_t surf_id;
    char shm_path[128];
    if (ntk_nova_create_surface(fd, width, height, 2, 0, &surf_id, shm_path) < 0) { 
        free(inst->title);
        free(w);
        return NULL;
    }

    inst->surface_id = surf_id;
    strcpy(inst->shm_path, shm_path);

    int sfd = open(shm_path, O_RDWR);
    if (sfd >= 0) {
        inst->pixels = mmap(NULL, (size_t)(width * height * 4), PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
        close(sfd);
        if (inst->pixels == MAP_FAILED) inst->pixels = NULL;
    }

    if (!inst->pixels) {
        ntk_nova_destroy_surface(fd, surf_id);
        free(inst->title);
        free(w);
        return NULL;
    }

    ntk_widget_set_geometry(w, NTK_RECT(0, 0, width, height));

    if (title) {
        ntk_nova_set_title(fd, surf_id, title);
    }

    ntk_app_register_window(w);
    return w;
}

void ntk_window_set_title(NtkWidget *w, const char *title) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    free(inst->title);
    inst->title = title ? strdup(title) : NULL;
    ntk_nova_set_title(ntk_app_get_fd(), inst->surface_id, title);
}

const char* ntk_window_get_title(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->title : NULL;
}

void        ntk_window_set_icon(NtkWidget *w, NtkPixmap *icon) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (inst->icon) ntk_pixmap_destroy(inst->icon);
    inst->icon = icon; 
}

void        ntk_window_set_icon_path(NtkWidget *w, const char *icon_path) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (inst) {
        ntk_nova_set_icon(ntk_app_get_fd(), inst->surface_id, icon_path);
    }
}

NtkPixmap* ntk_window_get_icon(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->icon : NULL;
}

void ntk_window_set_content(NtkWidget *w, NtkWidget *content) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (inst->content) {
        ntk_widget_remove_child(w, inst->content);
        ntk_widget_destroy(inst->content);
    }
    inst->content = content;
    if (content) {
        ntk_widget_add_child(w, content);
    }
    window_layout(w);
}

NtkWidget* ntk_window_get_content(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->content : NULL;
}

void ntk_window_detach_content(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (inst && inst->content) {
        ntk_widget_remove_child(w, inst->content);
        inst->content = NULL;
    }
}

void ntk_window_set_resizable(NtkWidget *w, bool resizable) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->resizable = resizable;
    uint32_t flags = resizable ? 0 : 0x2; 
    ntk_nova_set_flags(ntk_app_get_fd(), inst->surface_id, flags);
}

bool ntk_window_is_resizable(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst->resizable;
}

void ntk_window_set_minimizable(NtkWidget *w, bool minimizable) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->minimizable = minimizable;
}

void ntk_window_set_maximizable(NtkWidget *w, bool maximizable) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->maximizable = maximizable;
}

void ntk_window_set_closable(NtkWidget *w, bool closable) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->closable = closable;
}

void ntk_window_set_frameless(NtkWidget *w, bool frameless) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->frameless = frameless;
}

void ntk_window_set_always_on_top(NtkWidget *w, bool on_top) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->always_on_top = on_top;
}

void ntk_window_set_opacity(NtkWidget *w, float opacity) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->opacity = opacity;
}

void ntk_window_minimize(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    ntk_nova_set_state(ntk_app_get_fd(), inst->surface_id, 2); 
}

void ntk_window_maximize(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    ntk_nova_set_state(ntk_app_get_fd(), inst->surface_id, 3);
}

void ntk_window_restore(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    ntk_nova_set_state(ntk_app_get_fd(), inst->surface_id, 1); 
}

void ntk_window_close(NtkWidget *w) {
    ntk_widget_emit(w, "closed", NULL);
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    ntk_app_unregister_window(w);
    ntk_nova_destroy_surface(ntk_app_get_fd(), inst->surface_id);
    if (inst->pixels) {
        munmap(inst->pixels, (size_t)(inst->width * inst->height * 4));
        inst->pixels = NULL;
    }
    ntk_widget_destroy(w);
}

void ntk_window_center_on_screen(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    NtkSize scr = ntk_nova_get_screen_size(ntk_app_get_fd());
    int x = (scr.width - inst->width) / 2;
    int y = (scr.height - inst->height) / 2;
    ntk_nova_move_surface(ntk_app_get_fd(), inst->surface_id, x, y);
}

void ntk_window_center_on_parent(NtkWidget *w) {
    NtkWidget *parent = ntk_widget_get_parent(w);
    if (parent) {
        NtkWindowInstance *pinst = ntk_widget_get_instance_data(parent);
        NtkWindowInstance *cinst = ntk_widget_get_instance_data(w);
        ntk_window_center_on_screen(w);
    } else {
        ntk_window_center_on_screen(w);
    }
}

void ntk_window_set_menubar(NtkWidget *w, NtkWidget *menubar) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (inst->menubar) {
        ntk_widget_remove_child(w, inst->menubar);
        ntk_widget_destroy(inst->menubar);
    }
    inst->menubar = menubar;
    if (menubar) ntk_widget_add_child(w, menubar);
    window_layout(w);
}

void ntk_window_set_toolbar(NtkWidget *w, NtkWidget *toolbar) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (inst->toolbar) {
        ntk_widget_remove_child(w, inst->toolbar);
        ntk_widget_destroy(inst->toolbar);
    }
    inst->toolbar = toolbar;
    if (toolbar) ntk_widget_add_child(w, toolbar);
    window_layout(w);
}

void ntk_window_set_statusbar(NtkWidget *w, NtkWidget *statusbar) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    if (!inst) return;
    if (inst->statusbar) {
        ntk_widget_remove_child(w, inst->statusbar);
        ntk_widget_destroy(inst->statusbar);
    }
    inst->statusbar = statusbar;
    if (statusbar) ntk_widget_add_child(w, statusbar);
    window_layout(w);
}

NtkWidget* ntk_window_get_menubar(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->menubar : NULL;
}

NtkWidget* ntk_window_get_toolbar(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->toolbar : NULL;
}

NtkWidget* ntk_window_get_statusbar(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst->statusbar;
}
static void window_paint(NtkWidget *w, NtkPainter *p) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    NtkStyle *style = ntk_widget_get_style(w);
    NtkColor bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WINDOW_BG);
    ntk_painter_clear(p, bg);
    if (inst->menubar && ntk_widget_is_visible(inst->menubar)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->menubar);
        if (klass && klass->paint) klass->paint(inst->menubar, p);
    }
    if (inst->toolbar && ntk_widget_is_visible(inst->toolbar)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->toolbar);
        if (klass && klass->paint) klass->paint(inst->toolbar, p);
    }
    if (inst->statusbar && ntk_widget_is_visible(inst->statusbar)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->statusbar);
        if (klass && klass->paint) klass->paint(inst->statusbar, p);
    }
    if (inst->content && ntk_widget_is_visible(inst->content)) {
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->content);
        if (klass && klass->paint) klass->paint(inst->content, p);
    }
}

static void window_layout(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect r = ntk_widget_get_geometry(w);
    NtkStyle *style = ntk_widget_get_style(w);

    int top = 0;
    int bottom = r.height;

    if (inst->menubar && ntk_widget_is_visible(inst->menubar)) {
        int h = ntk_style_get_metric(style, NTK_STYLE_METRIC_MENUBAR_HEIGHT);
        if (h <= 0) h = 22;
        ntk_widget_set_geometry(inst->menubar, NTK_RECT(0, top, r.width, h));
        top += h;
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->menubar);
        if (klass && klass->layout) klass->layout(inst->menubar);
    }

    if (inst->toolbar && ntk_widget_is_visible(inst->toolbar)) {
        int h = ntk_style_get_metric(style, NTK_STYLE_METRIC_TOOLBAR_HEIGHT);
        if (h <= 0) h = 28;
        ntk_widget_set_geometry(inst->toolbar, NTK_RECT(0, top, r.width, h));
        top += h;
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->toolbar);
        if (klass && klass->layout) klass->layout(inst->toolbar);
    }

    if (inst->statusbar && ntk_widget_is_visible(inst->statusbar)) {
        int h = ntk_style_get_metric(style, NTK_STYLE_METRIC_STATUSBAR_HEIGHT);
        if (h <= 0) h = 20;
        bottom -= h;
        ntk_widget_set_geometry(inst->statusbar, NTK_RECT(0, bottom, r.width, h));
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->statusbar);
        if (klass && klass->layout) klass->layout(inst->statusbar);
    }

    if (inst->content && ntk_widget_is_visible(inst->content)) {
        ntk_widget_set_geometry(inst->content, NTK_RECT(0, top, r.width, bottom - top));
        const NtkWidgetClass *klass = ntk_widget_get_class(inst->content);
        if (klass && klass->layout) klass->layout(inst->content);
    }
}

static NtkSize window_preferred_size(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return NTK_SIZE(inst->width, inst->height);
}

static bool window_handle_event(NtkWidget *w, NtkEvent *e) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    
    if (e->type == NTK_EVENT_RESIZE) {
        int nw = e->width;
        int nh = e->height;
        if (nw > 0 && nh > 0 && (nw != inst->width || nh != inst->height)) {
            char new_shm[128];
            if (ntk_nova_resize_surface(ntk_app_get_fd(), inst->surface_id, (uint32_t)nw, (uint32_t)nh, new_shm) >= 0) {
                if (inst->pixels) {
                    munmap(inst->pixels, (size_t)(inst->width * inst->height * 4));
                    inst->pixels = NULL;
                }
                strcpy(inst->shm_path, new_shm);
                int sfd = open(new_shm, O_RDWR);
                if (sfd >= 0) {
                    inst->pixels = mmap(NULL, (size_t)(nw * nh * 4), PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
                    close(sfd);
                    if (inst->pixels == MAP_FAILED) inst->pixels = NULL;
                } else {
                    inst->pixels = NULL;
                }
                inst->width = nw;
                inst->height = nh;
                ntk_widget_set_geometry(w, NTK_RECT(0, 0, nw, nh));
                window_layout(w);
                return true;
            }
        }
    }
    return false;
}

static void window_destroy(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    free(inst->title);
    if (inst->icon) ntk_pixmap_destroy(inst->icon);
    if (inst->pixels) {
        munmap(inst->pixels, (size_t)(inst->width * inst->height * 4));
    }
}

uint32_t* ntk_window_get_pixels_internal(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->pixels : NULL;
}

uint32_t ntk_window_get_surf_id_internal(NtkWidget *w) {
    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    return inst ? inst->surface_id : 0;
}

NtkWidget* ntk_window_new_popup(NtkWidget *parent, int x, int y, int width, int height) {
    int fd = ntk_app_get_fd();
    if (fd < 0) return NULL;

    NtkWidget *w = ntk_widget_new_with_class(parent, &window_class, sizeof(NtkWindowInstance));
    if (!w) return NULL;

    NtkWindowInstance *inst = ntk_widget_get_instance_data(w);
    inst->width = width;
    inst->height = height;
    inst->title = NULL;
    inst->resizable = false;
    inst->minimizable = false;
    inst->maximizable = false;
    inst->closable = false;
    inst->frameless = true;
    inst->opacity = 1.0f;

    uint32_t surf_id;
    char shm_path[128];
    if (ntk_nova_create_surface(fd, width, height, 4, 0x2, &surf_id, shm_path) < 0) {
        free(w);
        return NULL;
    }

    inst->surface_id = surf_id;
    strcpy(inst->shm_path, shm_path);

    int sfd = open(shm_path, O_RDWR);
    if (sfd >= 0) {
        inst->pixels = mmap(NULL, (size_t)(width * height * 4), PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
        close(sfd);
        if (inst->pixels == MAP_FAILED) inst->pixels = NULL;
    }

    if (!inst->pixels) {
        ntk_nova_destroy_surface(fd, surf_id);
        free(w);
        return NULL;
    }

    ntk_widget_set_geometry(w, NTK_RECT(0, 0, width, height));
    ntk_nova_move_surface(fd, surf_id, x, y);
    ntk_app_register_window(w);
    return w;
}

void ntk_window_get_screen_position(NtkWidget *win, int *x_out, int *y_out) {
    if (!win) {
        if (x_out) *x_out = 0;
        if (y_out) *y_out = 0;
        return;
    }
    NtkWidget *cur = win;
    while (cur) {
        const NtkWidgetClass *klass = ntk_widget_get_class(cur);
        if (klass && strcmp(klass->type_name, "NtkWindow") == 0) {
            break;
        }
        NtkWidget *p = ntk_widget_get_parent(cur);
        if (!p) break;
        cur = p;
    }
    uint32_t surf_id = ntk_window_get_surf_id_internal(cur);
    int fd = ntk_app_get_fd();
    if (fd >= 0) {
        extern int nova_get_surface_geometry(int fd, uint32_t surf_id, int *x_out, int *y_out, uint32_t *w_out, uint32_t *h_out);
        nova_get_surface_geometry(fd, surf_id, x_out, y_out, NULL, NULL);
    } else {
        if (x_out) *x_out = 0;
        if (y_out) *y_out = 0;
    }
}
