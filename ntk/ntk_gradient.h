// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_GRADIENT_H
#define NTK_GRADIENT_H

#include "ntk_types.h"
#include "ntk_color.h"
NtkGradient*    ntk_gradient_new_linear(NtkPoint start, NtkPoint end);
NtkGradient*    ntk_gradient_new_radial(NtkPoint center, float radius);
void            ntk_gradient_destroy(NtkGradient *g);
NtkGradient*    ntk_gradient_clone(NtkGradient *g);
void            ntk_gradient_add_stop(NtkGradient *g, float position, NtkColor color);
void            ntk_gradient_clear_stops(NtkGradient *g);
void            ntk_gradient_set_spread(NtkGradient *g, NtkGradientSpread spread);
NtkColor        ntk_gradient_sample(NtkGradient *g, float t);

#endif // NTK_GRADIENT_H
