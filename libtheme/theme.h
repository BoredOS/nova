#ifndef THEME_H
#define THEME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t panel_bg;
    uint32_t panel_border;
    int border_radius;
    uint32_t text_primary;
    uint32_t text_error;
    char font_path[128];
    int font_size;
    uint32_t desktop_bg;
} ThemeConfig;

int theme_load(const char *config_path, ThemeConfig *out_theme);
uint32_t theme_resolve_color(const char *hex_str, uint32_t fallback);

#endif
