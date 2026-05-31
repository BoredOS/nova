#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Blends a sub-surface onto a target surface with global alpha transparency
void ui_blend_pixels(uint32_t *dest, int dest_w, int dest_h, int dx, int dy,
                     const uint32_t *src, int src_w, int src_h, float alpha);

// Draws a rounded rectangle panel onto a surface
void ui_draw_panel(uint32_t *buffer, int w, int h, int x, int y, int rw, int rh,
                   uint32_t color, uint32_t border_color, int radius);

// Initializes the font baking subsystem at a specific font size (such as 13 or 14)
int ui_font_init(const char *font_path, int font_size);

// Renders string using the pre-baked font bitmap atlas (fast and sharp)
void ui_draw_string(uint32_t *buffer, int w, int h, int x, int y,
                    const char *str, uint32_t color);

#endif
