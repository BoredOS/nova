// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_STATUSBAR_H
#define NTK_STATUSBAR_H
#include "ntk_widget.h"
NtkWidget*  ntk_statusbar_new(NtkWidget *parent);
void        ntk_statusbar_push(NtkWidget *sb, const char *text);
void        ntk_statusbar_pop(NtkWidget *sb);
void        ntk_statusbar_set_text(NtkWidget *sb, const char *text);

#endif // NTK_STATUSBAR_H
