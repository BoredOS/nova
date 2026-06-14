// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TOOLBAR_H
#define NTK_TOOLBAR_H
#include "ntk_widget.h"
#include "ntk_pixmap.h"
NtkWidget*  ntk_toolbar_new(NtkWidget *parent);
void        ntk_toolbar_add_button(NtkWidget *tb, NtkWidget *btn);
void        ntk_toolbar_add_separator(NtkWidget *tb);
NtkWidget*  ntk_tool_button_new(const char *label, NtkPixmap *icon);

#endif // NTK_TOOLBAR_H
