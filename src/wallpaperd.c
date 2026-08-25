// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: Background Wallpaper Daemon.
// BOREDOS_APP_ICONS: /Library/images/icons/serenityicons/32x32/app-terminal.png

#include "ntk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <syscall.h>

#define DEFAULT_WALLPAPER "/Library/Wallpapers/Curvature.png"

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static char *trim_spaces(char *str) {
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }
    return str;
}

static void load_wallpaper_path(const char *path, char *out_path, size_t max_len) {
    copy_string(out_path, max_len, DEFAULT_WALLPAPER);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *start = trim_spaces(line);
        if (*start == '\0' || *start == '#' || *start == ';') continue;
        if (*start == '[') continue;

        char *eq = strchr(start, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_spaces(start);
        char *val = trim_spaces(eq + 1);

        if (strcmp(key, "path") == 0) {
            copy_string(out_path, max_len, val);
        }
    }
    fclose(f);
}

#include "stb_image.h"

static void scale_nearest_argb(uint32_t *dst, int dst_w, int dst_h, const uint8_t *src_rgba, int src_w, int src_h) {
    for (int y = 0; y < dst_h; y++) {
        int src_y = (int)((uint64_t)y * (uint64_t)src_h / (uint64_t)dst_h);
        if (src_y >= src_h) src_y = src_h - 1;
        const uint8_t *src_row = src_rgba + ((size_t)src_y * (size_t)src_w * 4);
        uint32_t *dst_row = dst + ((size_t)y * (size_t)dst_w);
        for (int x = 0; x < dst_w; x++) {
            int src_x = (int)((uint64_t)x * (uint64_t)src_w / (uint64_t)dst_w);
            if (src_x >= src_w) src_x = src_w - 1;
            const uint8_t *px = src_row + (src_x * 4);
            dst_row[x] = ((uint32_t)px[3] << 24) | ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | (uint32_t)px[2];
        }
    }
}

static bool load_and_scale_wallpaper(const char *path, uint32_t *dest_pixels, int screen_w, int screen_h) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0 || size > 32 * 1024 * 1024) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);

    unsigned char *file_buf = malloc((size_t)size);
    if (!file_buf) {
        fclose(f);
        return false;
    }

    size_t rd = fread(file_buf, 1, (size_t)size, f);
    fclose(f);
    if (rd != (size_t)size) {
        free(file_buf);
        return false;
    }

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char *data = stbi_load_from_memory(file_buf, (int)size, &img_w, &img_h, &channels, 4);
    free(file_buf);

    if (!data) return false;

    scale_nearest_argb(dest_pixels, screen_w, screen_h, data, img_w, img_h);
    stbi_image_free(data);
    return true;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    NtkStyle *style = ntk_style_new_from_file("/Library/AppData/org.boredos.nova/nova.conf");
    NtkColor desktop_bg = 0xFF008080;
    if (style) {
        desktop_bg = ntk_style_get_color(style, NTK_STYLE_ROLE_WINDOW_BG);
        ntk_style_destroy(style);
    }

    char wallpaper_path[256];
    load_wallpaper_path("/Library/AppData/org.boredos.nova/wallpaper.conf", wallpaper_path, sizeof(wallpaper_path));

    int fd = ntk_nova_connect(NULL);
    if (fd < 0) {
        fprintf(stderr, "Wallpaperd Error: Cannot connect to Nova socket\n");
        return 1;
    }

    NtkSize screen_sz = ntk_nova_get_screen_size(fd);

    uint32_t surf_id = 0;
    char shm_path[128];
    // Background layer is 0
    if (ntk_nova_create_surface(fd, screen_sz.width, screen_sz.height, 0, 0, &surf_id, shm_path) < 0) {
        fprintf(stderr, "Wallpaperd Error: Surface allocation failed\n");
        ntk_nova_disconnect(fd);
        return 1;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        fprintf(stderr, "Wallpaperd Error: Cannot open SHM segment %s\n", shm_path);
        ntk_nova_disconnect(fd);
        return 1;
    }

    uint32_t *pixels = mmap(NULL, screen_sz.width * screen_sz.height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (pixels == MAP_FAILED) {
        fprintf(stderr, "Wallpaperd Error: mmap failed\n");
        ntk_nova_disconnect(fd);
        return 1;
    }

    if (!load_and_scale_wallpaper(wallpaper_path, pixels, screen_sz.width, screen_sz.height)) {
        for (int i = 0; i < screen_sz.width * screen_sz.height; i++) {
            pixels[i] = desktop_bg;
        }
    }

    NtkRect damage = NTK_RECT(0, 0, screen_sz.width, screen_sz.height);
    ntk_nova_damage_surface(fd, surf_id, 1, &damage);

    // Event loop
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        int pr = poll(&pfd, 1, 1000);
        if (pr > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))) {
            break;
        }
        if (pr > 0 && (pfd.revents & POLLIN)) {
            NtkEvent ev;
            if (ntk_nova_poll_event(fd, &ev) < 0) {
                break;
            }
        }
    }

    munmap(pixels, screen_sz.width * screen_sz.height * 4);
    ntk_nova_disconnect(fd);
    return 0;
}
