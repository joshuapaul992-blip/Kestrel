/* uapi/linux/sched/types.h -- Kestrel's Linux KPI: scheduling attributes */
#ifndef _LKPI_UAPI_LINUX_SCHED_TYPES_H
#define _LKPI_UAPI_LINUX_SCHED_TYPES_H

#include <linux/types.h>

struct sched_attr {
    u32 size;
    u32 sched_policy;
    u64 sched_flags;
    s32 sched_nice;
    u32 sched_priority;
    u64 sched_runtime;
    u64 sched_deadline;
    u64 sched_period;
    u32 sched_util_min;
    u32 sched_util_max;
};

#endif
