// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: Shows BoredOS information.

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
#include "stb_image.h"

#define NORMAL_LAYER 1
#define INITIAL_WIDTH 420
#define INITIAL_HEIGHT 200

static uint32_t *branding_pixels = NULL;
static int branding_w = 0;
static int branding_h = 0;

static void scale_rgba_to_argb(const unsigned char *rgba, int src_w, int src_h, uint32_t *dst, int dst_w, int dst_h) {
    if (src_w == dst_w && src_h == dst_h) {
        for (int i = 0; i < dst_w * dst_h; i++) {
            int idx = i * 4;
            dst[i] = ((uint32_t)rgba[idx + 3] << 24) | ((uint32_t)rgba[idx] << 16) |
                     ((uint32_t)rgba[idx + 1] << 8) | rgba[idx + 2];
        }
        return;
    }

    uint32_t step_x = (src_w << 16) / dst_w;
    uint32_t step_y = (src_h << 16) / dst_h;
    uint32_t curr_y = 0;

    for (int y = 0; y < dst_h; y++) {
        uint32_t src_y = curr_y >> 16;
        if (src_y >= (uint32_t)src_h) src_y = src_h - 1;
        uint32_t curr_x = 0;
        uint32_t src_row_off = src_y * src_w;
        uint32_t dst_row_off = y * dst_w;
        for (int x = 0; x < dst_w; x++) {
            uint32_t src_x = curr_x >> 16;
            if (src_x >= (uint32_t)src_w) src_x = src_w - 1;
            int idx = (src_row_off + src_x) * 4;
            dst[dst_row_off + x] = ((uint32_t)rgba[idx + 3] << 24) |
                                   ((uint32_t)rgba[idx] << 16) |
                                   ((uint32_t)rgba[idx + 1] << 8) |
                                   rgba[idx + 2];
            curr_x += step_x;
        }
        curr_y += step_y;
    }
}

static void load_branding_image(void) {
    const char *path = "/Library/images/branding/bOS_full_gradient_cropped.png";
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "About: Could not open branding image\n");
        return;
    }

    // Get file size
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (size <= 0) {
        close(fd);
        return;
    }

    unsigned char *buf = malloc(size);
    if (!buf) {
        close(fd);
        return;
    }

    ssize_t bytes_read = read(fd, buf, size);
    close(fd);

    if (bytes_read != size) {
        free(buf);
        return;
    }

    int img_w, img_h, channels;
    unsigned char *rgba = stbi_load_from_memory(buf, size, &img_w, &img_h, &channels, 4);
    free(buf);

    if (!rgba) {
        fprintf(stderr, "About: Failed to decode branding image\n");
        return;
    }

    branding_w = 350;
    branding_h = (img_h * branding_w) / img_w;

    branding_pixels = malloc(branding_w * branding_h * sizeof(uint32_t));
    if (branding_pixels) {
        scale_rgba_to_argb(rgba, img_w, img_h, branding_pixels, branding_w, branding_h);
    }
    stbi_image_free(rgba);
}

static void read_system_info(char *os_name, char *os_version, char *kernel_version, char *build_date) {
    strcpy(os_name, "BoredOS");
    strcpy(os_version, "Unknown Version");
    strcpy(kernel_version, "Unknown Kernel");
    strcpy(build_date, "Unknown Build");

    int fd = open("/proc/version", O_RDONLY);
    if (fd < 0) {
        return;
    }

    char v_buf[512];
    ssize_t bytes = read(fd, v_buf, sizeof(v_buf) - 1);
    close(fd);

    if (bytes <= 0) {
        return;
    }
    v_buf[bytes] = '\0';

    // Parse multi-line format
    char *line1 = v_buf;
    char *line2 = strchr(line1, '\n');
    if (line2) {
        *line2 = '\0';
        line2++;
    }

    char *line3 = line2 ? strchr(line2, '\n') : NULL;
    if (line3) {
        *line3 = '\0';
        line3++;
    }

    char *line4 = line3 ? strchr(line3, '\n') : NULL;
    if (line4) {
        *line4 = '\0';
        line4++;
    }

    if (line1 && strlen(line1) > 0) {
        strncpy(os_name, line1, 127);
    }
    if (line2 && strlen(line2) > 0) {
        strncpy(os_version, line2, 127);
    }
    if (line3 && strlen(line3) > 0) {
        strncpy(kernel_version, line3, 127);
    }
    if (line4 && strlen(line4) > 0) {
        strncpy(build_date, line4, 127);
    }
}

static void draw_ui(uint32_t *pixels, uint32_t w, uint32_t h, uint32_t bg_color) {
    // Fill background
    for (uint32_t i = 0; i < w * h; i++) {
        pixels[i] = bg_color;
    }

    int offset_x = 35;
    int offset_y = 15;

    // Draw branding image
    if (branding_pixels) {
        ui_blend_pixels(pixels, w, h, offset_x, offset_y,
                        branding_pixels, branding_w, branding_h, 1.0f);
    }

    // Draw system information
    int text_y = offset_y + branding_h + 15;

    char os_name[128];
    char os_version[128];
    char kernel_version[128];
    char build_date[128];
    read_system_info(os_name, os_version, kernel_version, build_date);

    ui_draw_string(pixels, w, h, offset_x, text_y, os_name, 0xFFFFFFFF);
    ui_draw_string(pixels, w, h, offset_x, text_y + 16, os_version, 0xFFFFFFFF);
    ui_draw_string(pixels, w, h, offset_x, text_y + 32, kernel_version, 0xFFFFFFFF);

    // Copyright
    ui_draw_string(pixels, w, h, offset_x, text_y + 48, "(C) 2023-2026 Christiaan (chris@boreddev.nl)", 0xFFA6ADC8);
    ui_draw_string(pixels, w, h, offset_x, text_y + 64, "All rights reserved.", 0xFFA6ADC8);
}

static bool apply_resize_surface(int fd, uint32_t surf_id, uint32_t new_w, uint32_t new_h,
                                 uint32_t *current_w, uint32_t *current_h, uint32_t **pixels,
                                 char *shm_path, size_t shm_path_len) {
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

    load_branding_image();

    ThemeConfig theme;
    theme_load("/etc/nova/nova.conf", &theme);
    if (ui_font_init(theme.font_path, theme.font_size) < 0) {
        fprintf(stderr, "About: Failed to initialize UI font\n");
        return 1;
    }

    int fd = nova_connect(NULL);
    uint32_t current_w = INITIAL_WIDTH;
    uint32_t current_h = INITIAL_HEIGHT;
    uint32_t surf_id = 0;
    char shm_path[128];

    if (nova_create_surface(fd, current_w, current_h, NORMAL_LAYER, 0, &surf_id, shm_path) < 0) {
        fprintf(stderr, "About: Surface allocation failed\n");
        close(fd);
        return 1;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        fprintf(stderr, "About: Cannot open SHM segment %s\n", shm_path);
        close(fd);
        return 1;
    }

    uint32_t *pixels = mmap(NULL, current_w * current_h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (pixels == MAP_FAILED) {
        fprintf(stderr, "About: mmap failed\n");
        close(fd);
        return 1;
    }

    nova_set_title(fd, surf_id, "About BoredOS");

    draw_ui(pixels, current_w, current_h, theme.panel_bg);
    NovaRect damage = {0, 0, current_w, current_h};
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

                        NovaRect r = {0, 0, current_w, current_h};
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
    if (branding_pixels) {
        free(branding_pixels);
    }
    close(fd);

    return 0;
}
