// BOREDOS_APP_DESC: Taskbar with start menu, window list, and clock.
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
#include <ctype.h>
#include <syscall.h>
#include "libtheme/theme.h"
#include "libui/ui.h"
#include "libnovaproto/novaproto.h"
#include "stb_image.h"

#define MAX_WINDOWS 32
#define MAX_APPS 128
#define ITEM_HEIGHT 30

#define TASKBAR_HEIGHT 26
#define TASKBAR_LAYER 3
#define MENU_LAYER 4

#define START_BTN_X 0
#define START_BTN_W 36
#define START_BTN_H 26

#define TAB_H TASKBAR_HEIGHT
#define TAB_W 32
#define MIN_TAB_W 32
#define MAX_TAB_W 32
#define TAB_GAP 0
#define TAB_PADDING 4
#define TAB_ICON_TEXT_SPACING 0

#define CLOCK_W 80
#define CLOCK_H 26

#define MENU_W 400
#define MENU_H 360
#define MENU_ACTION_ICON_SIZE 20
#define MENU_ACTION_ICON_SPACING 10

#define DEFAULT_LOGO_PATH "/Library/images/icons/boredos/bOS13.png"
#define DEFAULT_APP_ICON_PATH "/Library/images/icons/colloid/application-default-icon.png"
#define DEFAULT_DATE_FORMAT "%Y-%m-%d"
#define DEFAULT_TIME_FORMAT "%H:%M"

typedef struct {
    uint32_t *pixels;
    int w;
    int h;
} image_t;

typedef struct {
    uint32_t surface_id;
    char title[128];
    uint32_t state_flags;
    bool active;
    image_t icon_img;
} window_tab_t;

typedef struct {
    char filename[128];
    char display_name[128];
} app_entry_t;

#define MENU_ACTION_COUNT 3
static const char *menu_action_icon_paths[MENU_ACTION_COUNT] = {
    "/Library/images/icons/colloid/reboot.png",
    "/Library/images/icons/colloid/shutdown.png",
    "/Library/images/icons/colloid/log-out.png"
};
static const char *menu_action_fallback[MENU_ACTION_COUNT] = {
    "R",
    "S",
    "X"
};
static image_t menu_action_icons[MENU_ACTION_COUNT];

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
static uint32_t last_menu_click_ms = 0;

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

static ThemeConfig theme;
static taskbar_config_t config;

static image_t logo_img = {0};
static image_t app_icon_img = {0};

static char search_buf[64] = "";
static int search_len = 0;

static bool should_track_window(uint32_t surface_id, const char *title) {
    if (surface_id == bar_surf_id) return false;
    if (surface_id == menu_surf_id) return false;
    if (!title || title[0] == '\0') return false;
    return true;
}

static void set_default_config(taskbar_config_t *cfg) {
    if (!cfg) return;
    cfg->position_bottom = false;
    strncpy(cfg->logo_path, DEFAULT_LOGO_PATH, sizeof(cfg->logo_path) - 1);
    cfg->logo_path[sizeof(cfg->logo_path) - 1] = '\0';
    strncpy(cfg->date_format, DEFAULT_DATE_FORMAT, sizeof(cfg->date_format) - 1);
    cfg->date_format[sizeof(cfg->date_format) - 1] = '\0';
    cfg->active_tab_color = 0xFF383838;
    cfg->inactive_tab_color = 0xFF1F1E1E;
    cfg->gradient_top_color = 0xFF393939;
    cfg->gradient_bottom_color = 0xFF727272;
    strncpy(cfg->clock_format, "24h", sizeof(cfg->clock_format) - 1);
    cfg->clock_format[sizeof(cfg->clock_format) - 1] = '\0';
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
            strncpy(cfg->logo_path, val, sizeof(cfg->logo_path) - 1);
            cfg->logo_path[sizeof(cfg->logo_path) - 1] = '\0';
        } else if (strcmp(key, "date_format") == 0) {
            strncpy(cfg->date_format, val, sizeof(cfg->date_format) - 1);
            cfg->date_format[sizeof(cfg->date_format) - 1] = '\0';
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
            strncpy(cfg->clock_format, val, sizeof(cfg->clock_format) - 1);
            cfg->clock_format[sizeof(cfg->clock_format) - 1] = '\0';
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

static void add_window(uint32_t surface_id, const char *title, uint32_t state_flags, const char *icon_path) {
    if (!should_track_window(surface_id, title)) return;

    bool become_active = (state_flags & 1) != 0;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            strncpy(windows[i].title, title, sizeof(windows[i].title) - 1);
            windows[i].title[sizeof(windows[i].title) - 1] = '\0';
            windows[i].state_flags = state_flags;
            windows[i].active = become_active;
            if (become_active) {
                for (int j = 0; j < window_count; j++) {
                    if (j != i) windows[j].active = false;
                }
                last_active_surface_id = surface_id;
            }
            if (icon_path && icon_path[0]) {
                free_image(&windows[i].icon_img);
                load_scaled_image(icon_path, 23, 23, &windows[i].icon_img);
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
        strncpy(windows[window_count].title, title, sizeof(windows[window_count].title) - 1);
        windows[window_count].title[sizeof(windows[window_count].title) - 1] = '\0';
        windows[window_count].state_flags = state_flags;
        windows[window_count].active = become_active;
        memset(&windows[window_count].icon_img, 0, sizeof(image_t));
        if (icon_path && icon_path[0]) {
            load_scaled_image(icon_path, 23, 23, &windows[window_count].icon_img);
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

static void update_window_title(uint32_t surface_id, const char *title) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].surface_id == surface_id) {
            strncpy(windows[i].title, title, sizeof(windows[i].title) - 1);
            windows[i].title[sizeof(windows[i].title) - 1] = '\0';
            break;
        }
    }
}

static void load_applications(void) {
    FAT32_FileInfo entries[128];
    int count = sys_list("/bin", entries, 128);
    if (count < 0) return;

    app_count = 0;
    for (int i = 0; i < count; i++) {
        if (entries[i].is_directory) continue;
        if (strstr(entries[i].name, ".elf") == NULL) continue;
        if (app_count < MAX_APPS) {
            strncpy(apps[app_count].filename, entries[i].name, sizeof(apps[app_count].filename) - 1);
            apps[app_count].filename[sizeof(apps[app_count].filename) - 1] = '\0';

            char friendly[128];
            strncpy(friendly, entries[i].name, sizeof(friendly) - 1);
            friendly[sizeof(friendly) - 1] = '\0';

            char *dot = strstr(friendly, ".elf");
            if (dot) *dot = '\0';
            if (friendly[0]) {
                friendly[0] = (char)toupper((unsigned char)friendly[0]);
            }

            strncpy(apps[app_count].display_name, friendly, sizeof(apps[app_count].display_name) - 1);
            apps[app_count].display_name[sizeof(apps[app_count].display_name) - 1] = '\0';
            app_count++;
        }
    }
}

static bool matches_filter(const char *name, const char *filter) {
    if (filter[0] == '\0') return true;

    char name_lower[128];
    char filter_lower[64];

    int i = 0;
    while (name[i]) {
        name_lower[i] = (char)tolower((unsigned char)name[i]);
        i++;
    }
    name_lower[i] = '\0';

    i = 0;
    while (filter[i]) {
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
    int max_idx = MENU_ACTION_COUNT + filtered_count - 1;
    if (selected_idx > max_idx) {
        selected_idx = max_idx;
    }
    if (selected_idx < 0) selected_idx = 0;
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
    uint32_t top_color = config.gradient_top_color;
    uint32_t bottom_color = config.gradient_bottom_color;
    uint32_t top_a = (top_color >> 24) & 0xFF;
    uint32_t top_r = (top_color >> 16) & 0xFF;
    uint32_t top_g = (top_color >> 8) & 0xFF;
    uint32_t top_b = top_color & 0xFF;
    uint32_t bot_a = (bottom_color >> 24) & 0xFF;
    uint32_t bot_r = (bottom_color >> 16) & 0xFF;
    uint32_t bot_g = (bottom_color >> 8) & 0xFF;
    uint32_t bot_b = bottom_color & 0xFF;

    int denom = (bar_h > 1) ? (int)(bar_h - 1) : 1;
    for (uint32_t y = 0; y < bar_h; y++) {
        uint32_t a = lerp_channel(top_a, bot_a, (int)y, denom);
        uint32_t r = lerp_channel(top_r, bot_r, (int)y, denom);
        uint32_t g = lerp_channel(top_g, bot_g, (int)y, denom);
        uint32_t b = lerp_channel(top_b, bot_b, (int)y, denom);
        uint32_t color = (a << 24) | (r << 16) | (g << 8) | b;
        uint32_t *row = &bar_pixels[y * bar_w];
        for (uint32_t x = 0; x < bar_w; x++) {
            row[x] = color;
        }
    }

    if (config.position_bottom) {
        for (uint32_t x = 0; x < bar_w; x++) {
            bar_pixels[x] = config.taskbar_border_color;
        }
    } else {
        for (uint32_t x = 0; x < bar_w; x++) {
            bar_pixels[(bar_h - 1) * bar_w + x] = config.taskbar_border_color;
        }
    }

    int start_btn_y = (bar_h - START_BTN_H) / 2;
    if (logo_img.pixels) {
        int icon_x = START_BTN_X + (START_BTN_W - logo_img.w) / 2;
        int icon_y = start_btn_y + (START_BTN_H - logo_img.h) / 2;
        draw_image(bar_pixels, bar_w, bar_h, icon_x, icon_y, &logo_img);
    } else {
        ui_draw_string(bar_pixels, bar_w, bar_h, START_BTN_X + 6, start_btn_y + 6, "Start", theme.text_primary);
    }

    int tab_x = START_BTN_X + START_BTN_W + 10;
    int tab_y = 0;
    int tab_end = (int)bar_w - CLOCK_W - 10;

    for (int i = 0; i < window_count; i++) {
        int tab_w = TAB_W;
        if (tab_x + tab_w > tab_end) break;

        uint32_t tab_bg = config.taskbar_button_bg_color;

        ui_draw_panel(bar_pixels, bar_w, bar_h, tab_x, tab_y, tab_w, bar_h, tab_bg, 0, 0);

        int icon_size = windows[i].icon_img.pixels ? windows[i].icon_img.w : (app_icon_img.pixels ? app_icon_img.w : 0);
        int icon_x = tab_x + (tab_w - icon_size) / 2;
        int icon_y = (bar_h - icon_size) / 2;
        if (windows[i].icon_img.pixels) {
            draw_image(bar_pixels, bar_w, bar_h, icon_x, icon_y, &windows[i].icon_img);
        } else if (app_icon_img.pixels) {
            draw_image(bar_pixels, bar_w, bar_h, icon_x, icon_y, &app_icon_img);
        }

        tab_x += tab_w + TAB_GAP;
    }

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

    int time_w = get_approx_string_width(time_str);
    int clock_x = (int)bar_w - time_w - 12;
    int time_y = (bar_h - theme.font_size) / 2;
    ui_draw_string(bar_pixels, bar_w, bar_h, clock_x, time_y, time_str, theme.text_primary);

    NovaRect damage = { 0, 0, bar_w, bar_h };
    nova_damage_surface(fd, bar_surf_id, 1, &damage);
}


static void draw_menu(void) {
    if (!menu_pixels) return;

    ui_draw_panel(menu_pixels, MENU_W, MENU_H, 0, 0, MENU_W, MENU_H,
                  config.launcher_bg_color | 0xFF000000, config.launcher_border_color, 0);

    int action_y = 15;
    int action_x = 15;
    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        ui_draw_panel(menu_pixels, MENU_W, MENU_H, action_x, action_y,
                      MENU_ACTION_ICON_SIZE, MENU_ACTION_ICON_SIZE,
                      config.launcher_search_bg_color | 0xFF000000,
                      config.launcher_border_color, 4);

        if (menu_action_icons[i].pixels) {
            draw_image(menu_pixels, MENU_W, MENU_H, action_x, action_y, &menu_action_icons[i]);
        } else {
            ui_draw_string(menu_pixels, MENU_W, MENU_H, action_x + 6, action_y + 6,
                           menu_action_fallback[i], theme.text_primary);
        }
        action_x += MENU_ACTION_ICON_SIZE + MENU_ACTION_ICON_SPACING;
    }

    int search_y = action_y + MENU_ACTION_ICON_SIZE + 10;
    ui_draw_panel(menu_pixels, MENU_W, MENU_H, 15, search_y, MENU_W - 30, 32,
                  config.launcher_search_bg_color | 0xFF000000, config.launcher_border_color, 0);

    if (search_len == 0) {
        ui_draw_string(menu_pixels, MENU_W, MENU_H, 25, search_y + 8, "Search...", 0xFF6C7086);
    } else {
        ui_draw_string(menu_pixels, MENU_W, MENU_H, 25, search_y + 8, search_buf, theme.text_primary);
    }

    int y = search_y + 32 + 15;
    for (int i = 0; i < filtered_count && y < MENU_H - 20; i++) {
        uint32_t item_bg = (i == selected_idx) ? config.launcher_selected_bg_color : 0;
        uint32_t item_text = (i == selected_idx) ? 0xFFFFFFFF : theme.text_primary;

        if (i == selected_idx) {
            ui_draw_panel(menu_pixels, MENU_W, MENU_H, 15, y, MENU_W - 30, ITEM_HEIGHT, item_bg, 0, 0);
        }

        int icon_size = app_icon_img.pixels ? app_icon_img.w : 0;
        int icon_x = 25;
        int icon_y = y + (ITEM_HEIGHT - icon_size) / 2;
        if (app_icon_img.pixels) {
            draw_image(menu_pixels, MENU_W, MENU_H, icon_x, icon_y, &app_icon_img);
        }

        int text_x = 25 + (icon_size ? icon_size + 6 : 0);
        ui_draw_string(menu_pixels, MENU_W, MENU_H, text_x, y + 8, filtered_apps[i].display_name, item_text);
        y += ITEM_HEIGHT + 4;
    }

    if (filtered_count == 0) {
        ui_draw_string(menu_pixels, MENU_W, MENU_H, 25, y + 10, "No apps found", theme.text_error);
    }

    NovaRect damage = { 0, 0, MENU_W, MENU_H };
    nova_damage_surface(fd, menu_surf_id, 1, &damage);
}

static void close_menu(void);

static void close_taskbar(void) {
    close_menu();
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

static void perform_menu_action(int action_idx) {
    close_menu();
    switch (action_idx) {
        case 0:
            sys_system(SYSTEM_CMD_REBOOT, 0, 0, 0, 0);
            break;
        case 1:
            sys_system(SYSTEM_CMD_SHUTDOWN, 0, 0, 0, 0);
            break;
        case 2:
            nova_quit(fd);
            close_taskbar();
            break;
        default:
            break;
    }
}

static void launch_selected_app(void) {
    if (selected_idx >= 0 && selected_idx < filtered_count) {
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "/bin/%s", filtered_apps[selected_idx].filename);
        sys_spawn(full_path, NULL, 0x2 /* SPAWN_FLAG_INHERIT_TTY */, 0);
    }
}

static void reset_menu_search(void) {
    search_len = 0;
    search_buf[0] = '\0';
    selected_idx = 0;
    apply_filter();
}

static void close_menu(void) {
    if (!menu_open) return;
    nova_destroy_surface(fd, menu_surf_id);
    if (menu_pixels) {
        munmap(menu_pixels, MENU_W * MENU_H * 4);
        menu_pixels = NULL;
    }
    menu_surf_id = 0;
    menu_open = false;
    draw_taskbar();

    if (resume_focus_id != 0) {
        nova_set_state(fd, resume_focus_id, 1 /* active focused state */);
        resume_focus_id = 0;
    }
}

static void open_menu(void) {
    if (menu_open) return;

    resume_focus_id = last_active_surface_id;

    char shm_path[128];
    if (nova_create_surface(fd, MENU_W, MENU_H, MENU_LAYER, 0, &menu_surf_id, shm_path) < 0) {
        return;
    }

    int shm_fd = open(shm_path, O_RDWR);
    if (shm_fd < 0) {
        nova_destroy_surface(fd, menu_surf_id);
        menu_surf_id = 0;
        return;
    }

    menu_pixels = mmap(NULL, MENU_W * MENU_H * 4, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (menu_pixels == MAP_FAILED) {
        menu_pixels = NULL;
        nova_destroy_surface(fd, menu_surf_id);
        menu_surf_id = 0;
        return;
    }

    int menu_x = 0;
    int menu_y = config.position_bottom ? (screen_h - bar_h - MENU_H) : (int)bar_h;
    if (menu_y < 0) menu_y = 0;

    nova_move_surface(fd, menu_surf_id, menu_x, menu_y);
    nova_set_state(fd, menu_surf_id, 1 /* focused */);

    reset_menu_search();
    menu_open = true;
    draw_menu();
    draw_taskbar();
}

static void handle_bar_click(int click_x, int click_y) {
    int start_btn_y = (bar_h - START_BTN_H) / 2;
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
    int tab_y = 0;
    int tab_end = (int)bar_w - CLOCK_W - 10;

    for (int i = 0; i < window_count; i++) {
        int tab_w = TAB_W;
        if (tab_x + tab_w > tab_end) break;

        if (click_x >= tab_x && click_x < tab_x + tab_w &&
            click_y >= tab_y && click_y < tab_y + bar_h) {
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


static void handle_menu_click(int click_x, int click_y) {
    int action_y = 15;
    int action_x = 15;
    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        if (click_x >= action_x && click_x < action_x + MENU_ACTION_ICON_SIZE &&
            click_y >= action_y && click_y < action_y + MENU_ACTION_ICON_SIZE) {
            perform_menu_action(i);
            return;
        }
        action_x += MENU_ACTION_ICON_SIZE + MENU_ACTION_ICON_SPACING;
    }

    int search_y = action_y + MENU_ACTION_ICON_SIZE + 10;
    int y = search_y + 32 + 15;
    for (int i = 0; i < filtered_count; i++) {
        if (click_x >= 15 && click_x < MENU_W - 15 &&
            click_y >= y && click_y < y + ITEM_HEIGHT) {
            selected_idx = i;
            draw_menu();
            launch_selected_app();
            close_menu();
            return;
        }
        y += ITEM_HEIGHT + 4;
    }
}

static void handle_menu_key(const NovaEvent *ev) {
    if (!menu_open || !ev) return;

    uint32_t kc = ev->data.key.keycode;
    uint8_t pressed = ev->data.key.pressed;

    if (!pressed) return;

    if (kc == KEY_ESCAPE) {
        close_menu();
    } else if (kc == KEY_UP) {
        if (selected_idx > 0) {
            selected_idx--;
            draw_menu();
        }
    } else if (kc == KEY_DOWN) {
        if (selected_idx < filtered_count - 1) {
            selected_idx++;
            draw_menu();
        }
    } else if (kc == KEY_ENTER) {
        if (selected_idx >= 0 && selected_idx < filtered_count) {
            launch_selected_app();
            close_menu();
        }
    } else if (kc == KEY_BACKSPACE) {
        if (search_len > 0) {
            search_len--;
            search_buf[search_len] = '\0';
            apply_filter();
            draw_menu();
        }
    } else if (kc == KEY_SPACE) {
        if (search_len < (int)sizeof(search_buf) - 1) {
            search_buf[search_len++] = ' ';
            search_buf[search_len] = '\0';
            apply_filter();
            draw_menu();
        }
    } else if (kc >= KEY_A && kc <= KEY_Z) {
        char letter = (char)('a' + (kc - KEY_A));
        if (search_len < (int)sizeof(search_buf) - 1) {
            search_buf[search_len++] = letter;
            search_buf[search_len] = '\0';
            apply_filter();
            draw_menu();
        }
    } else if (kc >= KEY_0 && kc <= KEY_9) {
        char digit = (char)('0' + (kc - KEY_0));
        if (search_len < (int)sizeof(search_buf) - 1) {
            search_buf[search_len++] = digit;
            search_buf[search_len] = '\0';
            apply_filter();
            draw_menu();
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    theme_load("/etc/nova/nova.conf", &theme);
    ui_font_init(theme.font_path, theme.font_size);

    load_taskbar_config("/Library/conf/taskbar.conf", &config);

    load_scaled_image(config.logo_path, 20, 20, &logo_img);
    load_scaled_image(DEFAULT_APP_ICON_PATH, 23, 23, &app_icon_img);
    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        load_scaled_image(menu_action_icon_paths[i], MENU_ACTION_ICON_SIZE, MENU_ACTION_ICON_SIZE, &menu_action_icons[i]);
    }

    load_applications();
    apply_filter();

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
    draw_taskbar();

    while (1) {
        int timeout = 200;
        if (nova_pending_events()) {
            timeout = 0;
        }
        int pr = poll(&pfd, 1, timeout);

        uint32_t now = sys_system(SYSTEM_CMD_GET_TICKS, 0, 0, 0, 0) * 16;
        if (now - last_clock_tick >= 1000) {
            last_clock_tick = now;
            draw_taskbar();
        }

        if ((pr > 0 && (pfd.revents & POLLIN)) || nova_pending_events()) {
            NovaEvent ev;
            bool needs_draw = false;
            while (nova_pending_events() || (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))) {
                if (nova_poll_event(fd, &ev) == 0) {
                    switch (ev.type) {
                        case EVT_WINDOW_CREATED:
                            add_window(ev.surface_id, ev.data.window.title, ev.data.window.state_flags, ev.data.window.icon_path);
                            needs_draw = true;
                            break;

                        case EVT_WINDOW_DESTROYED:
                            remove_window(ev.surface_id);
                            needs_draw = true;
                            break;
                        case EVT_WINDOW_TITLE_CHANGED:
                            update_window_title(ev.surface_id, ev.data.window.title);
                            needs_draw = true;
                            break;
                        case EVT_STATE_CHANGED:
                            update_window_focus(ev.surface_id, ev.data.state.state_flags);
                            needs_draw = true;
                            break;
                        case EVT_POINTER: {
                            uint32_t buttons = ev.data.pointer.buttons;
                            if (buttons & 1) {
                                uint32_t now_ms = sys_system(SYSTEM_CMD_GET_TICKS, 0, 0, 0, 0) * 16;
                                if (ev.surface_id == bar_surf_id) {
                                    if (now_ms - last_bar_click_ms > 180) {
                                        last_bar_click_ms = now_ms;
                                        handle_bar_click(ev.data.pointer.x, ev.data.pointer.y);
                                    }
                                } else if (menu_open && ev.surface_id == menu_surf_id) {
                                    if (now_ms - last_menu_click_ms > 180) {
                                        last_menu_click_ms = now_ms;
                                        handle_menu_click(ev.data.pointer.x, ev.data.pointer.y);
                                    }
                                }
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
                draw_taskbar();
            }
        }
    }

    return 0;
}
