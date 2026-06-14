// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_IMAGE_H
#define NTK_IMAGE_H
#include "ntk_widget.h"
#include "ntk_pixmap.h"
NtkWidget*  ntk_image_new(NtkWidget *parent);
NtkWidget*  ntk_image_new_from_file(const char *path, NtkWidget *parent);
void        ntk_image_set_pixmap(NtkWidget *w, NtkPixmap *pm);
NtkPixmap*  ntk_image_get_pixmap(NtkWidget *w);
void        ntk_image_set_scale_mode(NtkWidget *w, NtkScaleMode mode);
NtkScaleMode ntk_image_get_scale_mode(NtkWidget *w);

#endif // NTK_IMAGE_H
