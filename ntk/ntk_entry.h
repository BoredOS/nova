// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_ENTRY_H
#define NTK_ENTRY_H

#include "ntk_widget.h"

// Lifecycle
NtkWidget*  ntk_text_entry_new(NtkWidget *parent);

// Operations
void        ntk_text_entry_set_text(NtkWidget *w, const char *text);
const char* ntk_text_entry_get_text(NtkWidget *w);
void        ntk_text_entry_set_placeholder(NtkWidget *w, const char *text);
const char* ntk_text_entry_get_placeholder(NtkWidget *w);
void        ntk_text_entry_set_read_only(NtkWidget *w, bool read_only);
bool        ntk_text_entry_is_read_only(NtkWidget *w);
void        ntk_text_entry_set_password_mode(NtkWidget *w, bool password);
bool        ntk_text_entry_is_password_mode(NtkWidget *w);

#endif // NTK_ENTRY_H
