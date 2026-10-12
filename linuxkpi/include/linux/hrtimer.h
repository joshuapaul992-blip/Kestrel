/* linux/hrtimer.h -- Kestrel's Linux KPI: high-resolution sleeps
 *
 * Only schedule_hrtimeout(): it sleeps through Kestrel's timer, rounded up
 * to whole jiffies. */
#ifndef _LKPI_LINUX_HRTIMER_H
#define _LKPI_LINUX_HRTIMER_H

#include <linux/types.h>
#include <linux/ktime.h>

enum hrtimer_mode {
    HRTIMER_MODE_ABS = 0x00,
    HRTIMER_MODE_REL = 0x01,
    HRTIMER_MODE_PINNED = 0x02,
    HRTIMER_MODE_SOFT = 0x04,
    HRTIMER_MODE_HARD = 0x08,
};

/* -EINTR when woken early, else 0; a NULL expiry sleeps until woken */
int schedule_hrtimeout(ktime_t *expires, const enum hrtimer_mode mode);
int schedule_hrtimeout_range(ktime_t *expires, u64 delta, const enum hrtimer_mode mode);

#endif
