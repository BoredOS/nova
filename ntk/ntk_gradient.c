// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_gradient.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define NTK_MAX_GRADIENT_STOPS 16

typedef struct {
    float    position;   
    NtkColor color;
} NtkGradientStop;

typedef enum {
    NTK_GRADIENT_LINEAR,
    NTK_GRADIENT_RADIAL
} NtkGradientType;

struct NtkGradient {
    NtkGradientType   type;
    NtkGradientSpread spread;
    NtkPoint          start;
    NtkPoint          end;
    NtkPoint          center;
    float             radius;
    NtkGradientStop   stops[NTK_MAX_GRADIENT_STOPS];
    int               stop_count;
};
NtkGradient* ntk_gradient_new_linear(NtkPoint start, NtkPoint end) {
    NtkGradient *g = calloc(1, sizeof(NtkGradient));
    if (!g) return NULL;
    g->type   = NTK_GRADIENT_LINEAR;
    g->start  = start;
    g->end    = end;
    g->spread = NTK_GRADIENT_PAD;
    return g;
}

NtkGradient* ntk_gradient_new_radial(NtkPoint center, float radius) {
    NtkGradient *g = calloc(1, sizeof(NtkGradient));
    if (!g) return NULL;
    g->type   = NTK_GRADIENT_RADIAL;
    g->center = center;
    g->radius = radius;
    g->spread = NTK_GRADIENT_PAD;
    return g;
}

void ntk_gradient_destroy(NtkGradient *g) {
    free(g);
}

NtkGradient* ntk_gradient_clone(NtkGradient *g) {
    if (!g) return NULL;
    NtkGradient *clone = malloc(sizeof(NtkGradient));
    if (!clone) return NULL;
    memcpy(clone, g, sizeof(NtkGradient));
    return clone;
}
void ntk_gradient_add_stop(NtkGradient *g, float position, NtkColor color) {
    if (!g || g->stop_count >= NTK_MAX_GRADIENT_STOPS) return;

    int idx = g->stop_count;
    for (int i = 0; i < g->stop_count; i++) {
        if (position < g->stops[i].position) {
            idx = i;
            break;
        }
    }

    for (int i = g->stop_count; i > idx; i--) {
        g->stops[i] = g->stops[i - 1];
    }

    g->stops[idx].position = position;
    g->stops[idx].color    = color;
    g->stop_count++;
}

void ntk_gradient_clear_stops(NtkGradient *g) {
    if (g) g->stop_count = 0;
}
void ntk_gradient_set_spread(NtkGradient *g, NtkGradientSpread spread) {
    if (g) g->spread = spread;
}
NtkColor ntk_gradient_sample(NtkGradient *g, float t) {
    if (!g || g->stop_count == 0) return 0xFF000000;
    switch (g->spread) {
        case NTK_GRADIENT_PAD:
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            break;
        case NTK_GRADIENT_REPEAT:
            t = t - (float)((int)t);
            if (t < 0.0f) t += 1.0f;
            break;
        case NTK_GRADIENT_REFLECT: {
            int period = (int)t;
            t = t - (float)period;
            if (t < 0.0f) { t = -t; period--; }
            if (period % 2 != 0) t = 1.0f - t;
            break;
        }
    }

    if (g->stop_count == 1) return g->stops[0].color;

    if (t <= g->stops[0].position) return g->stops[0].color;
    if (t >= g->stops[g->stop_count - 1].position) return g->stops[g->stop_count - 1].color;

    for (int i = 0; i < g->stop_count - 1; i++) {
        if (t >= g->stops[i].position && t <= g->stops[i + 1].position) {
            float range = g->stops[i + 1].position - g->stops[i].position;
            if (range < 0.0001f) return g->stops[i].color;
            float local_t = (t - g->stops[i].position) / range;
            return ntk_color_lerp(g->stops[i].color, g->stops[i + 1].color, local_t);
        }
    }

    return g->stops[g->stop_count - 1].color;
}
