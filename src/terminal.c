// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.

// BOREDOS_APP_DESC: Graphical terminal emulator using kernel PTY.
// BOREDOS_APP_ICONS: /Library/images/icons/colloid/utilities-terminal.png

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <syscall.h>
#include <utf-8.h>
#include "libapp/app.h"
#include "libui/ui.h"

#define NOVA_KMOD_CTRL 0x0002u

#define CELL_W      8
#define CELL_H      16
#define PTY_READ_SZ       4096
#define TERM_ESC_MAX_PARAMS 15
#define TERM_STARTUP_DRAIN_TRIES 64
#define TERM_STARTUP_EMPTY_SETTLE 2
#define TERM_FONT_SIZE 14

#define TERM_DEFAULT_FG 0xFFCCCCCCu
#define TERM_DEFAULT_BG 0xFF1E1E1Eu

typedef struct {
    char     utf8[5];
    uint8_t  wide_cont;
    uint32_t fg;
    uint32_t bg;
} TermCell;

typedef enum {
    PARSE_NORMAL = 0,
    PARSE_ESC,
    PARSE_CSI,
    PARSE_CSI_PRIVATE,
} ParseState;

typedef struct {
    int      pty_id;
    int      shell_pid;
    int      cols;
    int      rows;
    TermCell *cells;

    /* Backpointer to app and dirty box for efficient redraws */
    NovaApp *app;
    int dirty_x0, dirty_y0, dirty_x1, dirty_y1;
    bool has_dirty;

    int      cursor_x;
    int      cursor_y;
    int      saved_cursor_x;
    int      saved_cursor_y;
    uint32_t cur_fg;
    uint32_t cur_bg;
    bool     reverse;
    bool     cursor_visible;

    ParseState parse_state;
    int      esc_params[TERM_ESC_MAX_PARAMS + 1];
    int      esc_param_idx;

    unsigned char utf8_buf[5];
    int      utf8_len;
    int      utf8_needed;
} TermState;

typedef struct {
    char font_path[256];
    int font_size;
    uint32_t fg;
    uint32_t bg;
    uint32_t cursor;
} TermConfig;

static void term_load_config(TermConfig *cfg) {
    /* defaults */
    if (!cfg) return;
    cfg->font_path[0] = '\0';
    cfg->font_size = TERM_FONT_SIZE;
    cfg->fg = TERM_DEFAULT_FG;
    cfg->bg = TERM_DEFAULT_BG;
    cfg->cursor = 0xFFFFFFFFu;

    const char *paths[] = {"./terminal.conf", "/etc/nova/terminal.conf", NULL};
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) continue;
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\n' || *p == '\0') continue;
            char key[128], val[384];
]", key, val) == 2) {
            if (sscanf(p, "%127[^=]=%383[^\\n]", key, val) == 2) {
                /* trim whitespace on val */
                char *v = val;
                while (*v == ' ' || *v == '\t') v++;
                char *e = v + strlen(v) - 1;
                while (e > v && (*e == '\n' || *e == '\r' || *e == ' ' || *e == '\t')) *e-- = '\0';
                if (strcmp(key, "font") == 0) {
                    strncpy(cfg->font_path, v, sizeof(cfg->font_path)-1);
                    cfg->font_path[sizeof(cfg->font_path)-1] = '\0';
                } else if (strcmp(key, "font_size") == 0) {
                    cfg->font_size = atoi(v);
                    if (cfg->font_size <= 0) cfg->font_size = TERM_FONT_SIZE;
                } else if (strcmp(key, "foreground") == 0 || strcmp(key, "fg") == 0) {
                    cfg->fg = (uint32_t)strtoul(v, NULL, 0);
                } else if (strcmp(key, "background") == 0 || strcmp(key, "bg") == 0) {
                    cfg->bg = (uint32_t)strtoul(v, NULL, 0);
                } else if (strcmp(key, "cursor") == 0) {
                    cfg->cursor = (uint32_t)strtoul(v, NULL, 0);
                }
            }
        }
        fclose(f);
        break; /* stop at first found config */
    }
}

// ANSI color palette for standard and bright colors, used for SGR color codes
static const uint32_t ANSI_STD_FG[8] = {
    0xFF000000u, 0xFFFF4444u, 0xFF6A9955u, 0xFFFFCC00u,
    0xFF569CD6u, 0xFFC586C0u, 0xFF4EC9B0u, 0xFFFFFFFFu,
};

static const uint32_t ANSI_STD_BG[8] = {
    0xFF000000u, 0xFFFF4444u, 0xFF6A9955u, 0xFFFFCC00u,
    0xFF569CD6u, 0xFFC586C0u, 0xFF4EC9B0u, 0xFFFFFFFFu,
};

static const uint32_t ANSI_BRIGHT_FG[8] = {
    0xFF555555u, 0xFFFF5555u, 0xFF55FF55u, 0xFFFFFF55u,
    0xFF5555FFu, 0xFFFF55FFu, 0xFF55FFFFu, 0xFFFFFFFFu,
};

static const uint32_t ANSI_BRIGHT_BG[8] = {
    0xFF555555u, 0xFFFF5555u, 0xFF55FF55u, 0xFFFFFF55u,
    0xFF5555FFu, 0xFFFF55FFu, 0xFF55FFFFu, 0xFFFFFFFFu,
};

static const uint32_t ANSI_16[16] = {
    0xFF000000u, 0xFF800000u, 0xFF008000u, 0xFF808000u,
    0xFF000080u, 0xFF800080u, 0xFF008080u, 0xFFC0C0C0u,
    0xFF808080u, 0xFFFF0000u, 0xFF00FF00u, 0xFFFFFF00u,
    0xFF0000FFu, 0xFFFF00FFu, 0xFF00FFFFu, 0xFFFFFFFFu,
};

static int term_cols(uint32_t width) {
    int cols = (int)width / CELL_W;
    return cols > 0 ? cols : 1;
}

static int term_rows(uint32_t height) {
    int rows = (int)height / CELL_H;
    return rows > 0 ? rows : 1;
}

static void term_cell_clear(TermCell *cell, uint32_t fg, uint32_t bg) {
    if (!cell) return;
    cell->utf8[0] = '\0';
    cell->wide_cont = 0;
    cell->fg = fg;
    cell->bg = bg;
}

static uint32_t term_effective_fg(const TermState *st) {
    if (!st) return TERM_DEFAULT_FG;
    return st->reverse ? st->cur_bg : st->cur_fg;
}

static uint32_t term_effective_bg(const TermState *st) {
    if (!st) return TERM_DEFAULT_BG;
    return st->reverse ? st->cur_fg : st->cur_bg;
}

static int term_utf8_lead_len(unsigned char lead) {
    if ((lead & 0x80u) == 0u) return 1;
    if ((lead & 0xE0u) == 0xC0u) return 2;
    if ((lead & 0xF0u) == 0xE0u) return 3;
    if ((lead & 0xF8u) == 0xF0u) return 4;
    return 0;
}

// utf8
static int term_char_width(uint32_t cp) {
    if (cp < 0x1100u) return 1;
    if ((cp >= 0x1100u && cp <= 0x115Fu) ||
        (cp >= 0x2E80u && cp <= 0x9FFFu) ||
        (cp >= 0xAC00u && cp <= 0xD7A3u) ||
        (cp >= 0xF900u && cp <= 0xFAFFu) ||
        (cp >= 0xFE10u && cp <= 0xFE19u) ||
        (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
        (cp >= 0xFF00u && cp <= 0xFF60u) ||
        (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
        (cp >= 0x20000u && cp <= 0x2FFFDu) ||
        (cp >= 0x30000u && cp <= 0x3FFFDu)) {
        return 2;
    }
    return 1;
}

// move cursor left + handle wide char continuation cells
static void term_backspace(TermState *st) {
    if (!st || st->cursor_x <= 0) return;

    st->cursor_x--;
    if (st->cursor_x > 0 &&
        st->cells[st->cursor_y * st->cols + st->cursor_x].wide_cont) {
        st->cursor_x--;
    }
}

// Resize the terminal grid, preserving existing content as much as possible
static void term_resize_grid(TermState *st, int cols, int rows) {
    if (!st) return;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    TermCell *next = malloc((size_t)cols * (size_t)rows * sizeof(TermCell));
    if (!next) return;

    for (int i = 0; i < cols * rows; i++) {
        term_cell_clear(&next[i], TERM_DEFAULT_FG, TERM_DEFAULT_BG);
    }

    if (st->cells && st->cols > 0 && st->rows > 0) {
        int copy_cols = cols < st->cols ? cols : st->cols;
        int copy_rows = rows < st->rows ? rows : st->rows;
        for (int y = 0; y < copy_rows; y++) {
            memcpy(next + y * cols,
                   st->cells + y * st->cols,
                   (size_t)copy_cols * sizeof(TermCell));
        }
    }

    free(st->cells);
    st->cells = next;
    st->cols = cols;
    st->rows = rows;

    if (st->cursor_x >= cols) st->cursor_x = cols - 1;
    if (st->cursor_y >= rows) st->cursor_y = rows - 1;
}

// Scroll the terminal content up by one line, clearing the new bottom line
static void term_scroll_up(TermState *st) {
    if (!st || !st->cells || st->rows <= 1) return;

    memmove(st->cells,
            st->cells + st->cols,
            (size_t)st->cols * (size_t)(st->rows - 1) * sizeof(TermCell));

    TermCell *last = st->cells + st->cols * (st->rows - 1);
    uint32_t fg = term_effective_fg(st);
    uint32_t bg = term_effective_bg(st);
    for (int x = 0; x < st->cols; x++) {
        term_cell_clear(&last[x], fg, bg);
    }
}

static void term_advance_cursor(TermState *st, int cols_used) {
    if (!st) return;

    st->cursor_x += cols_used;
    if (st->cursor_x >= st->cols) {
        st->cursor_x = 0;
        st->cursor_y++;
    }
    if (st->cursor_y >= st->rows) {
        term_scroll_up(st);
        st->cursor_y = st->rows - 1;
    }
}

static void term_prepare_cell_for_write(TermState *st, int x, int y,
                                        uint32_t fg, uint32_t bg) {
    if (!st || !st->cells) return;
    if (x < 0 || x >= st->cols || y < 0 || y >= st->rows) return;

    int idx = y * st->cols + x;
    if (st->cells[idx].wide_cont && x > 0) {
        term_cell_clear(&st->cells[idx - 1], fg, bg);
    }
    if (x + 1 < st->cols && st->cells[idx + 1].wide_cont) {
        term_cell_clear(&st->cells[idx + 1], fg, bg);
    }
}

// Put a UTF-8 character at the current cursor position, handling wide chars and line wrapping
static void term_put_utf8(TermState *st, const char *utf8, int byte_len, uint32_t cp) {
    if (!st || !st->cells || !utf8 || byte_len <= 0) return;

    int width = term_char_width(cp);
    if (width > st->cols) {
        width = 1;
    }
    if (st->cursor_x + width > st->cols) {
        st->cursor_x = 0;
        st->cursor_y++;
        if (st->cursor_y >= st->rows) {
            term_scroll_up(st);
            st->cursor_y = st->rows - 1;
        }
    }

    uint32_t fg = term_effective_fg(st);
    uint32_t bg = term_effective_bg(st);
    term_prepare_cell_for_write(st, st->cursor_x, st->cursor_y, fg, bg);
    if (width == 2) {
        term_prepare_cell_for_write(st, st->cursor_x + 1, st->cursor_y, fg, bg);
    }

    TermCell *cell = &st->cells[st->cursor_y * st->cols + st->cursor_x];
    int copy_len = byte_len < 4 ? byte_len : 4;
    memcpy(cell->utf8, utf8, (size_t)copy_len);
    cell->utf8[copy_len] = '\0';
    cell->wide_cont = 0;
    cell->fg = fg;
    cell->bg = bg;

    if (width == 2) {
        TermCell *cont = &st->cells[st->cursor_y * st->cols + st->cursor_x + 1];
        term_cell_clear(cont, fg, bg);
        cont->wide_cont = 1;
    }

    term_advance_cursor(st, width);
    /* mark the just-written cell region dirty */
    if (st) {
        int last_x = st->cursor_x - width;
        if (last_x < 0) last_x = 0;
        int px0 = last_x * CELL_W;
        int py0 = st->cursor_y * CELL_H;
        int px1 = (st->cursor_x) * CELL_W + CELL_W - 1;
        int py1 = py0 + CELL_H - 1;
        if (!st->has_dirty) {
            st->dirty_x0 = px0; st->dirty_y0 = py0;
            st->dirty_x1 = px1; st->dirty_y1 = py1;
            st->has_dirty = true;
        } else {
            if (px0 < st->dirty_x0) st->dirty_x0 = px0;
            if (py0 < st->dirty_y0) st->dirty_y0 = py0;
            if (px1 > st->dirty_x1) st->dirty_x1 = px1;
            if (py1 > st->dirty_y1) st->dirty_y1 = py1;
        }
    }
}

static void term_clear_screen(TermState *st) {
    if (!st || !st->cells) return;
    uint32_t fg = term_effective_fg(st);
    uint32_t bg = term_effective_bg(st);
    for (int i = 0; i < st->cols * st->rows; i++) {
        term_cell_clear(&st->cells[i], fg, bg);
    }
    st->utf8_len = 0;
    st->utf8_needed = 0;
    /* mark full area dirty */
    if (st) {
        st->has_dirty = true;
        st->dirty_x0 = 0;
        st->dirty_y0 = 0;
        st->dirty_x1 = st->cols * CELL_W - 1;
        st->dirty_y1 = st->rows * CELL_H - 1;
    }
}

static void term_clear_row_range(TermState *st, int y, int x0, int x1) {
    if (!st || !st->cells) return;
    if (y < 0 || y >= st->rows) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= st->cols) x1 = st->cols - 1;
    if (x0 > x1) return;

    uint32_t fg = term_effective_fg(st);
    uint32_t bg = term_effective_bg(st);
    for (int x = x0; x <= x1; x++) {
        term_cell_clear(&st->cells[y * st->cols + x], fg, bg);
    }
        if (st) {
            int px0 = x0 * CELL_W;
            int py0 = y * CELL_H;
            int px1 = (x1 + 1) * CELL_W - 1;
            int py1 = (y + 1) * CELL_H - 1;
            if (!st->has_dirty) {
                st->dirty_x0 = px0; st->dirty_y0 = py0;
                st->dirty_x1 = px1; st->dirty_y1 = py1;
                st->has_dirty = true;
            } else {
                if (px0 < st->dirty_x0) st->dirty_x0 = px0;
                if (py0 < st->dirty_y0) st->dirty_y0 = py0;
                if (px1 > st->dirty_x1) st->dirty_x1 = px1;
                if (py1 > st->dirty_y1) st->dirty_y1 = py1;
            }
        }
static void term_clear_to_eos(TermState *st) {
    if (!st || !st->cells) return;
    term_clear_eol(st);
    for (int y = st->cursor_y + 1; y < st->rows; y++) {
        term_clear_row_range(st, y, 0, st->cols - 1);
    }
}

static void term_clear_to_bos(TermState *st) {
    if (!st || !st->cells) return;
    for (int y = 0; y < st->cursor_y; y++) {
        term_clear_row_range(st, y, 0, st->cols - 1);
    }
    term_clear_bol(st);
}

static void term_set_cursor(TermState *st, int row, int col) {
    if (!st) return;
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    st->cursor_y = row - 1;
    st->cursor_x = col - 1;
    if (st->cursor_y >= st->rows) st->cursor_y = st->rows - 1;
    if (st->cursor_x >= st->cols) st->cursor_x = st->cols - 1;
}

static void term_csi_begin(TermState *st) {
    if (!st) return;
    st->esc_param_idx = 0;
    memset(st->esc_params, 0, sizeof(st->esc_params));
}

// Convert a 256-color palette index to an ARGB color value (found on the internet)
static uint32_t term_color_256(int index) {
    if (index < 0) return TERM_DEFAULT_FG;
    if (index < 16) return ANSI_16[index];

    if (index < 232) {
        static const int levels[6] = { 0, 95, 135, 175, 215, 255 };
        int rem = index - 16;
        int r = levels[rem / 36];
        rem %= 36;
        int g = levels[rem / 6];
        int b = levels[rem % 6];
        return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    int gray = 8 + (index - 232) * 10;
    if (gray > 255) gray = 255;
    return 0xFF000000u | ((uint32_t)gray << 16) |
           ((uint32_t)gray << 8) | (uint32_t)gray;
}

// Apply SGR (Select Graphic Rendition) parameters to update current foreground/background colors
static void term_apply_sgr(TermState *st) {
    if (!st) return;

    for (int j = 0; j <= st->esc_param_idx; j++) {
        int p = st->esc_params[j];

        if (p == 0) {
            st->cur_fg = TERM_DEFAULT_FG;
            st->cur_bg = TERM_DEFAULT_BG;
            st->reverse = false;
        } else if (p == 1) {
            // bold/bright -> for simplicity, we just switch to the bright palette for fg colors
        } else if (p == 7) {
            st->reverse = true;
        } else if (p == 27) {
            st->reverse = false;
        } else if (p >= 30 && p <= 37) {
            st->cur_fg = ANSI_STD_FG[p - 30];
        } else if (p == 38) {
            if (j + 2 <= st->esc_param_idx && st->esc_params[j + 1] == 5) {
                st->cur_fg = term_color_256(st->esc_params[j + 2]);
                j += 2;
            } else if (j + 4 <= st->esc_param_idx && st->esc_params[j + 1] == 2) {
                st->cur_fg = 0xFF000000u |
                             ((uint32_t)st->esc_params[j + 2] << 16) |
                             ((uint32_t)st->esc_params[j + 3] << 8) |
                             (uint32_t)st->esc_params[j + 4];
                j += 4;
            }
        } else if (p == 39) {
            st->cur_fg = TERM_DEFAULT_FG;
        } else if (p >= 40 && p <= 47) {
            st->cur_bg = ANSI_STD_BG[p - 40];
        } else if (p == 48) {
            if (j + 2 <= st->esc_param_idx && st->esc_params[j + 1] == 5) {
                st->cur_bg = term_color_256(st->esc_params[j + 2]);
                j += 2;
            } else if (j + 4 <= st->esc_param_idx && st->esc_params[j + 1] == 2) {
                st->cur_bg = 0xFF000000u |
                             ((uint32_t)st->esc_params[j + 2] << 16) |
                             ((uint32_t)st->esc_params[j + 3] << 8) |
                             (uint32_t)st->esc_params[j + 4];
                j += 4;
            }
        } else if (p == 49) {
            st->cur_bg = TERM_DEFAULT_BG;
        } else if (p >= 90 && p <= 97) {
            st->cur_fg = ANSI_BRIGHT_FG[p - 90];
        } else if (p >= 100 && p <= 107) {
            st->cur_bg = ANSI_BRIGHT_BG[p - 100];
        }
    }
}

static void term_send_input_response(TermState *st, const char *data, int len) {
    if (!st || st->pty_id < 0 || !data || len <= 0) return;
    sys_tty_write_in(st->pty_id, data, len);
}

static void term_report_cursor(TermState *st) {
    if (!st) return;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dR",
                       st->cursor_y + 1, st->cursor_x + 1);
    term_send_input_response(st, buf, len);
}

static void term_save_cursor(TermState *st) {
    if (!st) return;
    st->saved_cursor_x = st->cursor_x;
    st->saved_cursor_y = st->cursor_y;
}

static void term_restore_cursor(TermState *st) {
    if (!st) return;
    st->cursor_x = st->saved_cursor_x;
    st->cursor_y = st->saved_cursor_y;
    if (st->cursor_x < 0) st->cursor_x = 0;
    if (st->cursor_y < 0) st->cursor_y = 0;
    if (st->cursor_x >= st->cols) st->cursor_x = st->cols - 1;
    if (st->cursor_y >= st->rows) st->cursor_y = st->rows - 1;
}

static void term_handle_csi(TermState *st, char final_ch) {
    if (!st) return;

    if (final_ch == 'm') {
        term_apply_sgr(st);
        return;
    }

    if (final_ch == 'K') {
        int mode = st->esc_params[0];
        if (mode == 1) {
            term_clear_bol(st);
        } else if (mode == 2) {
            term_clear_line(st);
        } else {
            term_clear_eol(st);
        }
        return;
    }

    if (final_ch == 'J') {
        int mode = st->esc_params[0];
        if (mode == 1) {
            term_clear_to_bos(st);
        } else if (mode == 2 || mode == 3) {
            term_clear_screen(st);
        } else {
            term_clear_to_eos(st);
        }
        return;
    }

    if (final_ch == 'H' || final_ch == 'f') {
        int row = st->esc_params[0];
        int col = (st->esc_param_idx >= 1) ? st->esc_params[1] : 0;
        if (row <= 0) row = 1;
        if (col <= 0) col = 1;
        term_set_cursor(st, row, col);
        return;
    }

    if (final_ch == 'G') {
        int col = st->esc_params[0];
        if (col <= 0) col = 1;
        st->cursor_x = col - 1;
        if (st->cursor_x >= st->cols) st->cursor_x = st->cols - 1;
        return;
    }

    if (final_ch == 'd') {
        int row = st->esc_params[0];
        if (row <= 0) row = 1;
        st->cursor_y = row - 1;
        if (st->cursor_y >= st->rows) st->cursor_y = st->rows - 1;
        return;
    }

    if (final_ch == 'A') {
        int n = st->esc_params[0];
        if (n <= 0) n = 1;
        st->cursor_y -= n;
        if (st->cursor_y < 0) st->cursor_y = 0;
        return;
    }

    if (final_ch == 'B') {
        int n = st->esc_params[0];
        if (n <= 0) n = 1;
        st->cursor_y += n;
        if (st->cursor_y >= st->rows) st->cursor_y = st->rows - 1;
        return;
    }

    if (final_ch == 'C') {
        int n = st->esc_params[0];
        if (n <= 0) n = 1;
        st->cursor_x += n;
        if (st->cursor_x >= st->cols) st->cursor_x = st->cols - 1;
        return;
    }

    if (final_ch == 'D') {
        int n = st->esc_params[0];
        if (n <= 0) n = 1;
        st->cursor_x -= n;
        if (st->cursor_x < 0) st->cursor_x = 0;
        return;
    }

    if (final_ch == 'n') {
        if (st->esc_params[0] == 6) {
            term_report_cursor(st);
        }
        return;
    }

    if (final_ch == 's') {
        term_save_cursor(st);
        return;
    }

    if (final_ch == 'u') {
        term_restore_cursor(st);
    }
}

// Invalid feedback
static void term_flush_invalid_utf8(TermState *st) {
    if (!st || st->utf8_len <= 0) return;

    static const char repl[] = "\xEF\xBF\xBD";
    term_put_utf8(st, repl, 3, 0xFFFDu);
    st->utf8_len = 0;
    st->utf8_needed = 0;
}

static void term_feed_text_byte(TermState *st, unsigned char byte) {
    if (!st) return;

    if (st->utf8_len > 0 && (byte & 0xC0u) != 0x80u) {
        term_flush_invalid_utf8(st);
    }

    if (st->utf8_len == 0) {
        st->utf8_needed = term_utf8_lead_len(byte);
        if (st->utf8_needed == 1) {
            char ch = (char)byte;
            term_put_utf8(st, &ch, 1, (uint32_t)byte);
            return;
        }
        if (st->utf8_needed <= 0) {
            static const char repl[] = "\xEF\xBF\xBD";
            term_put_utf8(st, repl, 3, 0xFFFDu);
            return;
        }
    }

    if (st->utf8_len >= (int)sizeof(st->utf8_buf)) {
        term_flush_invalid_utf8(st);
    }

    st->utf8_buf[st->utf8_len++] = byte;
    if (st->utf8_len < st->utf8_needed) {
        return;
    }

    st->utf8_buf[st->utf8_len] = '\0';
    int adv = 0;
    uint32_t cp = text_decode_utf8((const char *)st->utf8_buf, &adv);
    if (adv > 0) {
        term_put_utf8(st, (const char *)st->utf8_buf, adv, cp);
    } else {
        term_flush_invalid_utf8(st);
    }

    st->utf8_len = 0;
    st->utf8_needed = 0;
}

static void term_feed_byte(TermState *st, unsigned char byte) {
    if (!st) return;

    switch (st->parse_state) {
        case PARSE_NORMAL:
            if (byte == 0x1b) {
                term_flush_invalid_utf8(st);
                st->parse_state = PARSE_ESC;
            } else if (byte == '\r') {
                term_flush_invalid_utf8(st);
                st->cursor_x = 0;
            } else if (byte == '\n') {
                term_flush_invalid_utf8(st);
                st->cursor_x = 0;
                st->cursor_y++;
                if (st->cursor_y >= st->rows) {
                    term_scroll_up(st);
                    st->cursor_y = st->rows - 1;
                }
            } else if (byte == '\b') {
                term_flush_invalid_utf8(st);
                term_backspace(st);
            } else if (byte == '\t') {
                term_flush_invalid_utf8(st);
                int next = ((st->cursor_x / 8) + 1) * 8;
                st->cursor_x = next;
                if (st->cursor_x >= st->cols) {
                    st->cursor_x = 0;
                    st->cursor_y++;
                    if (st->cursor_y >= st->rows) {
                        term_scroll_up(st);
                        st->cursor_y = st->rows - 1;
                    }
                }
            } else if (byte >= 0x20 && byte != 0x7f) {
                term_feed_text_byte(st, byte);
            }
            break;

        case PARSE_ESC:
            if (byte == '[') {
                st->parse_state = PARSE_CSI;
                term_csi_begin(st);
            } else if (byte == '7' || byte == 's') {
                term_save_cursor(st);
                st->parse_state = PARSE_NORMAL;
            } else if (byte == '8' || byte == 'u') {
                term_restore_cursor(st);
                st->parse_state = PARSE_NORMAL;
            } else if (byte == 'c') {
                st->cur_fg = TERM_DEFAULT_FG;
                st->cur_bg = TERM_DEFAULT_BG;
                st->reverse = false;
                st->cursor_visible = true;
                term_set_cursor(st, 1, 1);
                term_clear_screen(st);
                st->parse_state = PARSE_NORMAL;
            } else {
                st->parse_state = PARSE_NORMAL;
            }
            break;

        case PARSE_CSI:
            if (byte == '?') {
                st->parse_state = PARSE_CSI_PRIVATE;
                term_csi_begin(st);
            } else if (byte >= '0' && byte <= '9') {
                st->esc_params[st->esc_param_idx] =
                    st->esc_params[st->esc_param_idx] * 10 + (byte - '0');
            } else if (byte == ';') {
                if (st->esc_param_idx < TERM_ESC_MAX_PARAMS) {
                    st->esc_param_idx++;
                }
            } else {
                term_handle_csi(st, (char)byte);
                st->parse_state = PARSE_NORMAL;
            }
            break;

        case PARSE_CSI_PRIVATE:
            if (byte >= '0' && byte <= '9') {
                st->esc_params[st->esc_param_idx] =
                    st->esc_params[st->esc_param_idx] * 10 + (byte - '0');
            } else if (byte == ';') {
                if (st->esc_param_idx < TERM_ESC_MAX_PARAMS) {
                    st->esc_param_idx++;
                }
            } else if (byte == 'h' || byte == 'l') {
                for (int i = 0; i <= st->esc_param_idx; i++) {
                    if (st->esc_params[i] == 25) {
                        st->cursor_visible = (byte == 'h');
                    }
                }
                st->parse_state = PARSE_NORMAL;
            } else {
                st->parse_state = PARSE_NORMAL;
            }
            break;
    }
}

static void term_feed(TermState *st, const char *data, int len) {
    for (int i = 0; i < len; i++) {
        term_feed_byte(st, (unsigned char)data[i]);
    }
}

static bool term_drain_pty(TermState *st) {
    if (!st || st->pty_id < 0) return false;

    char buf[PTY_READ_SZ];
    bool updated = false;

    for (;;) {
        int n = sys_tty_read_out(st->pty_id, buf, (int)sizeof(buf));
        if (n <= 0) break;
        term_feed(st, buf, n);
        updated = true;
    }

    return updated;
}

static void term_prime_initial_output(TermState *st) {
    bool saw_output = false;
    int empty_after_output = 0;

    for (int i = 0; i < TERM_STARTUP_DRAIN_TRIES; i++) {
        if (term_drain_pty(st)) {
            saw_output = true;
            empty_after_output = 0;
        } else if (saw_output) {
            empty_after_output++;
            if (empty_after_output >= TERM_STARTUP_EMPTY_SETTLE) {
                break;
            }
        }

        sys_yield();
    }
}

static void term_pty_write(TermState *st, const char *data, int len) {
    if (!st || st->pty_id < 0 || !data || len <= 0) return;
    sys_tty_write_in(st->pty_id, data, len);
}

static bool term_spawn_shell(TermState *st) {
    if (!st) return false;

    st->pty_id = sys_pty_create();
    if (st->pty_id < 0) {
        fprintf(stderr, "term: failed to create PTY\n");
        return false;
    }

    st->shell_pid = sys_spawn("/bin/bsh.elf", NULL,
                              SPAWN_FLAG_TERMINAL | SPAWN_FLAG_TTY_ID,
                              (uint64_t)st->pty_id);
    if (st->shell_pid < 0) {
        fprintf(stderr, "term: failed to spawn bsh on PTY %d\n", st->pty_id);
        sys_pty_destroy(st->pty_id);
        st->pty_id = -1;
        return false;
    }

    sys_tty_set_fg(st->pty_id, st->shell_pid);
    return true;
}

static void term_shutdown(TermState *st) {
    if (!st) return;
    if (st->pty_id >= 0) {
        /* ask kernel to notify/kill attached processes first */
        sys_tty_kill_all(st->pty_id);
        /* yield a few times to let kernel/gc settle attached fds */
        for (int i = 0; i < 4; i++) sys_yield();
        sys_pty_destroy(st->pty_id);
        st->pty_id = -1;
        st->shell_pid = -1;
    }
}

static void term_fill_rect(uint32_t *pixels, int width, int height,
                           int x, int y, int rw, int rh, uint32_t color) {
    if (!pixels || width <= 0 || height <= 0 || rw <= 0 || rh <= 0) return;

    int x1 = x;
    int y1 = y;
    int x2 = x + rw;
    int y2 = y + rh;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > width) x2 = width;
    if (y2 > height) y2 = height;
    if (x2 <= x1 || y2 <= y1) return;

    int fill_w = x2 - x1;
    for (int py = y1; py < y2; py++) {
        uint32_t *p = pixels + py * width + x1;
        uint32_t *end = p + fill_w;
        while (p < end) {
            *p++ = color;
        }
    }
}

static uint32_t term_cell_codepoint(const TermCell *cell) {
    if (!cell || cell->utf8[0] == '\0') return 0;
    int adv = 0;
    return text_decode_utf8(cell->utf8, &adv);
}

static bool term_box_segments(uint32_t cp,
                              bool *top, bool *right,
                              bool *bottom, bool *left) {
    if (!top || !right || !bottom || !left) return false;

    *top = false;
    *right = false;
    *bottom = false;
    *left = false;

    switch (cp) {
        case 0x2500: /* horizontal */
            *left = true; *right = true; return true;
        case 0x2502: /* vertical */
            *top = true; *bottom = true; return true;
        case 0x250C: /* down and right */
            *right = true; *bottom = true; return true;
        case 0x2510: /* down and left */
            *bottom = true; *left = true; return true;
        case 0x2514: /* up and right */
            *top = true; *right = true; return true;
        case 0x2518: /* up and left */
            *top = true; *left = true; return true;
        case 0x251C: /* vertical and right */
            *top = true; *right = true; *bottom = true; return true;
        case 0x2524: /* vertical and left */
            *top = true; *bottom = true; *left = true; return true;
        case 0x252C: /* down and horizontal */
            *right = true; *bottom = true; *left = true; return true;
        case 0x2534: /* up and horizontal */
            *top = true; *right = true; *left = true; return true;
        case 0x253C: /* cross */
            *top = true; *right = true; *bottom = true; *left = true; return true;
        default:
            return false;
    }
}

static bool term_draw_box_glyph(uint32_t *pixels, int width, int height,
                                int x, int y, uint32_t cp, uint32_t color) {
    if (cp == 0x2588) {
        term_fill_rect(pixels, width, height, x, y, CELL_W, CELL_H, color);
        return true;
    }

    bool top, right, bottom, left;
    if (!term_box_segments(cp, &top, &right, &bottom, &left)) {
        return false;
    }

    const int thickness = 2;
    int cx = x + CELL_W / 2 - thickness / 2;
    int cy = y + CELL_H / 2 - thickness / 2;

    if (left) {
        term_fill_rect(pixels, width, height,
                       x, cy, cx - x + thickness, thickness, color);
    }
    if (right) {
        term_fill_rect(pixels, width, height,
                       cx, cy, x + CELL_W - cx, thickness, color);
    }
    if (top) {
        term_fill_rect(pixels, width, height,
                       cx, y, thickness, cy - y + thickness, color);
    }
    if (bottom) {
        term_fill_rect(pixels, width, height,
                       cx, cy, thickness, y + CELL_H - cy, color);
    }

    return true;
}

static void term_invert_rect(uint32_t *pixels, int width, int height,
                             int x, int y, int rw, int rh) {
    if (!pixels || width <= 0 || height <= 0 || rw <= 0 || rh <= 0) return;

    int x1 = x;
    int y1 = y;
    int x2 = x + rw;
    int y2 = y + rh;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > width) x2 = width;
    if (y2 > height) y2 = height;
    if (x2 <= x1 || y2 <= y1) return;

    for (int py = y1; py < y2; py++) {
        uint32_t *row = pixels + py * width;
        for (int px = x1; px < x2; px++) {
            row[px] = (row[px] ^ 0x00FFFFFFu) | 0xFF000000u;
        }
    }
}

static void term_draw_cursor(TermState *st,
                             uint32_t *pixels, int width, int height) {
    if (!st || !st->cursor_visible) return;
    if (st->cursor_x < 0 || st->cursor_x >= st->cols) return;
    if (st->cursor_y < 0 || st->cursor_y >= st->rows) return;

    term_invert_rect(pixels, width, height,
                     st->cursor_x * CELL_W, st->cursor_y * CELL_H,
                     CELL_W, CELL_H);
}

static void term_draw_backgrounds(TermState *st,
                                  uint32_t *pixels, int width, int height) {
    term_fill_rect(pixels, width, height, 0, 0, width, height, TERM_DEFAULT_BG);

    for (int y = 0; y < st->rows; y++) {
        int run_start = -1;
        uint32_t run_bg = TERM_DEFAULT_BG;

        for (int x = 0; x < st->cols; x++) {
            uint32_t bg = st->cells[y * st->cols + x].bg;
            if (bg == TERM_DEFAULT_BG) {
                if (run_start >= 0) {
                    term_fill_rect(pixels, width, height,
                                   run_start * CELL_W, y * CELL_H,
                                   (x - run_start) * CELL_W, CELL_H,
                                   run_bg);
                    run_start = -1;
                }
                continue;
            }

            if (run_start < 0) {
                run_start = x;
                run_bg = bg;
            } else if (bg != run_bg) {
                term_fill_rect(pixels, width, height,
                               run_start * CELL_W, y * CELL_H,
                               (x - run_start) * CELL_W, CELL_H,
                               run_bg);
                run_start = x;
                run_bg = bg;
            }
        }

        if (run_start >= 0) {
            term_fill_rect(pixels, width, height,
                           run_start * CELL_W, y * CELL_H,
                           (st->cols - run_start) * CELL_W, CELL_H,
                           run_bg);
        }
    }
}

static void on_draw(NovaApp *app) {
    TermState *st = app_get_userdata(app);
    if (!st || !st->cells) return;

    uint32_t *pixels = app_pixels(app);
    int width = (int)app_width(app);
    int height = (int)app_height(app);
    if (!pixels) return;

    term_draw_backgrounds(st, pixels, width, height);

    for (int y = 0; y < st->rows; y++) {
        int run_x = -1;
        uint32_t run_fg = 0;
        char run_buf[4096];
        int run_len = 0;

        for (int x = 0; x < st->cols; x++) {
            TermCell *cell = &st->cells[y * st->cols + x];
            if (cell->utf8[0] == '\0' || cell->wide_cont) {
                /* flush any pending run */
                if (run_len > 0) {
                    run_buf[run_len] = '\0';
                    ui_draw_string(pixels, width, height,
                                   run_x * CELL_W, y * CELL_H + 1,
                                   run_buf, run_fg);
                    run_len = 0; run_x = -1;
                }
                /* draw box glyphs or skip */
                if (!cell->wide_cont && cell->utf8[0] != '\0') {
                    uint32_t cp = term_cell_codepoint(cell);
                    if (!term_draw_box_glyph(pixels, width, height,
                                             x * CELL_W, y * CELL_H,
                                             cp, cell->fg)) {
                        ui_draw_string(pixels, width, height,
                                       x * CELL_W, y * CELL_H + 1,
                                       cell->utf8, cell->fg);
                    }
                }
                continue;
            }

            /* start a new run if necessary */
            if (run_x < 0) {
                run_x = x;
                run_fg = cell->fg;
                run_len = 0;
            }

            /* if fg changes, flush and start new run */
            if (cell->fg != run_fg) {
                run_buf[run_len] = '\0';
                ui_draw_string(pixels, width, height,
                               run_x * CELL_W, y * CELL_H + 1,
                               run_buf, run_fg);
                run_x = x;
                run_fg = cell->fg;
                run_len = 0;
            }

            /* append cell utf8 to run buffer (safe append, small strings)
             * ensure we don't overflow run_buf
             */
            int cp_len = (int)strlen(cell->utf8);
            if (run_len + cp_len < (int)sizeof(run_buf) - 1) {
                memcpy(run_buf + run_len, cell->utf8, (size_t)cp_len);
                run_len += cp_len;
            } else {
                /* flush if buffer full */
                run_buf[run_len] = '\0';
                ui_draw_string(pixels, width, height,
                               run_x * CELL_W, y * CELL_H + 1,
                               run_buf, run_fg);
                run_x = -1; run_len = 0; x--; /* reprocess this cell next iter */
            }
        }

        if (run_len > 0 && run_x >= 0) {
            run_buf[run_len] = '\0';
            ui_draw_string(pixels, width, height,
                           run_x * CELL_W, y * CELL_H + 1,
                           run_buf, run_fg);
        }
    }

    term_draw_cursor(st, pixels, width, height);
}

static void on_idle(NovaApp *app) {
    TermState *st = app_get_userdata(app);
    if (!st || st->pty_id < 0) return;

    int cols = term_cols(app_width(app));
    int rows = term_rows(app_height(app));
    if (cols != st->cols || rows != st->rows) {
        term_resize_grid(st, cols, rows);
        app_request_redraw(app);
    }

    if (term_drain_pty(st)) {
        if (st->has_dirty) {
            int dx = st->dirty_x0;
            int dy = st->dirty_y0;
            int dw = st->dirty_x1 - st->dirty_x0 + 1;
            int dh = st->dirty_y1 - st->dirty_y0 + 1;
            if (dw > 0 && dh > 0) {
                app_request_redraw_rect(app, dx, dy, dw, dh);
            } else {
                app_request_redraw(app);
            }
            st->has_dirty = false;
        } else {
            app_request_redraw(app);
        }
    }
}

static void send_bytes(TermState *st, const char *data, int len) {
    term_pty_write(st, data, len);
}

static void on_key(NovaApp *app, uint32_t keycode, uint32_t modifiers, bool pressed) {
    if (!pressed) return;

    TermState *st = app_get_userdata(app);
    if (!st || st->pty_id < 0) return;

    bool ctrl = (modifiers & NOVA_KMOD_CTRL) != 0;

    if (ctrl && keycode >= KEY_A && keycode <= KEY_Z) {
        char ch = (char)keycode;
        send_bytes(st, &ch, 1);
        return;
    }

    switch (keycode) {
        case KEY_ENTER:
            send_bytes(st, "\r", 1);
            break;
        case KEY_BACKSPACE:
            send_bytes(st, "\x7f", 1);
            break;
        case KEY_TAB:
            send_bytes(st, "\t", 1);
            break;
        case KEY_ESCAPE:
            send_bytes(st, "\x1b", 1);
            break;
        case KEY_UP:
            send_bytes(st, "\x1b[A", 3);
            break;
        case KEY_DOWN:
            send_bytes(st, "\x1b[B", 3);
            break;
        case KEY_RIGHT:
            send_bytes(st, "\x1b[C", 3);
            break;
        case KEY_LEFT:
            send_bytes(st, "\x1b[D", 3);
            break;
        default:
            break;
    }
}

static void on_text(NovaApp *app, const char *text, uint32_t codepoint) {
    (void)codepoint;
    TermState *st = app_get_userdata(app);
    if (!st || st->pty_id < 0 || !text || !text[0]) return;
    term_pty_write(st, text, (int)strlen(text));
}

static bool on_close(NovaApp *app) {
    TermState *st = app_get_userdata(app);
    term_shutdown(st);
    return true;
}

static void term_init_monospace_font(const char *font_path, int font_size) {
    if (font_path && font_path[0]) {
        if (ui_font_init(font_path, font_size) == 0) return;
    }
    if (ui_font_init("/Library/Fonts/FiraCode-Regular.ttf", font_size) == 0) return;
    if (ui_font_init("/Library/Fonts/Hack-Regular.ttf", font_size) == 0) return;
    ui_font_init(NULL, font_size);
}

int main(void) {
    // Initialize terminal state with defaults and invalid PTY/shell IDs
    TermState st = {
        .pty_id = -1,
        .shell_pid = -1,
        .cur_fg = TERM_DEFAULT_FG,
        .cur_bg = TERM_DEFAULT_BG,
        .cursor_visible = true,
    };

    // Create the application window and initialize terminal state
    NovaApp *app = app_create("Terminal", 820, 480);
    if (!app) return 1;

    TermConfig cfg;
    term_load_config(&cfg);
    /* initialize font from config (falls back internally) */
    term_init_monospace_font(cfg.font_path, cfg.font_size);
    app_set_userdata(app, &st); // Associate our terminal state with the app for callback access
    st.app = app;
    st.has_dirty = false;
    /* apply configured colors */
    st.cur_fg = cfg.fg;
    st.cur_bg = cfg.bg;
    term_resize_grid(&st, term_cols(app_width(app)), term_rows(app_height(app)));

    // Spawn the shell process connected to our PTY and prime the initial output
    if (!term_spawn_shell(&st)) {
        app_destroy(app);
        free(st.cells);
        return 1;
    }
    term_prime_initial_output(&st); // Draw the first frame of bsh output 

    // Set up app callbacks for drawing, idle processing, key input, text input, and close events
    app_on_draw(app, on_draw);
    app_on_idle(app, on_idle);
    app_on_key(app, on_key);
    app_on_text(app, on_text);
    app_on_close(app, on_close);

    int rc = app_run(app);

    // Clean up terminal state and app resources on exit
    term_shutdown(&st);
    free(st.cells);
    app_destroy(app);
    return rc;
}
