// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_color.h"
#include <string.h>
#include <stdio.h>
NtkColor ntk_color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

NtkColor ntk_color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

NtkColor ntk_color_from_hex(const char *hex) {
    if (!hex) return 0xFF000000;
    if (*hex == '#') hex++;

    unsigned int val = 0;
    int len = 0;
    const char *p = hex;
    while (*p && len < 8) {
        char c = *p++;
        unsigned int d;
        if (c >= '0' && c <= '9')      d = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned int)(c - 'A' + 10);
        else break;
        val = (val << 4) | d;
        len++;
    }

    if (len == 6) {
        return 0xFF000000 | val;
    } else if (len == 8) {
        return val;
    } else if (len == 3) {
        uint8_t r = (uint8_t)((val >> 8) & 0xF);
        uint8_t g = (uint8_t)((val >> 4) & 0xF);
        uint8_t b = (uint8_t)( val       & 0xF);
        return 0xFF000000 | ((uint32_t)(r | (r << 4)) << 16)
                          | ((uint32_t)(g | (g << 4)) <<  8)
                          |  (uint32_t)(b | (b << 4));
    }

    return 0xFF000000;
}

NtkColor ntk_color_from_hsv(float h, float s, float v) {
    while (h < 0.0f)   h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    float c = v * s;
    float x = c * (1.0f - ((h / 60.0f) - 2.0f * (int)((h / 120.0f)) >= 0
                    ? (h / 60.0f) - 2.0f * (int)((h / 120.0f))
                    : -(h / 60.0f - 2.0f * (int)((h / 120.0f)))));
    float m = v - c;

    float r1, g1, b1;
    int hi = (int)(h / 60.0f) % 6;
    switch (hi) {
        case 0: r1 = c; g1 = x; b1 = 0; break;
        case 1: r1 = x; g1 = c; b1 = 0; break;
        case 2: r1 = 0; g1 = c; b1 = x; break;
        case 3: r1 = 0; g1 = x; b1 = c; break;
        case 4: r1 = x; g1 = 0; b1 = c; break;
        default: r1 = c; g1 = 0; b1 = x; break;
    }

    uint8_t r = (uint8_t)((r1 + m) * 255.0f + 0.5f);
    uint8_t g = (uint8_t)((g1 + m) * 255.0f + 0.5f);
    uint8_t b = (uint8_t)((b1 + m) * 255.0f + 0.5f);

    return ntk_color_from_rgb(r, g, b);
}
static inline uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static inline float clamp_f(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

NtkColor ntk_color_lerp(NtkColor a, NtkColor b, float t) {
    t = clamp_f(t, 0.0f, 1.0f);
    float inv = 1.0f - t;
    uint8_t ra = ntk_color_r(a), ga = ntk_color_g(a), ba = ntk_color_b(a), aa = ntk_color_a(a);
    uint8_t rb = ntk_color_r(b), gb = ntk_color_g(b), bb = ntk_color_b(b), ab = ntk_color_a(b);

    return ntk_color_from_rgba(
        (uint8_t)(ra * inv + rb * t + 0.5f),
        (uint8_t)(ga * inv + gb * t + 0.5f),
        (uint8_t)(ba * inv + bb * t + 0.5f),
        (uint8_t)(aa * inv + ab * t + 0.5f)
    );
}

NtkColor ntk_color_lighten(NtkColor c, float amount) {
    amount = clamp_f(amount, 0.0f, 1.0f);
    int r = ntk_color_r(c);
    int g = ntk_color_g(c);
    int b = ntk_color_b(c);

    r = r + (int)((255 - r) * amount);
    g = g + (int)((255 - g) * amount);
    b = b + (int)((255 - b) * amount);

    return ntk_color_from_rgba(clamp_u8(r), clamp_u8(g), clamp_u8(b), ntk_color_a(c));
}

NtkColor ntk_color_darken(NtkColor c, float amount) {
    amount = clamp_f(amount, 0.0f, 1.0f);
    float factor = 1.0f - amount;
    int r = (int)(ntk_color_r(c) * factor);
    int g = (int)(ntk_color_g(c) * factor);
    int b = (int)(ntk_color_b(c) * factor);

    return ntk_color_from_rgba(clamp_u8(r), clamp_u8(g), clamp_u8(b), ntk_color_a(c));
}

NtkColor ntk_color_with_alpha(NtkColor c, uint8_t alpha) {
    return (c & 0x00FFFFFF) | ((uint32_t)alpha << 24);
}
void ntk_color_to_rgb(NtkColor c, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (r) *r = ntk_color_r(c);
    if (g) *g = ntk_color_g(c);
    if (b) *b = ntk_color_b(c);
}

void ntk_color_to_rgba(NtkColor c, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a) {
    if (r) *r = ntk_color_r(c);
    if (g) *g = ntk_color_g(c);
    if (b) *b = ntk_color_b(c);
    if (a) *a = ntk_color_a(c);
}

const char* ntk_color_to_hex(NtkColor c) {
    static char buf[10]; 
    snprintf(buf, sizeof(buf), "#%08X", c);
    return buf;
}
