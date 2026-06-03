// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
#include "app.h"
#include "libui/ui.h"
#include "libnovaproto/novaproto.h"
#include "libtheme/theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// poll() forward declarations 
#ifndef POLLIN
#define POLLIN  0x001
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
struct pollfd {
    int   fd;
    short events;
    short revents;
};
extern int poll(struct pollfd *fds, unsigned long nfds, int timeout);
#endif

#define APP_MAX_DIRTY_RECTS 32

struct NovaApp {
    int fd;
    uint32_t surf_id;
    char shm_path[128];

    // Pixel buffer
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;

    // Theme
    ThemeConfig theme;

    // User callbacks
    AppDrawCallback cb_draw;
    AppKeyCallback cb_key;
    AppTextCallback cb_text;
    AppPointerCallback cb_pointer;
    AppCloseCallback cb_close;

    void *userdata;

    // Flags
    bool dirty;       // redraw requested
    bool full_dirty;
    bool drawing;
    bool running;
    NovaRect dirty_rects[APP_MAX_DIRTY_RECTS];
    int dirty_rect_count;
    NovaRect next_dirty_rects[APP_MAX_DIRTY_RECTS];
    int next_dirty_rect_count;
    bool next_full_dirty;
};

#define NORMAL_LAYER 1

static bool _map_shm(NovaApp *app, uint32_t w, uint32_t h, const char *path) {
    int sfd = open(path, O_RDWR);
    if (sfd < 0) return false;

    uint32_t *ptr = mmap(NULL, w * h * 4,PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
    close(sfd);
    if (ptr == MAP_FAILED) return false;

    app->pixels = ptr;
    return true;
}

static void _unmap_pixels(NovaApp *app) {
    if (app->pixels && app->pixels != MAP_FAILED) {
        munmap(app->pixels, app->width * app->height * 4);
        app->pixels = NULL;
    }
}

static void _damage_all(NovaApp *app) {
    NovaRect r = {0, 0, app->width, app->height};
    nova_damage_surface(app->fd, app->surf_id, 1, &r);
}

static bool _clip_rect(NovaApp *app, int *x, int *y, int *w, int *h) {
    if (!app || !x || !y || !w || !h || *w <= 0 || *h <= 0) return false;

    int x1 = *x;
    int y1 = *y;
    int x2 = *x + *w;
    int y2 = *y + *h;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > (int)app->width) x2 = (int)app->width;
    if (y2 > (int)app->height) y2 = (int)app->height;
    if (x2 <= x1 || y2 <= y1) return false;

    *x = x1;
    *y = y1;
    *w = x2 - x1;
    *h = y2 - y1;
    return true;
}

static void _add_dirty_rect(NovaRect *rects, int *count, int x, int y, int w, int h) {
    if (!rects || !count || w <= 0 || h <= 0) return;

    if (*count >= APP_MAX_DIRTY_RECTS) {
        int x1 = rects[0].x;
        int y1 = rects[0].y;
        int x2 = rects[0].x + (int)rects[0].w;
        int y2 = rects[0].y + (int)rects[0].h;

        if (x < x1) x1 = x;
        if (y < y1) y1 = y;
        if (x + w > x2) x2 = x + w;
        if (y + h > y2) y2 = y + h;

        rects[0].x = x1;
        rects[0].y = y1;
        rects[0].w = (uint32_t)(x2 - x1);
        rects[0].h = (uint32_t)(y2 - y1);
        *count = 1;
        return;
    }

    rects[*count].x = x;
    rects[*count].y = y;
    rects[*count].w = (uint32_t)w;
    rects[*count].h = (uint32_t)h;
    (*count)++;
}

static void _clear_next_dirty(NovaApp *app) {
    app->next_dirty_rect_count = 0;
    app->next_full_dirty = false;
}

static void _damage_dirty(NovaApp *app) {
    if (app->full_dirty || app->dirty_rect_count <= 0) {
        _damage_all(app);
        return;
    }

    nova_damage_surface(app->fd, app->surf_id,
                        app->dirty_rect_count, app->dirty_rects);
}

static bool _socket_readable(struct pollfd *pfd) {
    if (!pfd) return false;

    pfd->revents = 0;
    int pr = poll(pfd, 1, 0);
    return pr > 0 && (pfd->revents & POLLIN);
}

static void _do_draw(NovaApp *app) {
    if (!app->pixels) return;

    app->dirty = false;
    app->drawing = true;

    // Fill with theme background using a tight pointer loop.
    uint32_t  bg  = app->theme.panel_bg;
    uint32_t *p   = app->pixels;
    uint32_t *end = p + app->width * app->height;
    while (p < end) *p++ = bg;

    if (app->cb_draw) {
        app->cb_draw(app);
    }

    app->drawing = false;
    bool redraw_again = app->dirty;

    _damage_dirty(app);

    app->full_dirty = app->next_full_dirty;
    app->dirty_rect_count = app->next_dirty_rect_count;
    if (app->next_dirty_rect_count > 0) {
        memcpy(app->dirty_rects, app->next_dirty_rects,
               (size_t)app->next_dirty_rect_count * sizeof(NovaRect));
    }
    _clear_next_dirty(app);

    if (!redraw_again) {
        app->full_dirty = false;
        app->dirty_rect_count = 0;
    }
}

static bool _handle_resize(NovaApp *app, uint32_t new_w, uint32_t new_h) {
    char new_path[128];
    if (nova_resize_surface(app->fd, app->surf_id,
                            new_w, new_h, new_path) < 0) {
        return false;
    }

    _unmap_pixels(app);
    app->width  = new_w;
    app->height = new_h;
    strncpy(app->shm_path, new_path, sizeof(app->shm_path) - 1);
    app->shm_path[sizeof(app->shm_path) - 1] = '\0';

    if (!_map_shm(app, new_w, new_h, new_path)) return false;

    app->dirty = true;
    app->full_dirty = true;
    return true;
}

NovaApp *app_create(const char *title, uint32_t width, uint32_t height) {
    NovaApp *app = calloc(1, sizeof(NovaApp));
    if (!app) return NULL;

    app->width   = width;
    app->height  = height;
    app->running = true;
    app->dirty   = true;
    app->full_dirty = true;

    // Load system theme
    theme_load("/etc/nova/nova.conf", &app->theme);

    // Initialize font renderer
    if (ui_font_init(app->theme.font_path, app->theme.font_size) < 0) {
        fprintf(stderr, "libapp: Failed to load font '%s'\n",
                app->theme.font_path);
        free(app);
        return NULL;
    }

    // Connect to the compositor
    app->fd = nova_connect(NULL);
    if (app->fd < 0) {
        fprintf(stderr, "libapp: Failed to connect to nova compositor\n");
        free(app);
        return NULL;
    }

    // Allocate a surface
    if (nova_create_surface(app->fd, width, height, NORMAL_LAYER, 0,
                            &app->surf_id, app->shm_path) < 0) {
        fprintf(stderr, "libapp: Surface allocation failed\n");
        close(app->fd);
        free(app);
        return NULL;
    }

    // Map the shared memory pixel buffer
    if (!_map_shm(app, width, height, app->shm_path)) {
        fprintf(stderr, "libapp: Failed to map SHM buffer '%s'\n",
                app->shm_path);
        nova_destroy_surface(app->fd, app->surf_id);
        close(app->fd);
        free(app);
        return NULL;
    }

    // Set window title
    if (title) {
        nova_set_title(app->fd, app->surf_id, title);
    }

    return app;
}

// Destroy the application and free ressources
void app_destroy(NovaApp *app) {
    if (!app) return;

    nova_destroy_surface(app->fd, app->surf_id);
    _unmap_pixels(app);
    close(app->fd);
    free(app);
}

int app_run(NovaApp *app) {
    if (!app) return 1;

    // Initial draw
    _do_draw(app);

    struct pollfd pfd;
    pfd.fd     = app->fd;
    pfd.events = POLLIN;

    while (app->running) {
        // Use 0ms timeout if a redraw is already pending (don't stall),
        // otherwise wait up to 16ms (~60fps cap, needed for cursor blink etc.)
        int timeout = app->dirty ? 0 : 16;
        int pr = poll(&pfd, 1, timeout);
        if (pr < 0) break;
        if (pr > 0 && (pfd.revents & (POLLHUP | POLLERR))) {
            app->running = false;
            break;
        }

        // Drain currently available events before redrawing. nova_poll_event()
        // performs a blocking socket read, so guard each call with poll(0).
        if ((pr > 0 && (pfd.revents & POLLIN)) || nova_pending_events()) {
            NovaEvent ev;
            while (nova_pending_events() || _socket_readable(&pfd)) {
                if (nova_poll_event(app->fd, &ev) < 0) {
                    app->running = false;
                    break;
                }

                switch (ev.type) {
                    case EVT_CLOSE_REQUEST: {
                        bool allow = true;
                        if (app->cb_close) {
                            allow = app->cb_close(app);
                        }
                        if (allow) app->running = false;
                        break;
                    }

                    case EVT_RESIZE_REQUEST: {
                        if (_handle_resize(app,
                                           ev.data.resize.w,
                                           ev.data.resize.h)) {
                            _do_draw(app);
                        }
                        break;
                    }

                    case EVT_KEY: {
                        if (app->cb_key) {
                            app->cb_key(app,
                                        ev.data.key.keycode,
                                        ev.data.key.modifiers,
                                        ev.data.key.pressed);
                        }
                        if (ev.data.key.pressed &&
                            ev.data.key.text_len > 0 &&
                            app->cb_text) {
                            app->cb_text(app,
                                         ev.data.key.text,
                                         ev.data.key.codepoint);
                        }
                        break;
                    }

                    case EVT_POINTER: {
                        if (app->cb_pointer) {
                            app->cb_pointer(app,
                                            ev.data.pointer.x,
                                            ev.data.pointer.y,
                                            ev.data.pointer.buttons);
                        }
                        break;
                    }

                    case EVT_THEME_UPDATE: {
                        theme_load("/etc/nova/nova.conf", &app->theme);
                        ui_font_init(app->theme.font_path,
                                     app->theme.font_size);
                        app->dirty = true;
                        app->full_dirty = true;
                        break;
                    }

                    default:
                        break;
                }
            }
        }

        if (app->dirty) {
            _do_draw(app);
        }
    }

    return 0;
}

void app_request_redraw(NovaApp *app) {
    if (!app) return;
    app->dirty = true;
    if (app->drawing) {
        app->full_dirty = true;
        app->next_full_dirty = true;
    } else {
        app->full_dirty = true;
    }
}

void app_request_redraw_rect(NovaApp *app, int x, int y, int w, int h) {
    if (!app || !_clip_rect(app, &x, &y, &w, &h)) return;

    app->dirty = true;
    _add_dirty_rect(app->dirty_rects, &app->dirty_rect_count, x, y, w, h);
    if (app->drawing) {
        _add_dirty_rect(app->next_dirty_rects, &app->next_dirty_rect_count,
                        x, y, w, h);
    }
}

// Callback registration
void app_on_draw(NovaApp *app, AppDrawCallback cb) {
    if (app) app->cb_draw = cb;
}

void app_on_key(NovaApp *app, AppKeyCallback cb) {
    if (app) app->cb_key = cb;
}

void app_on_text(NovaApp *app, AppTextCallback cb) {
    if (app) app->cb_text = cb;
}

void app_on_pointer(NovaApp *app, AppPointerCallback cb) {
    if (app) app->cb_pointer = cb;
}

void app_on_close(NovaApp *app, AppCloseCallback cb) {
    if (app) app->cb_close = cb;
}

// WM

void app_set_title(NovaApp *app, const char *title) {
    if (app && title) {
        nova_set_title(app->fd, app->surf_id, title);
    }
}

void app_set_icon(NovaApp *app, const char *icon_path) {
    if (app && icon_path) {
        nova_set_icon(app->fd, app->surf_id, icon_path);
    }
}

void app_set_userdata(NovaApp *app, void *userdata) {
    if (app) app->userdata = userdata;
}

void *app_get_userdata(const NovaApp *app) {
    return app ? app->userdata : NULL;
}


uint32_t app_width(const NovaApp *app) {
    return app ? app->width : 0;
}

uint32_t app_height(const NovaApp *app) {
    return app ? app->height : 0;
}

const ThemeConfig *app_theme(const NovaApp *app) {
    return app ? &app->theme : NULL;
}


void app_clear(NovaApp *app) {
    if (!app || !app->pixels) return;
    uint32_t color = app->theme.panel_bg;
    for (uint32_t i = 0; i < app->width * app->height; i++) {
        app->pixels[i] = color;
    }
}

void app_clear_color(NovaApp *app, uint32_t color) {
    if (!app || !app->pixels) return;
    for (uint32_t i = 0; i < app->width * app->height; i++) {
        app->pixels[i] = color;
    }
}

void app_draw_string(NovaApp *app, int x, int y,
                     const char *text, uint32_t color) {
    if (!app || !app->pixels || !text) return;
    ui_draw_string(app->pixels, (int)app->width, (int)app->height,
                   x, y, text, color);
}

void app_draw_string_centered(NovaApp *app, int y,
                              const char *text, uint32_t color) {
    if (!app || !app->pixels || !text) return;
    int tw = ui_text_width(text);
    int x  = ((int)app->width - tw) / 2;
    ui_draw_string(app->pixels, (int)app->width, (int)app->height,
                   x, y, text, color);
}

int app_text_width(const char *text) {
    return ui_text_width(text);
}

int app_text_width_n(const char *text, int n) {
    return ui_text_width_n(text, n);
}

void app_draw_rect(NovaApp *app, int x, int y, int w, int h,
                   uint32_t fill_color, uint32_t border_color, int radius) {
    if (!app || !app->pixels) return;
    ui_draw_panel(app->pixels,
                  (int)app->width, (int)app->height,
                  x, y, w, h,
                  fill_color, border_color, radius);
}

void app_blit(NovaApp *app, int dx, int dy,
              const uint32_t *src, int src_w, int src_h, float alpha) {
    if (!app || !app->pixels || !src) return;
    ui_blend_pixels(app->pixels,
                    (int)app->width, (int)app->height,
                    dx, dy, src, src_w, src_h, alpha);
}

uint32_t *app_pixels(NovaApp *app) {
    return app ? app->pixels : NULL;
}
