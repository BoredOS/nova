// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_pixmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stb_image.h>

struct NtkPixmap {
    int            width;
    int            height;
    uint32_t      *pixels; 
};

NtkPixmap* ntk_pixmap_new(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;

    NtkPixmap *pm = calloc(1, sizeof(NtkPixmap));
    if (!pm) return NULL;

    pm->width  = width;
    pm->height = height;
    pm->pixels = calloc((size_t)(width * height), sizeof(uint32_t));
    if (!pm->pixels) {
        free(pm);
        return NULL;
    }

    return pm;
}

NtkPixmap* ntk_pixmap_new_from_file(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0 || size > 64 * 1024 * 1024) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: invalid size %ld for %s\n", size, path);
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    unsigned char *buf = malloc((size_t)size);
    if (!buf) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: failed to allocate %ld bytes\n", size);
        fclose(f);
        return NULL;
    }

    size_t total_rd = 0;
    while (total_rd < (size_t)size) {
        size_t rd = fread(buf + total_rd, 1, (size_t)size - total_rd, f);
        if (rd == 0) {
            break;
        }
        total_rd += rd;
    }
    fclose(f);

    if (total_rd != (size_t)size) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: read mismatch, expected %ld, got %zu for %s\n", size, total_rd, path);
        free(buf);
        return NULL;
    }

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char *data = stbi_load_from_memory(buf, (int)size, &img_w, &img_h, &channels, 4);
    free(buf);

    if (!data) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: stbi_load_from_memory failed for %s\n", path);
        return NULL;
    }

    uint32_t *pixels = malloc((size_t)(img_w * img_h) * sizeof(uint32_t));
    if (!pixels) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: failed to allocate pixels buffer (%dx%d) for %s\n", img_w, img_h, path);
        stbi_image_free(data);
        return NULL;
    }

    for (int i = 0; i < img_w * img_h; i++) {
        uint8_t r = data[i * 4];
        uint8_t g = data[i * 4 + 1];
        uint8_t b = data[i * 4 + 2];
        uint8_t a = data[i * 4 + 3];
        pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    stbi_image_free(data);

    NtkPixmap *pm = calloc(1, sizeof(NtkPixmap));
    if (!pm) {
        free(pixels);
        return NULL;
    }

    pm->width  = img_w;
    pm->height = img_h;
    pm->pixels = pixels;
    printf("[NTK PIXMAP] ntk_pixmap_new_from_file: successfully loaded %s (%dx%d, %d channels)\n", path, img_w, img_h, channels);
    return pm;
}

NtkPixmap* ntk_pixmap_new_from_data(unsigned char *data, int width, int height, int stride, NtkPixelFormat format) {
    if (!data || width <= 0 || height <= 0) return NULL;

    NtkPixmap *pm = ntk_pixmap_new(width, height);
    if (!pm) return NULL;

    for (int y = 0; y < height; y++) {
        unsigned char *row = data + y * stride;
        for (int x = 0; x < width; x++) {
            uint32_t pixel;
            switch (format) {
                case NTK_PIXEL_ARGB32:
                    pixel = ((uint32_t *)row)[x];
                    break;
                case NTK_PIXEL_RGBA32: {
                    int idx = x * 4;
                    pixel = ((uint32_t)row[idx + 3] << 24) |
                            ((uint32_t)row[idx    ] << 16) |
                            ((uint32_t)row[idx + 1] <<  8) |
                             (uint32_t)row[idx + 2];
                    break;
                }
                case NTK_PIXEL_RGB24: {
                    int idx = x * 3;
                    pixel = 0xFF000000 |
                            ((uint32_t)row[idx    ] << 16) |
                            ((uint32_t)row[idx + 1] <<  8) |
                             (uint32_t)row[idx + 2];
                    break;
                }
                default:
                    pixel = 0xFF000000;
                    break;
            }
            pm->pixels[y * width + x] = pixel;
        }
    }

    return pm;
}

NtkPixmap* ntk_pixmap_clone(NtkPixmap *pm) {
    if (!pm) return NULL;

    NtkPixmap *clone = ntk_pixmap_new(pm->width, pm->height);
    if (!clone) return NULL;

    memcpy(clone->pixels, pm->pixels, (size_t)(pm->width * pm->height) * sizeof(uint32_t));
    return clone;
}

void ntk_pixmap_destroy(NtkPixmap *pm) {
    if (!pm) return;
    free(pm->pixels);
    free(pm);
}
int ntk_pixmap_get_width(NtkPixmap *pm) {
    return pm ? pm->width : 0;
}

int ntk_pixmap_get_height(NtkPixmap *pm) {
    return pm ? pm->height : 0;
}

NtkSize ntk_pixmap_get_size(NtkPixmap *pm) {
    if (!pm) return NTK_SIZE_ZERO;
    return NTK_SIZE(pm->width, pm->height);
}
NtkPixmap* ntk_pixmap_scale(NtkPixmap *pm, NtkSize size, NtkScaleMode mode) {
    if (!pm || size.width <= 0 || size.height <= 0) return NULL;

    int dst_w = size.width;
    int dst_h = size.height;

    if (mode == NTK_SCALE_FIT) {
        float scale_x = (float)dst_w / (float)pm->width;
        float scale_y = (float)dst_h / (float)pm->height;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;
        dst_w = (int)(pm->width * scale);
        dst_h = (int)(pm->height * scale);
        if (dst_w <= 0) dst_w = 1;
        if (dst_h <= 0) dst_h = 1;
    } else if (mode == NTK_SCALE_FILL) {
        float scale_x = (float)dst_w / (float)pm->width;
        float scale_y = (float)dst_h / (float)pm->height;
        float scale = (scale_x > scale_y) ? scale_x : scale_y;
        dst_w = (int)(pm->width * scale);
        dst_h = (int)(pm->height * scale);
        if (dst_w <= 0) dst_w = 1;
        if (dst_h <= 0) dst_h = 1;
    }
    NtkPixmap *result = ntk_pixmap_new(dst_w, dst_h);
    if (!result) return NULL;
    for (int y = 0; y < dst_h; y++) {
        int src_y = (int)((uint64_t)y * (uint64_t)pm->height / (uint64_t)dst_h);
        if (src_y >= pm->height) src_y = pm->height - 1;
        for (int x = 0; x < dst_w; x++) {
            int src_x = (int)((uint64_t)x * (uint64_t)pm->width / (uint64_t)dst_w);
            if (src_x >= pm->width) src_x = pm->width - 1;
            result->pixels[y * dst_w + x] = pm->pixels[src_y * pm->width + src_x];
        }
    }

    return result;
}

NtkPixmap* ntk_pixmap_crop(NtkPixmap *pm, NtkRect rect) {
    if (!pm) return NULL;

    int x1 = rect.x < 0 ? 0 : rect.x;
    int y1 = rect.y < 0 ? 0 : rect.y;
    int x2 = rect.x + rect.width;
    int y2 = rect.y + rect.height;
    if (x2 > pm->width)  x2 = pm->width;
    if (y2 > pm->height) y2 = pm->height;

    int cw = x2 - x1;
    int ch = y2 - y1;
    if (cw <= 0 || ch <= 0) return NULL;

    NtkPixmap *result = ntk_pixmap_new(cw, ch);
    if (!result) return NULL;

    for (int y = 0; y < ch; y++) {
        memcpy(&result->pixels[y * cw],
               &pm->pixels[(y1 + y) * pm->width + x1],
               (size_t)cw * sizeof(uint32_t));
    }

    return result;
}
void ntk_pixmap_set_pixel(NtkPixmap *pm, int x, int y, NtkColor color) {
    if (!pm || x < 0 || y < 0 || x >= pm->width || y >= pm->height) return;
    pm->pixels[y * pm->width + x] = color;
}

NtkColor ntk_pixmap_get_pixel(NtkPixmap *pm, int x, int y) {
    if (!pm || x < 0 || y < 0 || x >= pm->width || y >= pm->height) return 0;
    return pm->pixels[y * pm->width + x];
}

unsigned char* ntk_pixmap_get_data(NtkPixmap *pm) {
    return pm ? (unsigned char *)pm->pixels : NULL;
}
void ntk_pixmap_fill(NtkPixmap *pm, NtkColor color) {
    if (!pm) return;
    int count = pm->width * pm->height;
    for (int i = 0; i < count; i++) {
        pm->pixels[i] = color;
    }
}
