// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_style.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct NtkStyle {
    NtkColor      colors[NTK_STYLE_ROLE_COUNT];
    NtkFont      *fonts[NTK_STYLE_ELEMENT_COUNT];
    int           metrics[NTK_STYLE_METRIC_COUNT];
    NtkPixmap    *pixmaps[NTK_STYLE_PIXMAP_COUNT];
    NtkGradient  *gradients[NTK_STYLE_GRADIENT_COUNT];
};

static NtkStyle *g_global_style = NULL;
static void style_set_defaults(NtkStyle *s) {
    s->colors[NTK_STYLE_ROLE_WINDOW_BG]            = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_PANEL_BG]              = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_WIDGET_BG]             = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_WIDGET_BG_HOVER]       = 0xFFC0C0C0;
    s->colors[NTK_STYLE_ROLE_WIDGET_BG_ACTIVE]      = 0xFF909090;
    s->colors[NTK_STYLE_ROLE_WIDGET_BG_DISABLED]    = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_WIDGET_BORDER]         = 0xFF000000;
    s->colors[NTK_STYLE_ROLE_TEXT_PRIMARY]           = 0xFF000000;
    s->colors[NTK_STYLE_ROLE_TEXT_SECONDARY]         = 0xFF606060;
    s->colors[NTK_STYLE_ROLE_TEXT_DISABLED]          = 0xFF808080;
    s->colors[NTK_STYLE_ROLE_TEXT_ERROR]             = 0xFFCC0000;
    s->colors[NTK_STYLE_ROLE_ACCENT]                = 0xFF000080;
    s->colors[NTK_STYLE_ROLE_SELECTION_BG]           = 0xFF000080;
    s->colors[NTK_STYLE_ROLE_SELECTION_TEXT]          = 0xFFFFFFFF;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_ACTIVE_START]  = 0xFF00007B;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_ACTIVE_END]    = 0xFF1C94D4;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_INACTIVE_START] = 0xFF808080;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_INACTIVE_END]  = 0xFFA0A0A0;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_TEXT_ACTIVE]   = 0xFFFFFFFF;
    s->colors[NTK_STYLE_ROLE_TITLEBAR_TEXT_INACTIVE] = 0xFFC0C0C0;
    s->colors[NTK_STYLE_ROLE_BORDER_LIGHT]           = 0xFFFFFFFF;
    s->colors[NTK_STYLE_ROLE_BORDER_DARK]            = 0xFF676767;
    s->colors[NTK_STYLE_ROLE_MENUBAR_BG]             = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_MENU_BG]                = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_MENU_HIGHLIGHT]         = 0xFF000080;
    s->colors[NTK_STYLE_ROLE_TOOLBAR_BG]             = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_STATUSBAR_BG]           = 0xFFB0B0B0;
    s->colors[NTK_STYLE_ROLE_SCROLLBAR_BG]           = 0xFFD0D0D0;
    s->colors[NTK_STYLE_ROLE_SCROLLBAR_THUMB]        = 0xFFB0B0B0;
    s->metrics[NTK_STYLE_METRIC_BORDER_WIDTH]     = 2;
    s->metrics[NTK_STYLE_METRIC_BORDER_RADIUS]    = 0;
    s->metrics[NTK_STYLE_METRIC_PADDING]           = 4;
    s->metrics[NTK_STYLE_METRIC_SPACING]           = 4;
    s->metrics[NTK_STYLE_METRIC_TITLEBAR_HEIGHT]  = 22;
    s->metrics[NTK_STYLE_METRIC_MENUBAR_HEIGHT]   = 20;
    s->metrics[NTK_STYLE_METRIC_TOOLBAR_HEIGHT]   = 28;
    s->metrics[NTK_STYLE_METRIC_STATUSBAR_HEIGHT] = 20;
    s->metrics[NTK_STYLE_METRIC_SCROLLBAR_WIDTH]  = 16;
    s->metrics[NTK_STYLE_METRIC_BUTTON_HEIGHT]    = 24;
    s->metrics[NTK_STYLE_METRIC_ENTRY_HEIGHT]     = 22;

    s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT] = ntk_font_new("default", 12, NTK_FONT_WEIGHT_NORMAL, NTK_FONT_STYLE_NORMAL);
}
NtkStyle* ntk_style_new(void) {
    NtkStyle *s = calloc(1, sizeof(NtkStyle));
    if (!s) return NULL;
    style_set_defaults(s);
    return s;
}

void ntk_style_destroy(NtkStyle *s) {
    if (!s) return;

    for (int i = 0; i < NTK_STYLE_ELEMENT_COUNT; i++) {
        if (s->fonts[i]) ntk_font_destroy(s->fonts[i]);
    }
    for (int i = 0; i < NTK_STYLE_PIXMAP_COUNT; i++) {
        if (s->pixmaps[i]) ntk_pixmap_destroy(s->pixmaps[i]);
    }
    for (int i = 0; i < NTK_STYLE_GRADIENT_COUNT; i++) {
        if (s->gradients[i]) ntk_gradient_destroy(s->gradients[i]);
    }

    if (g_global_style == s) g_global_style = NULL;
    free(s);
}

NtkStyle* ntk_style_new_from_file(const char *path) {
    if (!path) return NULL;

    NtkStyle *s = ntk_style_new();
    if (!s) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return s; 
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *start = line;
        while (*start == ' ' || *start == '\t') start++;
        if (*start == '#' || *start == ';' || *start == '\n' || *start == '\0') continue;
        if (*start == '[') continue;
        char *eq = strchr(start, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key = start;
        char *val = eq + 1;

        while (*key && (*key == ' ' || *key == '\t')) key++;
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t' || *kend == '\r' || *kend == '\n')) *kend-- = '\0';

        while (*val && (*val == ' ' || *val == '\t')) val++;
        char *vend = val + strlen(val) - 1;
        while (vend > val && (*vend == ' ' || *vend == '\t' || *vend == '\r' || *vend == '\n')) *vend-- = '\0';

        if (strcmp(key, "window_bg") == 0 || strcmp(key, "desktop_bg") == 0)
            s->colors[NTK_STYLE_ROLE_WINDOW_BG] = ntk_color_from_hex(val);
        else if (strcmp(key, "panel_bg") == 0)
            s->colors[NTK_STYLE_ROLE_PANEL_BG] = ntk_color_from_hex(val);
        else if (strcmp(key, "accent") == 0 || strcmp(key, "accent_color") == 0)
            s->colors[NTK_STYLE_ROLE_ACCENT] = ntk_color_from_hex(val);
        else if (strcmp(key, "text_primary") == 0)
            s->colors[NTK_STYLE_ROLE_TEXT_PRIMARY] = ntk_color_from_hex(val);
        else if (strcmp(key, "titlebar_active_start") == 0 || strcmp(key, "active_titlebar_top") == 0)
            s->colors[NTK_STYLE_ROLE_TITLEBAR_ACTIVE_START] = ntk_color_from_hex(val);
        else if (strcmp(key, "titlebar_active_end") == 0 || strcmp(key, "active_titlebar_bottom") == 0)
            s->colors[NTK_STYLE_ROLE_TITLEBAR_ACTIVE_END] = ntk_color_from_hex(val);
        else if (strcmp(key, "titlebar_inactive_start") == 0 || strcmp(key, "inactive_titlebar_top") == 0)
            s->colors[NTK_STYLE_ROLE_TITLEBAR_INACTIVE_START] = ntk_color_from_hex(val);
        else if (strcmp(key, "titlebar_inactive_end") == 0 || strcmp(key, "inactive_titlebar_bottom") == 0)
            s->colors[NTK_STYLE_ROLE_TITLEBAR_INACTIVE_END] = ntk_color_from_hex(val);
        else if (strcmp(key, "font_path") == 0) {
            NtkFont *font = ntk_font_new(val, 13, NTK_FONT_WEIGHT_NORMAL, NTK_FONT_STYLE_NORMAL);
            if (font) {
                if (s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT])
                    ntk_font_destroy(s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT]);
                s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT] = font;
            }
        }
        else if (strcmp(key, "font_size") == 0) {
            int size = atoi(val);
            if (size > 0 && s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT]) {
                ntk_font_set_size(s->fonts[NTK_STYLE_ELEMENT_DEFAULT_FONT], size);
            }
        }
        else if (strcmp(key, "border_radius") == 0) {
            s->metrics[NTK_STYLE_METRIC_BORDER_RADIUS] = atoi(val);
        }
        else if (strcmp(key, "titlebar_height") == 0) {
            s->metrics[NTK_STYLE_METRIC_TITLEBAR_HEIGHT] = atoi(val);
        }
    }

    fclose(f);
    return s;
}
void ntk_style_set_color(NtkStyle *s, NtkStyleRole role, NtkColor color) {
    if (!s || role < 0 || role >= NTK_STYLE_ROLE_COUNT) return;
    s->colors[role] = color;
}

NtkColor ntk_style_get_color(NtkStyle *s, NtkStyleRole role) {
    if (!s || role < 0 || role >= NTK_STYLE_ROLE_COUNT) return 0xFF000000;
    return s->colors[role];
}

void ntk_style_set_font(NtkStyle *s, NtkStyleElement element, NtkFont *font) {
    if (!s || element < 0 || element >= NTK_STYLE_ELEMENT_COUNT) return;
    if (s->fonts[element]) ntk_font_destroy(s->fonts[element]);
    s->fonts[element] = font; 
}

NtkFont* ntk_style_get_font(NtkStyle *s, NtkStyleElement element) {
    if (!s || element < 0 || element >= NTK_STYLE_ELEMENT_COUNT) return NULL;
    return s->fonts[element];
}

void ntk_style_set_metric(NtkStyle *s, NtkStyleMetric metric, int value) {
    if (!s || metric < 0 || metric >= NTK_STYLE_METRIC_COUNT) return;
    s->metrics[metric] = value;
}

int ntk_style_get_metric(NtkStyle *s, NtkStyleMetric metric) {
    if (!s || metric < 0 || metric >= NTK_STYLE_METRIC_COUNT) return 0;
    return s->metrics[metric];
}

void ntk_style_set_pixmap(NtkStyle *s, NtkStylePixmap role, NtkPixmap *pixmap) {
    if (!s || role < 0 || role >= NTK_STYLE_PIXMAP_COUNT) return;
    if (s->pixmaps[role]) ntk_pixmap_destroy(s->pixmaps[role]);
    s->pixmaps[role] = pixmap;
}

NtkPixmap* ntk_style_get_pixmap(NtkStyle *s, NtkStylePixmap role) {
    if (!s || role < 0 || role >= NTK_STYLE_PIXMAP_COUNT) return NULL;
    return s->pixmaps[role];
}

void ntk_style_set_gradient(NtkStyle *s, NtkStyleGradientRole role, NtkGradient *gradient) {
    if (!s || role < 0 || role >= NTK_STYLE_GRADIENT_COUNT) return;
    if (s->gradients[role]) ntk_gradient_destroy(s->gradients[role]);
    s->gradients[role] = gradient;
}

NtkGradient* ntk_style_get_gradient(NtkStyle *s, NtkStyleGradientRole role) {
    if (!s || role < 0 || role >= NTK_STYLE_GRADIENT_COUNT) return NULL;
    return s->gradients[role]; 
}

void ntk_style_apply(NtkStyle *s) {
    g_global_style = s;
}

void ntk_style_apply_to(NtkStyle *s, NtkWidget *w) {
    (void)s;
    (void)w;
}

NtkStyle* ntk_style_get_global(void) {
    if (!g_global_style) {
        g_global_style = ntk_style_new();
    }
    return g_global_style;
}

void ntk_style_inherit(NtkStyle *child, NtkStyle *parent) {
    if (!child || !parent) return;
    for (int i = 0; i < NTK_STYLE_ROLE_COUNT; i++) {
        if (child->colors[i] == 0) {
            child->colors[i] = parent->colors[i];
        }
    }

    for (int i = 0; i < NTK_STYLE_METRIC_COUNT; i++) {
        if (child->metrics[i] == 0) {
            child->metrics[i] = parent->metrics[i];
        }
    }

    for (int i = 0; i < NTK_STYLE_ELEMENT_COUNT; i++) {
        if (!child->fonts[i] && parent->fonts[i]) {
            child->fonts[i] = ntk_font_clone(parent->fonts[i]);
        }
    }
}

NtkColor ntk_style_resolve_color(const char *hex_str, NtkColor fallback) {
    if (!hex_str || hex_str[0] == '\0') return fallback;
    return ntk_color_from_hex(hex_str);
}
