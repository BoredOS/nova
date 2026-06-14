// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_GRID_H
#define NTK_GRID_H

#include "ntk_widget.h"
NtkWidget*  ntk_grid_new(NtkWidget *parent);
void        ntk_grid_set_column_spacing(NtkWidget *w, int spacing);
void        ntk_grid_set_row_spacing(NtkWidget *w, int spacing);
void        ntk_grid_set_column_homogeneous(NtkWidget *w, bool homogeneous);
void        ntk_grid_set_row_homogeneous(NtkWidget *w, bool homogeneous);
void        ntk_grid_attach(NtkWidget *grid, NtkWidget *child, int left, int top, int width, int height);

#endif // NTK_GRID_H
