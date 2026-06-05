// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
// BOREDOS_APP_DESC: Tiny libapp demo
// BOREDOS_APP_ICONS: /Library/images/icons/colloid/application-default-icon.png

// THIS FILE IS A DEMONSTRATION OF HOW TO USE THE LIBAPP API TO CREATE A NOVA APPLICATION.
// IT IS NOT MEANT TO BE A FULL-FEATURED APPLICATION,
// BUT RATHER A STARTING POINT FOR DEVELOPERS WHO WANT TO LEARN HOW TO USE LIBAPP.
// FEEL FREE TO MODIFY AND EXPERIMENT WITH THIS CODE TO SEE HOW THE API WORKS!

#include "libapp/app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BG      0xFF202328
#define PANEL   0xFF2B3038
#define BORDER  0xFF454B55
#define TEXT    0xFFEDEFF2
#define MUTED   0xFFA7ADB7
#define ACCENT  0xFF3584E4

typedef struct {
    int clicks;
    uint32_t last_buttons;
} DemoState;

static bool hit_button(int x, int y) {
    return x >= 24 && x < 168 && y >= 92 && y < 128;
}

static void on_draw(NovaApp *app) {
    DemoState *st = app_get_userdata(app);
    char count[48];

    app_clear_color(app, BG);
    app_draw_rect(app, 14, 14, 292, 154, PANEL, BORDER, 8);
    app_draw_string(app, 24, 26, "libapp demo", TEXT);
    app_draw_string(app, 24, 50, "Une fenetre Nova simple.", MUTED);

    snprintf(count, sizeof(count), "Clicks: %d", st ? st->clicks : 0);
    app_draw_string(app, 24, 66, count, MUTED);

    app_draw_rect(app, 24, 92, 144, 36, ACCENT, ACCENT, 6);
    app_draw_string(app, 58, 104, "Click", TEXT);
}

static void on_pointer(NovaApp *app, int x, int y, uint32_t buttons) {
    DemoState *st = app_get_userdata(app);
    if (!st) return;

    bool left_down = (buttons & 1) != 0;
    bool was_left_down = (st->last_buttons & 1) != 0;
    if (left_down && !was_left_down && hit_button(x, y)) {
        st->clicks++;
        app_request_redraw_rect(app, 20, 60, 180, 76);
    }

    st->last_buttons = buttons;
}

int main(void) {
    NovaApp *app = app_create("libapp demo", 320, 182);
    if (!app) return 1;

    DemoState state = {0};
    app_set_userdata(app, &state);
    app_set_icon(app, "/Library/images/icons/colloid/application-default-icon.png");
    app_on_draw(app, on_draw);
    app_on_pointer(app, on_pointer);

    int rc = app_run(app);
    app_destroy(app);
    return rc;
}