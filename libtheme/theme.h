#ifndef THEME_H
#define THEME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    // compositor
    uint32_t desktop_bg;
    uint32_t panel_bg;
    uint32_t panel_border;
    int      border_radius;        // global fallback radius (used by compositor)

    // typography
    char     font_path[128];
    int      font_size;
    uint32_t text_primary;
    uint32_t text_dim;             // secondary / muted text (labels, placeholders)
    uint32_t text_error;

    // widget accent
    uint32_t accent_color;         // interactive highlight (buttons pressed, slider thumb, focus ring)

    // widget geometry
    int      widget_radius_button; // corner radius for push-buttons
    int      widget_radius_input;  // corner radius for text inputs
    int      widget_radius_panel;  // corner radius for panel containers
    int      widget_radius_progress; // corner radius for progress bar / slider track
    int      widget_padding;       // inner horizontal padding for text inputs & buttons

    // widget colors (0 = derive automatically from panel_bg / accent)
    uint32_t widget_bg;            // explicit widget background (0 = auto-derive)
    uint32_t widget_bg_hover;      // explicit hover background  (0 = auto-derive)
    uint32_t widget_bg_active;     // explicit pressed/active bg (0 = auto-derive)
} ThemeConfig;

int      theme_load(const char *config_path, ThemeConfig *out_theme);
uint32_t theme_resolve_color(const char *hex_str, uint32_t fallback);

#endif
