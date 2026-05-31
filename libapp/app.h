// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "libnovaproto/novaproto.h"
#include "libtheme/theme.h"

typedef struct NovaApp NovaApp;

// Called every time the window needs to be redrawn.
// Use app_draw_* helpers inside this callback.
typedef void (*AppDrawCallback)(NovaApp *app);

// Called when a keyboard key is pressed or released.
typedef void (*AppKeyCallback)(NovaApp *app, uint32_t keycode, uint32_t modifiers, bool pressed);

// Called when the mouse moves or a button state changes.
// x/y are relative to the top-left corner of the content area.
typedef void (*AppPointerCallback)(NovaApp *app, int x, int y, uint32_t buttons);

// Called when the compositor requests the window to close.
// Return true to allow the close, false to cancel it.
typedef bool (*AppCloseCallback)(NovaApp *app);

// Create a new application window with the given title and initial dimensions.
// Loads the system theme automatically. Returns NULL on failure.
NovaApp *app_create(const char *title, uint32_t width, uint32_t height);

// Destroy the application and free all associated resources.
void app_destroy(NovaApp *app);

// Run the main event loop until the window is closed.
// Returns the exit code (0 = clean exit, non-zero = error).
int app_run(NovaApp *app);

// Request that the window be redrawn on the next iteration of the event loop.
// Safe to call from within callbacks.
void app_request_redraw(NovaApp *app);

void app_on_draw(NovaApp *app, AppDrawCallback cb);
void app_on_key(NovaApp *app, AppKeyCallback cb);
void app_on_pointer(NovaApp *app, AppPointerCallback cb);
void app_on_close(NovaApp *app, AppCloseCallback cb);

// Change the window title at runtime.
void app_set_title(NovaApp *app, const char *title);

// Set the window icon path (e.g. "/Library/images/icons/myapp.png").
void app_set_icon(NovaApp *app, const char *icon_path);

// Attach arbitrary user data to the app context for use in callbacks.
void  app_set_userdata(NovaApp *app, void *userdata);
void *app_get_userdata(const NovaApp *app);

// Returns the current content dimensions (updated after resize events).
uint32_t app_width(const NovaApp *app);
uint32_t app_height(const NovaApp *app);

// Returns the active theme (read-only).
const ThemeConfig *app_theme(const NovaApp *app);

// Pre-defined semantic color aliases that map to the active theme.
#define APP_COLOR_TEXT        (0xFFFFFFFF)
#define APP_COLOR_TEXT_DIM    (0xFFA6ADC8)
#define APP_COLOR_ACCENT      (0xFF89DCEB)

// Fill the entire background with the theme's panel color.
void app_clear(NovaApp *app);

// Fill the background with an explicit ARGB color.
void app_clear_color(NovaApp *app, uint32_t color);

// Draw a single-line string at (x, y).
void app_draw_string(NovaApp *app, int x, int y, const char *text, uint32_t color);

// Draw a string horizontally centered at vertical position y.
void app_draw_string_centered(NovaApp *app, int y, const char *text, uint32_t color);

// Measure the rendered pixel width of a string with the active font.
int app_text_width(const char *text);

// Draw a filled, optionally rounded rectangle.
// Use radius=0 for a sharp rectangle.
void app_draw_rect(NovaApp *app, int x, int y, int w, int h,
                   uint32_t fill_color, uint32_t border_color, int radius);

// Blit a raw ARGB pixel buffer (sub-surface) onto the window at (dx, dy).
void app_blit(NovaApp *app, int dx, int dy,
              const uint32_t *src, int src_w, int src_h, float alpha);

// Direct access to the raw pixel buffer (ARGB, row-major).
// Advanced usage only prefer the helpers above.
uint32_t *app_pixels(NovaApp *app);

#endif // APP_H
