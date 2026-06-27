// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.// BOREDOS_APP_DESC: Taskbar with start menu, window list, and clock.
// BOREDOS_APP_ICONS: /Library/images/icons/serenityicons/32x32/app-terminal.png
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <syscall.h>
#include "utf-8.h"
#include "ntk.h"
#include "libnovaproto/novaproto.h"
#include "stb_image.h"

typedef struct {
    uint32_t text_primary;
    uint32_t text_error;
    int      font_size;
    char     font_path[256];
} ThemeConfig;

static void theme_load(const char *path, ThemeConfig *t) {
    (void)path;
    t->text_primary = 0xFFFFFFFF;
    t->text_error = 0xFFF38BA8;
    t->font_size = 12;
    strcpy(t->font_path, "/Library/Fonts/Proggy.ttf");
}

static void ui_font_init(const char *path, int size) {
    (void)path;
    (void)size;
}

static void ui_blend_pixels(uint32_t *dest, int dest_w, int dest_h, int x, int y, uint32_t *src, int src_w, int src_h, float alpha) {
    NtkPainter *p = ntk_painter_new_from_buffer(dest, dest_w, dest_h);
    if (p) {
        ntk_painter_blend_buffer(p, x, y, src, src_w, src_h, alpha);
        ntk_painter_destroy(p);
    }
}

static void ui_draw_string(uint32_t *dest, int dest_w, int dest_h, int x, int y, const char *text, uint32_t color) {
    NtkPainter *p = ntk_painter_new_from_buffer(dest, dest_w, dest_h);
    if (p) {
        NtkStyle *style = ntk_style_get_global();
        NtkFont *font = style ? ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT) : NULL;
        if (font) ntk_painter_set_font(p, font);
        ntk_painter_set_color(p, color);
        NtkRect r = NTK_RECT(x, y, dest_w - x, 24);
        ntk_painter_draw_text_rect(p, text, r, NTK_ALIGN_START);
        ntk_painter_destroy(p);
    }
}

static void ui_draw_string_rect(uint32_t *dest, int dest_w, int dest_h, int x, int y, int w, int h, const char *text, uint32_t color) {
    NtkPainter *p = ntk_painter_new_from_buffer(dest, dest_w, dest_h);
    if (p) {
        NtkStyle *style = ntk_style_get_global();
        NtkFont *font = style ? ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT) : NULL;
        if (font) ntk_painter_set_font(p, font);
        ntk_painter_set_color(p, color);
        NtkRect r = NTK_RECT(x, y, w, h);
        ntk_painter_draw_text_rect(p, text, r, NTK_ALIGN_START);
        ntk_painter_destroy(p);
    }
}

static void ui_draw_string_rect_centered(uint32_t *dest, int dest_w, int dest_h, int x, int y, int w, int h, const char *text, uint32_t color) {
    NtkPainter *p = ntk_painter_new_from_buffer(dest, dest_w, dest_h);
    if (p) {
        NtkStyle *style = ntk_style_get_global();
        NtkFont *font = style ? ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT) : NULL;
        if (font) ntk_painter_set_font(p, font);
        ntk_painter_set_color(p, color);
        NtkRect r = NTK_RECT(x, y, w, h);
        ntk_painter_draw_text_rect(p, text, r, NTK_ALIGN_CENTER);
        ntk_painter_destroy(p);
    }
}

static uint32_t lerp_color(uint32_t c1, uint32_t c2, float t) {
    uint8_t a1 = (c1 >> 24) & 0xFF, r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t a2 = (c2 >> 24) & 0xFF, r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint8_t a = a1 + (a2 - a1) * t;
    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void ui_draw_panel(uint32_t *dest, int dest_w, int dest_h, int x, int y, int w, int h, uint32_t bg_color, int shadow, int border) {
    NtkPainter *p = ntk_painter_new_from_buffer(dest, dest_w, dest_h);
    if (p) {
        ntk_painter_set_color(p, bg_color);
        ntk_painter_fill_rect(p, NTK_RECT(x, y, w, h));
        NtkColor light = 0xFFFFFFFF;
        NtkColor dark = 0xFF808080;
        if (shadow == 1) {
            ntk_painter_draw_bevel_sunken(p, NTK_RECT(x, y, w, h), light, dark);
        } else if (shadow == 0 && border == 0) {
            ntk_painter_set_color(p, dark);
            ntk_painter_draw_rect(p, NTK_RECT(x, y, w, h));
        } else {
            ntk_painter_draw_bevel_raised(p, NTK_RECT(x, y, w, h), light, dark);
        }
        ntk_painter_destroy(p);
    }
}

#define MAX_WINDOWS 32
#define MAX_APPS 128
#define ITEM_HEIGHT 30

#define TASKBAR_HEIGHT 26
#define TASKBAR_LAYER 3
#define MENU_LAYER 4

#define START_BTN_X 2
#define START_BTN_W 80
#define START_BTN_H 22

#define TAB_H 22
#define MAX_TAB_W 140
#define TAB_GAP 4

#define CLOCK_W 80
#define CLOCK_H 22

#define MENU_W 180
#define MENU_H 280
#define SUB_MENU_W 180
#define SUB_MENU_H 240
#define MENU_ACTION_ICON_SIZE 16
#define MENU_ACTION_ICON_SPACING 8

#define DEFAULT_LOGO_PATH "/Library/Icons/boredos/bos.png"
#define DEFAULT_APP_ICON_PATH "/Library/Icons/serenityicons/32x32/app-terminal.png"
#define DESKTOP_APPS_DIR "/Library/AppData"
#define DESKTOP_SUFFIX ".desktop"
#define DEFAULT_DATE_FORMAT "%Y-%m-%d"
#define DEFAULT_TIME_FORMAT "%H:%M"

typedef struct {
    uint32_t *pixels;
    int w;
    int h;
} image_t;

#define NUM_CATEGORIES 7
static const char *categories[NUM_CATEGORIES] = {
    "Demos",
    "Development",
    "Games",
    "Graphics",
    "Internet",
    "Sound",
    "Utilities"
};

static const char *category_icons[NUM_CATEGORIES] = {
    "/Library/Icons/serenityicons/16x16/demos.png",
    "/Library/Icons/serenityicons/16x16/development.png",
    "/Library/Icons/serenityicons/16x16/games.png",
    "/Library/Icons/serenityicons/16x16/graphics.png",
    "/Library/Icons/serenityicons/16x16/internet.png",
    "/Library/Icons/serenityicons/16x16/multimedia.png",
    "/Library/Icons/serenityicons/16x16/utilities.png"
};

static image_t category_icon_imgs[NUM_CATEGORIES];
static image_t run_icon_img;
static image_t exit_icon_img;
static image_t boredos_icon_img;

typedef struct {
    uint32_t surface_id;
    char title[128];
    uint32_t state_flags;
    bool active;
    image_t icon_img;
} window_tab_t;

typedef struct {
    char desktop_file[128];
    char display_name[128];
    char exec[256];
    char args[256];
    char icon_path[256];
    bool terminal;
    image_t icon_img;
    char category[64];
} app_entry_t;



typedef struct {
    bool position_bottom;
    char logo_path[256];
    char date_format[64];
    uint32_t active_tab_color;
    uint32_t inactive_tab_color;
    uint32_t gradient_top_color;
    uint32_t gradient_bottom_color;
    char clock_format[8];
    uint32_t taskbar_border_color;
    uint32_t launcher_bg_color;
    uint32_t launcher_border_color;
    uint32_t launcher_search_bg_color;
    uint32_t launcher_selected_bg_color;
    uint32_t taskbar_button_bg_color;
} taskbar_config_t;


static window_tab_t windows[MAX_WINDOWS];
static int window_count = 0;

static app_entry_t apps[MAX_APPS];
static int app_count = 0;
static app_entry_t filtered_apps[MAX_APPS];
static int filtered_count = 0;
static int selected_idx = 0;

static uint32_t last_active_surface_id = 0;
static uint32_t resume_focus_id = 0;

static uint32_t last_bar_click_ms = 0;
static uint32_t last_bar_buttons = 0;
static uint32_t last_menu_buttons = 0;

static int fd = -1;
static uint32_t bar_surf_id = 0;
static uint32_t *bar_pixels = NULL;
static uint32_t bar_w = 0;
static uint32_t bar_h = TASKBAR_HEIGHT;
static int screen_w = 1024;
static int screen_h = 768;

static uint32_t menu_surf_id = 0;
static uint32_t *menu_pixels = NULL;
static bool menu_open = false;

static uint32_t sub_menu_surf_id = 0;
static uint32_t *sub_menu_pixels = NULL;
static bool sub_menu_open = false;
static int sub_menu_category_idx = -1;
static int sub_menu_hover_idx = -1;
static bool sub_menu_focused = false;

typedef enum {
    ITEM_ABOUT,
    ITEM_SEPARATOR,
    ITEM_CATEGORY,
    ITEM_RUN,
    ITEM_EXIT
} MenuItemType;

typedef struct {
    MenuItemType type;
    char label[64];
    int category_idx;
    int y;
    int h;
} StartMenuItem;

static StartMenuItem start_menu_items[32];
static int start_menu_item_count = 0;
static int main_menu_hover_idx = -1;
static int main_menu_active_height = 0;

static ThemeConfig theme;
static taskbar_config_t config;

static image_t logo_img = {0};
static image_t app_icon_img = {0};

static char search_buf[64] = "";
static int search_len = 0;

static bool bar_dirty = true;

static bool should_track_window(uint32_t surface_id, const char *title) {
    if (surface_id == bar_surf_id) return false;
    if (surface_id == menu_surf_id) return false;
    if (surface_id == sub_menu_surf_id) return false;
    if (!title || title[0] == '\0') return false;
    return true;
}

static __attribute__((noinline)) void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";

    volatile const char *vsrc = (volatile const char *)src;
    size_t i = 0;
    while (i + 1 < dst_size) {
        char c = vsrc[i];
        if (!c) break;
        dst[i] = c;
        i++;
    }
    dst[i] = '\0';
}

static void set_default_config(taskbar_config_t *cfg) {
    if (!cfg) return;
    cfg->position_bottom = false;
    copy_string(cfg->logo_path, sizeof(cfg->logo_path), DEFAULT_LOGO_PATH);
    copy_string(cfg->date_format, sizeof(cfg->date_format), DEFAULT_DATE_FORMAT);
    cfg->active_tab_color = 0xFF383838;
    cfg->inactive_tab_color = 0xFF1F1E1E;
    cfg->gradient_top_color = 0xFF393939;
    cfg->gradient_bottom_color = 0xFF727272;
    copy_string(cfg->clock_format, sizeof(cfg->clock_format), "24h");
    cfg->taskbar_border_color = 0xFF393939;
    cfg->launcher_bg_color = 0xFF393939;
    cfg->launcher_border_color = 0xFF3C3C3C;
    cfg->launcher_search_bg_color = 0xFF727272;
    cfg->launcher_selected_bg_color = 0xFF4D4D4D;
    cfg->taskbar_button_bg_color = 0xFF181818;
}

static uint32_t parse_color(const char *value) {
    if (!value || !*value) return 0;
    while (*value == ' ' || *value == '\t') value++;

    if (*value == '#') value++;
    if (strlen(value) == 6) {
        unsigned int rgb = 0;
        sscanf(value, "%x", &rgb);
        return 0xFF000000 | rgb;
    } else if (strlen(value) == 8) {
        unsigned int rgba = 0;
        sscanf(value, "%x", &rgba);
        uint32_t r = (rgba >> 24) & 0xFF;
        uint32_t g = (rgba >> 16) & 0xFF;
        uint32_t b = (rgba >> 8) & 0xFF;
        uint32_t a = rgba & 0xFF;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
    return 0;
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

static void load_taskbar_config(const char *path, taskbar_config_t *cfg) {
    set_default_config(cfg);

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

        if (strcmp(key, "position") == 0) {
            if (strcmp(val, "bottom") == 0) {
                cfg->position_bottom = true;
            } else if (strcmp(val, "top") == 0) {
                cfg->position_bottom = false;
            }
        } else if (strcmp(key, "logo_path") == 0) {
            copy_string(cfg->logo_path, sizeof(cfg->logo_path), val);
        } else if (strcmp(key, "date_format") == 0) {
            copy_string(cfg->date_format, sizeof(cfg->date_format), val);
        } else if (strcmp(key, "active_tab_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->active_tab_color = color;
        } else if (strcmp(key, "inactive_tab_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->inactive_tab_color = color;
        } else if (strcmp(key, "gradient_top_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->gradient_top_color = color;
        } else if (strcmp(key, "gradient_bottom_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->gradient_bottom_color = color;
        } else if (strcmp(key, "clock_format") == 0) {
            copy_string(cfg->clock_format, sizeof(cfg->clock_format), val);
        } else if (strcmp(key, "taskbar_border_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->taskbar_border_color = color;
        } else if (strcmp(key, "launcher_bg_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->launcher_bg_color = color;
        } else if (strcmp(key, "launcher_border_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->launcher_border_color = color;
        } else if (strcmp(key, "launcher_search_bg_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->launcher_search_bg_color = color;
        } else if (strcmp(key, "launcher_selected_bg_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->launcher_selected_bg_color = color;
        } else if (strcmp(key, "taskbar_button_bg_color") == 0) {
            uint32_t color = parse_color(val);
            if (color) cfg->taskbar_button_bg_color = color;
        }
    }

    fclose(f);
}


static void free_image(image_t *img) {
    if (!img || !img->pixels) return;
    free(img->pixels);
    img->pixels = NULL;
    img->w = 0;
    img->h = 0;
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

static bool load_image_rgba(const char *path, image_t *out) {
    if (!out) return false;
    out->pixels = NULL;
    out->w = 0;
    out->h = 0;

    unsigned char *file_buf = NULL;
    size_t file_size = 0;
    if (!load_file_to_buffer(path, &file_buf, &file_size)) return false;

    int w = 0, h = 0, comp = 0;
    unsigned char *rgba = stbi_load_from_memory(file_buf, (int)file_size, &w, &h, &comp, 4);
    free(file_buf);
    if (!rgba || w <= 0 || h <= 0) {
        if (rgba) stbi_image_free(rgba);
        return false;
    }

    uint32_t *argb = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!argb) {
        stbi_image_free(rgba);
        return false;
    }

    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        argb[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    stbi_image_free(rgba);

    out->pixels = argb;
    out->w = w;
    out->h = h;
    return true;
}

static image_t scale_image_nearest(const image_t *src, int target_w, int target_h) {
    image_t out = {0};
    if (!src || !src->pixels || target_w <= 0 || target_h <= 0) return out;

    out.pixels = malloc((size_t)target_w * (size_t)target_h * sizeof(uint32_t));
    if (!out.pixels) return out;

    out.w = target_w;
    out.h = target_h;

    for (int y = 0; y < target_h; y++) {
        int src_y = (y * src->h) / target_h;
        for (int x = 0; x < target_w; x++) {
            int src_x = (x * src->w) / target_w;
            out.pixels[y * target_w + x] = src->pixels[src_y * src->w + src_x];
        }
    }

    return out;
}

static bool load_scaled_image(const char *path, int target_w, int target_h, image_t *out) {
    image_t raw = {0};
    if (!load_image_rgba(path, &raw)) return false;

    if (raw.w == target_w && raw.h == target_h) {
        *out = raw;
        return true;
    }

    image_t scaled = scale_image_nearest(&raw, target_w, target_h);
    free_image(&raw);

    if (!scaled.pixels) return false;
    *out = scaled;
    return true;
}

static void draw_image(uint32_t *dest, int dest_w, int dest_h, int x, int y, const image_t *img) {
    if (!dest || !img || !img->pixels) return;
    ui_blend_pixels(dest, dest_w, dest_h, x, y, img->pixels, img->w, img->h, 1.0f);
}

static bool contains_nocase(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    if (!*needle) return true;
    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (haystack[i + j] && needle[j]) {
            char c1 = haystack[i + j];
            char c2 = needle[j];
            if (c1 >= 'A' && c1 <= 'Z') c1 = c1 - 'A' + 'a';
            if (c2 >= 'A' && c2 <= 'Z') c2 = c2 - 'A' + 'a';
            if (c1 != c2) break;
            j++;
        }
        if (!needle[j]) return true;
    }
    return false;
}

static const app_entry_t *find_app_for_window(const char *title) {
    if (!title || !title[0]) return NULL;
    for (int i = 0; i < app_count; i++) {
        if (contains_nocase(title, apps[i].display_name)) {
            return &apps[i];
        }
    }
    for (int i = 0; i < app_count; i++) {
        char base_name[128];
        copy_string(base_name, sizeof(base_name), apps[i].desktop_file);
        char *dot = strchr(base_name, '.');
        if (dot) *dot = '\0';

        if (contains_nocase(title, base_name) || contains_nocase(base_name, title)) {
            return &apps[i];
        }
    }

    // 3. Match by executable name
    for (int i = 0; i < app_count; i++) {
        const char *exec_name = strrchr(apps[i].exec, '/');
        if (exec_name) exec_name++;
        else exec_name = apps[i].exec;

        char exec_base[128];
        copy_string(exec_base, sizeof(exec_base), exec_name);
        char *dot = strchr(exec_base, '.');
        if (dot) *dot = '\0';

        if (exec_base[0] && (contains_nocase(title, exec_base) || contains_nocase(exec_base, title))) {
            return &apps[i];
        }
    }

    return NULL;
}

static void add_window(uint32_t surface_id, const char *title, uint32_t state_flags, const char *icon_path) {
    if (!should_track_window(surface_id, title)) return;

    bool become_active = (state_flags & 1) != 0;
    
    char resolved_path[256] = "";
    if (icon_path && icon_path[0]) {
        copy_string(resolved_path, sizeof(resolved_path), icon_path);
    } else {
        const app_entry_t *app = find_app_for_window(title);
        if (app && app->icon_path[0]) {
            copy_string(resolved_path, sizeof(resolved_path), app->icon_path);
        } else {
            copy_string(resolved_path, sizeof(resolved_path), DEFAULT_APP_ICON_PATH);
        }
        // Propagate resolved icon to compositor
        nova_set_icon(fd, surface_id, resolved_path);
    }

    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            copy_string(windows[i].title, sizeof(windows[i].title), title);
            windows[i].state_flags = state_flags;
            windows[i].active = become_active;
            if (become_active) {
                for (int j = 0; j < window_count; j++) {
                    if (j != i) windows[j].active = false;
                }
                last_active_surface_id = surface_id;
            }
            if (resolved_path[0]) {
                free_image(&windows[i].icon_img);
                load_scaled_image(resolved_path, 16, 16, &windows[i].icon_img);
            }
            return;
        }
    }

    if (window_count < MAX_WINDOWS) {
        if (become_active) {
            for (int j = 0; j < window_count; j++) {
                windows[j].active = false;
            }
            last_active_surface_id = surface_id;
        }
        windows[window_count].surface_id = surface_id;
        copy_string(windows[window_count].title, sizeof(windows[window_count].title), title);
        windows[window_count].state_flags = state_flags;
        windows[window_count].active = become_active;
        memset(&windows[window_count].icon_img, 0, sizeof(image_t));
        if (resolved_path[0]) {
            load_scaled_image(resolved_path, 16, 16, &windows[window_count].icon_img);
        }
        window_count++;
    }
}


static bool read_rtc(int dt[6]) {
    if (!dt) return false;
    if (sys_system(SYSTEM_CMD_RTC_GET, (uint64_t)dt, 0, 0, 0) != 0) {
        return false;
    }
    return true;
}

static void append_str(char *out, size_t out_size, size_t *pos, const char *src) {
    if (!out || !pos || !src || out_size == 0) return;
    while (*src && *pos + 1 < out_size) {
        out[(*pos)++] = *src++;
    }
}

static void append_number(char *out, size_t out_size, size_t *pos, int value, int width) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%0*d", width, value);
    append_str(out, out_size, pos, tmp);
}

static uint32_t lerp_channel(uint32_t a, uint32_t b, int t, int denom) {
    int delta = (int)b - (int)a;
    return (uint32_t)((int)a + (delta * t) / denom);
}

static void format_datetime(char *out, size_t out_size, const char *fmt, const int dt[6]) {
    if (!out || out_size == 0) return;
    if (!fmt) fmt = "";

    size_t pos = 0;
    for (const char *p = fmt; *p && pos + 1 < out_size; p++) {
        if (*p == '%' && p[1]) {
            char spec = p[1];
            p++;
            switch (spec) {
                case 'Y':
                    append_number(out, out_size, &pos, dt[0], 4);
                    break;
                case 'm':
                    append_number(out, out_size, &pos, dt[1], 2);
                    break;
                case 'd':
                    append_number(out, out_size, &pos, dt[2], 2);
                    break;
                case 'H':
                    append_number(out, out_size, &pos, dt[3], 2);
                    break;
                case 'M':
                    append_number(out, out_size, &pos, dt[4], 2);
                    break;
                case 'S':
                    append_number(out, out_size, &pos, dt[5], 2);
                    break;
                case '%':
                    out[pos++] = '%';
                    break;
                default: {
                    out[pos++] = spec;
                    break;
                }
            }
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
}

static void remove_window(uint32_t surface_id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            free_image(&windows[i].icon_img);
            for (int j = i; j < window_count - 1; j++) {
                windows[j] = windows[j + 1];
            }
            window_count--;
            if (last_active_surface_id == surface_id) {
                last_active_surface_id = 0;
            }
            break;
        }
    }
}


static void update_window_focus(uint32_t surface_id, uint32_t state_flags) {
    int found_idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx < 0) return;

    for (int i = 0; i < window_count; i++) {
        if (i == found_idx) {
            windows[i].state_flags = state_flags;
            windows[i].active = (state_flags & 1) != 0;
            if (windows[i].active) {
                last_active_surface_id = surface_id;
            }
        } else {
            windows[i].active = false;
        }
    }
}

static void update_window_title(uint32_t surface_id, const char *title, const char *icon_path) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            char resolved_path[256] = "";
            if (icon_path && icon_path[0]) {
                copy_string(resolved_path, sizeof(resolved_path), icon_path);
            } else {
                const app_entry_t *app = find_app_for_window(title);
                if (app && app->icon_path[0]) {
                    copy_string(resolved_path, sizeof(resolved_path), app->icon_path);
                } else {
                    copy_string(resolved_path, sizeof(resolved_path), DEFAULT_APP_ICON_PATH);
                }
            }

            if (strcmp(windows[i].title, title) != 0 || !windows[i].icon_img.pixels) {
                copy_string(windows[i].title, sizeof(windows[i].title), title);
                free_image(&windows[i].icon_img);
                load_scaled_image(resolved_path, 16, 16, &windows[i].icon_img);
                
                if (!icon_path || strcmp(icon_path, resolved_path) != 0) {
                    nova_set_icon(fd, surface_id, resolved_path);
                }
                bar_dirty = true;
            }
            break;
        }
    }
}


static bool str_eq_ci(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool str_has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool desktop_bool_true(const char *value) {
    if (!value) return false;
    return str_eq_ci(value, "true") || str_eq_ci(value, "yes") || strcmp(value, "1") == 0;
}

static void desktop_name_from_file(const char *filename, char *out, size_t out_size) {
    copy_string(out, out_size, filename);
    char *suffix = strstr(out, DESKTOP_SUFFIX);
    if (suffix) *suffix = '\0';

    bool upper_next = true;
    for (char *p = out; *p; p++) {
        if (*p == '-' || *p == '_') {
            *p = ' ';
            upper_next = true;
        } else if (upper_next) {
            *p = (char)toupper((unsigned char)*p);
            upper_next = false;
        }
    }
}

static void strip_desktop_exec_field_codes(char *text) {
    if (!text) return;

    char out[256];
    size_t pos = 0;
    for (size_t i = 0; text[i] && pos + 1 < sizeof(out); i++) {
        if (text[i] == '%' && text[i + 1]) {
            if (text[i + 1] == '%') {
                out[pos++] = '%';
            }
            i++;
            continue;
        }
        out[pos++] = text[i];
    }
    out[pos] = '\0';
    copy_string(text, 256, trim_spaces(out));
}

static const char *read_exec_token(const char *src, char *out, size_t out_size) {
    if (!src || !out || out_size == 0) return NULL;

    while (*src == ' ' || *src == '\t') src++;

    size_t pos = 0;
    bool in_quotes = false;
    char quote = '\0';
    while (*src) {
        char c = *src;
        if (in_quotes) {
            if (c == quote) {
                in_quotes = false;
                src++;
                continue;
            }
            if (c == '\\' && src[1]) {
                src++;
                c = *src;
            }
        } else {
            if (c == '"' || c == '\'') {
                in_quotes = true;
                quote = c;
                src++;
                continue;
            }
            if (c == ' ' || c == '\t') break;
        }

        if (pos + 1 < out_size) {
            out[pos++] = c;
        }
        src++;
    }

    out[pos] = '\0';
    while (*src == ' ' || *src == '\t') src++;
    return src;
}

static bool parse_exec_command(const char *raw_exec,
                               char *out_exec, size_t out_exec_size,
                               char *out_args, size_t out_args_size) {
    if (!raw_exec || !out_exec || !out_args) return false;

    const char *args = read_exec_token(raw_exec, out_exec, out_exec_size);
    if (!args || out_exec[0] == '\0') return false;

    copy_string(out_args, out_args_size, args);
    strip_desktop_exec_field_codes(out_exec);
    strip_desktop_exec_field_codes(out_args);
    return out_exec[0] != '\0';
}

static bool load_desktop_icon(const char *icon, image_t *out) {
    if (!icon || !icon[0] || !out) return false;

    if (icon[0] == '/' || strchr(icon, '/')) {
        return load_scaled_image(icon, 16, 16, out);
    }

    const char *ext = str_has_suffix(icon, ".png") ? "" : ".png";
    char path[256];
    snprintf(path, sizeof(path), "/Library/Icons/serenityicons/16x16/%s%s", icon, ext);
    if (load_scaled_image(path, 16, 16, out)) return true;

    snprintf(path, sizeof(path), "/Library/Icons/serenityicons/32x32/%s%s", icon, ext);
    if (load_scaled_image(path, 16, 16, out)) return true;

    snprintf(path, sizeof(path), "/Library/Icons/boredos/%s%s", icon, ext);
    return load_scaled_image(path, 16, 16, out);
}

static void clear_applications(void) {
    for (int i = 0; i < app_count; i++) {
        free_image(&apps[i].icon_img);
    }
    app_count = 0;
    filtered_count = 0;
    selected_idx = 0;
}

static bool add_desktop_application(const char *desktop_file,
                                    const char *name,
                                    const char *exec,
                                    const char *icon,
                                    bool terminal,
                                    const char *category) {
    if (app_count >= MAX_APPS || !exec || !exec[0]) return false;

    char parsed_exec[256];
    char parsed_args[256];
    if (!parse_exec_command(exec, parsed_exec, sizeof(parsed_exec), parsed_args, sizeof(parsed_args))) {
        return false;
    }

    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].exec, parsed_exec) == 0 && strcmp(apps[i].args, parsed_args) == 0) {
            return false;
        }
    }

    app_entry_t app;
    memset(&app, 0, sizeof(app));
    copy_string(app.desktop_file, sizeof(app.desktop_file), desktop_file);
    if (name && name[0]) {
        copy_string(app.display_name, sizeof(app.display_name), name);
    } else {
        desktop_name_from_file(desktop_file, app.display_name, sizeof(app.display_name));
    }

    copy_string(app.exec, sizeof(app.exec), parsed_exec);
    copy_string(app.args, sizeof(app.args), parsed_args);

    copy_string(app.icon_path, sizeof(app.icon_path), icon);
    app.terminal = terminal;
    if (app.icon_path[0]) {
        load_desktop_icon(app.icon_path, &app.icon_img);
    }
    copy_string(app.category, sizeof(app.category), category);

    apps[app_count++] = app;
    return true;
}

static bool load_desktop_file(const char *path, const char *desktop_file) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char name[128] = "";
    char exec[256] = "";
    char icon[256] = "";
    char type[64] = "";
    char categories_raw[256] = "";
    bool no_display = false;
    bool hidden = false;
    bool terminal = false;
    bool in_desktop_entry = false;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *start = trim_spaces(line);
        if (*start == '\0' || *start == '#' || *start == ';') continue;

        if (*start == '[') {
            in_desktop_entry = strcmp(start, "[Desktop Entry]") == 0;
            continue;
        }
        if (!in_desktop_entry) continue;

        char *eq = strchr(start, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_spaces(start);
        char *val = trim_spaces(eq + 1);

        if (strcmp(key, "Name") == 0) {
            copy_string(name, sizeof(name), val);
        } else if (strcmp(key, "Exec") == 0) {
            copy_string(exec, sizeof(exec), val);
        } else if (strcmp(key, "Icon") == 0) {
            copy_string(icon, sizeof(icon), val);
        } else if (strcmp(key, "Type") == 0) {
            copy_string(type, sizeof(type), val);
        } else if (strcmp(key, "NoDisplay") == 0) {
            no_display = desktop_bool_true(val);
        } else if (strcmp(key, "Hidden") == 0) {
            hidden = desktop_bool_true(val);
        } else if (strcmp(key, "Terminal") == 0) {
            terminal = desktop_bool_true(val);
        } else if (strcmp(key, "Categories") == 0) {
            copy_string(categories_raw, sizeof(categories_raw), val);
        }
    }

    fclose(f);

    if (hidden || no_display) return false;
    if (type[0] && !str_eq_ci(type, "Application")) return false;

    // Resolve primary category
    char category[64] = "Utilities";
    if (categories_raw[0] != '\0') {
        char temp_cats[256];
        copy_string(temp_cats, sizeof(temp_cats), categories_raw);
        char *tok = strtok(temp_cats, ";");
        while (tok) {
            char *t = trim_spaces(tok);
            if (str_eq_ci(t, "Demo") || str_eq_ci(t, "Demos")) {
                strcpy(category, "Demos");
                break;
            } else if (str_eq_ci(t, "Development") || str_eq_ci(t, "Dev")) {
                strcpy(category, "Development");
                break;
            } else if (str_eq_ci(t, "Game") || str_eq_ci(t, "Games")) {
                strcpy(category, "Games");
                break;
            } else if (str_eq_ci(t, "Graphics")) {
                strcpy(category, "Graphics");
                break;
            } else if (str_eq_ci(t, "Network") || str_eq_ci(t, "Internet")) {
                strcpy(category, "Internet");
                break;
            } else if (str_eq_ci(t, "AudioVideo") || str_eq_ci(t, "Audio") || str_eq_ci(t, "Video") || str_eq_ci(t, "Sound") || str_eq_ci(t, "Multimedia")) {
                strcpy(category, "Sound");
                break;
            } else if (str_eq_ci(t, "Utility") || str_eq_ci(t, "Utilities") || str_eq_ci(t, "System")) {
                strcpy(category, "Utilities");
                break;
            }
            tok = strtok(NULL, ";");
        }
    }

    return add_desktop_application(desktop_file, name, exec, icon, terminal, category);
}

static void load_applications(void) {
    clear_applications();

    FAT32_FileInfo apps_dirs[128];
    int dir_count = sys_list("/Library/AppData", apps_dirs, 128);
    if (dir_count < 0) return;

    for (int i = 0; i < dir_count; i++) {
        if (!apps_dirs[i].is_directory) continue;
        if (strcmp(apps_dirs[i].name, ".") == 0 || strcmp(apps_dirs[i].name, "..") == 0) continue;

        char app_dir_path[256];
        snprintf(app_dir_path, sizeof(app_dir_path), "/Library/AppData/%s", apps_dirs[i].name);

        FAT32_FileInfo files[32];
        int file_count = sys_list(app_dir_path, files, 32);
        if (file_count < 0) continue;

        for (int j = 0; j < file_count; j++) {
            if (files[j].is_directory) continue;
            if (!str_has_suffix(files[j].name, DESKTOP_SUFFIX)) continue;

            char desktop_full_path[256];
            snprintf(desktop_full_path, sizeof(desktop_full_path), "%s/%s", app_dir_path, files[j].name);

            load_desktop_file(desktop_full_path, files[j].name);
        }
    }
}

static bool matches_filter(const char *name, const char *filter) {
    if (filter[0] == '\0') return true;

    char name_lower[128];
    char filter_lower[64];

    int i = 0;
    while (name[i] && i < (int)sizeof(name_lower) - 1) {
        name_lower[i] = (char)tolower((unsigned char)name[i]);
        i++;
    }
    name_lower[i] = '\0';

    i = 0;
    while (filter[i] && i < (int)sizeof(filter_lower) - 1) {
        filter_lower[i] = (char)tolower((unsigned char)filter[i]);
        i++;
    }
    filter_lower[i] = '\0';

    return strstr(name_lower, filter_lower) != NULL;
}

static void apply_filter(void) {
    filtered_count = 0;
    for (int i = 0; i < app_count; i++) {
        if (matches_filter(apps[i].display_name, search_buf)) {
            filtered_apps[filtered_count] = apps[i];
            filtered_count++;
        }
    }
    int max_idx = filtered_count - 1;
    if (selected_idx > max_idx) {
        selected_idx = max_idx;
    }
    if (selected_idx < 0) selected_idx = 0;
}

static void search_backspace_utf8(void) {
    if (search_len <= 0) return;

    const char *prev = text_prev_utf8(search_buf, search_buf + search_len);
    int prev_pos = (int)(prev - search_buf);
    if (prev_pos < 0 || prev_pos >= search_len) {
        prev_pos = search_len - 1;
    }
    search_len = prev_pos;
    search_buf[search_len] = '\0';
}

static bool search_append_utf8(const char *text, uint8_t text_len) {
    if (!text || text_len == 0) return false;
    if (text_len > 4) text_len = 4;
    if (search_len + text_len >= (int)sizeof(search_buf)) return false;

    memcpy(search_buf + search_len, text, text_len);
    search_len += text_len;
    search_buf[search_len] = '\0';
    return true;
}

static bool socket_readable(struct pollfd *pfd) {
    if (!pfd) return false;

    pfd->revents = 0;
    int pr = poll(pfd, 1, 0);
    return pr > 0 && (pfd->revents & POLLIN);
}

static int get_approx_string_width(const char *str) {
    if (!str) return 0;
    int len = 0;
    while (str[len]) len++;
    int width = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c == ':' || c == '.' || c == ' ' || c == 'i' || c == 'l' || c == '1') {
            width += 4;
        } else if (c == '-' || c == '[' || c == ']') {
            width += 6;
        } else if (c == 'm' || c == 'w' || c == 'M' || c == 'W') {
            width += 10;
        } else {
            width += 8;
        }
    }
    return width;
}

static int get_approx_char_width(char c) {
    if (c == ':' || c == '.' || c == ' ' || c == 'i' || c == 'l' || c == '1') {
        return 4;
    } else if (c == '-' || c == '[' || c == ']') {
        return 6;
    } else if (c == 'm' || c == 'w' || c == 'M' || c == 'W') {
        return 10;
    }
    return 8;
}



static void draw_taskbar(void) {
    for (uint32_t y = 0; y < bar_h; y++) {
        uint32_t *row = &bar_pixels[y * bar_w];
        for (uint32_t x = 0; x < bar_w; x++) {
            row[x] = 0xFFB0B0B0;
        }
    }

    for (uint32_t x = 0; x < bar_w; x++) {
        bar_pixels[x] = 0xFFFFFFFF;
        bar_pixels[bar_w + x] = 0xFFDFDFDF;
    }

    int start_btn_y = 2;
    ui_draw_panel(bar_pixels, bar_w, bar_h, START_BTN_X, start_btn_y, START_BTN_W, START_BTN_H, 0xFFB0B0B0, menu_open ? 1 : 0, 1);
    
    int btn_offset = menu_open ? 1 : 0;
    int logo_w = logo_img.pixels ? logo_img.w : 0;
    int logo_x = START_BTN_X + 6 + btn_offset;
    int logo_y = start_btn_y + (START_BTN_H - 16) / 2 + btn_offset;
    if (logo_img.pixels) {
        draw_image(bar_pixels, bar_w, bar_h, logo_x, logo_y, &logo_img);
    }
    
    int text_x = logo_x + (logo_w ? logo_w + 6 : 0);
    ui_draw_string_rect(bar_pixels, bar_w, bar_h, text_x, start_btn_y + btn_offset - 1, START_BTN_W - (text_x - START_BTN_X) - 2, START_BTN_H, "BoredOS", 0xFF000000);
    ui_draw_string_rect(bar_pixels, bar_w, bar_h, text_x + 1, start_btn_y + btn_offset - 1, START_BTN_W - (text_x - START_BTN_X) - 2, START_BTN_H, "BoredOS", 0xFF000000);

    int tab_x = START_BTN_X + START_BTN_W + 10;
    int tab_end = (int)bar_w - CLOCK_W - 10;

    int tab_w = MAX_TAB_W;
    int max_needed = window_count * (MAX_TAB_W + TAB_GAP);
    int space_avail = tab_end - tab_x;
    if (window_count > 0 && max_needed > space_avail) {
        tab_w = space_avail / window_count - TAB_GAP;
        if (tab_w < 32) tab_w = 32;
    }

    for (int i = 0; i < window_count; i++) {
        if (tab_x + tab_w > tab_end) break;

        bool active = windows[i].active;
        ui_draw_panel(bar_pixels, bar_w, bar_h, tab_x, 2, tab_w, TAB_H, active ? 0xFF989898 : 0xFFB0B0B0, active ? 1 : 0, 1);

        int offset = active ? 1 : 0;
        int icon_x = tab_x + 6 + offset;
        int icon_y = 2 + (TAB_H - 16) / 2 + offset;
        if (windows[i].icon_img.pixels) {
            draw_image(bar_pixels, bar_w, bar_h, icon_x, icon_y, &windows[i].icon_img);
        } else if (app_icon_img.pixels) {
            draw_image(bar_pixels, bar_w, bar_h, icon_x, icon_y, &app_icon_img);
        }

        if (tab_w > 48) {
            int title_x = icon_x + 20;
            int title_w = tab_w - 30;
            ui_draw_string_rect(bar_pixels, bar_w, bar_h, title_x, 2 + offset, title_w, TAB_H, windows[i].title, 0xFF000000);
        }

        tab_x += tab_w + TAB_GAP;
    }

    int clock_x = (int)bar_w - CLOCK_W - 4;
    ui_draw_panel(bar_pixels, bar_w, bar_h, clock_x, 2, CLOCK_W, CLOCK_H, 0xFFB0B0B0, 1, 1);

    char time_str[32] = "--:--";
    int dt[6] = {0};

    if (read_rtc(dt)) {
        if (strcmp(config.clock_format, "12h") == 0) {
            int hour = dt[3];
            const char *am_pm = (hour >= 12) ? "PM" : "AM";
            int hour12 = hour % 12;
            if (hour12 == 0) hour12 = 12;
            snprintf(time_str, sizeof(time_str), "%d:%02d %s", hour12, dt[4], am_pm);
        } else {
            snprintf(time_str, sizeof(time_str), "%02d:%02d", dt[3], dt[4]);
        }
    }

    ui_draw_string_rect_centered(bar_pixels, bar_w, bar_h, clock_x, 2, CLOCK_W, CLOCK_H, time_str, 0xFF000000);

    NovaRect damage = { 0, 0, bar_w, bar_h };
    nova_damage_surface(fd, bar_surf_id, 1, &damage);
}


static int get_apps_in_category(const char *cat_name, const app_entry_t **out_apps) {
    int count = 0;
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].desktop_file, "about.desktop") == 0) continue;
        if (strcmp(apps[i].category, cat_name) == 0) {
            if (out_apps) out_apps[count] = &apps[i];
            count++;
        }
    }
    return count;
}

static void rebuild_start_menu_items(void) {
    start_menu_item_count = 0;
    int y = 2; 
        start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_ABOUT,
        .label = "About BoredOS",
        .category_idx = -1,
        .y = y,
        .h = 24
    };
    y += 24;
        start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_SEPARATOR,
        .label = "",
        .category_idx = -1,
        .y = y,
        .h = 8
    };
    y += 8;
    
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (get_apps_in_category(categories[i], NULL) > 0) {
            start_menu_items[start_menu_item_count++] = (StartMenuItem){
                .type = ITEM_CATEGORY,
                .label = "",
                .category_idx = i,
                .y = y,
                .h = 24
            };
            strcpy(start_menu_items[start_menu_item_count - 1].label, categories[i]);
            y += 24;
        }
    }
    
    start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_SEPARATOR,
        .label = "",
        .category_idx = -1,
        .y = y,
        .h = 8
    };
    y += 8;
    
    start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_RUN,
        .label = "Run...",
        .category_idx = -1,
        .y = y,
        .h = 24
    };
    y += 24;
        start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_SEPARATOR,
        .label = "",
        .category_idx = -1,
        .y = y,
        .h = 8
    };
    y += 8;
    
    start_menu_items[start_menu_item_count++] = (StartMenuItem){
        .type = ITEM_EXIT,
        .label = "Exit...",
        .category_idx = -1,
        .y = y,
        .h = 24
    };
    y += 24;
    
    main_menu_active_height = y + 2;
}

static int menu_visible_y(void);
static void draw_submenu(void);

static int get_submenu_y(int item_idx) {
    int main_y = menu_visible_y();
    if (item_idx < 0 || item_idx >= start_menu_item_count) {
        return main_y;
    }
    
    int cat_idx = start_menu_items[item_idx].category_idx;
    if (cat_idx < 0 || cat_idx >= NUM_CATEGORIES) {
        return main_y;
    }
    
    int y_offset_main = config.position_bottom ? (MENU_H - main_menu_active_height) : 0;
    int item_y = start_menu_items[item_idx].y + y_offset_main;
    
    int sub_y = 0;
    if (config.position_bottom) {
        const app_entry_t *sub_apps[32];
        int sub_app_count = get_apps_in_category(categories[cat_idx], sub_apps);
        int sub_menu_active_height = 4 + sub_app_count * 24;
        
        sub_y = main_y + item_y + start_menu_items[item_idx].h - SUB_MENU_H;
        
        int min_sub_y = sub_menu_active_height - SUB_MENU_H;
        if (sub_y < min_sub_y) {
            sub_y = min_sub_y;
        }
    } else {
        sub_y = main_y + item_y - 2;
        if (sub_y + SUB_MENU_H > screen_h) {
            sub_y = screen_h - SUB_MENU_H;
        }
        if (sub_y < 0) sub_y = 0;
    }
    return sub_y;
}

static void draw_menu(void) {
    if (!menu_pixels) return;

    memset(menu_pixels, 0, MENU_W * MENU_H * 4);
    rebuild_start_menu_items();

    int y_offset = config.position_bottom ? (MENU_H - main_menu_active_height) : 0;
    ui_draw_panel(menu_pixels, MENU_W, MENU_H, 0, y_offset, MENU_W, main_menu_active_height, 0xFFFFFFFF, 0, 1);

    for (int y = y_offset + 2; y < y_offset + main_menu_active_height - 2; y++) {
        for (int x = 2; x <= 31; x++) {
            menu_pixels[y * MENU_W + x] = 0xFFB0B0B0; 
        }
        menu_pixels[y * MENU_W + 32] = 0xFF808080; 
    }

    for (int i = 0; i < start_menu_item_count; i++) {
        StartMenuItem item = start_menu_items[i];
        int item_y = item.y + y_offset;

        if (item.type == ITEM_SEPARATOR) {
            int mid_y = item_y + item.h / 2;
            for (int px = 33; px < MENU_W - 3; px++) {
                menu_pixels[mid_y * MENU_W + px] = 0xFFC0C0C0; 
            }
        } else {
            bool hovered = (i == main_menu_hover_idx);
            uint32_t text_col = hovered ? 0xFFFFFFFF : 0xFF000000;

            if (hovered) {
                ui_draw_panel(menu_pixels, MENU_W, MENU_H, 33, item_y, MENU_W - 36, item.h, 0xFF000080, 0, 0);
            }

            const image_t *icon_img = NULL;
            if (item.type == ITEM_ABOUT) {
                icon_img = boredos_icon_img.pixels ? &boredos_icon_img : &app_icon_img;
            } else if (item.type == ITEM_CATEGORY) {
                icon_img = category_icon_imgs[item.category_idx].pixels ? &category_icon_imgs[item.category_idx] : &app_icon_img;
            } else if (item.type == ITEM_RUN) {
                icon_img = run_icon_img.pixels ? &run_icon_img : &app_icon_img;
            } else if (item.type == ITEM_EXIT) {
                icon_img = exit_icon_img.pixels ? &exit_icon_img : &app_icon_img;
            }

            if (icon_img && icon_img->pixels) {
                int icon_size = icon_img->w;
                int icon_x = 9; 
                int icon_y = item_y + (item.h - icon_size) / 2;
                draw_image(menu_pixels, MENU_W, MENU_H, icon_x, icon_y, icon_img);
            }

            ui_draw_string(menu_pixels, MENU_W, MENU_H, 42, item_y, item.label, text_col);

            if (item.type == ITEM_CATEGORY) {
                int arrow_x = MENU_W - 14;
                int arrow_y = item_y + (item.h - 8) / 2;
                for (int ay = 0; ay < 9; ay++) {
                    int w = 5 - abs(4 - ay);
                    for (int ax = 0; ax < w; ax++) {
                        menu_pixels[(arrow_y + ay) * MENU_W + (arrow_x + ax)] = text_col;
                    }
                }
            }
        }
    }

    NovaRect damage = { 0, 0, MENU_W, MENU_H };
    nova_damage_surface(fd, menu_surf_id, 1, &damage);
}

static void draw_submenu(void) {
    if (!sub_menu_pixels || sub_menu_category_idx < 0 || sub_menu_category_idx >= NUM_CATEGORIES) return;

    const app_entry_t *sub_apps[32];
    int sub_app_count = get_apps_in_category(categories[sub_menu_category_idx], sub_apps);
    int sub_menu_active_height = 4 + sub_app_count * 24;

    memset(sub_menu_pixels, 0, SUB_MENU_W * SUB_MENU_H * 4);
    
    int y_offset = config.position_bottom ? (SUB_MENU_H - sub_menu_active_height) : 0;
    ui_draw_panel(sub_menu_pixels, SUB_MENU_W, SUB_MENU_H, 0, y_offset, SUB_MENU_W, sub_menu_active_height, 0xFFFFFFFF, 0, 1);

    int y = y_offset + 2;
    for (int i = 0; i < sub_app_count; i++) {
        const app_entry_t *app = sub_apps[i];
        bool hovered = (i == sub_menu_hover_idx);
        uint32_t text_col = hovered ? 0xFFFFFFFF : 0xFF000000;

        if (hovered) {
            ui_draw_panel(sub_menu_pixels, SUB_MENU_W, SUB_MENU_H, 2, y, SUB_MENU_W - 4, 24, 0xFF000080, 0, 0);
        }

        const image_t *icon_img = app->icon_img.pixels ? &app->icon_img : &app_icon_img;
        int icon_size = icon_img->pixels ? icon_img->w : 0;
        int icon_x = 8;
        int icon_y = y + (24 - icon_size) / 2;
        if (icon_img->pixels) {
            draw_image(sub_menu_pixels, SUB_MENU_W, SUB_MENU_H, icon_x, icon_y, icon_img);
        }

        ui_draw_string(sub_menu_pixels, SUB_MENU_W, SUB_MENU_H, 34, y, app->display_name, text_col);
        y += 24;
    }

    NovaRect damage = { 0, 0, SUB_MENU_W, SUB_MENU_H };
    nova_damage_surface(fd, sub_menu_surf_id, 1, &damage);
}

static int menu_hidden_x(void) {
    return -MENU_W - 16;
}

static int menu_hidden_y(void) {
    return config.position_bottom ? screen_h + 16 : -MENU_H - 16;
}

static int menu_visible_y(void) {
    int menu_y = config.position_bottom ? (screen_h - (int)bar_h - MENU_H) : (int)bar_h;
    return menu_y < 0 ? 0 : menu_y;
}

static void close_menu(void);

static void destroy_menu_surface(void) {
    if (menu_surf_id) {
        nova_destroy_surface(fd, menu_surf_id);
        menu_surf_id = 0;
    }
    if (menu_pixels) {
        munmap(menu_pixels, MENU_W * MENU_H * 4);
        menu_pixels = NULL;
    }
    if (sub_menu_surf_id) {
        nova_destroy_surface(fd, sub_menu_surf_id);
        sub_menu_surf_id = 0;
    }
    if (sub_menu_pixels) {
        munmap(sub_menu_pixels, SUB_MENU_W * SUB_MENU_H * 4);
        sub_menu_pixels = NULL;
    }
    menu_open = false;
    sub_menu_open = false;
}

static void close_taskbar(void) {
    close_menu();
    destroy_menu_surface();
    clear_applications();
    if (bar_surf_id) {
        nova_destroy_surface(fd, bar_surf_id);
        bar_surf_id = 0;
    }
    if (bar_pixels) {
        munmap(bar_pixels, bar_w * bar_h * 4);
        bar_pixels = NULL;
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    exit(0);
}

static bool spawn_app_entry(const app_entry_t *app) {
    if (!app || !app->exec[0]) return false;

    const char *args = app->args[0] ? app->args : NULL;
    uint64_t flags = SPAWN_FLAG_INHERIT_TTY;
    if (app->terminal) {
        flags |= SPAWN_FLAG_TERMINAL;
    }

    int pid = sys_spawn(app->exec, args, flags, 0);
    if (pid > 0) return true;

    if (app->exec[0] != '/') {
        char path[256];
        snprintf(path, sizeof(path), "/bin/%s", app->exec);
        pid = sys_spawn(path, args, flags, 0);
        if (pid > 0) return true;

        if (!str_has_suffix(app->exec, ".elf")) {
            snprintf(path, sizeof(path), "/bin/%s.elf", app->exec);
            pid = sys_spawn(path, args, flags, 0);
            if (pid > 0) return true;
        }
    }

    return false;
}


static bool ensure_submenu_surface(void) {
    if (sub_menu_surf_id && sub_menu_pixels) return true;

    char shm_path[128];
    if (nova_create_surface(fd, SUB_MENU_W, SUB_MENU_H, MENU_LAYER, SURFACE_FLAG_TRANSPARENT, &sub_menu_surf_id, shm_path) < 0) {
        return false;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        nova_destroy_surface(fd, sub_menu_surf_id);
        sub_menu_surf_id = 0;
        return false;
    }

    sub_menu_pixels = mmap(NULL, SUB_MENU_W * SUB_MENU_H * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (sub_menu_pixels == MAP_FAILED) {
        sub_menu_pixels = NULL;
        nova_destroy_surface(fd, sub_menu_surf_id);
        sub_menu_surf_id = 0;
        return false;
    }

    nova_move_surface(fd, sub_menu_surf_id, -SUB_MENU_W - 16, 0);
    return true;
}

static void handle_menu_pointer(int px, int py, uint32_t buttons) {
    (void)px;
    if (!menu_open) return;

    bool left_pressed = (buttons & 1) != 0;
    bool left_went_down = left_pressed && ((last_menu_buttons & 1) == 0);
    last_menu_buttons = buttons;

    int y_offset_main = config.position_bottom ? (MENU_H - main_menu_active_height) : 0;
    int new_hover_idx = -1;
    for (int i = 0; i < start_menu_item_count; i++) {
        StartMenuItem item = start_menu_items[i];
        if (py >= item.y + y_offset_main && py < item.y + y_offset_main + item.h) {
            if (item.type != ITEM_SEPARATOR) {
                new_hover_idx = i;
            }
            break;
        }
    }

    if (new_hover_idx != main_menu_hover_idx) {
        main_menu_hover_idx = new_hover_idx;
        
        if (new_hover_idx >= 0 && start_menu_items[new_hover_idx].type == ITEM_CATEGORY) {
            int cat_idx = start_menu_items[new_hover_idx].category_idx;
            sub_menu_category_idx = cat_idx;
            sub_menu_hover_idx = -1;
            sub_menu_open = true;
            
            if (ensure_submenu_surface()) {
                int main_x = 0;
                int sub_x = main_x + MENU_W - 2;
                int sub_y = get_submenu_y(new_hover_idx);
                
                nova_move_surface(fd, sub_menu_surf_id, sub_x, sub_y);
                draw_submenu();
            }
        } else {
            sub_menu_open = false;
            sub_menu_category_idx = -1;
            sub_menu_hover_idx = -1;
            if (sub_menu_surf_id) {
                nova_move_surface(fd, sub_menu_surf_id, -SUB_MENU_W - 16, 0);
            }
        }
        draw_menu();
    }

    if (left_went_down && new_hover_idx >= 0) {
        StartMenuItem item = start_menu_items[new_hover_idx];
        if (item.type == ITEM_ABOUT) {
            for (int i = 0; i < app_count; i++) {
                if (strcmp(apps[i].desktop_file, "about.desktop") == 0) {
                    spawn_app_entry(&apps[i]);
                    break;
                }
            }
            close_menu();
        } else if (item.type == ITEM_RUN) {
            for (int i = 0; i < app_count; i++) {
                if (strcmp(apps[i].desktop_file, "run.desktop") == 0) {
                    spawn_app_entry(&apps[i]);
                    break;
                }
            }
            close_menu();
        } else if (item.type == ITEM_EXIT) {
            nova_quit(fd);
            close_taskbar();
            close_menu();
        }
    }
}

static void handle_submenu_pointer(int px, int py, uint32_t buttons) {
    (void)px;
    if (!menu_open || !sub_menu_open || sub_menu_category_idx < 0) return;

    bool left_pressed = (buttons & 1) != 0;
    bool left_went_down = left_pressed && ((last_menu_buttons & 1) == 0);
    last_menu_buttons = buttons;

    const app_entry_t *sub_apps[32];
    int sub_app_count = get_apps_in_category(categories[sub_menu_category_idx], sub_apps);

    int y_offset_sub = config.position_bottom ? (SUB_MENU_H - (4 + sub_app_count * 24)) : 0;
    int new_hover_idx = -1;
    int y = y_offset_sub + 2;
    for (int i = 0; i < sub_app_count; i++) {
        if (py >= y && py < y + 24) {
            new_hover_idx = i;
            break;
        }
        y += 24;
    }

    if (new_hover_idx != sub_menu_hover_idx) {
        sub_menu_hover_idx = new_hover_idx;
        draw_submenu();
    }

    if (left_went_down && new_hover_idx >= 0) {
        const app_entry_t *app = sub_apps[new_hover_idx];
        spawn_app_entry(app);
        close_menu();
    }
}

static void close_menu(void) {
    if (!menu_open) return;
    menu_open = false;
    sub_menu_open = false;
    sub_menu_category_idx = -1;
    sub_menu_hover_idx = -1;
    main_menu_hover_idx = -1;
    last_menu_buttons = 0;
    if (menu_surf_id) {
        nova_set_state(fd, menu_surf_id, 0);
        nova_move_surface(fd, menu_surf_id, menu_hidden_x(), menu_hidden_y());
    }
    if (sub_menu_surf_id) {
        nova_set_state(fd, sub_menu_surf_id, 0);
        nova_move_surface(fd, sub_menu_surf_id, -SUB_MENU_W - 16, 0);
    }

    if (resume_focus_id != 0) {
        nova_set_state(fd, resume_focus_id, 1 /* active focused state */);
        resume_focus_id = 0;
    }
}

static bool ensure_menu_surface(void) {
    if (menu_surf_id && menu_pixels) return true;

    char shm_path[128];
    if (nova_create_surface(fd, MENU_W, MENU_H, MENU_LAYER, SURFACE_FLAG_TRANSPARENT, &menu_surf_id, shm_path) < 0) {
        return false;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        nova_destroy_surface(fd, menu_surf_id);
        menu_surf_id = 0;
        return false;
    }

    menu_pixels = mmap(NULL, MENU_W * MENU_H * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (menu_pixels == MAP_FAILED) {
        menu_pixels = NULL;
        nova_destroy_surface(fd, menu_surf_id);
        menu_surf_id = 0;
        return false;
    }

    nova_move_surface(fd, menu_surf_id, menu_hidden_x(), menu_hidden_y());
    return true;
}

static void open_menu(void) {
    if (menu_open) return;
    if (!ensure_menu_surface()) return;

    resume_focus_id = last_active_surface_id;
    last_menu_buttons = 0;
    main_menu_hover_idx = -1;
    sub_menu_open = false;
    sub_menu_category_idx = -1;
    sub_menu_hover_idx = -1;
    sub_menu_focused = false;
    
    menu_open = true;
    nova_move_surface(fd, menu_surf_id, 0, menu_visible_y());
    draw_menu();
    nova_set_state(fd, menu_surf_id, 1 /* focused */);
}

static void handle_bar_click(int click_x, int click_y) {
    int start_btn_y = 2;
    bool on_start = (click_x >= START_BTN_X && click_x < START_BTN_X + START_BTN_W &&
                     click_y >= start_btn_y && click_y < start_btn_y + START_BTN_H);

    if (on_start) {
        if (menu_open) {
            close_menu();
        } else {
            open_menu();
        }
        return;
    }

    int tab_x = START_BTN_X + START_BTN_W + 10;
    int tab_end = (int)bar_w - CLOCK_W - 10;

    int tab_w = MAX_TAB_W;
    int max_needed = window_count * (MAX_TAB_W + TAB_GAP);
    int space_avail = tab_end - tab_x;
    if (window_count > 0 && max_needed > space_avail) {
        tab_w = space_avail / window_count - TAB_GAP;
        if (tab_w < 32) tab_w = 32;
    }

    for (int i = 0; i < window_count; i++) {
        if (tab_x + tab_w > tab_end) break;

        if (click_x >= tab_x && click_x < tab_x + tab_w &&
            click_y >= 2 && click_y < 2 + TAB_H) {
            nova_set_state(fd, windows[i].surface_id, 1 /* active focused state */);
            for (int j = 0; j < window_count; j++) {
                windows[j].active = (j == i);
            }
            draw_taskbar();
            if (menu_open) {
                close_menu();
            }
            return;
        }
        tab_x += tab_w + TAB_GAP;
    }
}




static void handle_menu_key(const NovaEvent *ev) {
    if (!menu_open || !ev) return;

    uint32_t kc = ev->data.key.keycode;
    uint8_t pressed = ev->data.key.pressed;
    if (!pressed) return;

    if (kc == KEY_ESCAPE) {
        close_menu();
        return;
    }

    if (sub_menu_open && sub_menu_focused) {
        const app_entry_t *sub_apps[32];
        int sub_app_count = get_apps_in_category(categories[sub_menu_category_idx], sub_apps);
        if (sub_app_count == 0) return;

        if (kc == KEY_UP) {
            if (sub_menu_hover_idx > 0) {
                sub_menu_hover_idx--;
            } else {
                sub_menu_hover_idx = sub_app_count - 1;
            }
            draw_submenu();
        } else if (kc == KEY_DOWN) {
            if (sub_menu_hover_idx < sub_app_count - 1) {
                sub_menu_hover_idx++;
            } else {
                sub_menu_hover_idx = 0;
            }
            draw_submenu();
        } else if (kc == KEY_LEFT) {
            sub_menu_focused = false;
            sub_menu_hover_idx = -1;
            draw_submenu();
            draw_menu();
        } else if (kc == KEY_ENTER) {
            if (sub_menu_hover_idx >= 0 && sub_menu_hover_idx < sub_app_count) {
                spawn_app_entry(sub_apps[sub_menu_hover_idx]);
                close_menu();
            }
        }
    } else {
        if (kc == KEY_UP) {
            int idx = main_menu_hover_idx;
            do {
                if (idx > 0) idx--;
                else idx = start_menu_item_count - 1;
            } while (idx != main_menu_hover_idx && start_menu_items[idx].type == ITEM_SEPARATOR);
            
            if (start_menu_items[idx].type != ITEM_SEPARATOR) {
                main_menu_hover_idx = idx;
                if (start_menu_items[idx].type == ITEM_CATEGORY) {
                    sub_menu_category_idx = start_menu_items[idx].category_idx;
                    sub_menu_open = true;
                    sub_menu_hover_idx = -1;
                    if (ensure_submenu_surface()) {
                        int main_x = 0;
                        int sub_x = main_x + MENU_W - 2;
                        int sub_y = get_submenu_y(idx);
                        nova_move_surface(fd, sub_menu_surf_id, sub_x, sub_y);
                        draw_submenu();
                    }
                } else {
                    sub_menu_open = false;
                    sub_menu_category_idx = -1;
                    sub_menu_hover_idx = -1;
                    if (sub_menu_surf_id) {
                        nova_move_surface(fd, sub_menu_surf_id, -SUB_MENU_W - 16, 0);
                    }
                }
                draw_menu();
            }
        } else if (kc == KEY_DOWN) {
            int idx = main_menu_hover_idx;
            do {
                if (idx < start_menu_item_count - 1) idx++;
                else idx = 0;
            } while (idx != main_menu_hover_idx && start_menu_items[idx].type == ITEM_SEPARATOR);

            if (start_menu_items[idx].type != ITEM_SEPARATOR) {
                main_menu_hover_idx = idx;
                if (start_menu_items[idx].type == ITEM_CATEGORY) {
                    sub_menu_category_idx = start_menu_items[idx].category_idx;
                    sub_menu_open = true;
                    sub_menu_hover_idx = -1;
                    if (ensure_submenu_surface()) {
                        int main_x = 0;
                        int sub_x = main_x + MENU_W - 2;
                        int sub_y = get_submenu_y(idx);
                        nova_move_surface(fd, sub_menu_surf_id, sub_x, sub_y);
                        draw_submenu();
                    }
                } else {
                    sub_menu_open = false;
                    sub_menu_category_idx = -1;
                    sub_menu_hover_idx = -1;
                    if (sub_menu_surf_id) {
                        nova_move_surface(fd, sub_menu_surf_id, -SUB_MENU_W - 16, 0);
                    }
                }
                draw_menu();
            }
        } else if (kc == KEY_RIGHT) {
            if (main_menu_hover_idx >= 0 && start_menu_items[main_menu_hover_idx].type == ITEM_CATEGORY) {
                sub_menu_focused = true;
                sub_menu_hover_idx = 0;
                draw_submenu();
                draw_menu();
            }
        } else if (kc == KEY_ENTER) {
            if (main_menu_hover_idx >= 0) {
                StartMenuItem item = start_menu_items[main_menu_hover_idx];
                if (item.type == ITEM_ABOUT) {
                    for (int i = 0; i < app_count; i++) {
                        if (strcmp(apps[i].desktop_file, "about.desktop") == 0) {
                            spawn_app_entry(&apps[i]);
                            break;
                        }
                    }
                    close_menu();
                } else if (item.type == ITEM_RUN) {
                    for (int i = 0; i < app_count; i++) {
                        if (strcmp(apps[i].desktop_file, "run.desktop") == 0) {
                            spawn_app_entry(&apps[i]);
                            break;
                        }
                    }
                    close_menu();
                } else if (item.type == ITEM_EXIT) {
                    nova_quit(fd);
                    close_taskbar();
                    close_menu();
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    theme_load("/Library/AppData/org.boredos.nova/nova.conf", &theme);
    ui_font_init(theme.font_path, theme.font_size);

    load_taskbar_config("/Library/AppData/org.boredos.nova/taskbar.conf", &config);

    load_scaled_image(config.logo_path, 16, 16, &logo_img);
    load_scaled_image(DEFAULT_APP_ICON_PATH, 16, 16, &app_icon_img);
    load_scaled_image("/Library/Icons/boredos/bos.png", 16, 16, &boredos_icon_img);
    load_scaled_image("/Library/Icons/serenityicons/16x16/app-run.png", 16, 16, &run_icon_img);
    load_scaled_image("/Library/Icons/serenityicons/16x16/power.png", 16, 16, &exit_icon_img);
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        load_scaled_image(category_icons[i], 16, 16, &category_icon_imgs[i]);
    }

    load_applications();

    fd = nova_connect(NULL);
    if (fd < 0) {
        fprintf(stderr, "Taskbar Error: Cannot connect to Nova socket\n");
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0) {
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            screen_w = vinfo.xres;
            screen_h = vinfo.yres;
        }
        close(fb);
    }

    bar_w = (uint32_t)screen_w;

    char shm_path[128];
    if (nova_create_surface(fd, bar_w, bar_h, TASKBAR_LAYER, 0, &bar_surf_id, shm_path) < 0) {
        fprintf(stderr, "Taskbar Error: Surface allocation failed\n");
        close(fd);
        return 1;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        fprintf(stderr, "Taskbar Error: Cannot open SHM segment %s\n", shm_path);
        close(fd);
        return 1;
    }

    bar_pixels = mmap(NULL, bar_w * bar_h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (bar_pixels == MAP_FAILED) {
        fprintf(stderr, "Taskbar Error: mmap failed\n");
        close(fd);
        return 1;
    }

    int bar_y = config.position_bottom ? (screen_h - TASKBAR_HEIGHT) : 0;
    nova_move_surface(fd, bar_surf_id, 0, bar_y);

    window_count = 0;
    nova_query_windows(fd);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    uint32_t last_clock_tick = 0;
    bar_dirty = true;

    while (1) {
        int timeout = 200;
        if (nova_pending_events()) {
            timeout = 0;
        }
        int pr = poll(&pfd, 1, timeout);

        uint32_t now = sys_system(SYSTEM_CMD_GET_TICKS, 0, 0, 0, 0) * 16;
        if (now - last_clock_tick >= 1000) {
            last_clock_tick = now;
            bar_dirty = true;
        }

        if ((pr > 0 && (pfd.revents & POLLIN)) || nova_pending_events()) {
            NovaEvent ev;
            bool needs_draw = false;
            while (nova_pending_events() || socket_readable(&pfd)) {
                if (nova_poll_event(fd, &ev) == 0) {
                    switch (ev.type) {
                        case EVT_WINDOW_CREATED:
                            add_window(ev.surface_id, ev.data.window.title, ev.data.window.state_flags, ev.data.window.icon_path);
                            needs_draw = true;
                            break;

                        case EVT_WINDOW_DESTROYED:
                            last_bar_buttons = 0;
                            last_menu_buttons = 0;
                            remove_window(ev.surface_id);
                            needs_draw = true;
                            break;
                        case EVT_WINDOW_TITLE_CHANGED:
                            update_window_title(ev.surface_id, ev.data.window.title, ev.data.window.icon_path);
                            needs_draw = true;
                            break;
                        case EVT_STATE_CHANGED:
                            if ((ev.data.state.state_flags & 1) == 0) {
                                last_bar_buttons = 0;
                                last_menu_buttons = 0;
                            }
                            update_window_focus(ev.surface_id, ev.data.state.state_flags);
                            needs_draw = true;
                            break;
                        case EVT_POINTER: {
                            uint32_t buttons = ev.data.pointer.buttons;
                            if (ev.surface_id == bar_surf_id) {
                                bool left_pressed = (buttons & 1) != 0;
                                bool left_went_down = left_pressed && ((last_bar_buttons & 1) == 0);
                                last_bar_buttons = buttons;
                                if (left_went_down && now - last_bar_click_ms > 80) {
                                    last_bar_click_ms = now;
                                    handle_bar_click(ev.data.pointer.x, ev.data.pointer.y);
                                }
                            } else if (ev.surface_id == menu_surf_id) {
                                handle_menu_pointer(ev.data.pointer.x, ev.data.pointer.y, buttons);
                            } else if (sub_menu_open && ev.surface_id == sub_menu_surf_id) {
                                handle_submenu_pointer(ev.data.pointer.x, ev.data.pointer.y, buttons);
                            }
                            break;
                        }
                        case EVT_KEY:
                            if (menu_open && ev.surface_id == menu_surf_id) {
                                handle_menu_key(&ev);
                            }
                            break;
                        case EVT_FOCUS_OUT:
                            if (menu_open && ev.surface_id == menu_surf_id) {
                                close_menu();
                            }
                            break;
                        default:
                            break;
                    }
                } else {
                    break;
                }
            }
            if (needs_draw) {
                bar_dirty = true;
            }
        }

        if (bar_dirty) {
            bar_dirty = false;
            draw_taskbar();
        }
    }

    return 0;
}
