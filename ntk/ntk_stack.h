// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_STACK_H
#define NTK_STACK_H
#include "ntk_widget.h"
NtkWidget*  ntk_stack_new(NtkWidget *parent);
void        ntk_stack_add_page(NtkWidget *stack, NtkWidget *page, const char *id);
void        ntk_stack_set_visible_page(NtkWidget *stack, const char *id);
NtkWidget*  ntk_stack_get_visible_page(NtkWidget *stack);

#endif // NTK_STACK_H
