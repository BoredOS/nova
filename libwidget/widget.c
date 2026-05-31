// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "widget.h"
#include "libapp/app.h"
#include "libui/ui.h"
#include "libnovaproto/novaproto.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Compile-time limits
// ---------------------------------------------------------------------------

#define WCTX_MAX_TEXTINPUTS   16   // max concurrent text input fields
#define CURSOR_BLINK_FRAMES   30   // frames per cursor half-period

// Default accent: Catppuccin Mocha blue (#89B4FA)
#define DEFAULT_ACCENT        0xFF89B4FAU

// Default corner radii
#define RADIUS_BUTTON         6
#define RADIUS_INPUT          4
#define RADIUS_PANEL          8
#define RADIUS_PROGRESS       4

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

static inline uint32_t col_a(uint32_t c) { return (c >> 24) & 0xFF; }
static inline uint32_t col_r(uint32_t c) { return (c >> 16) & 0xFF; }
static inline uint32_t col_g(uint32_t c) { return (c >>  8) & 0xFF; }
static inline uint32_t col_b(uint32_t c) { return  c        & 0xFF; }

static inline uint32_t col_make(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Brighten or darken RGB channels, keep alpha.
static uint32_t col_shift(uint32_t c, int amt) {
    int r = (int)col_r(c) + amt; if (r < 0) r = 0; if (r > 255) r = 255;
    int g = (int)col_g(c) + amt; if (g < 0) g = 0; if (g > 255) g = 255;
    int b = (int)col_b(c) + amt; if (b < 0) b = 0; if (b > 255) b = 255;
    return col_make(col_a(c), (uint32_t)r, (uint32_t)g, (uint32_t)b);
}

// Replace alpha channel.
static inline uint32_t col_alpha(uint32_t c, uint32_t a) {
    return (c & 0x00FFFFFFU) | (a << 24);
}

// ---------------------------------------------------------------------------
// Text input persistent state (cursor + scroll, keyed by sequential index)
// ---------------------------------------------------------------------------

typedef struct {
    int  cursor;    // byte position of cursor in buffer
    bool in_use;
} TextState;

// ---------------------------------------------------------------------------
// WCtx internal structure
// ---------------------------------------------------------------------------

struct WCtx {
    NovaApp    *app;
    uint32_t    accent;

    // Per-frame widget counters (reset in wctx_begin)
    int         widget_idx;
    int         tinput_idx;

    // Mouse state
    int         mouse_x, mouse_y;
    uint32_t    mouse_cur;   // current button bitmask
    uint32_t    mouse_prev;  // previous frame button bitmask

    // Derived single-frame flags (set in wctx_begin from cur/prev)
    bool        lmb_just_pressed;
    bool        lmb_just_released;
    bool        lmb_held;

    // Hot/active IDs (widget_idx-based)
    int         hot_id;     // widget under cursor
    int         active_id;  // widget being held/dragged
    int         focus_id;   // widget with keyboard focus (text input)

    // Slider drag state
    int         drag_widget_id;
    int         drag_start_x;
    float       drag_start_val;
    float       drag_min, drag_max;
    int         drag_width;

    // Pending key event (set by key callback, consumed in wctx_begin)
    bool        has_key;
    uint32_t    key_code;
    uint32_t    key_mods;

    // Frame tick for cursor blink
    uint32_t    frame_tick;

    // Text input persistent state table
    TextState   tstates[WCTX_MAX_TEXTINPUTS];
};

// ---------------------------------------------------------------------------
// Global context pointer (one per process — BoredOS apps are single-window)
// ---------------------------------------------------------------------------

static WCtx *g_ctx = NULL;

// ---------------------------------------------------------------------------
// Input callbacks (installed by wctx_create)
// ---------------------------------------------------------------------------

static void _cb_pointer(NovaApp *app, int x, int y, uint32_t buttons) {
    if (!g_ctx) return;
    g_ctx->mouse_x   = x;
    g_ctx->mouse_y   = y;
    g_ctx->mouse_cur = buttons;
    app_request_redraw(app);
}

static void _cb_key(NovaApp *app, uint32_t keycode, uint32_t modifiers, bool pressed) {
    if (!g_ctx || !pressed) return;
    g_ctx->has_key   = true;
    g_ctx->key_code  = keycode;
    g_ctx->key_mods  = modifiers;
    app_request_redraw(app);
}

// ---------------------------------------------------------------------------
// Keycode → printable char conversion
// ---------------------------------------------------------------------------

static char _key_to_char(uint32_t kc, uint32_t mods) {
    bool shift = (mods & 0x1) != 0;

    // Letters: KEY_A=1 .. KEY_Z=26
    if (kc >= KEY_A && kc <= KEY_Z) {
        char base = (char)('a' + (int)(kc - KEY_A));
        return shift ? (char)(base - 32) : base;
    }

    // Digits: KEY_0=27 .. KEY_9=36
    if (kc >= KEY_0 && kc <= KEY_9) {
        if (!shift) return (char)('0' + (int)(kc - KEY_0));
        static const char shift_sym[] = ")!@#$%^&*(";
        return shift_sym[kc - KEY_0];
    }

    if (kc == KEY_SPACE)    return ' ';

    return '\0'; // non-printable
}

// ---------------------------------------------------------------------------
// Hit test helper
// ---------------------------------------------------------------------------

static bool _hit(WCtx *ctx, int x, int y, int w, int h) {
    return (ctx->mouse_x >= x && ctx->mouse_x < x + w &&
            ctx->mouse_y >= y && ctx->mouse_y < y + h);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

WCtx *wctx_create(NovaApp *app) {
    WCtx *ctx = calloc(1, sizeof(WCtx));
    if (!ctx) return NULL;
    ctx->app       = app;
    ctx->accent    = DEFAULT_ACCENT;
    ctx->hot_id    = -1;
    ctx->active_id = -1;
    ctx->focus_id  = -1;
    ctx->drag_widget_id = -1;
    g_ctx = ctx;
    app_on_pointer(app, _cb_pointer);
    app_on_key(app, _cb_key);
    return ctx;
}

void wctx_destroy(WCtx *ctx) {
    if (!ctx) return;
    if (g_ctx == ctx) g_ctx = NULL;
    free(ctx);
}

void wctx_set_accent(WCtx *ctx, uint32_t argb) {
    if (ctx) ctx->accent = argb;
}

// ---------------------------------------------------------------------------
// Frame begin/end
// ---------------------------------------------------------------------------

void wctx_begin(WCtx *ctx) {
    if (!ctx) return;

    // Derive single-frame flags
    bool cur = (ctx->mouse_cur  & 0x1) != 0;
    bool prv = (ctx->mouse_prev & 0x1) != 0;
    ctx->lmb_just_pressed  = cur && !prv;
    ctx->lmb_just_released = !cur && prv;
    ctx->lmb_held          = cur;

    // Reset per-frame counters
    ctx->widget_idx = 0;
    ctx->tinput_idx = 0;
    ctx->hot_id     = -1;  // rebuilt each frame by widgets

    ctx->frame_tick++;
}

void wctx_end(WCtx *ctx) {
    if (!ctx) return;

    // Advance mouse state
    ctx->mouse_prev = ctx->mouse_cur;

    // Clear consumed key
    ctx->has_key = false;

    // If mouse released, clear active
    if (ctx->lmb_just_released) {
        ctx->active_id      = -1;
        ctx->drag_widget_id = -1;
    }
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

static void _draw_rect_solid(WCtx *ctx, int x, int y, int w, int h, uint32_t color) {
    uint32_t *px = app_pixels(ctx->app);
    int       sw = (int)app_width(ctx->app);
    int       sh = (int)app_height(ctx->app);
    if (!px) return;
    int x1 = x + w, y1 = y + h;
    if (x  <  0) x  = 0;
    if (y  <  0) y  = 0;
    if (x1 > sw) x1 = sw;
    if (y1 > sh) y1 = sh;
    for (int py = y; py < y1; py++) {
        uint32_t *row = px + py * sw;
        for (int px2 = x; px2 < x1; px2++) row[px2] = color;
    }
}

// ---------------------------------------------------------------------------
// Display-only widgets
// ---------------------------------------------------------------------------

void widget_label(WCtx *ctx, int x, int y, const char *text, uint32_t color) {
    if (!ctx || !text) return;
    ctx->widget_idx++;
    app_draw_string(ctx->app, x, y, text, color);
}

void widget_labelf(WCtx *ctx, int x, int y, uint32_t color, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    widget_label(ctx, x, y, buf, color);
}

void widget_separator(WCtx *ctx, int x, int y, int length, bool horizontal) {
    if (!ctx) return;
    ctx->widget_idx++;
    const ThemeConfig *th = app_theme(ctx->app);
    uint32_t color = col_alpha(th->panel_border, 0xCC);
    if (horizontal)
        _draw_rect_solid(ctx, x, y, length, 1, color);
    else
        _draw_rect_solid(ctx, x, y, 1, length, color);
}

void widget_progressbar(WCtx *ctx, int x, int y, int w, int h, float value) {
    if (!ctx) return;
    ctx->widget_idx++;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    const ThemeConfig *th = app_theme(ctx->app);
    uint32_t track_bg     = col_shift(th->panel_bg, -12);
    uint32_t track_border = th->panel_border;
    uint32_t fill         = ctx->accent;

    // Track
    app_draw_rect(ctx->app, x, y, w, h, track_bg, track_border, RADIUS_PROGRESS);

    // Fill
    int fill_w = (int)((float)(w - 2) * value);
    if (fill_w > 0) {
        app_draw_rect(ctx->app, x + 1, y + 1, fill_w, h - 2,
                      fill, 0x00000000, RADIUS_PROGRESS - 1);
    }
}

void widget_image(WCtx *ctx, int x, int y,
                  const uint32_t *pixels, int pw, int ph, float alpha) {
    if (!ctx || !pixels) return;
    ctx->widget_idx++;
    app_blit(ctx->app, x, y, pixels, pw, ph, alpha);
}

void widget_panel(WCtx *ctx, int x, int y, int w, int h, int radius) {
    if (!ctx) return;
    ctx->widget_idx++;
    const ThemeConfig *th = app_theme(ctx->app);
    uint32_t bg     = col_shift(th->panel_bg, 8);
    uint32_t border = th->panel_border;
    app_draw_rect(ctx->app, x, y, w, h, bg, border, radius);
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

bool widget_button(WCtx *ctx, int x, int y, int w, int h, const char *label) {
    if (!ctx) return false;
    int id = ctx->widget_idx++;

    const ThemeConfig *th = app_theme(ctx->app);
    bool hovered = _hit(ctx, x, y, w, h);

    // Update hot
    if (hovered) ctx->hot_id = id;

    // Update active on press
    if (hovered && ctx->lmb_just_pressed)
        ctx->active_id = id;

    // Detect click: released while still active and hovered
    bool clicked = (ctx->active_id == id && ctx->lmb_just_released && hovered);
    if (clicked) ctx->active_id = -1;

    // Choose colors
    uint32_t bg, border, text_color;
    if (ctx->active_id == id) {
        bg         = col_shift(ctx->accent, -20);
        border     = ctx->accent;
        text_color = 0xFFFFFFFF;
    } else if (hovered) {
        bg         = col_shift(th->panel_bg, 30);
        border     = col_shift(th->panel_border, 20);
        text_color = th->text_primary;
    } else {
        bg         = col_shift(th->panel_bg, 15);
        border     = th->panel_border;
        text_color = th->text_primary;
    }

    app_draw_rect(ctx->app, x, y, w, h, bg, border, RADIUS_BUTTON);

    // Centered label
    int tw = app_text_width(label);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - app_theme(ctx->app)->font_size) / 2;
    app_draw_string(ctx->app, tx, ty, label, text_color);

    return clicked;
}

// ---------------------------------------------------------------------------
// Checkbox
// ---------------------------------------------------------------------------

bool widget_checkbox(WCtx *ctx, int x, int y, const char *label, bool *checked) {
    if (!ctx || !checked) return false;
    int id = ctx->widget_idx++;

    const ThemeConfig *th = app_theme(ctx->app);
    int box_size = th->font_size + 2;

    bool hovered = _hit(ctx, x, y, box_size + app_text_width(label) + 8, box_size);
    if (hovered) ctx->hot_id = id;
    if (hovered && ctx->lmb_just_pressed) ctx->active_id = id;

    bool toggled = (ctx->active_id == id && ctx->lmb_just_released && hovered);
    if (toggled) {
        *checked = !(*checked);
        ctx->active_id = -1;
    }

    // Box
    uint32_t box_bg = *checked
        ? ctx->accent
        : col_shift(th->panel_bg, hovered ? 25 : 10);
    uint32_t box_border = hovered ? col_shift(th->panel_border, 20) : th->panel_border;

    app_draw_rect(ctx->app, x, y, box_size, box_size, box_bg, box_border, 3);

    // Check mark (simple ✓ drawn as two rects)
    if (*checked) {
        int mx = x + 3, my = y + box_size / 2;
        _draw_rect_solid(ctx, mx,     my,     3, 3, 0xFFFFFFFF);
        _draw_rect_solid(ctx, mx + 2, my - 3, 3, 6, 0xFFFFFFFF);
    }

    // Label text
    int ty = y + (box_size - th->font_size) / 2;
    app_draw_string(ctx->app, x + box_size + 8, ty, label, th->text_primary);

    return toggled;
}

// ---------------------------------------------------------------------------
// Radio button
// ---------------------------------------------------------------------------

bool widget_radio(WCtx *ctx, int x, int y, int option_index,
                  int *selected, const char *label) {
    if (!ctx || !selected) return false;
    int id = ctx->widget_idx++;

    const ThemeConfig *th = app_theme(ctx->app);
    int dot_size = th->font_size + 2;

    bool hovered = _hit(ctx, x, y, dot_size + app_text_width(label) + 8, dot_size);
    if (hovered) ctx->hot_id = id;
    if (hovered && ctx->lmb_just_pressed) ctx->active_id = id;

    bool changed = false;
    if (ctx->active_id == id && ctx->lmb_just_released && hovered) {
        *selected = option_index;
        ctx->active_id = -1;
        changed = true;
    }

    bool is_selected = (*selected == option_index);

    // Outer circle (approximated by rounded rect)
    uint32_t ring_color = hovered
        ? col_shift(th->panel_border, 30)
        : th->panel_border;
    uint32_t bg = col_shift(th->panel_bg, 10);
    app_draw_rect(ctx->app, x, y, dot_size, dot_size, bg, ring_color, dot_size / 2);

    // Inner fill when selected
    if (is_selected) {
        int pad = 4;
        app_draw_rect(ctx->app,
                      x + pad, y + pad,
                      dot_size - pad * 2, dot_size - pad * 2,
                      ctx->accent, 0x00000000, (dot_size - pad * 2) / 2);
    }

    // Label
    int ty = y + (dot_size - th->font_size) / 2;
    app_draw_string(ctx->app, x + dot_size + 8, ty, label, th->text_primary);

    return changed;
}

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------

bool widget_slider(WCtx *ctx, int x, int y, int w,
                   float *value, float min_val, float max_val) {
    if (!ctx || !value) return false;
    int id = ctx->widget_idx++;

    const ThemeConfig *th = app_theme(ctx->app);
    int track_h   = 6;
    int thumb_w   = 14;
    int thumb_h   = 22;
    int track_y   = y + (thumb_h - track_h) / 2;
    int total_h   = thumb_h;

    bool hovered = _hit(ctx, x, y, w, total_h);
    if (hovered) ctx->hot_id = id;

    // Start drag
    if (hovered && ctx->lmb_just_pressed) {
        ctx->active_id        = id;
        ctx->drag_widget_id   = id;
        ctx->drag_start_x     = ctx->mouse_x;
        ctx->drag_start_val   = *value;
        ctx->drag_min         = min_val;
        ctx->drag_max         = max_val;
        ctx->drag_width       = w - thumb_w;
    }

    bool changed = false;

    // While dragging
    if (ctx->drag_widget_id == id && ctx->lmb_held) {
        int usable = w - thumb_w;
        if (usable > 0) {
            float norm = (float)(ctx->mouse_x - x - thumb_w / 2) / (float)usable;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            float new_val = min_val + norm * (max_val - min_val);
            if (new_val != *value) {
                *value  = new_val;
                changed = true;
            }
        }
    }

    // Track
    uint32_t track_bg     = col_shift(th->panel_bg, -15);
    uint32_t track_border = th->panel_border;
    app_draw_rect(ctx->app, x, track_y, w, track_h, track_bg, track_border, RADIUS_PROGRESS);

    // Filled portion
    float norm = (*value - min_val) / (max_val - min_val);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    int fill_w = (int)((float)(w - thumb_w) * norm) + thumb_w / 2;
    if (fill_w > 0)
        app_draw_rect(ctx->app, x, track_y, fill_w, track_h,
                      col_alpha(ctx->accent, 0xCC), 0x00000000, RADIUS_PROGRESS);

    // Thumb
    int thumb_x = x + (int)((float)(w - thumb_w) * norm);
    bool thumb_hov = (ctx->drag_widget_id == id) || _hit(ctx, thumb_x, y, thumb_w, thumb_h);
    uint32_t thumb_bg = thumb_hov
        ? ctx->accent
        : col_shift(th->panel_bg, 40);
    uint32_t thumb_border = thumb_hov
        ? col_shift(ctx->accent, 20)
        : col_shift(th->panel_border, 20);
    app_draw_rect(ctx->app, thumb_x, y, thumb_w, thumb_h,
                  thumb_bg, thumb_border, RADIUS_BUTTON);

    return changed;
}

// ---------------------------------------------------------------------------
// Text Input
// ---------------------------------------------------------------------------

bool widget_textinput(WCtx *ctx, int x, int y, int w, int h,
                      char *buf, size_t buf_size, const char *placeholder) {
    if (!ctx || !buf || buf_size == 0) return false;
    int id = ctx->widget_idx++;

    // Get or allocate text state
    int ti = ctx->tinput_idx++;
    if (ti >= WCTX_MAX_TEXTINPUTS) return false;
    TextState *ts = &ctx->tstates[ti];

    // Clamp cursor
    int len = (int)strlen(buf);
    if (ts->cursor > len) ts->cursor = len;

    const ThemeConfig *th = app_theme(ctx->app);
    bool hovered = _hit(ctx, x, y, w, h);
    if (hovered) ctx->hot_id = id;

    // Focus on click
    if (hovered && ctx->lmb_just_pressed)
        ctx->focus_id = id;

    bool focused = (ctx->focus_id == id);

    // Handle key input when focused
    bool entered = false;
    if (focused && ctx->has_key) {
        uint32_t kc   = ctx->key_code;
        uint32_t mods = ctx->key_mods;

        if (kc == KEY_ENTER) {
            entered = true;
        } else if (kc == KEY_BACKSPACE) {
            if (ts->cursor > 0 && len > 0) {
                memmove(buf + ts->cursor - 1, buf + ts->cursor,
                        (size_t)(len - ts->cursor + 1));
                ts->cursor--;
            }
        } else if (kc == KEY_LEFT) {
            if (ts->cursor > 0) ts->cursor--;
        } else if (kc == KEY_RIGHT) {
            if (ts->cursor < len) ts->cursor++;
        } else if (kc == KEY_ESCAPE) {
            ctx->focus_id = -1;
        } else {
            char ch = _key_to_char(kc, mods);
            if (ch && len + 1 < (int)buf_size) {
                memmove(buf + ts->cursor + 1, buf + ts->cursor,
                        (size_t)(len - ts->cursor + 1));
                buf[ts->cursor] = ch;
                ts->cursor++;
            }
        }
        ctx->has_key = false; // consumed
    }

    // Draw box
    uint32_t bg     = col_shift(th->panel_bg, -10);
    uint32_t border = focused
        ? ctx->accent
        : (hovered ? col_shift(th->panel_border, 20) : th->panel_border);
    app_draw_rect(ctx->app, x, y, w, h, bg, border, RADIUS_INPUT);

    // Padding inside box
    int pad = 6;
    int text_x = x + pad;
    int text_y = y + (h - th->font_size) / 2;

    // Draw text or placeholder
    len = (int)strlen(buf);
    if (len == 0 && !focused && placeholder) {
        app_draw_string(ctx->app, text_x, text_y, placeholder,
                        col_alpha(th->text_primary, 0x60));
    } else if (len > 0) {
        // Clip text to box width — simple approach: draw and let ui_draw_string clip
        app_draw_string(ctx->app, text_x, text_y, buf, th->text_primary);
    }

    // Draw cursor (blinking when focused)
    if (focused) {
        bool cursor_vis = ((ctx->frame_tick / CURSOR_BLINK_FRAMES) % 2) == 0;
        if (cursor_vis) {
            // Measure text up to cursor position
            char tmp[256];
            int clen = ts->cursor < 255 ? ts->cursor : 255;
            memcpy(tmp, buf, (size_t)clen);
            tmp[clen] = '\0';
            int cx = text_x + app_text_width(tmp);
            _draw_rect_solid(ctx, cx, y + 4, 1, h - 8, th->text_primary);
        }
    }

    return entered;
}
