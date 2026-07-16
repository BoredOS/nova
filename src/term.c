// Copyright (c) 2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <syscall.h>
#include <ntk.h>
#include <novaproto.h>

#include <stb_truetype.h>
#include "utf-8.h"

int sys_pty_create(void);
int sys_pty_destroy(int pty_id);
int sys_tty_read_out(int tty_id, char *buf, int len);
int sys_tty_write_in(int tty_id, const char *buf, int len);

#define SPAWN_FLAG_TERMINAL     0x1
#define SPAWN_FLAG_INHERIT_TTY  0x2
#define SPAWN_FLAG_TTY_ID       0x4

typedef struct {
    uint32_t codepoint;
    uint32_t fg;
    uint32_t bg;
} term_cell_t;

typedef struct {
    term_cell_t *cells;
} term_line_t;

typedef struct {
    int pty_id;
    int cols;
    int rows;
    int cursor_x;
    int cursor_y;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t curr_fg;
    uint32_t curr_bg;
    int saved_x;
    int saved_y;
    bool cursor_visible;

    char font_path[256];
    int font_size;

    term_cell_t *screen_grid;

    // Scrollback
    term_line_t *scrollback;
    int scrollback_max;
    int scrollback_count;
    int scrollback_head;
    int scrollback_offset;

    // UTF-8 stream decoder state
    int utf8_state;
    uint32_t utf8_codepoint;

    // Escape sequence parser state
    int esc_state;
    int esc_params[8];
    int esc_num_params;
} term_t;

typedef struct {
    uint32_t codepoint;
    unsigned char *bitmap;
    int w, h;
    int xoff, yoff;
    int advance;
} cached_glyph_t;

#define GLYPH_CACHE_SIZE 2048
static cached_glyph_t glyph_cache[GLYPH_CACHE_SIZE];
static int glyph_cache_count = 0;

static stbtt_fontinfo font_info;
static float font_scale = 0.0f;
static int cell_width = 8;
static int cell_height = 16;
static int font_ascent = 0;

enum {
    STATE_NORMAL = 0,
    STATE_ESC,
    STATE_CSI,
    STATE_DEC
};

struct NtkPainter {
    uint32_t   *buffer;
    int         width;
    int         height;
    bool        owns_buffer;  
    NtkColor    color;
    float       stroke_width;
    void       *font;
    float       opacity;
    bool        has_clip;
    NtkRect     clip;
};

static uint32_t parse_hex_color(const char *val, uint32_t fallback) {
    if (!val || *val == '\0') return fallback;
    if (*val == '#') val++;
    if (strlen(val) == 6) {
        unsigned int rgb = 0;
        if (sscanf(val, "%x", &rgb) == 1) {
            return 0xFF000000 | rgb;
        }
    }
    return fallback;
}

static void load_term_config(term_t *term, const char *path) {
    strcpy(term->font_path, "/Library/Fonts/Hack-Regular.ttf");
    term->font_size = 14;
    term->scrollback_max = 1000;
    term->fg_color = 0xFFCCCCCC;
    term->bg_color = 0xFF121212;

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *start = line;
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
        if (*start == '\0' || *start == '#' || *start == ';') continue;
        if (*start == '[') continue;

        char *eq = strchr(start, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = eq - 1;
        while (key >= start && (*key == ' ' || *key == '\t')) *key-- = '\0';
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t' || *val == '\r' || *val == '\n') val++;
        char *val_end = val + strlen(val) - 1;
        while (val_end >= val && (*val_end == ' ' || *val_end == '\t' || *val_end == '\r' || *val_end == '\n')) *val_end-- = '\0';

        char *key_trimmed = start;
        if (strcmp(key_trimmed, "font_path") == 0) {
            strncpy(term->font_path, val, sizeof(term->font_path) - 1);
        } else if (strcmp(key_trimmed, "font_size") == 0) {
            term->font_size = atoi(val);
            if (term->font_size < 6) term->font_size = 6;
        } else if (strcmp(key_trimmed, "scrollback") == 0) {
            term->scrollback_max = atoi(val);
            if (term->scrollback_max < 0) term->scrollback_max = 0;
        } else if (strcmp(key_trimmed, "background") == 0) {
            term->bg_color = parse_hex_color(val, term->bg_color);
        } else if (strcmp(key_trimmed, "foreground") == 0) {
            term->fg_color = parse_hex_color(val, term->fg_color);
        }
    }
    fclose(f);
}

static unsigned char* load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return NULL; }
    if (out_size) *out_size = sz;
    return buf;
}

static cached_glyph_t* get_glyph(uint32_t codepoint) {
    for (int i = 0; i < glyph_cache_count; i++) {
        if (glyph_cache[i].codepoint == codepoint) {
            return &glyph_cache[i];
        }
    }

    if (glyph_cache_count >= GLYPH_CACHE_SIZE) {
        for (int i = 0; i < glyph_cache_count; i++) {
            free(glyph_cache[i].bitmap);
        }
        glyph_cache_count = 0;
    }

    cached_glyph_t *g = &glyph_cache[glyph_cache_count];
    g->codepoint = codepoint;

    int w, h, xoff, yoff;
    unsigned char *bmp = stbtt_GetCodepointBitmap(&font_info, font_scale, font_scale, codepoint, &w, &h, &xoff, &yoff);
    g->bitmap = bmp;
    g->w = w;
    g->h = h;
    g->xoff = xoff;
    g->yoff = yoff;

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance, &lsb);
    g->advance = (int)(advance * font_scale + 0.5f);

    glyph_cache_count++;
    return g;
}

static uint32_t convert_256_color(uint8_t idx) {
    if (idx < 16) {
        static const uint32_t ansi_colors[] = {
            0xFF000000, 0xFF800000, 0xFF008000, 0xFF808000,
            0xFF000080, 0xFF800080, 0xFF008080, 0xFFC0C0C0,
            0xFF808080, 0xFFFF0000, 0xFF00FF00, 0xFFFFFF00,
            0xFF0000FF, 0xFFFF00FF, 0xFF00FFFF, 0xFFFFFFFF
        };
        return ansi_colors[idx];
    }
    if (idx >= 16 && idx <= 231) {
        int r = ((idx - 16) / 36) * 51;
        int g = (((idx - 16) % 36) / 6) * 51;
        int b = ((idx - 16) % 6) * 51;
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    int gray = (idx - 232) * 10 + 8;
    return 0xFF000000 | (gray << 16) | (gray << 8) | gray;
}

static void scrollback_push(term_t *term, term_cell_t *line_cells) {
    if (term->scrollback_max <= 0) return;

    if (term->scrollback_count < term->scrollback_max) {
        term_line_t *line = &term->scrollback[term->scrollback_count];
        line->cells = malloc(term->cols * sizeof(term_cell_t));
        memcpy(line->cells, line_cells, term->cols * sizeof(term_cell_t));
        term->scrollback_count++;
    } else {
        term_line_t *line = &term->scrollback[term->scrollback_head];
        memcpy(line->cells, line_cells, term->cols * sizeof(term_cell_t));
        term->scrollback_head = (term->scrollback_head + 1) % term->scrollback_max;
    }
}

static term_cell_t get_viewport_cell(term_t *term, int r, int c) {
    int total_history = term->scrollback_count;
    int view_start = total_history - term->scrollback_offset;
    int target_line_idx = view_start + r;

    if (target_line_idx < total_history) {
        if (target_line_idx < 0) {
            term_cell_t empty = { .codepoint = ' ', .fg = term->fg_color, .bg = term->bg_color };
            return empty;
        }
        int idx = (term->scrollback_head + target_line_idx) % term->scrollback_max;
        return term->scrollback[idx].cells[c];
    } else {
        int screen_r = target_line_idx - total_history;
        return term->screen_grid[screen_r * term->cols + c];
    }
}

static void process_char(term_t *term, uint32_t codepoint) {
    if (term->esc_state == STATE_ESC) {
        if (codepoint == '[') {
            term->esc_state = STATE_CSI;
            term->esc_num_params = 0;
            memset(term->esc_params, 0, sizeof(term->esc_params));
        } else if (codepoint == 's') {
            term->saved_x = term->cursor_x;
            term->saved_y = term->cursor_y;
            term->esc_state = STATE_NORMAL;
        } else if (codepoint == 'u') {
            term->cursor_x = term->saved_x;
            term->cursor_y = term->saved_y;
            term->esc_state = STATE_NORMAL;
        } else {
            term->esc_state = STATE_NORMAL;
        }
        return;
    }

    if (term->esc_state == STATE_CSI) {
        if (codepoint == '?') {
            term->esc_state = STATE_DEC;
            term->esc_num_params = 0;
            memset(term->esc_params, 0, sizeof(term->esc_params));
            return;
        }
        if (codepoint >= '0' && codepoint <= '9') {
            term->esc_params[term->esc_num_params] = term->esc_params[term->esc_num_params] * 10 + (codepoint - '0');
            return;
        }
        if (codepoint == ';') {
            if (term->esc_num_params < 7) term->esc_num_params++;
            return;
        }

        term->esc_state = STATE_NORMAL;
        if (codepoint == 'K') { // Erase Line
            int mode = term->esc_params[0];
            if (mode == 0) { // Erase from cursor to end
                for (int c = term->cursor_x; c < term->cols; c++) {
                    int idx = term->cursor_y * term->cols + c;
                    term->screen_grid[idx].codepoint = ' ';
                    term->screen_grid[idx].fg = term->curr_fg;
                    term->screen_grid[idx].bg = term->curr_bg;
                }
            } else if (mode == 1) { // Erase from start to cursor
                for (int c = 0; c <= term->cursor_x; c++) {
                    int idx = term->cursor_y * term->cols + c;
                    term->screen_grid[idx].codepoint = ' ';
                    term->screen_grid[idx].fg = term->curr_fg;
                    term->screen_grid[idx].bg = term->curr_bg;
                }
            } else if (mode == 2) { // Erase entire line
                for (int c = 0; c < term->cols; c++) {
                    int idx = term->cursor_y * term->cols + c;
                    term->screen_grid[idx].codepoint = ' ';
                    term->screen_grid[idx].fg = term->curr_fg;
                    term->screen_grid[idx].bg = term->curr_bg;
                }
            }
        } else if (codepoint == 'J') { // Erase in Display
            int mode = term->esc_params[0];
            if (mode == 2) { // Entire screen
                for (int i = 0; i < term->cols * term->rows; i++) {
                    term->screen_grid[i].codepoint = ' ';
                    term->screen_grid[i].fg = term->curr_fg;
                    term->screen_grid[i].bg = term->curr_bg;
                }
            } else if (mode == 1) { // From beginning to cursor
                int end_idx = term->cursor_y * term->cols + term->cursor_x;
                for (int i = 0; i <= end_idx; i++) {
                    term->screen_grid[i].codepoint = ' ';
                    term->screen_grid[i].fg = term->curr_fg;
                    term->screen_grid[i].bg = term->curr_bg;
                }
            } else { // From cursor to end
                int start_idx = term->cursor_y * term->cols + term->cursor_x;
                for (int i = start_idx; i < term->cols * term->rows; i++) {
                    term->screen_grid[i].codepoint = ' ';
                    term->screen_grid[i].fg = term->curr_fg;
                    term->screen_grid[i].bg = term->curr_bg;
                }
            }
        } else if (codepoint == 'H' || codepoint == 'f') { // Move Cursor
            int row = term->esc_params[0];
            int col = term->esc_params[1];
            if (row > 0) row--;
            if (col > 0) col--;
            term->cursor_y = row;
            term->cursor_x = col;
            if (term->cursor_x >= term->cols) term->cursor_x = term->cols - 1;
            if (term->cursor_y >= term->rows) term->cursor_y = term->rows - 1;
            if (term->cursor_x < 0) term->cursor_x = 0;
            if (term->cursor_y < 0) term->cursor_y = 0;
        } else if (codepoint == 'A') { // Up
            int n = term->esc_params[0]; if (n == 0) n = 1;
            term->cursor_y -= n;
            if (term->cursor_y < 0) term->cursor_y = 0;
        } else if (codepoint == 'B') { // Down
            int n = term->esc_params[0]; if (n == 0) n = 1;
            term->cursor_y += n;
            if (term->cursor_y >= term->rows) term->cursor_y = term->rows - 1;
        } else if (codepoint == 'C') { // Right
            int n = term->esc_params[0]; if (n == 0) n = 1;
            term->cursor_x += n;
            if (term->cursor_x >= term->cols) term->cursor_x = term->cols - 1;
        } else if (codepoint == 'D') { // Left
            int n = term->esc_params[0]; if (n == 0) n = 1;
            term->cursor_x -= n;
            if (term->cursor_x < 0) term->cursor_x = 0;
        } else if (codepoint == 'm') { // SGR Styling
            for (int j = 0; j <= term->esc_num_params; j++) {
                int p = term->esc_params[j];
                if (p == 0) {
                    term->curr_fg = term->fg_color;
                    term->curr_bg = term->bg_color;
                } else if (p >= 30 && p <= 37) {
                    static const uint32_t colors[] = {
                        0xFF000000, 0xFFFF4444, 0xFF6A9955, 0xFFFFCC00,
                        0xFF569CD6, 0xFFC586C0, 0xFF4EC9B0, 0xFFFFFFFF
                    };
                    term->curr_fg = colors[p - 30];
                } else if (p == 38) {
                    if (j + 2 <= term->esc_num_params && term->esc_params[j+1] == 5) {
                        uint8_t idx = (uint8_t)term->esc_params[j+2];
                        term->curr_fg = convert_256_color(idx);
                        j += 2;
                    } else if (j + 4 <= term->esc_num_params && term->esc_params[j+1] == 2) {
                        term->curr_fg = 0xFF000000 | (term->esc_params[j+2] << 16) | (term->esc_params[j+3] << 8) | term->esc_params[j+4];
                        j += 4;
                    }
                } else if (p == 39) {
                    term->curr_fg = term->fg_color;
                } else if (p >= 40 && p <= 47) {
                    static const uint32_t colors[] = {
                        0xFF000000, 0xFFFF4444, 0xFF6A9955, 0xFFFFCC00,
                        0xFF569CD6, 0xFFC586C0, 0xFF4EC9B0, 0xFFFFFFFF
                    };
                    term->curr_bg = colors[p - 40];
                } else if (p == 48) {
                    if (j + 2 <= term->esc_num_params && term->esc_params[j+1] == 5) {
                        uint8_t idx = (uint8_t)term->esc_params[j+2];
                        term->curr_bg = convert_256_color(idx);
                        j += 2;
                    } else if (j + 4 <= term->esc_num_params && term->esc_params[j+1] == 2) {
                        term->curr_bg = 0xFF000000 | (term->esc_params[j+2] << 16) | (term->esc_params[j+3] << 8) | term->esc_params[j+4];
                        j += 4;
                    }
                } else if (p == 49) {
                    term->curr_bg = term->bg_color;
                } else if (p >= 90 && p <= 97) {
                    static const uint32_t colors[] = {
                        0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
                        0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF
                    };
                    term->curr_fg = colors[p - 90];
                } else if (p >= 100 && p <= 107) {
                    static const uint32_t colors[] = {
                        0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
                        0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF
                    };
                    term->curr_bg = colors[p - 100];
                }
            }
        } else if (codepoint == 'n') {
            if (term->esc_params[0] == 6) {
                char buf[32];
                int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dR", term->cursor_y + 1, term->cursor_x + 1);
                sys_tty_write_in(term->pty_id, buf, len);
            }
        } else if (codepoint == 's') {
            term->saved_x = term->cursor_x;
            term->saved_y = term->cursor_y;
        } else if (codepoint == 'u') {
            term->cursor_x = term->saved_x;
            term->cursor_y = term->saved_y;
        }
        return;
    }

    if (term->esc_state == STATE_DEC) {
        if (codepoint >= '0' && codepoint <= '9') {
            term->esc_params[term->esc_num_params] = term->esc_params[term->esc_num_params] * 10 + (codepoint - '0');
            return;
        }
        if (codepoint == ';') {
            if (term->esc_num_params < 7) term->esc_num_params++;
            return;
        }

        term->esc_state = STATE_NORMAL;
        if (codepoint == 'h' || codepoint == 'l') {
            if (term->esc_params[0] == 25) {
                term->cursor_visible = (codepoint == 'h');
            }
        }
        return;
    }

    if (codepoint == '\x1b') {
        term->esc_state = STATE_ESC;
        return;
    }

    if (codepoint == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows) {
            term->cursor_y = term->rows - 1;
            scrollback_push(term, term->screen_grid);
            memmove(term->screen_grid, term->screen_grid + term->cols, (term->rows - 1) * term->cols * sizeof(term_cell_t));
            for (int c = 0; c < term->cols; c++) {
                int idx = (term->rows - 1) * term->cols + c;
                term->screen_grid[idx].codepoint = ' ';
                term->screen_grid[idx].fg = term->curr_fg;
                term->screen_grid[idx].bg = term->curr_bg;
            }
        }
    } else if (codepoint == '\r') {
        term->cursor_x = 0;
    } else if (codepoint == '\t') {
        term->cursor_x = (term->cursor_x + 8) & ~7;
        if (term->cursor_x >= term->cols) term->cursor_x = term->cols - 1;
    } else if (codepoint == '\b') {
        if (term->cursor_x > 0) term->cursor_x--;
    } else {
        int idx = term->cursor_y * term->cols + term->cursor_x;
        term->screen_grid[idx].codepoint = codepoint;
        term->screen_grid[idx].fg = term->curr_fg;
        term->screen_grid[idx].bg = term->curr_bg;
        term->cursor_x++;
        if (term->cursor_x >= term->cols) {
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows) {
                term->cursor_y = term->rows - 1;
                scrollback_push(term, term->screen_grid);
                memmove(term->screen_grid, term->screen_grid + term->cols, (term->rows - 1) * term->cols * sizeof(term_cell_t));
                for (int c = 0; c < term->cols; c++) {
                    int idx_line = (term->rows - 1) * term->cols + c;
                    term->screen_grid[idx_line].codepoint = ' ';
                    term->screen_grid[idx_line].fg = term->curr_fg;
                    term->screen_grid[idx_line].bg = term->curr_bg;
                }
            }
        }
    }
}

static void on_key_press(NtkWidget *widget, NtkEvent *event, term_t *term) {
    (void)widget;

    if (event->type != NTK_EVENT_KEY_PRESS) return;

    NtkKeyCode kc = event->key_code;
    uint32_t mods = event->modifiers;

    if (mods & NTK_MOD_CTRL) {
        if (kc >= KEY_A && kc <= KEY_Z) {
            char ctrl_char = (char)(kc - KEY_A + 1);
            sys_tty_write_in(term->pty_id, &ctrl_char, 1);
            return;
        }
    }

    if (kc == KEY_UP) {
        sys_tty_write_in(term->pty_id, "\x1b[A", 3);
    } else if (kc == KEY_DOWN) {
        sys_tty_write_in(term->pty_id, "\x1b[B", 3);
    } else if (kc == KEY_RIGHT) {
        sys_tty_write_in(term->pty_id, "\x1b[C", 3);
    } else if (kc == KEY_LEFT) {
        sys_tty_write_in(term->pty_id, "\x1b[D", 3);
    } else if (kc == KEY_ENTER) {
        sys_tty_write_in(term->pty_id, "\r", 1);
    } else if (kc == KEY_ESCAPE) {
        sys_tty_write_in(term->pty_id, "\x1b", 1);
    } else if (kc == KEY_BACKSPACE) {
        char bs = 127;
        sys_tty_write_in(term->pty_id, &bs, 1);
    } else if (kc == KEY_TAB) {
        sys_tty_write_in(term->pty_id, "\t", 1);
    } else if (event->text[0] != '\0') {
        sys_tty_write_in(term->pty_id, event->text, strlen(event->text));
    }
}

static void on_canvas_scroll(NtkWidget *widget, NtkEvent *event, term_t *term) {
    int dy = event->scroll_dy;
    term->scrollback_offset += dy;
    if (term->scrollback_offset < 0) term->scrollback_offset = 0;
    if (term->scrollback_offset > term->scrollback_count) term->scrollback_offset = term->scrollback_count;

    ntk_widget_repaint(widget);
}

static void on_canvas_resize(NtkWidget *widget, int width, int height, term_t *term) {
    (void)widget;

    int new_cols = width / cell_width;
    int new_rows = height / cell_height;

    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (term->screen_grid && new_cols == term->cols && new_rows == term->rows) return;

    term_cell_t *new_grid = calloc(new_cols * new_rows, sizeof(term_cell_t));
    for (int r = 0; r < new_rows; r++) {
        for (int c = 0; c < new_cols; c++) {
            new_grid[r * new_cols + c].codepoint = ' ';
            new_grid[r * new_cols + c].fg = term->fg_color;
            new_grid[r * new_cols + c].bg = term->bg_color;
        }
    }

    if (term->screen_grid) {
        int copy_rows = (term->rows < new_rows) ? term->rows : new_rows;
        int copy_cols = (term->cols < new_cols) ? term->cols : new_cols;
        for (int r = 0; r < copy_rows; r++) {
            for (int c = 0; c < copy_cols; c++) {
                new_grid[r * new_cols + c] = term->screen_grid[r * term->cols + c];
            }
        }
        free(term->screen_grid);
    }

    term->screen_grid = new_grid;
    term->cols = new_cols;
    term->rows = new_rows;

    if (term->cursor_x >= term->cols) term->cursor_x = term->cols - 1;
    if (term->cursor_y >= term->rows) term->cursor_y = term->rows - 1;
}

static void draw_cell_fast(struct NtkPainter *p, int gx, int gy, term_cell_t cell, uint32_t term_bg) {
    int cell_w = cell_width;
    int cell_h = cell_height;

    if (gx + cell_w <= 0 || gy + cell_h <= 0 || gx >= p->width || gy >= p->height) return;

    int clip_x1 = 0, clip_y1 = 0, clip_x2 = p->width, clip_y2 = p->height;
    if (p->has_clip) {
        clip_x1 = p->clip.x;
        clip_y1 = p->clip.y;
        clip_x2 = p->clip.x + p->clip.width;
        clip_y2 = p->clip.y + p->clip.height;
    }

    if (gx + cell_w <= clip_x1 || gy + cell_h <= clip_y1 || gx >= clip_x2 || gy >= clip_y2) return;

    bool fully_visible = (gx >= clip_x1 && gy >= clip_y1 && gx + cell_w <= clip_x2 && gy + cell_h <= clip_y2);

    if (cell.bg != term_bg) {
        uint32_t bg_color = cell.bg;
        if (fully_visible) {
            for (int y = 0; y < cell_h; y++) {
                uint32_t *row_ptr = &p->buffer[(gy + y) * p->width + gx];
                for (int x = 0; x < cell_w; x++) {
                    row_ptr[x] = bg_color;
                }
            }
        } else {
            for (int y = 0; y < cell_h; y++) {
                int py = gy + y;
                if (py < clip_y1 || py >= clip_y2) continue;
                uint32_t *row_ptr = &p->buffer[py * p->width];
                for (int x = 0; x < cell_w; x++) {
                    int px = gx + x;
                    if (px < clip_x1 || px >= clip_x2) continue;
                    row_ptr[px] = bg_color;
                }
            }
        }
    }

    if (cell.codepoint != ' ') {
        cached_glyph_t *g = get_glyph(cell.codepoint);
        if (g && g->bitmap && g->w > 0 && g->h > 0) {
            int bx = gx + g->xoff;
            int by = gy + (int)(font_ascent * font_scale) + g->yoff;

            uint32_t fg = cell.fg;
            uint8_t fg_r = (fg >> 16) & 0xFF;
            uint8_t fg_g = (fg >> 8) & 0xFF;
            uint8_t fg_b = fg & 0xFF;

            for (int y = 0; y < g->h; y++) {
                int py = by + y;
                if (py < clip_y1 || py >= clip_y2) continue;

                unsigned char *bmp_row = &g->bitmap[y * g->w];
                uint32_t *dest_row = &p->buffer[py * p->width];

                for (int x = 0; x < g->w; x++) {
                    int px = bx + x;
                    if (px < clip_x1 || px >= clip_x2) continue;

                    uint8_t alpha = bmp_row[x];
                    if (alpha == 255) {
                        dest_row[px] = fg;
                    } else if (alpha > 0) {
                        uint32_t bg = dest_row[px];
                        uint8_t bg_r = (bg >> 16) & 0xFF;
                        uint8_t bg_g = (bg >> 8) & 0xFF;
                        uint8_t bg_b = bg & 0xFF;

                        uint8_t r = (fg_r * alpha + bg_r * (255 - alpha)) >> 8;
                        uint8_t g_val = (fg_g * alpha + bg_g * (255 - alpha)) >> 8;
                        uint8_t b = (fg_b * alpha + bg_b * (255 - alpha)) >> 8;

                        dest_row[px] = 0xFF000000 | (r << 16) | (g_val << 8) | b;
                    }
                }
            }
        }
    }
}

static void draw_cursor_fast(struct NtkPainter *p, term_t *term) {
    if (term->cursor_visible && term->scrollback_offset == 0) {
        int cx = term->cursor_x * cell_width;
        int cy = term->cursor_y * cell_height;
        int cell_w = cell_width;
        int cell_h = cell_height;

        int clip_x1 = 0, clip_y1 = 0, clip_x2 = p->width, clip_y2 = p->height;
        if (p->has_clip) {
            clip_x1 = p->clip.x;
            clip_y1 = p->clip.y;
            clip_x2 = p->clip.x + p->clip.width;
            clip_y2 = p->clip.y + p->clip.height;
        }

        for (int y = 0; y < cell_h; y++) {
            int py = cy + y;
            if (py < clip_y1 || py >= clip_y2) continue;
            uint32_t *dest_row = &p->buffer[py * p->width];
            for (int x = 0; x < cell_w; x++) {
                int px = cx + x;
                if (px < clip_x1 || px >= clip_x2) continue;
                uint32_t bg = dest_row[px];
                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;

                uint8_t r = (170 + bg_r) >> 1;
                uint8_t g_val = (170 + bg_g) >> 1;
                uint8_t b = (170 + bg_b) >> 1;

                dest_row[px] = 0xFF000000 | (r << 16) | (g_val << 8) | b;
            }
        }
    }
}

static void on_canvas_draw(NtkCanvas *canvas, NtkPainter *painter, term_t *term) {
    (void)canvas;
    
    uint32_t bg_color = term->bg_color;
    for (int i = 0; i < painter->width * painter->height; i++) {
        painter->buffer[i] = bg_color;
    }

    for (int r = 0; r < term->rows; r++) {
        for (int c = 0; c < term->cols; c++) {
            term_cell_t cell = get_viewport_cell(term, r, c);
            int gx = c * cell_width;
            int gy = r * cell_height;
            draw_cell_fast(painter, gx, gy, cell, bg_color);
        }
    }

    draw_cursor_fast(painter, term);
}

static bool terminal_handle_event(NtkWidget *w, NtkEvent *e) {
    term_t *term = (term_t *)ntk_widget_get_instance_data(w);

    if (e->type == NTK_EVENT_MOUSE_PRESS) {
        ntk_widget_set_focus(w);
        ntk_widget_repaint(w);
        return true;
    }

    if (e->type == NTK_EVENT_KEY_PRESS) {
        on_key_press(w, e, term);
        return true;
    }

    if (e->type == NTK_EVENT_SCROLL) {
        on_canvas_scroll(w, e, term);
        return true;
    }

    if (e->type == NTK_EVENT_RESIZE) {
        on_canvas_resize(w, e->width, e->height, term);
        ntk_widget_repaint(w);
        return true;
    }

    return false;
}

static void terminal_paint(NtkWidget *w, NtkPainter *p) {
    term_t *term = (term_t *)ntk_widget_get_instance_data(w);
    on_canvas_draw((NtkCanvas *)w, p, term);
}

static const NtkWidgetClass terminal_widget_class = {
    .type_name      = "TerminalWidget",
    .paint          = terminal_paint,
    .layout         = NULL,
    .preferred_size = NULL,
    .handle_event   = terminal_handle_event,
    .destroy        = NULL
};

static void on_pty_data(int fd, void *userdata) {
    (void)fd;
    NtkWidget *widget = (NtkWidget *)userdata;
    term_t *term = (term_t *)ntk_widget_get_instance_data(widget);

    char buf[4096];
    bool got_data = false;
    while (1) {
        int len = sys_tty_read_out(term->pty_id, buf, sizeof(buf));
        if (len <= 0) break;
        got_data = true;

        for (int i = 0; i < len; i++) {
            unsigned char uc = (unsigned char)buf[i];
            if (term->utf8_state > 0 && (uc & 0xC0) == 0x80) {
                term->utf8_codepoint = (term->utf8_codepoint << 6) | (uc & 0x3F);
                term->utf8_state--;
                if (term->utf8_state == 0) {
                    process_char(term, term->utf8_codepoint);
                }
            } else {
                term->utf8_state = 0;
                if ((uc & 0x80) == 0) {
                    process_char(term, uc);
                } else if ((uc & 0xE0) == 0xC0) {
                    term->utf8_codepoint = uc & 0x1F;
                    term->utf8_state = 1;
                } else if ((uc & 0xF0) == 0xE0) {
                    term->utf8_codepoint = uc & 0x0F;
                    term->utf8_state = 2;
                } else if ((uc & 0xF8) == 0xF0) {
                    term->utf8_codepoint = uc & 0x07;
                    term->utf8_state = 3;
                }
            }
        }
    }

    if (got_data) {
        ntk_widget_repaint(widget);
    }
}


int main(void) {
    NtkApp *app = ntk_app_new();
    if (!app) return 1;

    term_t dummy;
    memset(&dummy, 0, sizeof(term_t));
    strcpy(dummy.font_path, "/Library/Fonts/Hack-Regular.ttf");
    dummy.font_size = 14;
    load_term_config(&dummy, "/Library/AppData/org.boredos.nova/term.conf");

    size_t font_sz = 0;
    unsigned char *font_data = load_file(dummy.font_path, &font_sz);
    if (!font_data) {
        font_data = load_file("/Library/Fonts/Hack-Regular.ttf", &font_sz);
        if (!font_data) {
            font_data = load_file("/Library/Fonts/Proggy.ttf", &font_sz);
            if (!font_data) {
                font_data = load_file("/Library/Fonts/inter.ttf", &font_sz);
            }
        }
    }

    if (font_data) {
        if (!stbtt_InitFont(&font_info, font_data, stbtt_GetFontOffsetForIndex(font_data, 0))) {
            printf("[Terminal] Failed to init font from file data\n");
        }
        font_scale = stbtt_ScaleForPixelHeight(&font_info, (float)dummy.font_size);
        int asc, desc, lg;
        stbtt_GetFontVMetrics(&font_info, &asc, &desc, &lg);
        font_ascent = asc;
        cell_height = (int)((asc - desc + lg) * font_scale + 0.5f);

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font_info, 'A', &adv, &lsb);
        cell_width = (int)(adv * font_scale + 0.5f);
    } else {
        cell_width = 8;
        cell_height = 16;
    }

    int cols = 140;
    int rows = 30;

    NtkWidget *win = ntk_window_new("Terminal", cols * cell_width, rows * cell_height);
    if (!win) {
        printf("Failed to create NTK Window\n");
        if (font_data) free(font_data);
        ntk_app_destroy(app);
        return 1;
    }
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_window_set_content(win, vbox);

    NtkWidget *term_widget = ntk_widget_new_with_class(vbox, &terminal_widget_class, sizeof(term_t));
    if (!term_widget) {
        printf("Failed to create TerminalWidget\n");
        if (font_data) free(font_data);
        ntk_app_destroy(app);
        return 1;
    }
    ntk_box_pack_start(vbox, term_widget, true, true, 0);

    term_t *term = (term_t *)ntk_widget_get_instance_data(term_widget);
    memset(term, 0, sizeof(term_t));
    term->cols = cols;
    term->rows = rows;
    term->fg_color = dummy.fg_color;
    term->bg_color = dummy.bg_color;
    term->curr_fg = term->fg_color;
    term->curr_bg = term->bg_color;
    term->cursor_visible = true;
    term->scrollback_max = dummy.scrollback_max;
    strcpy(term->font_path, dummy.font_path);
    term->font_size = dummy.font_size;

    term->screen_grid = malloc(term->cols * term->rows * sizeof(term_cell_t));
    for (int i = 0; i < term->cols * term->rows; i++) {
        term->screen_grid[i].codepoint = ' ';
        term->screen_grid[i].fg = term->fg_color;
        term->screen_grid[i].bg = term->bg_color;
    }

    term->scrollback = calloc(term->scrollback_max, sizeof(term_line_t));

    term->pty_id = sys_pty_create();
    if (term->pty_id < 0) {
        printf("Failed to create PTY\n");
        free(term->screen_grid);
        free(term->scrollback);
        if (font_data) free(font_data);
        ntk_app_destroy(app);
        return 1;
    }

    int child_pid = sys_spawn("/bin/bsh.elf", NULL, SPAWN_FLAG_TERMINAL | SPAWN_FLAG_TTY_ID, term->pty_id);
    if (child_pid <= 0) {
        printf("Failed to spawn shell /bin/bsh.elf\n");
        sys_pty_destroy(term->pty_id);
        free(term->screen_grid);
        free(term->scrollback);
        if (font_data) free(font_data);
        ntk_app_destroy(app);
        return 1;
    }

    ntk_widget_set_focus(term_widget);

    ntk_app_set_custom_poll_fd(term->pty_id, on_pty_data, term_widget);

    ntk_widget_show(win);
    ntk_app_run(app);

    sys_pty_destroy(term->pty_id);
    free(term->screen_grid);
    if (term->scrollback) {
        for (int i = 0; i < term->scrollback_count; i++) {
            free(term->scrollback[i].cells);
        }
        free(term->scrollback);
    }
    if (font_data) free(font_data);
    ntk_app_destroy(app);
    return 0;
}
