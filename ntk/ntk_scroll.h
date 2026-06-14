// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_SCROLL_H
#define NTK_SCROLL_H

#include "ntk_widget.h"
NtkWidget*  ntk_scroll_area_new(NtkWidget *parent);
void        ntk_scroll_area_set_content(NtkWidget *scroll, NtkWidget *content);
NtkWidget*  ntk_scroll_area_get_content(NtkWidget *scroll);
void        ntk_scroll_area_set_policy(NtkWidget *scroll, NtkScrollPolicy h_policy, NtkScrollPolicy v_policy);
void        ntk_scroll_area_scroll_to(NtkWidget *scroll, int x, int y);
void        ntk_scroll_area_get_offsets(NtkWidget *scroll, int *x_out, int *y_out);
NtkWidget*  ntk_viewport_new(NtkWidget *parent);
NtkWidget*  ntk_scrollbar_new(NtkOrientation orientation, NtkWidget *parent);
void        ntk_scrollbar_set_range(NtkWidget *sb, int min_val, int max_val, int page_size);
void        ntk_scrollbar_set_value(NtkWidget *sb, int value);
int         ntk_scrollbar_get_value(NtkWidget *sb);

#endif 
