// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_painter.h"
#include "ntk_widget.h"
#include "ntk_font.h"
#include "ntk_pixmap.h"
#include "ntk_gradient.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define sqrtf(x) ((float)sqrt(x))
#define cosf(x)  ((float)cos(x))
#define sinf(x)  ((float)sin(x))
#define NTK_PAINTER_MAX_TRANSFORMS 16

struct NtkPainter {
    uint32_t   *buffer;
    int         width;
    int         height;
    bool        owns_buffer;  
    NtkColor    color;
    float       stroke_width;
    NtkFont    *font;
    float       opacity;
    bool        has_clip;
    NtkRect     clip;
    NtkTransform transforms[NTK_PAINTER_MAX_TRANSFORMS];
    int          transform_depth;
};
static inline void painter_set_pixel(NtkPainter *p, int x, int y, NtkColor color) {
    if (x < 0 || y < 0 || x >= p->width || y >= p->height) return;
    if (p->has_clip) {
        if (x < p->clip.x || y < p->clip.y ||
            x >= p->clip.x + p->clip.width ||
            y >= p->clip.y + p->clip.height) return;
    }

    uint8_t sa = ntk_color_a(color);
    if (p->opacity < 1.0f) {
        sa = (uint8_t)(sa * p->opacity);
    }

    if (sa == 0) return;

    uint32_t *dst = &p->buffer[y * p->width + x];

    if (sa == 255) {
        *dst = color | 0xFF000000;
        return;
    }

    uint32_t d = *dst;
    uint8_t sr = ntk_color_r(color), sg = ntk_color_g(color), sb = ntk_color_b(color);
    uint8_t dr = ntk_color_r(d), dg = ntk_color_g(d), db = ntk_color_b(d);

    uint32_t inv_a = 255 - sa;
    uint8_t or = (uint8_t)((sr * sa + dr * inv_a) / 255);
    uint8_t og = (uint8_t)((sg * sa + dg * inv_a) / 255);
    uint8_t ob = (uint8_t)((sb * sa + db * inv_a) / 255);

    *dst = 0xFF000000 | ((uint32_t)or << 16) | ((uint32_t)og << 8) | ob;
}

static inline int iabs(int x) { return x < 0 ? -x : x; }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
NtkPainter* ntk_painter_new(NtkWidget *target) {
    if (!target) return NULL;
    uint32_t *buffer = ntk_widget_get_pixel_buffer(target);
    if (!buffer) return NULL;
    NtkWidget *cur = target;
    while (cur) {
        const NtkWidgetClass *klass = ntk_widget_get_class(cur);
        if (klass && strcmp(klass->type_name, "NtkWindow") == 0) {
            break;
        }
        cur = ntk_widget_get_parent(cur);
    }
    if (!cur) return NULL;

    NtkSize sz = ntk_widget_get_size(cur);
    return ntk_painter_new_from_buffer(buffer, sz.width, sz.height);
}

NtkPainter* ntk_painter_new_from_buffer(uint32_t *buffer, int width, int height) {
    if (!buffer || width <= 0 || height <= 0) return NULL;

    NtkPainter *p = calloc(1, sizeof(NtkPainter));
    if (!p) return NULL;

    p->buffer       = buffer;
    p->width        = width;
    p->height       = height;
    p->owns_buffer  = false;
    p->color        = 0xFFFFFFFF;
    p->stroke_width = 1.0f;
    p->opacity      = 1.0f;

    return p;
}

void ntk_painter_destroy(NtkPainter *p) {
    if (!p) return;
    if (p->owns_buffer) free(p->buffer);
    free(p);
}
void ntk_painter_set_color(NtkPainter *p, NtkColor color) {
    if (p) p->color = color;
}

void ntk_painter_set_stroke_width(NtkPainter *p, float width) {
    if (p) p->stroke_width = width;
}

void ntk_painter_set_font(NtkPainter *p, NtkFont *font) {
    if (p) p->font = font; 
}

void ntk_painter_set_clip_rect(NtkPainter *p, NtkRect rect) {
    if (!p) return;
    p->has_clip = true;
    p->clip = rect;
}

void ntk_painter_clear_clip(NtkPainter *p) {
    if (p) p->has_clip = false;
}

void ntk_painter_set_opacity(NtkPainter *p, float opacity) {
    if (!p) return;
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    p->opacity = opacity;
}
void ntk_painter_push_transform(NtkPainter *p, NtkTransform *t) {
    if (!p || p->transform_depth >= NTK_PAINTER_MAX_TRANSFORMS) return;
    if (t) p->transforms[p->transform_depth] = *t;
    p->transform_depth++;
}

void ntk_painter_pop_transform(NtkPainter *p) {
    if (p && p->transform_depth > 0) p->transform_depth--;
}
void ntk_painter_draw_line(NtkPainter *p, int x1, int y1, int x2, int y2) {
    if (!p) return;
    int dx = iabs(x2 - x1);
    int dy = -iabs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        painter_set_pixel(p, x1, y1, p->color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}
void ntk_painter_draw_rect(NtkPainter *p, NtkRect rect) {
    if (!p) return;

    int x1 = rect.x, y1 = rect.y;
    int x2 = rect.x + rect.width - 1;
    int y2 = rect.y + rect.height - 1;
    for (int x = x1; x <= x2; x++) {
        painter_set_pixel(p, x, y1, p->color);
        painter_set_pixel(p, x, y2, p->color);
    }
    for (int y = y1 + 1; y < y2; y++) {
        painter_set_pixel(p, x1, y, p->color);
        painter_set_pixel(p, x2, y, p->color);
    }
}

void ntk_painter_fill_rect(NtkPainter *p, NtkRect rect) {
    if (!p) return;

    int x1 = imax(rect.x, 0);
    int y1 = imax(rect.y, 0);
    int x2 = imin(rect.x + rect.width, p->width);
    int y2 = imin(rect.y + rect.height, p->height);

    if (p->has_clip) {
        x1 = imax(x1, p->clip.x);
        y1 = imax(y1, p->clip.y);
        x2 = imin(x2, p->clip.x + p->clip.width);
        y2 = imin(y2, p->clip.y + p->clip.height);
    }

    uint8_t sa = ntk_color_a(p->color);
    if (p->opacity < 1.0f) sa = (uint8_t)(sa * p->opacity);

    if (sa == 255) {
        for (int y = y1; y < y2; y++) {
            uint32_t *row = &p->buffer[y * p->width];
            uint32_t c = p->color | 0xFF000000;
            for (int x = x1; x < x2; x++) {
                row[x] = c;
            }
        }
    } else if (sa > 0) {
        for (int y = y1; y < y2; y++) {
            for (int x = x1; x < x2; x++) {
                painter_set_pixel(p, x, y, p->color);
            }
        }
    }
}
void ntk_painter_fill_rounded_rect(NtkPainter *p, NtkRect rect, int radius) {
    if (!p) return;
    if (radius <= 0) {
        ntk_painter_fill_rect(p, rect);
        return;
    }

    int rx = rect.x, ry = rect.y, rw = rect.width, rh = rect.height;
    if (radius > rw / 2) radius = rw / 2;
    if (radius > rh / 2) radius = rh / 2;
    ntk_painter_fill_rect(p, NTK_RECT(rx + radius, ry, rw - 2 * radius, rh)); 
    ntk_painter_fill_rect(p, NTK_RECT(rx, ry + radius, radius, rh - 2 * radius)); 
    ntk_painter_fill_rect(p, NTK_RECT(rx + rw - radius, ry + radius, radius, rh - 2 * radius)); 
    int r = radius;
    int cx, cy, d;
    cx = rx + r;
    cy = ry + r;
    {
        int px = 0, py = r;
        d = 1 - r;
        while (px <= py) {
            for (int x = cx - py; x <= cx - 1; x++) {
                painter_set_pixel(p, x, cy - px, p->color);
                if (px != 0) painter_set_pixel(p, x, cy + px - 2 * r + rh - 1, p->color);
            }
            for (int x = cx - px; x <= cx - 1; x++) {
                painter_set_pixel(p, x, cy - py, p->color);
                if (py != 0) painter_set_pixel(p, x, cy + py - 2 * r + rh - 1, p->color);
            }

            int cx2 = rx + rw - r - 1;
            for (int x = cx2 + 1; x <= cx2 + py; x++) {
                painter_set_pixel(p, x, cy - px, p->color);
                if (px != 0) painter_set_pixel(p, x, cy + px - 2 * r + rh - 1, p->color);
            }
            for (int x = cx2 + 1; x <= cx2 + px; x++) {
                painter_set_pixel(p, x, cy - py, p->color);
                if (py != 0) painter_set_pixel(p, x, cy + py - 2 * r + rh - 1, p->color);
            }

            px++;
            if (d < 0) {
                d += 2 * px + 1;
            } else {
                py--;
                d += 2 * (px - py) + 1;
            }
        }
    }
}

void ntk_painter_draw_rounded_rect(NtkPainter *p, NtkRect rect, int radius) {
    if (!p) return;
    if (radius <= 0) {
        ntk_painter_draw_rect(p, rect);
        return;
    }

    int rx = rect.x, ry = rect.y, rw = rect.width, rh = rect.height;
    if (radius > rw / 2) radius = rw / 2;
    if (radius > rh / 2) radius = rh / 2;

    for (int x = rx + radius; x < rx + rw - radius; x++) {
        painter_set_pixel(p, x, ry, p->color);
        painter_set_pixel(p, x, ry + rh - 1, p->color);
    }
    for (int y = ry + radius; y < ry + rh - radius; y++) {
        painter_set_pixel(p, rx, y, p->color);
        painter_set_pixel(p, rx + rw - 1, y, p->color);
    }

    int r = radius;
    int px = 0, py = r;
    int d = 1 - r;

    while (px <= py) {
        painter_set_pixel(p, rx + r - py, ry + r - px, p->color);
        painter_set_pixel(p, rx + r - px, ry + r - py, p->color);
        painter_set_pixel(p, rx + rw - 1 - r + py, ry + r - px, p->color);
        painter_set_pixel(p, rx + rw - 1 - r + px, ry + r - py, p->color);
        painter_set_pixel(p, rx + r - py, ry + rh - 1 - r + px, p->color);
        painter_set_pixel(p, rx + r - px, ry + rh - 1 - r + py, p->color);
        painter_set_pixel(p, rx + rw - 1 - r + py, ry + rh - 1 - r + px, p->color);
        painter_set_pixel(p, rx + rw - 1 - r + px, ry + rh - 1 - r + py, p->color);

        px++;
        if (d < 0) {
            d += 2 * px + 1;
        } else {
            py--;
            d += 2 * (px - py) + 1;
        }
    }
}
static inline int local_ceil(float x) {
    int ix = (int)x;
    if ((float)ix < x) ix++;
    return ix;
}

static inline int local_floor(float x) {
    return (int)x;
}

void ntk_painter_draw_ellipse(NtkPainter *p, NtkRect bounds) {
    if (!p) return;
    if (bounds.width <= 0 || bounds.height <= 0) return;

    int h = bounds.height;
    int *x1_arr = malloc(sizeof(int) * h);
    int *x2_arr = malloc(sizeof(int) * h);
    if (!x1_arr || !x2_arr) {
        if (x1_arr) free(x1_arr);
        if (x2_arr) free(x2_arr);
        return;
    }

    float cx = (float)bounds.x + (float)bounds.width / 2.0f - 0.5f;
    float cy = (float)bounds.y + (float)bounds.height / 2.0f - 0.5f;
    float rx = (float)bounds.width / 2.0f;
    float ry = (float)bounds.height / 2.0f;

    int first_row = -1;
    int last_row = -1;

    for (int i = 0; i < h; i++) {
        int y = bounds.y + i;
        float dy = ((float)y + 0.5f - cy) / ry;
        float dy2 = dy * dy;
        if (dy2 >= 1.0f) {
            x1_arr[i] = -1;
            x2_arr[i] = -1;
            continue;
        }
        float dx_max = rx * sqrtf(1.0f - dy2);
        int x1 = local_ceil(cx - dx_max - 0.5f);
        int x2 = local_floor(cx + dx_max - 0.5f);

        // Clamp to bounds
        if (x1 < bounds.x) x1 = bounds.x;
        if (x2 >= bounds.x + bounds.width) x2 = bounds.x + bounds.width - 1;

        if (x1 <= x2) {
            x1_arr[i] = x1;
            x2_arr[i] = x2;
            if (first_row == -1) first_row = i;
            last_row = i;
        } else {
            x1_arr[i] = -1;
            x2_arr[i] = -1;
        }
    }

    for (int i = 0; i < h; i++) {
        if (x1_arr[i] == -1) continue;

        if (i == first_row || i == last_row) {
            for (int x = x1_arr[i]; x <= x2_arr[i]; x++) {
                painter_set_pixel(p, x, bounds.y + i, p->color);
            }
        } else {
            painter_set_pixel(p, x1_arr[i], bounds.y + i, p->color);
            painter_set_pixel(p, x2_arr[i], bounds.y + i, p->color);
        }
        if (i + 1 < h && x1_arr[i + 1] != -1) {
            int y_curr = bounds.y + i;
            int y_next = bounds.y + i + 1;
            if (x1_arr[i] > x1_arr[i + 1]) {
                for (int x = x1_arr[i + 1]; x <= x1_arr[i]; x++) {
                    painter_set_pixel(p, x, y_next, p->color);
                }
            } else if (x1_arr[i] < x1_arr[i + 1]) {
                for (int x = x1_arr[i]; x <= x1_arr[i + 1]; x++) {
                    painter_set_pixel(p, x, y_curr, p->color);
                }
            }
            if (x2_arr[i] < x2_arr[i + 1]) {
                for (int x = x2_arr[i]; x <= x2_arr[i + 1]; x++) {
                    painter_set_pixel(p, x, y_next, p->color);
                }
            } else if (x2_arr[i] > x2_arr[i + 1]) {
                for (int x = x2_arr[i + 1]; x <= x2_arr[i]; x++) {
                    painter_set_pixel(p, x, y_curr, p->color);
                }
            }
        }
    }

    free(x1_arr);
    free(x2_arr);
}

void ntk_painter_fill_ellipse(NtkPainter *p, NtkRect bounds) {
    if (!p) return;
    if (bounds.width <= 0 || bounds.height <= 0) return;

    float cx = (float)bounds.x + (float)bounds.width / 2.0f - 0.5f;
    float cy = (float)bounds.y + (float)bounds.height / 2.0f - 0.5f;
    float rx = (float)bounds.width / 2.0f;
    float ry = (float)bounds.height / 2.0f;

    for (int y = bounds.y; y < bounds.y + bounds.height; y++) {
        float dy = ((float)y + 0.5f - cy) / ry;
        float dy2 = dy * dy;
        if (dy2 >= 1.0f) continue;
        float dx_max = rx * sqrtf(1.0f - dy2);
        int x1 = local_ceil(cx - dx_max - 0.5f);
        int x2 = local_floor(cx + dx_max - 0.5f);

        if (x1 < bounds.x) x1 = bounds.x;
        if (x2 >= bounds.x + bounds.width) x2 = bounds.x + bounds.width - 1;

        for (int x = x1; x <= x2; x++) {
            painter_set_pixel(p, x, y, p->color);
        }
    }
}

void ntk_painter_draw_circle(NtkPainter *p, int cx, int cy, int r) {
    if (!p || r < 0) return;
    if (r == 0) { painter_set_pixel(p, cx, cy, p->color); return; }

    int x = 0, y = r;
    int d = 1 - r;

    while (x <= y) {
        painter_set_pixel(p, cx + x, cy - y, p->color);
        painter_set_pixel(p, cx - x, cy - y, p->color);
        painter_set_pixel(p, cx + x, cy + y, p->color);
        painter_set_pixel(p, cx - x, cy + y, p->color);
        painter_set_pixel(p, cx + y, cy - x, p->color);
        painter_set_pixel(p, cx - y, cy - x, p->color);
        painter_set_pixel(p, cx + y, cy + x, p->color);
        painter_set_pixel(p, cx - y, cy + x, p->color);

        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}

void ntk_painter_fill_circle(NtkPainter *p, int cx, int cy, int r) {
    if (!p || r < 0) return;
    if (r == 0) { painter_set_pixel(p, cx, cy, p->color); return; }

    int x = 0, y = r;
    int d = 1 - r;

    while (x <= y) {
        for (int i = cx - y; i <= cx + y; i++) {
            painter_set_pixel(p, i, cy - x, p->color);
            if (x != 0) painter_set_pixel(p, i, cy + x, p->color);
        }
        for (int i = cx - x; i <= cx + x; i++) {
            painter_set_pixel(p, i, cy - y, p->color);
            painter_set_pixel(p, i, cy + y, p->color);
        }

        x++;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            y--;
            d += 2 * (x - y) + 1;
        }
    }
}


void ntk_painter_draw_arc(NtkPainter *p, NtkRect bounds, float start_angle, float span_angle) {
    if (!p) return;

    int cx = bounds.x + bounds.width / 2;
    int cy = bounds.y + bounds.height / 2;
    float rx = (float)bounds.width / 2.0f;
    float ry = (float)bounds.height / 2.0f;
    float step = 1.0f / (rx > ry ? rx : ry);
    if (step < 0.01f) step = 0.01f;

    float end_angle = start_angle + span_angle;
    float a = start_angle;
    float dir = (span_angle >= 0) ? step : -step;

    while ((span_angle >= 0 && a <= end_angle) || (span_angle < 0 && a >= end_angle)) {
        int x = cx + (int)(rx * cosf(a) + 0.5f);
        int y = cy - (int)(ry * sinf(a) + 0.5f);
        painter_set_pixel(p, x, y, p->color);
        a += dir;
    }
}
void ntk_painter_draw_polygon(NtkPainter *p, NtkPoint *points, int count) {
    if (!p || !points || count < 2) return;
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        ntk_painter_draw_line(p, points[i].x, points[i].y, points[next].x, points[next].y);
    }
}

void ntk_painter_fill_polygon(NtkPainter *p, NtkPoint *points, int count) {
    if (!p || !points || count < 3) return;
    int min_y = points[0].y, max_y = points[0].y;
    for (int i = 1; i < count; i++) {
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }
    int *node_x = malloc((size_t)count * sizeof(int));
    if (!node_x) return;

    for (int y = min_y; y <= max_y; y++) {
        int nodes = 0;
        int j = count - 1;
        for (int i = 0; i < count; i++) {
            if ((points[i].y < y && points[j].y >= y) || (points[j].y < y && points[i].y >= y)) {
                node_x[nodes++] = points[i].x + (y - points[i].y) *
                    (points[j].x - points[i].x) / (points[j].y - points[i].y);
            }
            j = i;
        }
        for (int i = 0; i < nodes - 1; i++) {
            for (int k = i + 1; k < nodes; k++) {
                if (node_x[i] > node_x[k]) {
                    int tmp = node_x[i]; node_x[i] = node_x[k]; node_x[k] = tmp;
                }
            }
        }
        for (int i = 0; i < nodes - 1; i += 2) {
            for (int x = node_x[i]; x <= node_x[i + 1]; x++) {
                painter_set_pixel(p, x, y, p->color);
            }
        }
    }

    free(node_x);
}
void ntk_painter_draw_pixmap(NtkPainter *p, NtkPixmap *pm, int x, int y) {
    if (!p || !pm) return;

    int pw = ntk_pixmap_get_width(pm);
    int ph = ntk_pixmap_get_height(pm);
    unsigned char *data = ntk_pixmap_get_data(pm);
    if (!data) return;

    uint32_t *src = (uint32_t *)data;
    for (int sy = 0; sy < ph; sy++) {
        for (int sx = 0; sx < pw; sx++) {
            uint32_t pixel = src[sy * pw + sx];
            if (ntk_color_a(pixel) > 0) {
                painter_set_pixel(p, x + sx, y + sy, pixel);
            }
        }
    }
}

void ntk_painter_draw_pixmap_scaled(NtkPainter *p, NtkPixmap *pm, NtkRect dest) {
    if (!p || !pm) return;

    int pw = ntk_pixmap_get_width(pm);
    int ph = ntk_pixmap_get_height(pm);
    unsigned char *data = ntk_pixmap_get_data(pm);
    if (!data || pw <= 0 || ph <= 0) return;

    uint32_t *src = (uint32_t *)data;
    for (int dy = 0; dy < dest.height; dy++) {
        int sy = (int)((uint64_t)dy * (uint64_t)ph / (uint64_t)dest.height);
        if (sy >= ph) sy = ph - 1;
        for (int dx = 0; dx < dest.width; dx++) {
            int sx = (int)((uint64_t)dx * (uint64_t)pw / (uint64_t)dest.width);
            if (sx >= pw) sx = pw - 1;
            uint32_t pixel = src[sy * pw + sx];
            if (ntk_color_a(pixel) > 0) {
                painter_set_pixel(p, dest.x + dx, dest.y + dy, pixel);
            }
        }
    }
}
void ntk_painter_draw_text(NtkPainter *p, const char *text, int x, int y) {
    if (!p || !text || !p->font) return;

    ntk_font_ensure_baked(p->font);

    float cur_x = (float)x;
    int ascent = ntk_font_get_ascent(p->font);
    int baseline_y = y + ascent;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        int gw, gh, gxoff, gyoff, advance;
        unsigned char *glyph = ntk_font_get_glyph(p->font, c, &gw, &gh, &gxoff, &gyoff, &advance);

        if (glyph && gw > 0 && gh > 0) {
            int gx = (int)(cur_x + 0.5f) + gxoff;
            int gy = baseline_y + gyoff;

            uint8_t cr = ntk_color_r(p->color);
            uint8_t cg = ntk_color_g(p->color);
            uint8_t cb = ntk_color_b(p->color);

            for (int row = 0; row < gh; row++) {
                for (int col = 0; col < gw; col++) {
                    uint8_t alpha = glyph[row * 512 + col]; 
                    if (alpha > 0) {
                        NtkColor gc = ntk_color_from_rgba(cr, cg, cb, alpha);
                        painter_set_pixel(p, gx + col, gy + row, gc);
                    }
                }
            }
        }

        cur_x += advance;
        text++;
    }
}

void ntk_painter_draw_text_rect(NtkPainter *p, const char *text, NtkRect rect, NtkAlign align) {
    if (!p || !text || !p->font) return;

    NtkSize text_size = ntk_font_measure_text(p->font, text);
    int x, y;

    switch (align) {
        case NTK_ALIGN_CENTER:
            x = rect.x + (rect.width - text_size.width) / 2;
            break;
        case NTK_ALIGN_END:
            x = rect.x + rect.width - text_size.width;
            break;
        case NTK_ALIGN_FILL:
        case NTK_ALIGN_START:
        default:
            x = rect.x;
            break;
    }

    y = rect.y + (rect.height - text_size.height) / 2;

    NtkRect saved_clip = p->clip;
    bool had_clip = p->has_clip;
    ntk_painter_set_clip_rect(p, rect);

    ntk_painter_draw_text(p, text, x, y);

    if (had_clip)
        p->clip = saved_clip;
    else
        p->has_clip = false;
}
void ntk_painter_draw_gradient(NtkPainter *p, NtkGradient *g, NtkRect rect) {
    if (!p || !g) return;

    for (int y = 0; y < rect.height; y++) {
        for (int x = 0; x < rect.width; x++) {
            float t;
            t = (float)y / (float)(rect.height > 1 ? rect.height - 1 : 1);

            NtkColor c = ntk_gradient_sample(g, t);
            painter_set_pixel(p, rect.x + x, rect.y + y, c);
        }
    }
}
void ntk_painter_draw_bevel_raised(NtkPainter *p, NtkRect rect, NtkColor light, NtkColor dark) {
    if (!p) return;

    int x1 = rect.x, y1 = rect.y;
    int x2 = rect.x + rect.width - 1;
    int y2 = rect.y + rect.height - 1;

    NtkColor outer_light = 0xFFFFFFFF; 
    NtkColor inner_light = 0xFFFFFFFF; 
    NtkColor inner_dark  = 0xFF676767; 
    NtkColor outer_dark  = 0xFF676767; 
    NtkColor saved = p->color;
    p->color = outer_light;
    for (int x = x1; x < x2; x++) painter_set_pixel(p, x, y1, p->color);
    for (int y = y1; y < y2; y++) painter_set_pixel(p, x1, y, p->color);
    p->color = outer_dark;
    for (int x = x1; x <= x2; x++) painter_set_pixel(p, x, y2, p->color);
    for (int y = y1; y <= y2; y++) painter_set_pixel(p, x2, y, p->color);
    p->color = inner_light;
    for (int x = x1 + 1; x < x2 - 1; x++) painter_set_pixel(p, x, y1 + 1, p->color);
    for (int y = y1 + 1; y < y2 - 1; y++) painter_set_pixel(p, x1 + 1, y, p->color);
    p->color = inner_dark;
    for (int x = x1 + 1; x < x2 - 1; x++) painter_set_pixel(p, x, y2 - 1, p->color);
    for (int y = y1 + 1; y < y2 - 1; y++) painter_set_pixel(p, x2 - 1, y, p->color);

    p->color = saved;
}

void ntk_painter_draw_bevel_sunken(NtkPainter *p, NtkRect rect, NtkColor light, NtkColor dark) {
    if (!p) return;

    int x1 = rect.x, y1 = rect.y;
    int x2 = rect.x + rect.width - 1;
    int y2 = rect.y + rect.height - 1;

    NtkColor outer_shadow = 0xFF676767; 
    NtkColor inner_shadow = 0xFF676767; 
    NtkColor inner_highlight = 0xFFFFFFFF; 
    NtkColor outer_highlight = 0xFFFFFFFF; 
    NtkColor saved = p->color;
    p->color = outer_shadow;
    for (int x = x1; x < x2; x++) painter_set_pixel(p, x, y1, p->color);
    for (int y = y1; y < y2; y++) painter_set_pixel(p, x1, y, p->color);
    p->color = outer_highlight;
    for (int x = x1; x <= x2; x++) painter_set_pixel(p, x, y2, p->color);
    for (int y = y1; y <= y2; y++) painter_set_pixel(p, x2, y, p->color);
    p->color = inner_shadow;
    for (int x = x1 + 1; x < x2 - 1; x++) painter_set_pixel(p, x, y1 + 1, p->color);
    for (int y = y1 + 1; y < y2 - 1; y++) painter_set_pixel(p, x1 + 1, y, p->color);
    p->color = inner_highlight;
    for (int x = x1 + 1; x < x2 - 1; x++) painter_set_pixel(p, x, y2 - 1, p->color);
    for (int y = y1 + 1; y < y2 - 1; y++) painter_set_pixel(p, x2 - 1, y, p->color);

    p->color = saved;
}
void ntk_painter_blend_buffer(NtkPainter *p, int dx, int dy,
                               const uint32_t *src, int src_w, int src_h,
                               float alpha) {
    if (!p || !src || src_w <= 0 || src_h <= 0 || alpha <= 0.0f) return;

    float saved_opacity = p->opacity;
    p->opacity *= alpha;

    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            uint32_t pixel = src[y * src_w + x];
            if (ntk_color_a(pixel) > 0) {
                painter_set_pixel(p, dx + x, dy + y, pixel);
            }
        }
    }

    p->opacity = saved_opacity;
}
void ntk_painter_clear(NtkPainter *p, NtkColor color) {
    if (!p) return;
    int count = p->width * p->height;
    for (int i = 0; i < count; i++) {
        p->buffer[i] = color;
    }
}
