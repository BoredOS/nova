// BOREDOS_APP_DESC: Background Wallpaper Daemon.
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
#include "libnovaproto/novaproto.h"
#include "libtheme/theme.h"
#include "stb_image.h"

#define DEFAULT_WALLPAPER "/Library/images/Wallpapers/boredos.png"

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
    // Set default
    strncpy(out_path, DEFAULT_WALLPAPER, max_len - 1);
    out_path[max_len - 1] = '\0';

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
            strncpy(out_path, val, max_len - 1);
            out_path[max_len - 1] = '\0';
        }
    }
    fclose(f);
}

static bool load_file_to_buffer(const char *path, unsigned char **out_buf, size_t *out_size) {
    if (!path || !out_buf || !out_size) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
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

static bool load_and_scale_wallpaper(const char *path, uint32_t *dest_pixels, int screen_w, int screen_h) {
    unsigned char *file_buf = NULL;
    size_t file_size = 0;
    if (!load_file_to_buffer(path, &file_buf, &file_size)) {
        return false;
    }

    int img_w = 0, img_h = 0, comp = 0;
    unsigned char *rgba = stbi_load_from_memory(file_buf, (int)file_size, &img_w, &img_h, &comp, 4);
    free(file_buf);
    if (!rgba || img_w <= 0 || img_h <= 0) {
        if (rgba) stbi_image_free(rgba);
        return false;
    }

    for (int y = 0; y < screen_h; y++) {
        int src_y = (y * img_h) / screen_h;
        uint8_t *src_row = &rgba[src_y * img_w * 4];
        uint32_t *dest_row = &dest_pixels[y * screen_w];
        for (int x = 0; x < screen_w; x++) {
            int src_x = (x * img_w) / screen_w;
            uint8_t r = src_row[src_x * 4 + 0];
            uint8_t g = src_row[src_x * 4 + 1];
            uint8_t b = src_row[src_x * 4 + 2];
            uint8_t a = src_row[src_x * 4 + 3];
            dest_row[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }

    stbi_image_free(rgba);
    return true;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    ThemeConfig theme;
    theme_load("/etc/nova/nova.conf", &theme);

    char wallpaper_path[256];
    load_wallpaper_path("/Library/conf/wallpaper.conf", wallpaper_path, sizeof(wallpaper_path));

    int fd = nova_connect(NULL);
    if (fd < 0) {
        fprintf(stderr, "Wallpaperd Error: Cannot connect to Nova socket\n");
        return 1;
    }

    int screen_w = 1024;
    int screen_h = 768;

    struct fb_var_screeninfo vinfo;
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0) {
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            screen_w = vinfo.xres;
            screen_h = vinfo.yres;
        }
        close(fb);
    }

    uint32_t surf_id = 0;
    char shm_path[128];
    // Background layer is 0
    if (nova_create_surface(fd, screen_w, screen_h, 0, 0, &surf_id, shm_path) < 0) {
        fprintf(stderr, "Wallpaperd Error: Surface allocation failed\n");
        close(fd);
        return 1;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        fprintf(stderr, "Wallpaperd Error: Cannot open SHM segment %s\n", shm_path);
        close(fd);
        return 1;
    }

    uint32_t *pixels = mmap(NULL, screen_w * screen_h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (pixels == MAP_FAILED) {
        fprintf(stderr, "Wallpaperd Error: mmap failed\n");
        close(fd);
        return 1;
    }

    // Set fallback solid background color in case image load fails
    for (int i = 0; i < screen_w * screen_h; i++) {
        pixels[i] = theme.desktop_bg;
    }

    // Load and scale image to screen buffer
    load_and_scale_wallpaper(wallpaper_path, pixels, screen_w, screen_h);

    // Damage entire surface to draw it
    NovaRect damage = { 0, 0, screen_w, screen_h };
    nova_damage_surface(fd, surf_id, 1, &damage);

    // Low power event polling loop to maintain socket connection with the compositor
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        int pr = poll(&pfd, 1, 1000);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            NovaEvent ev;
            if (nova_poll_event(fd, &ev) < 0) {
                // Connection closed by compositor
                break;
            }
        }
    }

    munmap(pixels, screen_w * screen_h * 4);
    close(fd);
    return 0;
}
