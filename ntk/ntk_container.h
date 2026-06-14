// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_CONTAINER_H
#define NTK_CONTAINER_H
#include "ntk_widget.h"
NtkWidget*  ntk_container_new(NtkWidget *parent);
void        ntk_container_layout_children(NtkWidget *w);

#endif // NTK_CONTAINER_H
