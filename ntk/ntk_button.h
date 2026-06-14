// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_BUTTON_H
#define NTK_BUTTON_H

#include "ntk_widget.h"
#include "ntk_pixmap.h"

// Button Lifecycle
NtkWidget*  ntk_button_new(const char *label, NtkWidget *parent);
NtkWidget*  ntk_button_new_with_icon(const char *label, NtkPixmap *icon, NtkWidget *parent);

// Button Operations
void        ntk_button_set_label(NtkWidget *w, const char *label);
const char* ntk_button_get_label(NtkWidget *w);
void        ntk_button_set_icon(NtkWidget *w, NtkPixmap *icon);
NtkPixmap*  ntk_button_get_icon(NtkWidget *w);
void        ntk_button_set_flat(NtkWidget *w, bool flat);
bool        ntk_button_is_flat(NtkWidget *w);

// Toggle Button Lifecycle
NtkWidget*  ntk_toggle_button_new(const char *label, NtkWidget *parent);

// Toggle Button Operations
void        ntk_toggle_button_set_active(NtkWidget *w, bool active);
bool        ntk_toggle_button_is_active(NtkWidget *w);

#endif // NTK_BUTTON_H
