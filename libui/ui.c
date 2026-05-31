#define STB_TRUETYPE_IMPLEMENTATION
#include "ui.h"
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Software Blending
void ui_blend_pixels(uint32_t *dest, int dest_w, int dest_h, int dx, int dy,
                     const uint32_t *src, int src_w, int src_h, float alpha) {
    if (!dest || !src || alpha <= 0.0f) return;
    if (alpha > 1.0f) alpha = 1.0f;

    uint32_t global_alpha = (uint32_t)(alpha * 255.0f);

    int start_x = dx < 0 ? -dx : 0;
    int start_y = dy < 0 ? -dy : 0;
    int end_x = (dx + src_w > dest_w) ? (dest_w - dx) : src_w;
    int end_y = (dy + src_h > dest_h) ? (dest_h - dy) : src_h;

    for (int sy = start_y; sy < end_y; sy++) {
        int ty = dy + sy;
        uint32_t *dest_row = &dest[ty * dest_w + dx];
        const uint32_t *src_row = &src[sy * src_w];

        for (int sx = start_x; sx < end_x; sx++) {
            uint32_t src_pixel = src_row[sx];
            uint32_t src_a = ((src_pixel >> 24) & 0xFF) * global_alpha / 255;
            if (src_a == 0) continue;

            if (src_a == 255) {
                dest_row[sx] = src_pixel;
                continue;
            }

            uint32_t dest_pixel = dest_row[sx];
            uint32_t dest_a = (dest_pixel >> 24) & 0xFF;

            uint32_t out_a = src_a + dest_a * (255 - src_a) / 255;
            if (out_a == 0) continue;

            uint32_t src_r = (src_pixel >> 16) & 0xFF;
            uint32_t src_g = (src_pixel >> 8) & 0xFF;
            uint32_t src_b = src_pixel & 0xFF;

            uint32_t dest_r = (dest_pixel >> 16) & 0xFF;
            uint32_t dest_g = (dest_pixel >> 8) & 0xFF;
            uint32_t dest_b = dest_pixel & 0xFF;

            uint32_t out_r = (src_r * src_a + dest_r * dest_a * (255 - src_a) / 255) / out_a;
            uint32_t out_g = (src_g * src_a + dest_g * dest_a * (255 - src_a) / 255) / out_a;
            uint32_t out_b = (src_b * src_a + dest_b * dest_a * (255 - src_a) / 255) / out_a;

            dest_row[sx] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

// Anti-aliased rounded panel renderer
void ui_draw_panel(uint32_t *buffer, int w, int h, int x, int y, int rw, int rh,
                   uint32_t color, uint32_t border_color, int radius) {
    if (!buffer) return;

    int x1 = x;
    int y1 = y;
    int x2 = x + rw;
    int y2 = y + rh;

    // Constrain boundaries
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > w) x2 = w;
    if (y2 > h) y2 = h;

    uint32_t bg_a = (color >> 24) & 0xFF;
    uint32_t bg_r = (color >> 16) & 0xFF;
    uint32_t bg_g = (color >> 8) & 0xFF;
    uint32_t bg_b = color & 0xFF;

    uint32_t bo_a = (border_color >> 24) & 0xFF;
    uint32_t bo_r = (border_color >> 16) & 0xFF;
    uint32_t bo_g = (border_color >> 8) & 0xFF;
    uint32_t bo_b = border_color & 0xFF;

    for (int py = y1; py < y2; py++) {
        uint32_t *row = &buffer[py * w];
        for (int px = x1; px < x2; px++) {
            // Determine corner sectors
            int dx = 0, dy = 0;
            if (px < x + radius) dx = (x + radius) - px;
            else if (px >= x + rw - radius) dx = px - (x + rw - radius - 1);
            
            if (py < y + radius) dy = (y + radius) - py;
            else if (py >= y + rh - radius) dy = py - (y + rh - radius - 1);

            uint32_t draw_a = bg_a;
            uint32_t draw_r = bg_r;
            uint32_t draw_g = bg_g;
            uint32_t draw_b = bg_b;

            if (dx > 0 && dy > 0) {
                float dist = (float)sqrt((double)(dx * dx + dy * dy));
                if (dist > (float)radius) {
                    // Out of rounded boundary, apply soft antialiasing blending at edge
                    float diff = dist - (float)radius;
                    if (diff < 1.0f) {
                        float edge_alpha = 1.0f - diff;
                        draw_a = (uint32_t)((float)bg_a * edge_alpha);
                    } else {
                        continue; // Fully transparent corner pixel
                    }
                }
            }

            // Draw border outline (1px ring)
            bool is_border = false;
            if (dx > 0 && dy > 0) {
                float dist = (float)sqrt((double)(dx * dx + dy * dy));
                if (dist >= (float)(radius - 1.0f) && dist <= (float)radius) {
                    is_border = true;
                }
            } else {
                if (px == x || px == x + rw - 1 || py == y || py == y + rh - 1) {
                    is_border = true;
                }
            }

            if (is_border) {
                draw_a = bo_a;
                draw_r = bo_r;
                draw_g = bo_g;
                draw_b = bo_b;
            }

            if (draw_a == 255) {
                row[px] = (draw_a << 24) | (draw_r << 16) | (draw_g << 8) | draw_b;
            } else if (draw_a > 0) {
                // Alpha blend over background
                uint32_t bg_pixel = row[px];
                uint32_t dest_a = (bg_pixel >> 24) & 0xFF;
                uint32_t dest_r = (bg_pixel >> 16) & 0xFF;
                uint32_t dest_g = (bg_pixel >> 8) & 0xFF;
                uint32_t dest_b = bg_pixel & 0xFF;

                uint32_t out_a = draw_a + dest_a * (255 - draw_a) / 255;
                if (out_a == 0) continue;

                uint32_t out_r = (draw_r * draw_a + dest_r * dest_a * (255 - draw_a) / 255) / out_a;
                uint32_t out_g = (draw_g * draw_a + dest_g * dest_a * (255 - draw_a) / 255) / out_a;
                uint32_t out_b = (draw_b * draw_a + dest_b * dest_a * (255 - draw_a) / 255) / out_a;

                row[px] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }
    }
}

// Baked Font Atlas state
#define ATLAS_W 512
#define ATLAS_H 512
static unsigned char *g_font_atlas = NULL;
static stbtt_bakedchar g_chardata[96]; // ASCII 32..127
static bool g_font_initialized = false;
static int g_baked_font_size = 13;

int ui_font_init(const char *font_path, int font_size) {
    const char *path = font_path;
    if (!path) path = "/Library/Fonts/inter.ttf";

    FILE *f = fopen(path, "rb");
    if (!f) {
        // Fallback font path in case of missing folder structures
        path = "/usr/include/Library/Fonts/inter.ttf";
        f = fopen(path, "rb");
    }
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *ttf_buffer = malloc(size);
    if (!ttf_buffer) {
        fclose(f);
        return -1;
    }

    if (fread(ttf_buffer, 1, size, f) != size) {
        free(ttf_buffer);
        fclose(f);
        return -1;
    }
    fclose(f);

    if (g_font_atlas) free(g_font_atlas);
    g_font_atlas = malloc(ATLAS_W * ATLAS_H);
    if (!g_font_atlas) {
        free(ttf_buffer);
        return -1;
    }

    // Bake Inter outline font directly into a sharp alpha-map atlas
    stbtt_BakeFontBitmap(ttf_buffer, 0, (float)font_size, g_font_atlas, ATLAS_W, ATLAS_H, 32, 96, g_chardata);
    free(ttf_buffer);

    g_font_initialized = true;
    g_baked_font_size = font_size;
    return 0;
}

void ui_draw_string(uint32_t *buffer, int w, int h, int x, int y,
                    const char *str, uint32_t color) {
    if (!g_font_initialized || !buffer || !str) return;

    uint32_t src_r = (color >> 16) & 0xFF;
    uint32_t src_g = (color >> 8) & 0xFF;
    uint32_t src_b = color & 0xFF;
    uint32_t src_a = (color >> 24) & 0xFF;

    float fx = (float)x;
    float fy = (float)y;

    // Shift font vertically to match the baseline alignment correctly
    fy += (float)g_baked_font_size * 0.85f;

    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) continue;

        stbtt_bakedchar *bc = &g_chardata[c - 32];
        
        int x0 = bc->x0;
        int y0 = bc->y0;
        int x1 = bc->x1;
        int y1 = bc->y1;

        int gw = x1 - x0;
        int gh = y1 - y0;

        int dx = (int)(fx + bc->xoff + 0.5f);
        int dy = (int)(fy + bc->yoff + 0.5f);

        // Blit glyph from baked font atlas to 32-bit ARGB target canvas
        for (int gy = 0; gy < gh; gy++) {
            int ty = dy + gy;
            if (ty < 0 || ty >= h) continue;

            uint32_t *row = &buffer[ty * w];
            const unsigned char *atlas_row = &g_font_atlas[(y0 + gy) * ATLAS_W + x0];

            for (int gx = 0; gx < gw; gx++) {
                int tx = dx + gx;
                if (tx < 0 || tx >= w) continue;

                uint32_t alpha = atlas_row[gx] * src_a / 255;
                if (alpha == 0) continue;

                if (alpha == 255) {
                    row[tx] = (0xFF << 24) | (src_r << 16) | (src_g << 8) | src_b;
                    continue;
                }

                uint32_t dest_pixel = row[tx];
                uint32_t dest_a = (dest_pixel >> 24) & 0xFF;
                uint32_t dest_r = (dest_pixel >> 16) & 0xFF;
                uint32_t dest_g = (dest_pixel >> 8) & 0xFF;
                uint32_t dest_b = dest_pixel & 0xFF;

                uint32_t out_a = alpha + dest_a * (255 - alpha) / 255;
                if (out_a == 0) continue;

                uint32_t out_r = (src_r * alpha + dest_r * dest_a * (255 - alpha) / 255) / out_a;
                uint32_t out_g = (src_g * alpha + dest_g * dest_a * (255 - alpha) / 255) / out_a;
                uint32_t out_b = (src_b * alpha + dest_b * dest_a * (255 - alpha) / 255) / out_a;

                row[tx] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
            }
        }

        fx += bc->xadvance;
    }
}
