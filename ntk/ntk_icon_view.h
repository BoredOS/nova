// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_ICON_VIEW_H
#define NTK_ICON_VIEW_H
#include "ntk_widget.h"
#include "ntk_pixmap.h"
NtkWidget*  ntk_icon_view_new(NtkWidget *parent);
void        ntk_icon_view_append(NtkWidget *w, const char *label, NtkPixmap *icon);
void        ntk_icon_view_set_selected(NtkWidget *w, int index);
int         ntk_icon_view_get_selected(NtkWidget *w);

#endif // NTK_ICON_VIEW_H
