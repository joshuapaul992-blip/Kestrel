/* include/kernel/kfb.h -- /dev/fbN: one device per monitor, for graphics
 * clients (the X server). Shared with user space (ports/xlibre hw/kestrel).
 *
 *   ioctl(fd, KFB_GET_INFO, &info)     geometry and pixel format
 *   ioctl(fd, KDSETMODE, KD_GRAPHICS)  take the monitor: the text terminal
 *                                      stops drawing (it keeps its text and
 *                                      repaints it on KD_TEXT, or when the
 *                                      owner closes the device or exits)
 *   ioctl(fd, KFB_BLIT, &blit)         copy a rectangle of 32-bpp pixels
 *                                      from the caller's memory to the screen
 *   ioctl(fd, KFB_GET_MODES, &modes)   the modes the monitor can be switched
 *                                      to (just the current one when its
 *                                      driver cannot switch), and its size
 *   ioctl(fd, KFB_SET_MODE, &mode)     switch to one of them (width, height
 *                                      and, unless 0, refresh_mhz must
 *                                      match); the terminal follows. Only
 *                                      the KD_GRAPHICS owner, if there is one
 *   ioctl(fd, FBIOGET_VSCREENINFO, &v) Linux-style subset, for old tools */
#ifndef KESTREL_KFB_H
#define KESTREL_KFB_H

#include <stdint.h>

#define KFB_GET_INFO        0x4b00
#define KFB_BLIT            0x4b01
#define KFB_GET_MODES       0x4b02
#define KFB_SET_MODE        0x4b03
#define KDSETMODE           0x4b3a
#define KD_TEXT             0x00
#define KD_GRAPHICS         0x01
#define FBIOGET_VSCREENINFO 0x4600

struct kfb_info {
    uint32_t width, height;     /* pixels */
    uint32_t pitch;             /* bytes per row of a full-screen image */
    uint32_t bpp;               /* 32 */
    uint8_t  r_pos, g_pos, b_pos, index;   /* channel bit positions; monitor number */
    char     name[32];          /* "boot frame buffer", "GP106 head 1 DP-2" ... */
    char     monitor[16];       /* EDID name or "" */
};

struct kfb_blit {
    int32_t  x, y, w, h;        /* destination rectangle */
    uint64_t src;               /* first pixel of the rectangle in the caller's image */
    uint32_t src_pitch;         /* bytes per row of the caller's image */
    uint32_t reserved;
};

#define KFB_MAX_MODES       24          /* struct kfb_modes fits sys_ioctl's 512-byte copy */
#define KFB_MODE_PREFERRED  1           /* the monitor's own choice (EDID) */
#define KFB_MODE_CURRENT    2

struct kfb_mode {
    uint32_t width, height;     /* pixels */
    uint32_t refresh_mhz;       /* 60000 = 60 Hz; 0 = unknown */
    uint32_t flags;             /* KFB_MODE_* (ignored by KFB_SET_MODE) */
};

struct kfb_modes {
    uint32_t count;
    uint32_t width_mm, height_mm;   /* the picture's physical size, 0 = unknown */
    uint32_t reserved;
    struct kfb_mode mode[KFB_MAX_MODES];
};

struct fb_var_screeninfo_lite { uint32_t xres, yres, bits_per_pixel, line_length; uint64_t smem_start; };

#endif
