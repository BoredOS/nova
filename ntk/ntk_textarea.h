// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TEXTAREA_H
#define NTK_TEXTAREA_H

#include "ntk_widget.h"
NtkWidget*  ntk_text_area_new(NtkWidget *parent);
void        ntk_text_area_set_text(NtkWidget *w, const char *text);
const char* ntk_text_area_get_text(NtkWidget *w);
void        ntk_text_area_append(NtkWidget *w, const char *text);

#endif // NTK_TEXTAREA_H
