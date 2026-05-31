#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <stdint.h>

/* Framebuffer variable screen info */
struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    struct {
        uint32_t offset;
        uint32_t length;
    } red, green, blue, transp;
    uint32_t nonstd;
    uint32_t activate;
    uint32_t height;
    uint32_t width;
    uint32_t accel_flags;
};

struct fb_fix_screeninfo {
    char id[16];
    uint32_t smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t type_aux;
    uint32_t visual;
    uint16_t xpanstep;
    uint16_t ypanstep;
    uint16_t ywrapstep;
    uint32_t line_length;
    uint32_t mmio_start;
    uint32_t mmio_len;
    uint32_t accel;
};

#define FBIOGET_VSCREENINFO _IOR('F', 0x00, struct fb_var_screeninfo)
#define FBIOPUT_VSCREENINFO _IOW('F', 0x01, struct fb_var_screeninfo)
#define FBIOGET_FSCREENINFO _IOR('F', 0x02, struct fb_fix_screeninfo)

#ifndef _IOR
#define _IOC(dir, type, nr, size) (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IOR(type, nr, size) _IOC(2, type, nr, sizeof(size))
#define _IOW(type, nr, size) _IOC(1, type, nr, sizeof(size))
#endif

#endif
