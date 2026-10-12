/* linux/sysfb.h -- Kestrel's Linux KPI: the firmware frame buffer device (Kestrel's head 0; see linuxkpi/glue) */
#ifndef _LKPI_LINUX_SYSFB_H
#define _LKPI_LINUX_SYSFB_H

#include <linux/types.h>

void sysfb_disable(struct device *dev);

#endif
