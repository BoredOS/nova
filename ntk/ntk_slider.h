// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_SLIDER_H
#define NTK_SLIDER_H
#include "ntk_widget.h"
NtkWidget*  ntk_slider_new(NtkOrientation orientation, NtkWidget *parent);
void        ntk_slider_set_range(NtkWidget *w, int min_val, int max_val);
void        ntk_slider_set_value(NtkWidget *w, int value);
int         ntk_slider_get_value(NtkWidget *w);

#endif // NTK_SLIDER_H
