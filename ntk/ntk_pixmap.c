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
    size_t pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / sizeof(uint32_t)) return NULL;

    NtkPixmap *pm = calloc(1, sizeof(NtkPixmap));
    if (!pm) return NULL;

    pm->width  = width;
    pm->height = height;
    pm->pixels = calloc(pixel_count, sizeof(uint32_t));
    if (!pm->pixels) {
        free(pm);
        return NULL;
    }

    return pm;
}

static NtkPixmap *pixmap_new_from_file(const char *path,
                                       int max_width,
                                       int max_height) {
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

    if (max_width > 0 && max_height > 0) {
        int source_width = 0;
        int source_height = 0;
        int source_channels = 0;
        if (!stbi_info_from_memory(buf, (int)size, &source_width,
                                   &source_height, &source_channels) ||
            source_width <= 0 || source_height <= 0 ||
            (uint64_t)source_width * (uint64_t)source_height >
                16u * 1024u * 1024u) {
            printf("[NTK PIXMAP] thumbnail source is invalid or too large: %s\n",
                   path);
            free(buf);
            return NULL;
        }
    }

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char *data = stbi_load_from_memory(buf, (int)size, &img_w, &img_h, &channels, 4);
    free(buf);

    if (!data) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: stbi_load_from_memory failed for %s\n", path);
        return NULL;
    }

    int output_width = img_w;
    int output_height = img_h;
    if (max_width > 0 && max_height > 0 &&
        (img_w > max_width || img_h > max_height)) {
        if ((int64_t)img_w * max_height > (int64_t)img_h * max_width) {
            output_width = max_width;
            output_height = (int)((int64_t)img_h * max_width / img_w);
        } else {
            output_height = max_height;
            output_width = (int)((int64_t)img_w * max_height / img_h);
        }
        if (output_width < 1) output_width = 1;
        if (output_height < 1) output_height = 1;
    }

    NtkPixmap *pm = ntk_pixmap_new(output_width, output_height);
    if (!pm) {
        printf("[NTK PIXMAP] ntk_pixmap_new_from_file: failed to allocate pixels buffer (%dx%d) for %s\n", output_width, output_height, path);
        stbi_image_free(data);
        return NULL;
    }

    if (output_width == img_w && output_height == img_h) {
        size_t pixel_count = (size_t)img_w * (size_t)img_h;
        for (size_t index = 0; index < pixel_count; ++index) {
            uint8_t *pixel = data + index * 4;
            pm->pixels[index] = ((uint32_t)pixel[3] << 24) |
                                ((uint32_t)pixel[0] << 16) |
                                ((uint32_t)pixel[1] << 8) |
                                (uint32_t)pixel[2];
        }
    } else {
        for (int y = 0; y < output_height; ++y) {
            int source_y = (int)((int64_t)y * img_h / output_height);
            for (int x = 0; x < output_width; ++x) {
                int source_x = (int)((int64_t)x * img_w / output_width);
                size_t source_index = ((size_t)source_y * (size_t)img_w +
                                       (size_t)source_x) * 4;
                uint8_t *pixel = data + source_index;
                pm->pixels[(size_t)y * (size_t)output_width + (size_t)x] =
                    ((uint32_t)pixel[3] << 24) |
                    ((uint32_t)pixel[0] << 16) |
                    ((uint32_t)pixel[1] << 8) |
                    (uint32_t)pixel[2];
            }
        }
    }

    stbi_image_free(data);
    return pm;
}

NtkPixmap* ntk_pixmap_new_from_file(const char *path) {
    return pixmap_new_from_file(path, 0, 0);
}

NtkPixmap* ntk_pixmap_new_thumbnail_from_file(const char *path,
                                              int max_width,
                                              int max_height) {
    if (max_width <= 0 || max_height <= 0) return NULL;
    return pixmap_new_from_file(path, max_width, max_height);
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
