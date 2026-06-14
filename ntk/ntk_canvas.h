// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_CANVAS_H
#define NTK_CANVAS_H

#include "ntk_widget.h"

// Lifecycle
NtkWidget*  ntk_canvas_new(NtkWidget *parent);

// Operations
void        ntk_canvas_set_draw_callback(NtkWidget *w, NtkDrawCallback cb, void *userdata);

#endif // NTK_CANVAS_H
