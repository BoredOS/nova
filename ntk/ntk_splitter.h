// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_SPLITTER_H
#define NTK_SPLITTER_H

#include "ntk_widget.h"
NtkWidget*  ntk_splitter_new(NtkOrientation orientation, NtkWidget *parent);
void        ntk_splitter_set_widgets(NtkWidget *splitter, NtkWidget *w1, NtkWidget *w2);
void        ntk_splitter_set_position(NtkWidget *splitter, int position);
int         ntk_splitter_get_position(NtkWidget *splitter);

#endif // NTK_SPLITTER_H
