// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TABLE_H
#define NTK_TABLE_H
#include "ntk_widget.h"
NtkWidget*  ntk_table_view_new(NtkWidget *parent);
void        ntk_table_view_set_columns(NtkWidget *w, int cols, const char **headers, int *widths);
void        ntk_table_view_set_cell(NtkWidget *w, int row, int col, const char *text);
const char* ntk_table_view_get_cell(NtkWidget *w, int row, int col);
void        ntk_table_view_set_row_count(NtkWidget *w, int count);

#endif // NTK_TABLE_H
