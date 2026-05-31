// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: A simple GUI Hello World application.
// BOREDOS_APP_ICONS: /Library/images/icons/colloid/application-default-icon.png

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <syscall.h>
#include "libtheme/theme.h"
#include "libui/ui.h"
#include "libnovaproto/novaproto.h"

#define NORMAL_LAYER 1

static void draw_ui(uint32_t *pixels, uint32_t w, uint32_t h, uint32_t bg_color) {
    for (uint32_t i = 0; i < w * h; i++) {
        pixels[i] = bg_color;
    }
    ui_draw_string(pixels, w, h, ((int)w - 120) / 2, (int)h / 2 - 35, "Hello, BoredOS!", 0xFFFFFFFF);
    ui_draw_string(pixels, w, h, ((int)w - 234) / 2, (int)h / 2 - 5, "A simple GUI application using libui.", 0xFFA6ADC8);
}

static bool apply_resize_surface(int fd, uint32_t surf_id, uint32_t new_w, uint32_t new_h, uint32_t *current_w, uint32_t *current_h, uint32_t **pixels, char *shm_path, size_t shm_path_len) {
    char new_shm_path[128];
    if (nova_resize_surface(fd, surf_id, new_w, new_h, new_shm_path) < 0) {
        return false;
    }

    int new_shm_fd = open(new_shm_path, O_RDWR);
    if (new_shm_fd < 0) {
        return false;
    }

    uint32_t *new_pixels = mmap(NULL, new_w * new_h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, new_shm_fd, 0);
    close(new_shm_fd);
    if (new_pixels == MAP_FAILED) {
        return false;
    }

    if (*pixels && *pixels != MAP_FAILED) {
        munmap(*pixels, *current_w * *current_h * 4);
    }

    *pixels = new_pixels;
    *current_w = new_w;
    *current_h = new_h;
    if (shm_path && shm_path_len > 0) {
        strncpy(shm_path, new_shm_path, shm_path_len - 1);
        shm_path[shm_path_len - 1] = '\0';
    }

    return true;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    ThemeConfig theme;
    theme_load("/etc/nova/nova.conf", &theme);
    if (ui_font_init(theme.font_path, theme.font_size) < 0) {
        fprintf(stderr, "HelloWorld Error: Failed to initialize UI font %s\n", theme.font_path);
        return 1;
    }

    int fd = nova_connect(NULL);
    uint32_t current_w = 300;
    uint32_t current_h = 200;
    uint32_t surf_id = 0;
    char shm_path[128];
    if (nova_create_surface(fd, current_w, current_h, NORMAL_LAYER, 0, &surf_id, shm_path) < 0) {
        fprintf(stderr, "HelloWorld Error: Surface allocation failed\n");
        close(fd);
        return 1;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        fprintf(stderr, "HelloWorld Error: Cannot open SHM segment %s\n", shm_path);
        close(fd);
        return 1;
    }

    uint32_t *pixels = mmap(NULL, current_w * current_h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (pixels == MAP_FAILED) {
        fprintf(stderr, "HelloWorld Error: mmap failed\n");
        close(fd);
        return 1;
    }

    nova_set_title(fd, surf_id, "Hello World");

    draw_ui(pixels, current_w, current_h, theme.panel_bg);
    NovaRect damage = { 0, 0, current_w, current_h };
    nova_damage_surface(fd, surf_id, 1, &damage);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    bool running = true;
    while (running) {
        int timeout = 100;
        int pr = poll(&pfd, 1, timeout);

        if (pr < 0) {
            break;
        }

        if ((pr > 0 && (pfd.revents & POLLIN)) || nova_pending_events()) {
            NovaEvent ev;
            while (nova_poll_event(fd, &ev) == 0) {
                if (ev.type == EVT_CLOSE_REQUEST) {
                    running = false;
                    break;
                } else if (ev.type == EVT_RESIZE_REQUEST) {
                    uint32_t new_w = ev.data.resize.w;
                    uint32_t new_h = ev.data.resize.h;

                    if (apply_resize_surface(fd, surf_id, new_w, new_h, &current_w, &current_h, &pixels, shm_path, sizeof(shm_path))) {
                        draw_ui(pixels, current_w, current_h, theme.panel_bg);

                        NovaRect r = { 0, 0, current_w, current_h };
                        nova_damage_surface(fd, surf_id, 1, &r);
                    }
                }
            }
        }
    }

    nova_destroy_surface(fd, surf_id);
    if (pixels && pixels != MAP_FAILED) {
        munmap(pixels, current_w * current_h * 4);
    }
    close(fd);

    return 0;
}
