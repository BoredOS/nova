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

// Fills the entire buffer with a solid color
void ui_clear(uint32_t *buffer, int w, int h, uint32_t color);

// Initializes the font baking subsystem at a specific font size (such as 13 or 14)
int ui_font_init(const char *font_path, int font_size);

// Releases font resources
void ui_font_shutdown(void);

// Returns the rendered pixel width of a string with the current baked font
int ui_text_width(const char *text);

// Returns the rendered pixel width of the first n characters of a string.
int ui_text_width_n(const char *text, int n);

// Renders a string using the pre-baked font bitmap atlas
void ui_draw_text(uint32_t *buffer, int w, int h, int x, int y,
                  const char *text, uint32_t color);

// Renders string using the pre-baked font bitmap atlas (alias for ui_draw_text)
void ui_draw_string(uint32_t *buffer, int w, int h, int x, int y,
                    const char *str, uint32_t color);

// Renders a string centered horizontally at the given y position
void ui_draw_text_centered(uint32_t *buffer, int w, int h, int y,
                           const char *text, uint32_t color);

#endif
