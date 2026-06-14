// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_CHECK_H
#define NTK_CHECK_H

#include "ntk_widget.h"

// CheckBox Lifecycle
NtkWidget*  ntk_checkbox_new(const char *label, NtkWidget *parent);

// CheckBox Operations
void        ntk_checkbox_set_checked(NtkWidget *w, bool checked);
bool        ntk_checkbox_is_checked(NtkWidget *w);
void        ntk_checkbox_set_tristate(NtkWidget *w, bool tristate);
bool        ntk_checkbox_is_tristate(NtkWidget *w);
void        ntk_checkbox_set_state(NtkWidget *w, NtkCheckState state);
NtkCheckState ntk_checkbox_get_state(NtkWidget *w);

// RadioButton Lifecycle
NtkWidget*  ntk_radio_button_new(const char *label, NtkWidget *parent);

// RadioButton Operations
void        ntk_radio_button_set_selected(NtkWidget *w, bool selected);
bool        ntk_radio_button_is_selected(NtkWidget *w);

// RadioGroup Lifecycle & Operations
NtkRadioGroup* ntk_radio_group_new(void);
void           ntk_radio_group_destroy(NtkRadioGroup *g);
void           ntk_radio_group_add(NtkRadioGroup *g, NtkWidget *rb);
void           ntk_radio_group_set_selected(NtkRadioGroup *g, int index);
int            ntk_radio_group_get_selected(NtkRadioGroup *g);

#endif // NTK_CHECK_H
