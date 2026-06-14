// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TABS_H
#define NTK_TABS_H
#include "ntk_widget.h"
NtkWidget*  ntk_tab_widget_new(NtkWidget *parent);
void        ntk_tab_widget_add_page(NtkWidget *tw, NtkWidget *page, const char *label);
void        ntk_tab_widget_remove_page(NtkWidget *tw, int index);
void        ntk_tab_widget_set_current(NtkWidget *tw, int index);
int         ntk_tab_widget_get_current(NtkWidget *tw);
int         ntk_tab_widget_get_count(NtkWidget *tw);
NtkWidget*  ntk_tab_widget_get_page(NtkWidget *tw, int index);

#endif // NTK_TABS_H
