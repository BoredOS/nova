// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: Shows BoredOS information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "libapp/app.h"
#include "libwidget/widget.h"
#include "stb_image.h"

#define BRANDING_PATH     "/Library/images/branding/bOS_full_gradient_cropped.png"
#define BRANDING_TARGET_W 350
#define BRANDING_FALLBACK_H 88
#define OFFSET_X          35
#define OFFSET_Y          15
#define LINE_H            17

typedef struct {
    WCtx     *ctx;
    uint32_t *branding_pixels;
    int       branding_w;
    int       branding_h;

    // System info
    char      os_name[128];
    char      os_version[128];
    char      kernel_version[128];
    char      build_date[128];
} AboutState;

static bool load_file_to_buffer(const char *path, unsigned char **out_buf, size_t *out_size) {
    if (!path || !out_buf || !out_size) return false;
    *out_buf = NULL;
    *out_size = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size <= 0 || size > 16 * 1024 * 1024) {
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    unsigned char *buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_size = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read_size != (size_t)size) {
        free(buf);
        return false;
    }

    *out_buf = buf;
    *out_size = (size_t)size;
    return true;
}

static void scale_rgba_to_argb(const unsigned char *rgba,
                                int src_w, int src_h,
                                uint32_t *dst, int dst_w, int dst_h) {
    if (src_w == dst_w && src_h == dst_h) {
        for (int i = 0; i < dst_w * dst_h; i++) {
            int idx = i * 4;
            dst[i] = ((uint32_t)rgba[idx + 3] << 24) |
                     ((uint32_t)rgba[idx    ] << 16) |
                     ((uint32_t)rgba[idx + 1] <<  8) |
                      (uint32_t)rgba[idx + 2];
        }
        return;
    }

    uint32_t step_x = ((uint32_t)src_w << 16) / (uint32_t)dst_w;
    uint32_t step_y = ((uint32_t)src_h << 16) / (uint32_t)dst_h;
    uint32_t curr_y = 0;

    for (int y = 0; y < dst_h; y++) {
        uint32_t sy = curr_y >> 16;
        if (sy >= (uint32_t)src_h) sy = (uint32_t)(src_h - 1);
        uint32_t curr_x = 0;
        for (int x = 0; x < dst_w; x++) {
            uint32_t sx = curr_x >> 16;
            if (sx >= (uint32_t)src_w) sx = (uint32_t)(src_w - 1);
            int idx = ((int)(sy * (uint32_t)src_w + sx)) * 4;
            dst[y * dst_w + x] = ((uint32_t)rgba[idx + 3] << 24) |
                                  ((uint32_t)rgba[idx    ] << 16) |
                                  ((uint32_t)rgba[idx + 1] <<  8) |
                                   (uint32_t)rgba[idx + 2];
            curr_x += step_x;
        }
        curr_y += step_y;
    }
}

static void load_branding_image(AboutState *st) {
    st->branding_w = BRANDING_TARGET_W;
    st->branding_h = BRANDING_FALLBACK_H;

    unsigned char *buf = NULL;
    size_t size = 0;
    if (!load_file_to_buffer(BRANDING_PATH, &buf, &size)) return;

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char *rgba = stbi_load_from_memory(buf, (int)size,
                                                &img_w, &img_h, &channels, 4);
    free(buf);
    if (!rgba || img_w <= 0 || img_h <= 0) {
        if (rgba) stbi_image_free(rgba);
        return;
    }

    int target_h = (int)(((int64_t)img_h * BRANDING_TARGET_W) / img_w);
    if (target_h <= 0) {
        stbi_image_free(rgba);
        return;
    }
    st->branding_h = target_h;

    st->branding_pixels = malloc((size_t)(st->branding_w * st->branding_h) * sizeof(uint32_t));
    if (st->branding_pixels) {
        scale_rgba_to_argb(rgba, img_w, img_h,
                           st->branding_pixels, st->branding_w, st->branding_h);
    } else {
        st->branding_h = BRANDING_FALLBACK_H;
    }
    stbi_image_free(rgba);
}

static void read_system_info(AboutState *st) {
    strcpy(st->os_name,        "BoredOS");
    strcpy(st->os_version,     "Unknown Version");
    strcpy(st->kernel_version, "Unknown Kernel");
    strcpy(st->build_date,     "Unknown Build");

    char v_buf[512];
    FILE *f = fopen("/proc/version", "r");
    if (!f) return;

    size_t bytes = fread(v_buf, 1, sizeof(v_buf) - 1, f);
    fclose(f);
    if (bytes == 0) return;
    v_buf[bytes] = '\0';

    char *l1 = v_buf;
    char *l2 = strchr(l1, '\n'); if (l2) { *l2++ = '\0'; } else l2 = NULL;
    char *l3 = l2 ? strchr(l2, '\n') : NULL; if (l3) { *l3++ = '\0'; }
    char *l4 = l3 ? strchr(l3, '\n') : NULL; if (l4) { *l4++ = '\0'; }

    if (l1 && *l1) strncpy(st->os_name,        l1, 127);
    if (l2 && *l2) strncpy(st->os_version,     l2, 127);
    if (l3 && *l3) strncpy(st->kernel_version, l3, 127);
    if (l4 && *l4) strncpy(st->build_date,     l4, 127);
}

static void on_draw(NovaApp *app) {
    AboutState *st = app_get_userdata(app);
    int w = (int)app_width(app);

    wctx_begin(st->ctx);

    // Branding image
    if (st->branding_pixels) {
        int img_x = (w - st->branding_w) / 2;
        widget_image(st->ctx, img_x, OFFSET_Y,
                     st->branding_pixels, st->branding_w, st->branding_h, 1.0f);
    } else {
        int img_x = (w - st->branding_w) / 2;
        app_draw_rect(app, img_x, OFFSET_Y, st->branding_w, st->branding_h,
                      0xFF24273A, 0xFF45475A, 8);
        app_draw_string_centered(app, OFFSET_Y + st->branding_h / 2 - 8,
                                 "BoredOS", 0xFFFFFFFF);
    }

    // System info block
    int text_y = OFFSET_Y + st->branding_h + 14;

    widget_label(st->ctx, OFFSET_X, text_y,              st->os_name,        0xFFFFFFFF);
    widget_label(st->ctx, OFFSET_X, text_y + LINE_H,     st->os_version,     0xFFFFFFFF);
    widget_label(st->ctx, OFFSET_X, text_y + LINE_H * 2, st->kernel_version, 0xFFFFFFFF);

    widget_separator(st->ctx, OFFSET_X, text_y + LINE_H * 3 + 4, w - OFFSET_X * 2, true);

    widget_label(st->ctx, OFFSET_X, text_y + LINE_H * 3 + 10,
                 "(C) 2023-2026 Christiaan (chris@boreddev.nl)", 0xFFA6ADC8);
    widget_label(st->ctx, OFFSET_X, text_y + LINE_H * 4 + 10,
                 "All rights reserved.", 0xFFA6ADC8);

    wctx_end(st->ctx);
}

int main(void) {
    AboutState st = {0};

    // Load resources before creating the window so we know the exact height
    load_branding_image(&st);
    read_system_info(&st);

    // Height: top padding + branding + gap + 5 text rows + bottom padding
    int win_h = OFFSET_Y + st.branding_h + 14 + LINE_H * 5 + 22;

    NovaApp *app = app_create("About BoredOS", 420, (uint32_t)win_h);
    if (!app) { free(st.branding_pixels); return 1; }

    st.ctx = wctx_create(app);
    if (!st.ctx) { free(st.branding_pixels); app_destroy(app); return 1; }

    app_set_userdata(app, &st);
    app_on_draw(app, on_draw);

    int rc = app_run(app);

    wctx_destroy(st.ctx);
    if (st.branding_pixels) free(st.branding_pixels);
    app_destroy(app);
    return rc;
}
