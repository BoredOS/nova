// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_COLOR_H
#define NTK_COLOR_H
#include "ntk_types.h"
NtkColor    ntk_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);
NtkColor    ntk_color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
NtkColor    ntk_color_from_hex(const char *hex);
NtkColor    ntk_color_from_hsv(float h, float s, float v);
NtkColor    ntk_color_lerp(NtkColor a, NtkColor b, float t);
NtkColor    ntk_color_lighten(NtkColor c, float amount);
NtkColor    ntk_color_darken(NtkColor c, float amount);
NtkColor    ntk_color_with_alpha(NtkColor c, uint8_t alpha);
void        ntk_color_to_rgb(NtkColor c, uint8_t *r, uint8_t *g, uint8_t *b);
void        ntk_color_to_rgba(NtkColor c, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
const char* ntk_color_to_hex(NtkColor c);
static inline uint8_t ntk_color_r(NtkColor c) { return (uint8_t)((c >> 16) & 0xFF); }
static inline uint8_t ntk_color_g(NtkColor c) { return (uint8_t)((c >>  8) & 0xFF); }
static inline uint8_t ntk_color_b(NtkColor c) { return (uint8_t)( c        & 0xFF); }
static inline uint8_t ntk_color_a(NtkColor c) { return (uint8_t)((c >> 24) & 0xFF); }

#endif // NTK_COLOR_H
