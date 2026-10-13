/* linux/fb.h -- Kestrel's Linux KPI: frame buffer devices (DRM's fbdev emulation is not used: the KPI's
 * display client feeds Kestrel's terminals instead) */
#ifndef _LKPI_LINUX_FB_H
#define _LKPI_LINUX_FB_H

#include <linux/types.h>
#include <linux/kernel.h>

struct fb_info;
struct fb_var_screeninfo;
struct fb_videomode;
struct fb_cmap;
#define FB_BLANK_UNBLANK        0
#define FB_BLANK_NORMAL         1
#define FB_BLANK_POWERDOWN      4



#endif
