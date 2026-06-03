// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef WIDGET_H
#define WIDGET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct NovaApp NovaApp;
typedef struct WCtx WCtx;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Create a widget context attached to an app.
// This installs pointer/key handlers internally — do not override them after.
WCtx *wctx_create(NovaApp *app);
void  wctx_destroy(WCtx *ctx);

// Override the default accent color (default: 0xFF89B4FA — soft blue).
void wctx_set_accent(WCtx *ctx, uint32_t argb);

// Call at the start of your AppDrawCallback before any widget_* call.
void wctx_begin(WCtx *ctx);

// Call at the end of your AppDrawCallback after all widget_* calls.
void wctx_end(WCtx *ctx);

// ---------------------------------------------------------------------------
// Display-only widgets
// ---------------------------------------------------------------------------

// Single-line text label.
void widget_label(WCtx *ctx, int x, int y, const char *text, uint32_t color);

// printf-style formatted label.
void widget_labelf(WCtx *ctx, int x, int y, uint32_t color, const char *fmt, ...);

// Horizontal (horizontal=true) or vertical separator line.
void widget_separator(WCtx *ctx, int x, int y, int length, bool horizontal);

// Filled progress bar; value in [0.0, 1.0].
void widget_progressbar(WCtx *ctx, int x, int y, int w, int h, float value);

// Pixel image blit with alpha blending.
void widget_image(WCtx *ctx, int x, int y,
                  const uint32_t *pixels, int pw, int ph, float alpha);

// Decorative rounded panel (visual grouping box).
void widget_panel(WCtx *ctx, int x, int y, int w, int h, int radius);

// ---------------------------------------------------------------------------
// Interactive widgets
// Return true when the widget is activated this frame.
// ---------------------------------------------------------------------------

// Push-button: returns true on click (mouse released over widget).
bool widget_button(WCtx *ctx, int x, int y, int w, int h, const char *label);

// Checkbox: toggles *checked, returns true on toggle.
bool widget_checkbox(WCtx *ctx, int x, int y, const char *label, bool *checked);

// Radio button: call once per option; sets *selected = option_index on click.
// Returns true when this option becomes selected.
bool widget_radio(WCtx *ctx, int x, int y, int option_index,
                  int *selected, const char *label);

// Horizontal slider: modifies *value ∈ [min_val, max_val]. Returns true on change.
bool widget_slider(WCtx *ctx, int x, int y, int w,
                   float *value, float min_val, float max_val);

// Single-line text input field. Returns true when Enter is pressed.
// placeholder shown when buf is empty.
bool widget_textinput(WCtx *ctx, int x, int y, int w, int h,
                      char *buf, size_t buf_size, const char *placeholder);

#endif // WIDGET_H
