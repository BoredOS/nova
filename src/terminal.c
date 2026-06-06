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

    int      cursor_x;
    int      cursor_y;
    uint32_t cur_fg;
    uint32_t cur_bg;

    ParseState parse_state;
    int      esc_params[TERM_ESC_MAX_PARAMS + 1];
    int      esc_param_idx;

    unsigned char utf8_buf[4];
    int      utf8_len;
    int      utf8_needed;
} TermState;

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

static int term_utf8_lead_len(unsigned char lead) {
    if ((lead & 0x80u) == 0u) return 1;
    if ((lead & 0xE0u) == 0xC0u) return 2;
    if ((lead & 0xF0u) == 0xE0u) return 3;
    if ((lead & 0xF8u) == 0xF0u) return 4;
    return 1;
}

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

static void term_backspace(TermState *st) {
    if (!st || st->cursor_x <= 0) return;

    st->cursor_x--;
    if (st->cursor_x > 0 &&
        st->cells[st->cursor_y * st->cols + st->cursor_x].wide_cont) {
        st->cursor_x--;
    }
}

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

static void term_scroll_up(TermState *st) {
    if (!st || !st->cells || st->rows <= 1) return;

    memmove(st->cells,
            st->cells + st->cols,
            (size_t)st->cols * (size_t)(st->rows - 1) * sizeof(TermCell));

    TermCell *last = st->cells + st->cols * (st->rows - 1);
    for (int x = 0; x < st->cols; x++) {
        term_cell_clear(&last[x], st->cur_fg, st->cur_bg);
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

static void term_put_utf8(TermState *st, const char *utf8, int byte_len, uint32_t cp) {
    if (!st || !st->cells || !utf8 || byte_len <= 0) return;

    int width = term_char_width(cp);
    if (width == 2 && st->cursor_x + 2 > st->cols) {
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

    TermCell *cell = &st->cells[st->cursor_y * st->cols + st->cursor_x];
    int copy_len = byte_len < 4 ? byte_len : 4;
    memcpy(cell->utf8, utf8, (size_t)copy_len);
    cell->utf8[copy_len] = '\0';
    cell->wide_cont = 0;
    cell->fg = st->cur_fg;
    cell->bg = st->cur_bg;

    if (width == 2) {
        TermCell *cont = &st->cells[st->cursor_y * st->cols + st->cursor_x + 1];
        term_cell_clear(cont, st->cur_fg, st->cur_bg);
        cont->wide_cont = 1;
    }

    term_advance_cursor(st, width);
}

static void term_clear_screen(TermState *st) {
    if (!st || !st->cells) return;
    for (int i = 0; i < st->cols * st->rows; i++) {
        term_cell_clear(&st->cells[i], st->cur_fg, st->cur_bg);
    }
    st->cursor_x = 0;
    st->cursor_y = 0;
    st->utf8_len = 0;
    st->utf8_needed = 0;
}

static void term_clear_eol(TermState *st) {
    if (!st || !st->cells) return;
    if (st->cursor_y < 0 || st->cursor_y >= st->rows) return;
    for (int x = st->cursor_x; x < st->cols; x++) {
        term_cell_clear(&st->cells[st->cursor_y * st->cols + x],
                        st->cur_fg, st->cur_bg);
    }
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

static void term_apply_sgr(TermState *st) {
    if (!st) return;

    for (int j = 0; j <= st->esc_param_idx; j++) {
        int p = st->esc_params[j];

        if (p == 0) {
            st->cur_fg = TERM_DEFAULT_FG;
            st->cur_bg = TERM_DEFAULT_BG;
        } else if (p == 1) {
            /* bold/intense - palette already includes bright variants */
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

static void term_handle_csi(TermState *st, char final_ch) {
    if (!st) return;

    if (final_ch == 'm') {
        term_apply_sgr(st);
        return;
    }

    if (final_ch == 'K') {
        term_clear_eol(st);
        return;
    }

    if (final_ch == 'J') {
        int mode = st->esc_params[0];
        if (mode == 2) {
            term_clear_screen(st);
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
    }
}

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
            } else if (byte >= 0x20 && byte != 0x7f) {
                term_feed_text_byte(st, byte);
            }
            break;

        case PARSE_ESC:
            if (byte == '[') {
                st->parse_state = PARSE_CSI;
                term_csi_begin(st);
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
                /* DEC private modes, e.g. ESC[?25h cursor show */
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
        sys_tty_kill_all(st->pty_id);
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
        for (int x = 0; x < st->cols; x++) {
            TermCell *cell = &st->cells[y * st->cols + x];
            if (!cell->wide_cont && cell->utf8[0] != '\0') {
                ui_draw_string(pixels, width, height,
                               x * CELL_W, y * CELL_H + 1,
                               cell->utf8, cell->fg);
            }
        }
    }
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

    char buf[PTY_READ_SZ];
    bool updated = false;

    for (;;) {
        int n = sys_tty_read_out(st->pty_id, buf, (int)sizeof(buf));
        if (n <= 0) break;
        term_feed(st, buf, n);
        updated = true;
    }

    if (updated) {
        app_request_redraw(app);
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

int main(void) {
    TermState st = {
        .pty_id = -1,
        .shell_pid = -1,
        .cur_fg = TERM_DEFAULT_FG,
        .cur_bg = TERM_DEFAULT_BG,
    };

    NovaApp *app = app_create("Terminal", 720, 480);
    if (!app) return 1;

    app_set_userdata(app, &st);
    term_resize_grid(&st, term_cols(app_width(app)), term_rows(app_height(app)));

    if (!term_spawn_shell(&st)) {
        app_destroy(app);
        free(st.cells);
        return 1;
    }

    app_on_draw(app, on_draw);
    app_on_idle(app, on_idle);
    app_on_key(app, on_key);
    app_on_text(app, on_text);
    app_on_close(app, on_close);

    int rc = app_run(app);

    term_shutdown(&st);
    free(st.cells);
    app_destroy(app);
    return rc;
}
