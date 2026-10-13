/* video/nomodeset.h -- Kestrel's Linux KPI: nomodeset */
#ifndef _LKPI_VIDEO_NOMODESET_H
#define _LKPI_VIDEO_NOMODESET_H

#include <linux/types.h>

bool video_firmware_drivers_only(void);        /* true unless linuxkpi.drm=1 (linuxkpi/src/drm.c) */

#endif
