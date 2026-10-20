/* include/kernel/linuxkpi.h -- the Linux driver compatibility layer (linuxkpi/) */
#ifndef KERNEL_LINUXKPI_H
#define KERNEL_LINUXKPI_H

#include <stddef.h>

/* Start the KPI's threads, build the Linux view of the PCI devices and run
 * the built-in Linux drivers' initcalls (they probe here). Boot thread,
 * after PCI, MSI, the scheduler and the root file system are up; returns
 * when the initcalls (and the self-test, with linuxkpi.selftest=1) are done. */
void linuxkpi_init(void);

/* /proc/dri: the atomic state of every Linux DRM device (CRTCs, planes,
 * connectors and their frame buffers), as Linux's debugfs dri/N/state. */
size_t linuxkpi_drm_state(char *buf, size_t cap);

#endif
