// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_PAINTER_H
#define NTK_PAINTER_H

#include "ntk_types.h"
#include "ntk_color.h"
#include "ntk_font.h"
#include "ntk_pixmap.h"
#include "ntk_gradient.h"
NtkPainter*     ntk_painter_new(NtkWidget *target);
NtkPainter*     ntk_painter_new_from_buffer(uint32_t *buffer, int width, int height);
void            ntk_painter_destroy(NtkPainter *p);
void            ntk_painter_set_color(NtkPainter *p, NtkColor color);
void            ntk_painter_set_stroke_width(NtkPainter *p, float width);
void            ntk_painter_set_font(NtkPainter *p, NtkFont *font);
void            ntk_painter_set_clip_rect(NtkPainter *p, NtkRect rect);
void            ntk_painter_clear_clip(NtkPainter *p);
bool            ntk_painter_has_clip(NtkPainter *p);
NtkRect         ntk_painter_get_clip_rect(NtkPainter *p);
void            ntk_painter_set_opacity(NtkPainter *p, float opacity);
void            ntk_painter_push_transform(NtkPainter *p, NtkTransform *t);
void            ntk_painter_pop_transform(NtkPainter *p);
void            ntk_painter_draw_line(NtkPainter *p, int x1, int y1, int x2, int y2);
void            ntk_painter_draw_rect(NtkPainter *p, NtkRect rect);
void            ntk_painter_fill_rect(NtkPainter *p, NtkRect rect);
void            ntk_painter_draw_rounded_rect(NtkPainter *p, NtkRect rect, int radius);
void            ntk_painter_fill_rounded_rect(NtkPainter *p, NtkRect rect, int radius);
void            ntk_painter_draw_ellipse(NtkPainter *p, NtkRect bounds);
void            ntk_painter_fill_ellipse(NtkPainter *p, NtkRect bounds);
void            ntk_painter_draw_circle(NtkPainter *p, int cx, int cy, int r);
void            ntk_painter_fill_circle(NtkPainter *p, int cx, int cy, int r);
void            ntk_painter_draw_arc(NtkPainter *p, NtkRect bounds, float start_angle, float span_angle);
void            ntk_painter_draw_polygon(NtkPainter *p, NtkPoint *points, int count);
void            ntk_painter_fill_polygon(NtkPainter *p, NtkPoint *points, int count);
void            ntk_painter_draw_pixmap(NtkPainter *p, NtkPixmap *pm, int x, int y);
void            ntk_painter_draw_pixmap_scaled(NtkPainter *p, NtkPixmap *pm, NtkRect dest);
void            ntk_painter_draw_text(NtkPainter *p, const char *text, int x, int y);
void            ntk_painter_draw_text_rect(NtkPainter *p, const char *text, NtkRect rect, NtkAlign align);
void            ntk_painter_draw_gradient(NtkPainter *p, NtkGradient *g, NtkRect rect);
void            ntk_painter_draw_bevel_raised(NtkPainter *p, NtkRect rect, NtkColor light, NtkColor dark);
void            ntk_painter_draw_bevel_sunken(NtkPainter *p, NtkRect rect, NtkColor light, NtkColor dark);
void            ntk_painter_blend_buffer(NtkPainter *p, int dx, int dy,
                                          const uint32_t *src, int src_w, int src_h,
                                          float alpha);
void            ntk_painter_clear(NtkPainter *p, NtkColor color);

#endif // NTK_PAINTER_H
