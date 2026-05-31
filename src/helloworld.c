// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: A simple GUI Hello World application
// BOREDOS_APP_ICONS: /Library/images/icons/colloid/application-default-icon.png

#include "libapp/app.h"
#include "libwidget/widget.h"

typedef struct {
    WCtx  *ctx;

    bool   feature_enabled;
    bool   dark_mode;
    int    theme_choice;
    float  brightness;
    char   username[64];
    int    click_count;
} AppState;


static void on_draw(NovaApp *app) {
    AppState *st = app_get_userdata(app);
    int w = (int)app_width(app);

    wctx_begin(st->ctx);

    widget_label(st->ctx, 20, 16, "Hello, BoredOS!", 0xFFFFFFFF); // ik there is the title window, but this is just for demo purposes
    widget_separator(st->ctx, 20, 36, w - 40, true);

    widget_panel(st->ctx, 16, 48, w - 32, 110, 8);
    widget_label(st->ctx, 28, 58, "Features", 0xFFA6ADC8);

    if (widget_checkbox(st->ctx, 28, 80, "Enable feature", &st->feature_enabled))
        app_request_redraw(app);

    if (widget_checkbox(st->ctx, 28, 106, "Dark mode overlay", &st->dark_mode))
        app_request_redraw(app);

    widget_label(st->ctx, 28, 132, "Theme:", 0xFFA6ADC8);
    widget_radio(st->ctx, 90,  132, 0, &st->theme_choice, "Blue");
    widget_radio(st->ctx, 155, 132, 1, &st->theme_choice, "Purple");
    widget_radio(st->ctx, 225, 132, 2, &st->theme_choice, "Green");

    // Update accent based on theme choice
    static const uint32_t accents[3] = {0xFF89B4FA, 0xFFCBA6F7, 0xFFA6E3A1};
    wctx_set_accent(st->ctx, accents[st->theme_choice]);

    widget_panel(st->ctx, 16, 170, w - 32, 66, 8);
    widget_label(st->ctx, 28, 180, "Name", 0xFFA6ADC8);
    if (widget_textinput(st->ctx, 28, 198, w - 56, 28,
                         st->username, sizeof(st->username),
                         "Type your name…")) {
    }

    widget_panel(st->ctx, 16, 248, w - 32, 56, 8);
    widget_labelf(st->ctx, 28, 258, 0xFFA6ADC8,
                  "Brightness: %.0f%%", st->brightness * 100.0f);
    widget_slider(st->ctx, 28, 278, w - 56, &st->brightness, 0.0f, 1.0f);

    widget_label(st->ctx, 20, 316, "Progress", 0xFFA6ADC8);
    widget_progressbar(st->ctx, 20, 334, w - 40, 14, st->brightness);

    if (widget_button(st->ctx, 20, 360, 120, 32, "Click me!")) {
        st->click_count++;
        app_request_redraw(app);
    }

    widget_labelf(st->ctx, 152, 370, 0xFFA6ADC8, "Count: %d", st->click_count);

    if (widget_button(st->ctx, w - 100, 360, 80, 32, "Reset")) {
        st->click_count = 0;
        st->brightness  = 0.5f;
        app_request_redraw(app);
    }

    wctx_end(st->ctx);
}

int main(void) {
    NovaApp *app = app_create("Hello, BoredOS!", 340, 406);
    if (!app) return 1;

    AppState st = {0};
    st.brightness  = 0.5f;
    st.theme_choice = 0;
    st.ctx = wctx_create(app);
    if (!st.ctx) {
        app_destroy(app);
        return 1;
    }

    app_set_userdata(app, &st);
    app_on_draw(app, on_draw);

    int rc = app_run(app);

    wctx_destroy(st.ctx);
    app_destroy(app);
    return rc;
}
