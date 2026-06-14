// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_DIALOG_H
#define NTK_DIALOG_H

#include "ntk_widget.h"

// Message Dialogs
NtkDialogResult ntk_dialog_message(NtkWidget *parent, const char *title, const char *message, NtkMessageType type, NtkDialogButtons buttons);
bool            ntk_dialog_question(NtkWidget *parent, const char *title, const char *message);
void            ntk_dialog_error(NtkWidget *parent, const char *title, const char *message);
void            ntk_dialog_warning(NtkWidget *parent, const char *title, const char *message);

// Input Dialogs
char*           ntk_dialog_get_text(NtkWidget *parent, const char *title, const char *label, const char *initial_value);
int             ntk_dialog_get_integer(NtkWidget *parent, const char *title, const char *label, int initial_value, int min_val, int max_val, bool *ok);
double          ntk_dialog_get_double(NtkWidget *parent, const char *title, const char *label, double initial_value, double min_val, double max_val, bool *ok);
int             ntk_dialog_get_item(NtkWidget *parent, const char *title, const char *label, const char **items, int item_count, int initial_index, bool *ok);

// File Dialogs
char*           ntk_dialog_open_file(NtkWidget *parent, const char *title, const char *initial_path, const char *filter);
char*           ntk_dialog_save_file(NtkWidget *parent, const char *title, const char *initial_path, const char *filter);
char*           ntk_dialog_open_directory(NtkWidget *parent, const char *title, const char *initial_path);

// Pickers
NtkColor        ntk_dialog_pick_color(NtkWidget *parent, const char *title, NtkColor initial_color, bool *ok);
NtkFont*        ntk_dialog_pick_font(NtkWidget *parent, const char *title, NtkFont *initial_font, bool *ok);

#endif // NTK_DIALOG_H
