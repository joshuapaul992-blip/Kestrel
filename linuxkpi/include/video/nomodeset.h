/* video/nomodeset.h -- Kestrel's Linux KPI: nomodeset */
#ifndef _LKPI_VIDEO_NOMODESET_H
#define _LKPI_VIDEO_NOMODESET_H

#include <linux/types.h>

bool video_firmware_drivers_only(void);        /* true with linuxkpi.drm=0 or Safe Mode (linuxkpi/src/drm.c) */

#endif
