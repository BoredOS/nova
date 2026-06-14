// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_LABEL_H
#define NTK_LABEL_H

#include "ntk_widget.h"
NtkWidget*  ntk_label_new(const char *text, NtkWidget *parent);
void        ntk_label_set_text(NtkWidget *w, const char *text);
const char* ntk_label_get_text(NtkWidget *w);
void        ntk_label_set_alignment(NtkWidget *w, NtkAlign align);
void        ntk_label_set_color(NtkWidget *w, NtkColor color);

#endif // NTK_LABEL_H
