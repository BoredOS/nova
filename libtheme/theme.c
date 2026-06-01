#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim(char *str) {
    char *end;
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static uint32_t parse_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

uint32_t theme_resolve_color(const char *hex_str, uint32_t fallback) {
    if (!hex_str || hex_str[0] == '\0') return fallback;

    const char *ptr = hex_str;
    if (ptr[0] == '#') ptr++;
    else if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) ptr += 2;

    size_t len = strlen(ptr);
    if (len == 6) {
        uint32_t r = (parse_hex_digit(ptr[0]) << 4) | parse_hex_digit(ptr[1]);
        uint32_t g = (parse_hex_digit(ptr[2]) << 4) | parse_hex_digit(ptr[3]);
        uint32_t b = (parse_hex_digit(ptr[4]) << 4) | parse_hex_digit(ptr[5]);
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    } else if (len == 8) {
        uint32_t r = (parse_hex_digit(ptr[0]) << 4) | parse_hex_digit(ptr[1]);
        uint32_t g = (parse_hex_digit(ptr[2]) << 4) | parse_hex_digit(ptr[3]);
        uint32_t b = (parse_hex_digit(ptr[4]) << 4) | parse_hex_digit(ptr[5]);
        uint32_t a = (parse_hex_digit(ptr[6]) << 4) | parse_hex_digit(ptr[7]);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
    return fallback;
}

int theme_load(const char *config_path, ThemeConfig *out_theme) {
    if (!out_theme) return -1;

    // defauilts
    out_theme->desktop_bg   = 0xFF1E1E2E;
    out_theme->panel_bg     = 0xFF2D2D2D;
    out_theme->panel_border = 0xFF4D4D4D;
    out_theme->border_radius = 8;

    strncpy(out_theme->font_path, "/Library/Fonts/inter.ttf",
            sizeof(out_theme->font_path) - 1);
    out_theme->font_size    = 14;
    out_theme->text_primary = 0xFFEEEEEE;
    out_theme->text_dim     = 0xFFA6ADC8;
    out_theme->text_error   = 0xFFFF8A8A;

    out_theme->accent_color = 0xFF89B4FA;

    out_theme->widget_radius_button   = 6;
    out_theme->widget_radius_input    = 4;
    out_theme->widget_radius_panel    = 8;
    out_theme->widget_radius_progress = 4;
    out_theme->widget_padding         = 6;

    out_theme->widget_bg        = 0; // 0 = auto-derive
    out_theme->widget_bg_hover  = 0;
    out_theme->widget_bg_active = 0;

    if (!config_path) return 0;

    FILE *f = fopen(config_path, "r");
    if (!f) return 0;

    bool has_desktop_bg = false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == ';' ||
            line[0] == '#'  || line[0] == '[') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        // Desktop / compositor
        if      (strcmp(key, "panel_bg")     == 0) out_theme->panel_bg     = theme_resolve_color(val, out_theme->panel_bg);
        else if (strcmp(key, "panel_border") == 0) out_theme->panel_border = theme_resolve_color(val, out_theme->panel_border);
        else if (strcmp(key, "border_radius")== 0) out_theme->border_radius = atoi(val);
        else if (strcmp(key, "desktop_bg")   == 0) { out_theme->desktop_bg = theme_resolve_color(val, out_theme->desktop_bg); has_desktop_bg = true; }

        // Typography
        else if (strcmp(key, "font_path")    == 0) { strncpy(out_theme->font_path, val, sizeof(out_theme->font_path) - 1); out_theme->font_path[sizeof(out_theme->font_path)-1] = '\0'; }
        else if (strcmp(key, "font_size")    == 0) out_theme->font_size    = atoi(val);
        else if (strcmp(key, "text_primary") == 0) out_theme->text_primary = theme_resolve_color(val, out_theme->text_primary);
        else if (strcmp(key, "text_dim")     == 0) out_theme->text_dim     = theme_resolve_color(val, out_theme->text_dim);
        else if (strcmp(key, "text_error")   == 0) out_theme->text_error   = theme_resolve_color(val, out_theme->text_error);

        // Accent
        else if (strcmp(key, "accent_color") == 0) out_theme->accent_color = theme_resolve_color(val, out_theme->accent_color);

        // Widget geometry
        else if (strcmp(key, "widget_radius_button")   == 0) out_theme->widget_radius_button   = atoi(val);
        else if (strcmp(key, "widget_radius_input")    == 0) out_theme->widget_radius_input     = atoi(val);
        else if (strcmp(key, "widget_radius_panel")    == 0) out_theme->widget_radius_panel     = atoi(val);
        else if (strcmp(key, "widget_radius_progress") == 0) out_theme->widget_radius_progress  = atoi(val);
        else if (strcmp(key, "widget_padding")         == 0) out_theme->widget_padding          = atoi(val);

        // Widget explicit colors
        else if (strcmp(key, "widget_bg")        == 0) out_theme->widget_bg        = theme_resolve_color(val, 0);
        else if (strcmp(key, "widget_bg_hover")  == 0) out_theme->widget_bg_hover  = theme_resolve_color(val, 0);
        else if (strcmp(key, "widget_bg_active") == 0) out_theme->widget_bg_active = theme_resolve_color(val, 0);
    }

    if (!has_desktop_bg) out_theme->desktop_bg = out_theme->panel_bg;

    fclose(f);
    return 0;
}
