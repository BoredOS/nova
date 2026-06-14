// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_PROGRESS_H
#define NTK_PROGRESS_H

#include "ntk_widget.h"
NtkWidget*  ntk_progress_bar_new(NtkWidget *parent);
void        ntk_progress_bar_set_value(NtkWidget *w, float value);
float       ntk_progress_bar_get_value(NtkWidget *w);
void        ntk_progress_bar_set_text(NtkWidget *w, const char *text);
const char* ntk_progress_bar_get_text(NtkWidget *w);

#endif // NTK_PROGRESS_H
