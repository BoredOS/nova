// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_stack.h"
#include <stdlib.h>
#include <string.h>

#define NTK_STACK_MAX_PAGES 16

typedef struct {
    NtkWidget *page;
    char      *id;
} NtkStackPage;

typedef struct {
    NtkStackPage pages[NTK_STACK_MAX_PAGES];
    int          page_count;
    int          current_index;
} NtkStackInstance;

static void stack_paint(NtkWidget *w, NtkPainter *p);
static void stack_layout(NtkWidget *w);
static NtkSize stack_preferred_size(NtkWidget *w);
static void stack_destroy(NtkWidget *w);

static const NtkWidgetClass stack_class = {
    .type_name      = "NtkStack",
    .paint          = stack_paint,
    .layout         = stack_layout,
    .preferred_size = stack_preferred_size,
    .handle_event   = NULL,
    .destroy        = stack_destroy
};

NtkWidget* ntk_stack_new(NtkWidget *parent) {
    NtkWidget *w = ntk_widget_new_with_class(parent, &stack_class, sizeof(NtkStackInstance));
    if (!w) return NULL;

    NtkStackInstance *inst = ntk_widget_get_instance_data(w);
    inst->page_count = 0;
    inst->current_index = -1;

    return w;
}

void ntk_stack_add_page(NtkWidget *stack, NtkWidget *page, const char *id) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(stack);
    if (!inst || inst->page_count >= NTK_STACK_MAX_PAGES) return;

    ntk_widget_add_child(stack, page);

    NtkStackPage *p = &inst->pages[inst->page_count++];
    p->page = page;
    p->id = id ? strdup(id) : strdup("");

    if (inst->page_count == 1) {
        inst->current_index = 0;
        ntk_widget_show(page);
    } else {
        ntk_widget_hide(page);
    }

    stack_layout(stack);
    ntk_widget_repaint(stack);
}

void ntk_stack_set_visible_page(NtkWidget *stack, const char *id) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(stack);
    if (!inst || !id) return;

    for (int i = 0; i < inst->page_count; i++) {
        if (strcmp(inst->pages[i].id, id) == 0) {
            if (inst->current_index != i) {
                if (inst->current_index >= 0) {
                    ntk_widget_hide(inst->pages[inst->current_index].page);
                }
                inst->current_index = i;
                ntk_widget_show(inst->pages[i].page);
                
                stack_layout(stack);
                ntk_widget_repaint(stack);
            }
            return;
        }
    }
}

NtkWidget* ntk_stack_get_visible_page(NtkWidget *stack) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(stack);
    if (inst && inst->current_index >= 0) {
        return inst->pages[inst->current_index].page;
    }
    return NULL;
}
static void stack_paint(NtkWidget *w, NtkPainter *p) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(w);
    if (inst->current_index >= 0) {
        NtkWidget *page = inst->pages[inst->current_index].page;
        if (page && ntk_widget_is_visible(page)) {
            const NtkWidgetClass *klass = ntk_widget_get_class(page);
            if (klass && klass->paint) klass->paint(page, p);
        }
    }
}

static void stack_layout(NtkWidget *w) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(w);
    NtkRect geom = ntk_widget_get_geometry(w);

    if (inst->current_index >= 0) {
        NtkWidget *page = inst->pages[inst->current_index].page;
        if (page) {
            ntk_widget_set_geometry(page, geom);
            const NtkWidgetClass *klass = ntk_widget_get_class(page);
            if (klass && klass->layout) klass->layout(page);
        }
    }
}

static NtkSize stack_preferred_size(NtkWidget *w) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(w);
    int max_w = 0;
    int max_h = 0;

    for (int i = 0; i < inst->page_count; i++) {
        NtkSize pref = ntk_widget_get_preferred_size(inst->pages[i].page);
        if (pref.width > max_w) max_w = pref.width;
        if (pref.height > max_h) max_h = pref.height;
    }

    return NTK_SIZE(max_w, max_h);
}

static void stack_destroy(NtkWidget *w) {
    NtkStackInstance *inst = ntk_widget_get_instance_data(w);
    for (int i = 0; i < inst->page_count; i++) {
        free(inst->pages[i].id);
    }
}
