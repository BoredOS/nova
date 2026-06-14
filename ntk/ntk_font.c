// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "ntk_font.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define NTK_FONT_ATLAS_W 512
#define NTK_FONT_ATLAS_H 512
#define NTK_FONT_FIRST_CHAR 32
#define NTK_FONT_NUM_CHARS  96

struct NtkFont {
    char            family[128];
    int             size;
    NtkFontWeight   weight;
    NtkFontStyle    style;
    bool            underline;
    bool            strikethrough;

    unsigned char  *ttf_data;      
    size_t          ttf_size;
    stbtt_fontinfo  info;
    bool            info_valid;

    stbtt_bakedchar baked_chars[NTK_FONT_NUM_CHARS];
    unsigned char  *atlas;          
    bool            baked;
    float           scale;
    int             ascent;
    int             descent;
    int             line_gap;
    int             cap_height;
    int             x_height;
};
static unsigned char* load_font_file(const char *path, size_t *out_size) {
    if (!path || !out_size) return NULL;
    *out_size = 0;

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[NTK FONT] load_font_file: failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > 16 * 1024 * 1024) {
        printf("[NTK FONT] load_font_file: invalid size %ld for %s\n", sz, path);
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    unsigned char *buf = malloc((size_t)sz);
    if (!buf) {
        printf("[NTK FONT] load_font_file: failed to allocate %ld bytes\n", sz);
        fclose(f);
        return NULL;
    }

    size_t total_rd = 0;
    while (total_rd < (size_t)sz) {
        size_t rd = fread(buf + total_rd, 1, (size_t)sz - total_rd, f);
        if (rd == 0) {
            break;
        }
        total_rd += rd;
    }

    fclose(f);
    if (total_rd != (size_t)sz) {
        printf("[NTK FONT] load_font_file: read mismatch, expected %ld, got %zu for %s\n", sz, total_rd, path);
        free(buf);
        return NULL;
    }

    printf("[NTK FONT] load_font_file: successfully loaded %zu bytes from %s\n", total_rd, path);
    *out_size = (size_t)sz;
    return buf;
}

static const char* resolve_font_path(const char *family) {
    static char path_buf[256];

    printf("[NTK FONT] resolve_font_path: resolving family '%s'\n", family);

    FILE *f = fopen(family, "rb");
    if (f) {
        fclose(f);
        printf("[NTK FONT] resolve_font_path: found exact path '%s'\n", family);
        return family;
    }

    static const char *dirs[] = {
        "/Library/Fonts",
        "/Library/Fonts/Emoji",
        NULL
    };

    static const char *exts[] = { ".ttf", ".otf", "", NULL };

    for (int d = 0; dirs[d]; d++) {
        for (int e = 0; exts[e]; e++) {
            snprintf(path_buf, sizeof(path_buf), "%s/%s%s", dirs[d], family, exts[e]);
            f = fopen(path_buf, "rb");
            if (f) {
                fclose(f);
                printf("[NTK FONT] resolve_font_path: found '%s'\n", path_buf);
                return path_buf;
            }
        }
    }

    static const char *fallbacks[] = {
        "/Library/Fonts/Proggy.ttf",
        "/Library/Fonts/inter.ttf",
        "/Library/Fonts/Roboto.ttf",
        NULL
    };
    for (int i = 0; fallbacks[i]; i++) {
        f = fopen(fallbacks[i], "rb");
        if (f) {
            fclose(f);
            printf("[NTK FONT] resolve_font_path: using fallback '%s'\n", fallbacks[i]);
            return fallbacks[i];
        }
    }

    printf("[NTK FONT] resolve_font_path: failed to resolve family '%s'\n", family);
    return NULL;
}
NtkFont* ntk_font_new(const char *family, int size, NtkFontWeight weight, NtkFontStyle style) {
    if (!family || size <= 0) return NULL;

    printf("[NTK FONT] ntk_font_new: family='%s', size=%d\n", family, size);

    const char *path = resolve_font_path(family);
    if (!path) {
        printf("[NTK FONT] ntk_font_new: resolve_font_path failed for '%s'\n", family);
        return NULL;
    }

    NtkFont *f = calloc(1, sizeof(NtkFont));
    if (!f) return NULL;

    strncpy(f->family, family, sizeof(f->family) - 1);
    f->size   = size;
    f->weight = weight;
    f->style  = style;

    f->ttf_data = load_font_file(path, &f->ttf_size);
    if (!f->ttf_data) {
        printf("[NTK FONT] ntk_font_new: load_font_file failed for %s\n", path);
        free(f);
        return NULL;
    }

    if (!stbtt_InitFont(&f->info, f->ttf_data, stbtt_GetFontOffsetForIndex(f->ttf_data, 0))) {
        printf("[NTK FONT] ntk_font_new: stbtt_InitFont failed for %s\n", path);
        free(f->ttf_data);
        free(f);
        return NULL;
    }
    f->info_valid = true;

    f->scale = stbtt_ScaleForPixelHeight(&f->info, (float)size);
    int asc, desc, lg;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &lg);
    f->ascent   = (int)(asc * f->scale + 0.5f);
    f->descent  = (int)(desc * f->scale - 0.5f);
    f->line_gap = (int)(lg * f->scale + 0.5f);

    return f;
}

NtkFont* ntk_font_new_from_string(const char *description) {
    if (!description) return NULL;
    char buf[256];
    strncpy(buf, description, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *family = buf;
    int size = 13;
    NtkFontWeight weight = NTK_FONT_WEIGHT_NORMAL;
    NtkFontStyle style = NTK_FONT_STYLE_NORMAL;
    char *tokens[8];
    int ntokens = 0;
    char *tok = strtok(buf, " \t");
    while (tok && ntokens < 8) {
        tokens[ntokens++] = tok;
        tok = strtok(NULL, " \t");
    }

    if (ntokens == 0) return NULL;
    family = tokens[0];

    for (int i = 1; i < ntokens; i++) {
        char c = tokens[i][0];
        if (c >= '0' && c <= '9') {
            size = atoi(tokens[i]);
        } else if (strcmp(tokens[i], "bold") == 0 || strcmp(tokens[i], "Bold") == 0) {
            weight = NTK_FONT_WEIGHT_BOLD;
        } else if (strcmp(tokens[i], "italic") == 0 || strcmp(tokens[i], "Italic") == 0) {
            style = NTK_FONT_STYLE_ITALIC;
        }
    }

    return ntk_font_new(family, size, weight, style);
}

NtkFont* ntk_font_clone(NtkFont *f) {
    if (!f) return NULL;
    return ntk_font_new(f->family, f->size, f->weight, f->style);
}

void ntk_font_destroy(NtkFont *f) {
    if (!f) return;
    free(f->ttf_data);
    free(f->atlas);
    free(f);
}
void ntk_font_set_family(NtkFont *f, const char *family) {
    if (!f || !family) return;
    strncpy(f->family, family, sizeof(f->family) - 1);
    f->baked = false; 
}

void ntk_font_set_size(NtkFont *f, int size) {
    if (!f || size <= 0) return;
    f->size = size;
    f->baked = false;
    if (f->info_valid) {
        f->scale = stbtt_ScaleForPixelHeight(&f->info, (float)size);
        int asc, desc, lg;
        stbtt_GetFontVMetrics(&f->info, &asc, &desc, &lg);
        f->ascent   = (int)(asc * f->scale + 0.5f);
        f->descent  = (int)(desc * f->scale - 0.5f);
        f->line_gap = (int)(lg * f->scale + 0.5f);
    }
}

void ntk_font_set_weight(NtkFont *f, NtkFontWeight weight) {
    if (f) { f->weight = weight; f->baked = false; }
}

void ntk_font_set_style(NtkFont *f, NtkFontStyle style) {
    if (f) { f->style = style; f->baked = false; }
}

void ntk_font_set_underline(NtkFont *f, bool underline) {
    if (f) f->underline = underline;
}

void ntk_font_set_strikethrough(NtkFont *f, bool strikethrough) {
    if (f) f->strikethrough = strikethrough;
}
void ntk_font_ensure_baked(NtkFont *f) {
    if (!f || f->baked || !f->info_valid) return;

    if (!f->atlas) {
        f->atlas = malloc(NTK_FONT_ATLAS_W * NTK_FONT_ATLAS_H);
        if (!f->atlas) return;
    }

    memset(f->atlas, 0, NTK_FONT_ATLAS_W * NTK_FONT_ATLAS_H);
    stbtt_BakeFontBitmap(f->ttf_data, 0, (float)f->size,
                          f->atlas, NTK_FONT_ATLAS_W, NTK_FONT_ATLAS_H,
                          NTK_FONT_FIRST_CHAR, NTK_FONT_NUM_CHARS, f->baked_chars);
    f->baked = true;
    int raw_line_height = f->ascent - f->descent + f->line_gap;
    int cap_idx = 'H' - NTK_FONT_FIRST_CHAR;
    if (cap_idx >= 0 && cap_idx < NTK_FONT_NUM_CHARS) {
        f->cap_height = (int)(-f->baked_chars[cap_idx].yoff + 0.5f);
    }
    if (f->cap_height <= 0) {
        f->cap_height = (int)(f->ascent * 0.8f + 0.5f);
    }

    int x_idx = 'x' - NTK_FONT_FIRST_CHAR;
    if (x_idx >= 0 && x_idx < NTK_FONT_NUM_CHARS) {
        f->x_height = (int)(-f->baked_chars[x_idx].yoff + 0.5f);
    }
    if (f->x_height <= 0) {
        f->x_height = (int)(f->ascent * 0.6f + 0.5f);
    }
    f->ascent = (f->cap_height + raw_line_height) / 2;
    f->descent = (f->cap_height - raw_line_height) / 2;
    f->line_gap = 0;
}
NtkSize ntk_font_measure_text(NtkFont *f, const char *text) {
    if (!f || !text) return NTK_SIZE_ZERO;

    ntk_font_ensure_baked(f);
    if (!f->baked) return NTK_SIZE_ZERO;

    int x = 0;
    int max_h = f->ascent - f->descent;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= NTK_FONT_FIRST_CHAR && c < NTK_FONT_FIRST_CHAR + NTK_FONT_NUM_CHARS) {
            stbtt_bakedchar *bc = &f->baked_chars[c - NTK_FONT_FIRST_CHAR];
            x += (int)(bc->xadvance + 0.5f);
        }
        text++;
    }

    return NTK_SIZE(x, max_h);
}

int ntk_font_get_ascent(NtkFont *f) {
    return f ? f->ascent : 0;
}

int ntk_font_get_descent(NtkFont *f) {
    return f ? f->descent : 0;
}

int ntk_font_get_line_height(NtkFont *f) {
    if (!f) return 0;
    return f->ascent - f->descent + f->line_gap;
}
unsigned char* ntk_font_get_glyph(NtkFont *f, uint32_t codepoint,
                                   int *out_w, int *out_h,
                                   int *out_xoff, int *out_yoff,
                                   int *out_advance) {
    if (!f || !f->info_valid) return NULL;

    ntk_font_ensure_baked(f);

    if (codepoint >= NTK_FONT_FIRST_CHAR &&
        codepoint < NTK_FONT_FIRST_CHAR + NTK_FONT_NUM_CHARS && f->baked) {
        stbtt_bakedchar *bc = &f->baked_chars[codepoint - NTK_FONT_FIRST_CHAR];
        int gw = bc->x1 - bc->x0;
        int gh = bc->y1 - bc->y0;
        if (out_w) *out_w = gw;
        if (out_h) *out_h = gh;
        if (out_xoff) *out_xoff = (int)(bc->xoff + 0.5f);
        if (out_yoff) *out_yoff = (int)(bc->yoff + 0.5f);
        if (out_advance) *out_advance = (int)(bc->xadvance + 0.5f);

        if (f->atlas && gw > 0 && gh > 0) {
            return &f->atlas[bc->y0 * NTK_FONT_ATLAS_W + bc->x0];
        }
    }

    return NULL;
}

int ntk_font_get_cap_height(NtkFont *f) {
    if (!f) return 0;
    ntk_font_ensure_baked(f);
    return f->cap_height;
}

int ntk_font_get_x_height(NtkFont *f) {
    if (!f) return 0;
    ntk_font_ensure_baked(f);
    return f->x_height;
}
