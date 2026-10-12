/* video/nomodeset.h -- Kestrel's Linux KPI: nomodeset */
#ifndef _LKPI_VIDEO_NOMODESET_H
#define _LKPI_VIDEO_NOMODESET_H

#include <linux/types.h>

static inline bool video_firmware_drivers_only(void) { return false; }

#endif
