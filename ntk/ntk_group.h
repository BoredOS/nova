// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_GROUP_H
#define NTK_GROUP_H

#include "ntk_widget.h"
NtkWidget*  ntk_group_box_new(const char *title, NtkWidget *parent);
void        ntk_group_box_set_title(NtkWidget *w, const char *title);
const char* ntk_group_box_get_title(NtkWidget *w);
void        ntk_group_box_set_checkable(NtkWidget *w, bool checkable);
bool        ntk_group_box_is_checkable(NtkWidget *w);
void        ntk_group_box_set_checked(NtkWidget *w, bool checked);
bool        ntk_group_box_is_checked(NtkWidget *w);

#endif // NTK_GROUP_H
