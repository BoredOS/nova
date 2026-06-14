// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_MENU_H
#define NTK_MENU_H
#include "ntk_widget.h"
#include "ntk_pixmap.h"
NtkWidget*  ntk_menu_bar_new(NtkWidget *parent);
void        ntk_menu_bar_add_menu(NtkWidget *mb, NtkWidget *menu, const char *label);
NtkWidget*  ntk_menu_new(void);
void        ntk_menu_add_item(NtkWidget *m, NtkWidget *item);
void        ntk_menu_add_separator(NtkWidget *m);
void        ntk_menu_popup(NtkWidget *m, int x, int y);
void        ntk_menu_close(NtkWidget *m);
NtkWidget*  ntk_menu_item_new(const char *label);
NtkWidget*  ntk_menu_item_new_with_icon(const char *label, NtkPixmap *icon);
void        ntk_menu_item_set_submenu(NtkWidget *item, NtkWidget *submenu);
bool        ntk_menu_item_has_submenu(NtkWidget *item);
NtkWidget*  ntk_menu_item_get_submenu(NtkWidget *item);

#endif // NTK_MENU_H
