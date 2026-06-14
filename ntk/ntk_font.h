// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_FONT_H
#define NTK_FONT_H

#include "ntk_types.h"
NtkFont*        ntk_font_new(const char *family, int size, NtkFontWeight weight, NtkFontStyle style);
NtkFont*        ntk_font_new_from_string(const char *description);
NtkFont*        ntk_font_clone(NtkFont *f);
void            ntk_font_destroy(NtkFont *f);
void            ntk_font_set_family(NtkFont *f, const char *family);
void            ntk_font_set_size(NtkFont *f, int size);
void            ntk_font_set_weight(NtkFont *f, NtkFontWeight weight);
void            ntk_font_set_style(NtkFont *f, NtkFontStyle style);
void            ntk_font_set_underline(NtkFont *f, bool underline);
void            ntk_font_set_strikethrough(NtkFont *f, bool strikethrough);
NtkSize         ntk_font_measure_text(NtkFont *f, const char *text);
int             ntk_font_get_ascent(NtkFont *f);
int             ntk_font_get_descent(NtkFont *f);
int             ntk_font_get_line_height(NtkFont *f);
int             ntk_font_get_cap_height(NtkFont *f);
int             ntk_font_get_x_height(NtkFont *f);
void            ntk_font_ensure_baked(NtkFont *f);
unsigned char*  ntk_font_get_glyph(NtkFont *f, uint32_t codepoint,
                                    int *out_w, int *out_h,
                                    int *out_xoff, int *out_yoff,
                                    int *out_advance);

#endif // NTK_FONT_H
