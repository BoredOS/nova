// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_STYLE_H
#define NTK_STYLE_H

#include "ntk_types.h"
#include "ntk_color.h"
#include "ntk_font.h"
#include "ntk_pixmap.h"
#include "ntk_gradient.h"
NtkStyle*       ntk_style_new(void);
void            ntk_style_destroy(NtkStyle *s);
NtkStyle*       ntk_style_new_from_file(const char *path);
void            ntk_style_set_color(NtkStyle *s, NtkStyleRole role, NtkColor color);
NtkColor        ntk_style_get_color(NtkStyle *s, NtkStyleRole role);
void            ntk_style_set_font(NtkStyle *s, NtkStyleElement element, NtkFont *font);
NtkFont*        ntk_style_get_font(NtkStyle *s, NtkStyleElement element);
void            ntk_style_set_metric(NtkStyle *s, NtkStyleMetric metric, int value);
int             ntk_style_get_metric(NtkStyle *s, NtkStyleMetric metric);
void            ntk_style_set_pixmap(NtkStyle *s, NtkStylePixmap role, NtkPixmap *pixmap);
NtkPixmap*      ntk_style_get_pixmap(NtkStyle *s, NtkStylePixmap role);
void            ntk_style_set_gradient(NtkStyle *s, NtkStyleGradientRole role, NtkGradient *gradient);
NtkGradient*    ntk_style_get_gradient(NtkStyle *s, NtkStyleGradientRole role);
void            ntk_style_apply(NtkStyle *s);                          // apply globally
void            ntk_style_apply_to(NtkStyle *s, NtkWidget *w);         // apply to subtree
NtkStyle*       ntk_style_get_global(void);
void            ntk_style_inherit(NtkStyle *child, NtkStyle *parent);
NtkColor        ntk_style_resolve_color(const char *hex_str, NtkColor fallback);

#endif // NTK_STYLE_H
