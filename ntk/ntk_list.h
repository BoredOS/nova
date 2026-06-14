// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_LIST_H
#define NTK_LIST_H
#include "ntk_widget.h"
NtkWidget*  ntk_list_box_new(NtkWidget *parent);
void        ntk_list_box_append(NtkWidget *w, const char *text);
void        ntk_list_box_set_selected(NtkWidget *w, int index);
int         ntk_list_box_get_selected(NtkWidget *w);
const char* ntk_list_box_get_item(NtkWidget *w, int index);
int         ntk_list_box_get_count(NtkWidget *w);

#endif // NTK_LIST_H
