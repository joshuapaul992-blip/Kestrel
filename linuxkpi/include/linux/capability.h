/* linux/capability.h -- Kestrel's Linux KPI: capabilities (Kestrel's kernel threads and root have them all) */
#ifndef _LKPI_LINUX_CAPABILITY_H
#define _LKPI_LINUX_CAPABILITY_H

#include <linux/types.h>

#define CAP_SYS_ADMIN           21
#define CAP_SYS_RAWIO           17
static inline bool capable(int cap) { return true; }
static inline bool perfmon_capable(void) { return true; }

#endif
