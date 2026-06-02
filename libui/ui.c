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

void ui_clear(uint32_t *buffer, int w, int h, uint32_t color) {
    if (!buffer || w <= 0 || h <= 0) return;
    for (int i = 0; i < w * h; i++) {
        buffer[i] = color;
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

    if (radius > rw / 2) radius = rw / 2;
    if (radius > rh / 2) radius = rh / 2;

    for (int py = y1; py < y2; py++) {
        uint32_t *row = &buffer[py * w];
        bool is_corner_y = (py < y + radius || py >= y + rh - radius);
        int dy = 0;
        if (py < y + radius) {
            dy = (y + radius) - py;
        } else if (py >= y + rh - radius) {
            dy = py - (y + rh - radius - 1);
        }

        for (int px = x1; px < x2; px++) {
            bool is_corner_x = (px < x + radius || px >= x + rw - radius);

            uint32_t draw_a = bg_a;
            uint32_t draw_r = bg_r;
            uint32_t draw_g = bg_g;
            uint32_t draw_b = bg_b;

            if (is_corner_y && is_corner_x && radius > 0) {
                int dx = 0;
                if (px < x + radius) {
                    dx = (x + radius) - px;
                } else {
                    dx = px - (x + rw - radius - 1);
                }

                // We are in one of the 4 actual corners 
                float dist = (float)sqrt((double)(dx * dx + dy * dy));
                if (dist > (float)radius) {
                    float diff = dist - (float)radius;
                    if (diff < 1.0f) {
                        float edge_alpha = 1.0f - diff;
                        draw_a = (uint32_t)((float)bg_a * edge_alpha);
                    } else {
                        continue; // Fully transparent corner pixel
                    }
                }

                // Border calculation on corners
                if (dist >= (float)(radius - 1.0f) && dist <= (float)radius) {
                    draw_a = bo_a;
                    draw_r = bo_r;
                    draw_g = bo_g;
                    draw_b = bo_b;
                }
            } else {
                // Non-corner: simple 1px straight borders
                if (px == x || px == x + rw - 1 || py == y || py == y + rh - 1) {
                    draw_a = bo_a;
                    draw_r = bo_r;
                    draw_g = bo_g;
                    draw_b = bo_b;
                }
            }

            if (draw_a == 255) {
                row[px] = (0xFF << 24) | (draw_r << 16) | (draw_g << 8) | draw_b;
            } else if (draw_a > 0) {
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

// Dynamic font state. Glyphs are rasterized on demand so UTF-8 text is not
// limited to the initial ASCII atlas.
#define GLYPH_CACHE_SIZE 256
typedef struct {
    uint32_t codepoint;
    unsigned char *bitmap;
    int w, h;
    int xoff, yoff;
    int xadvance;
    bool valid;
} glyph_cache_entry_t;

static unsigned char *g_ttf_buffer = NULL;
static stbtt_fontinfo g_font_info;
static glyph_cache_entry_t g_glyph_cache[GLYPH_CACHE_SIZE];
static int g_glyph_cache_next = 0;
static bool g_font_initialized = false;
static int g_baked_font_size = 13;
static float g_font_scale = 1.0f;
static int g_font_ascent = 0;

static void clear_glyph_cache(void) {
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (g_glyph_cache[i].bitmap) {
            stbtt_FreeBitmap(g_glyph_cache[i].bitmap, NULL);
        }
        memset(&g_glyph_cache[i], 0, sizeof(g_glyph_cache[i]));
    }
    g_glyph_cache_next = 0;
}

static uint32_t decode_utf8_char(const char *s, int remaining, int *advance) {
    const unsigned char *p = (const unsigned char *)s;
    if (!s || remaining <= 0 || p[0] == '\0') {
        if (advance) *advance = 0;
        return 0;
    }

    if (p[0] < 0x80) {
        if (advance) *advance = 1;
        return p[0];
    }

    if ((p[0] & 0xE0) == 0xC0 && remaining >= 2 &&
        (p[1] & 0xC0) == 0x80) {
        uint32_t cp = ((uint32_t)(p[0] & 0x1F) << 6) |
                      (uint32_t)(p[1] & 0x3F);
        if (cp >= 0x80) {
            if (advance) *advance = 2;
            return cp;
        }
    }

    if ((p[0] & 0xF0) == 0xE0 && remaining >= 3 &&
        (p[1] & 0xC0) == 0x80 &&
        (p[2] & 0xC0) == 0x80) {
        uint32_t cp = ((uint32_t)(p[0] & 0x0F) << 12) |
                      ((uint32_t)(p[1] & 0x3F) << 6) |
                      (uint32_t)(p[2] & 0x3F);
        if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF)) {
            if (advance) *advance = 3;
            return cp;
        }
    }

    if ((p[0] & 0xF8) == 0xF0 && remaining >= 4 &&
        (p[1] & 0xC0) == 0x80 &&
        (p[2] & 0xC0) == 0x80 &&
        (p[3] & 0xC0) == 0x80) {
        uint32_t cp = ((uint32_t)(p[0] & 0x07) << 18) |
                      ((uint32_t)(p[1] & 0x3F) << 12) |
                      ((uint32_t)(p[2] & 0x3F) << 6) |
                      (uint32_t)(p[3] & 0x3F);
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            if (advance) *advance = 4;
            return cp;
        }
    }

    if (advance) *advance = 1;
    return 0xFFFD;
}

static glyph_cache_entry_t *get_glyph(uint32_t cp) {
    if (!g_font_initialized) return NULL;
    if (cp == 0) return NULL;

    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (g_glyph_cache[i].valid && g_glyph_cache[i].codepoint == cp) {
            return &g_glyph_cache[i];
        }
    }

    uint32_t render_cp = cp;
    if (render_cp != ' ' && stbtt_FindGlyphIndex(&g_font_info, (int)render_cp) == 0) {
        render_cp = '?';
    }

    glyph_cache_entry_t *entry = &g_glyph_cache[g_glyph_cache_next];
    g_glyph_cache_next = (g_glyph_cache_next + 1) % GLYPH_CACHE_SIZE;

    if (entry->bitmap) {
        stbtt_FreeBitmap(entry->bitmap, NULL);
    }
    memset(entry, 0, sizeof(*entry));

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&g_font_info, (int)render_cp, &advance, &lsb);

    entry->codepoint = cp;
    entry->xadvance = (int)((float)advance * g_font_scale + 0.5f);
    if (entry->xadvance <= 0) entry->xadvance = g_baked_font_size / 2;
    entry->valid = true;

    if (render_cp != ' ') {
        entry->bitmap = stbtt_GetCodepointBitmap(&g_font_info,
                                                 0,
                                                 g_font_scale,
                                                 (int)render_cp,
                                                 &entry->w,
                                                 &entry->h,
                                                 &entry->xoff,
                                                 &entry->yoff);
    }

    return entry;
}

int ui_font_init(const char *font_path, int font_size) {
    const char *path = font_path;
    if (!path) path = "/Library/Fonts/inter.ttf";

    FILE *f = fopen(path, "rb");
    if (!f) {
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

    if (fread(ttf_buffer, 1, (size_t)size, f) != (size_t)size) {
        free(ttf_buffer);
        fclose(f);
        return -1;
    }
    fclose(f);

    clear_glyph_cache();
    if (g_ttf_buffer) {
        free(g_ttf_buffer);
        g_ttf_buffer = NULL;
    }

    g_ttf_buffer = ttf_buffer;
    if (!stbtt_InitFont(&g_font_info, g_ttf_buffer, 0)) {
        free(ttf_buffer);
        g_ttf_buffer = NULL;
        return -1;
    }

    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&g_font_info, &g_font_ascent, &descent, &line_gap);
    g_font_scale = stbtt_ScaleForPixelHeight(&g_font_info, (float)font_size);

    g_font_initialized = true;
    g_baked_font_size = font_size;
    return 0;
}

void ui_font_shutdown(void) {
    clear_glyph_cache();
    if (g_ttf_buffer) {
        free(g_ttf_buffer);
        g_ttf_buffer = NULL;
    }
    g_font_initialized = false;
    g_baked_font_size = 13;
    g_font_scale = 1.0f;
    g_font_ascent = 0;
}

int ui_text_width(const char *text) {
    if (!g_font_initialized || !text) return 0;

    int width = 0;
    int remaining = (int)strlen(text);
    const char *p = text;
    while (remaining > 0 && *p) {
        int adv = 0;
        uint32_t cp = decode_utf8_char(p, remaining, &adv);
        if (adv <= 0) break;
        if (cp >= 32) {
            glyph_cache_entry_t *g = get_glyph(cp);
            if (g) width += g->xadvance;
        }
        p += adv;
        remaining -= adv;
    }
    return width;
}

int ui_text_width_n(const char *text, int n) {
    if (!g_font_initialized || !text || n <= 0) return 0;

    int width = 0;
    int total_len = (int)strlen(text);
    if (n > total_len) n = total_len;
    const char *p = text;
    int remaining = n;
    while (remaining > 0 && *p) {
        int adv = 0;
        uint32_t cp = decode_utf8_char(p, remaining, &adv);
        if (adv <= 0 || adv > remaining) break;
        if (cp >= 32) {
            glyph_cache_entry_t *g = get_glyph(cp);
            if (g) width += g->xadvance;
        }
        p += adv;
        remaining -= adv;
    }
    return width;
}

void ui_draw_text(uint32_t *buffer, int w, int h, int x, int y,
                  const char *text, uint32_t color) {
    if (!g_font_initialized || !buffer || !text) return;

    uint32_t src_r = (color >> 16) & 0xFF;
    uint32_t src_g = (color >> 8) & 0xFF;
    uint32_t src_b = color & 0xFF;
    uint32_t src_a = (color >> 24) & 0xFF;

    int pen_x = x;
    int baseline_y = y + (int)((float)g_font_ascent * g_font_scale + 0.5f);

    int remaining = (int)strlen(text);
    const char *p = text;

    while (remaining > 0 && *p) {
        int adv = 0;
        uint32_t cp = decode_utf8_char(p, remaining, &adv);
        if (adv <= 0) break;
        p += adv;
        remaining -= adv;

        if (cp < 32) continue;

        glyph_cache_entry_t *glyph = get_glyph(cp);
        if (!glyph) continue;

        int gw = glyph->w;
        int gh = glyph->h;
        int dx = pen_x + glyph->xoff;
        int dy = baseline_y + glyph->yoff;

        // Blit the cached glyph bitmap to the 32-bit ARGB target canvas.
        for (int gy = 0; gy < gh; gy++) {
            int ty = dy + gy;
            if (ty < 0 || ty >= h) continue;

            uint32_t *row = &buffer[ty * w];
            const unsigned char *glyph_row = glyph->bitmap ? &glyph->bitmap[gy * gw] : NULL;
            if (!glyph_row) continue;

            for (int gx = 0; gx < gw; gx++) {
                int tx = dx + gx;
                if (tx < 0 || tx >= w) continue;

                uint32_t alpha = glyph_row[gx] * src_a / 255;
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

        pen_x += glyph->xadvance;
    }
}

void ui_draw_string(uint32_t *buffer, int w, int h, int x, int y,
                    const char *str, uint32_t color) {
    ui_draw_text(buffer, w, h, x, y, str, color);
}

void ui_draw_text_centered(uint32_t *buffer, int w, int h, int y,
                           const char *text, uint32_t color) {
    if (!text) return;
    int text_width = ui_text_width(text);
    int x = (w - text_width) / 2;
    ui_draw_text(buffer, w, h, x, y, text, color);
}
