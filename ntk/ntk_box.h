// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_BOX_H
#define NTK_BOX_H

#include "ntk_widget.h"

// Lifecycle
NtkWidget*  ntk_box_new(NtkOrientation orientation, NtkWidget *parent);

// Box properties
void        ntk_box_set_spacing(NtkWidget *w, int spacing);
int         ntk_box_get_spacing(NtkWidget *w);
void        ntk_box_set_homogeneous(NtkWidget *w, bool homogeneous);
bool        ntk_box_is_homogeneous(NtkWidget *w);

// Packing
void        ntk_box_pack_start(NtkWidget *box, NtkWidget *child, bool expand, bool fill, int padding);
void        ntk_box_pack_end(NtkWidget *box, NtkWidget *child, bool expand, bool fill, int padding);

#endif // NTK_BOX_H
