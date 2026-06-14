// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_PIXMAP_H
#define NTK_PIXMAP_H

#include "ntk_types.h"
#include "ntk_color.h"
NtkPixmap*      ntk_pixmap_new(int width, int height);
NtkPixmap*      ntk_pixmap_new_from_file(const char *path);
NtkPixmap*      ntk_pixmap_new_from_data(unsigned char *data, int width, int height, int stride, NtkPixelFormat format);
NtkPixmap*      ntk_pixmap_clone(NtkPixmap *pm);
void            ntk_pixmap_destroy(NtkPixmap *pm);
int             ntk_pixmap_get_width(NtkPixmap *pm);
int             ntk_pixmap_get_height(NtkPixmap *pm);
NtkSize         ntk_pixmap_get_size(NtkPixmap *pm);
NtkPixmap*      ntk_pixmap_scale(NtkPixmap *pm, NtkSize size, NtkScaleMode mode);
NtkPixmap*      ntk_pixmap_crop(NtkPixmap *pm, NtkRect rect);
void            ntk_pixmap_set_pixel(NtkPixmap *pm, int x, int y, NtkColor color);
NtkColor        ntk_pixmap_get_pixel(NtkPixmap *pm, int x, int y);
unsigned char*  ntk_pixmap_get_data(NtkPixmap *pm);
void            ntk_pixmap_fill(NtkPixmap *pm, NtkColor color);

#endif // NTK_PIXMAP_H
