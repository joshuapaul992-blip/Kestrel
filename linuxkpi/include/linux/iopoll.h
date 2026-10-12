/* linux/iopoll.h -- Kestrel's Linux KPI: poll a register until a condition holds
 *
 * read_poll_timeout(op, val, cond, sleep_us, timeout_us, sleep_before_read,
 * args...) sets val = op(args) until cond is true or timeout_us passes (0:
 * forever), sleeping sleep_us between reads; 0 or -ETIMEDOUT, with val read
 * one last time after the timeout. The _atomic forms busy-wait. */
#ifndef _LKPI_LINUX_IOPOLL_H
#define _LKPI_LINUX_IOPOLL_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/io.h>

#define read_poll_timeout(op, val, cond, sleep_us, timeout_us, sleep_before_read, args...) ({ \
    u64 __timeout_us = (timeout_us);                                                        \
    unsigned long __sleep_us = (sleep_us);                                                  \
    ktime_t __end = ktime_add_us(ktime_get(), __timeout_us);                                \
    if ((sleep_before_read) && __sleep_us) usleep_range((__sleep_us >> 2) + 1, __sleep_us); \
    for (;;) {                                                                              \
        (val) = op(args);                                                                   \
        if (cond) break;                                                                    \
        if (__timeout_us && ktime_compare(ktime_get(), __end) > 0) {                        \
            (val) = op(args);                                                               \
            break;                                                                          \
        }                                                                                   \
        if (__sleep_us) usleep_range((__sleep_us >> 2) + 1, __sleep_us);                    \
        cpu_relax();                                                                        \
    }                                                                                       \
    (cond) ? 0 : -ETIMEDOUT;                                                                \
})

#define read_poll_timeout_atomic(op, val, cond, delay_us, timeout_us, delay_before_read, args...) ({ \
    u64 __timeout_us = (timeout_us);                                                        \
    unsigned long __delay_us = (delay_us);                                                  \
    ktime_t __end = ktime_add_us(ktime_get(), __timeout_us);                                \
    if ((delay_before_read) && __delay_us) udelay(__delay_us);                              \
    for (;;) {                                                                              \
        (val) = op(args);                                                                   \
        if (cond) break;                                                                    \
        if (__timeout_us && ktime_compare(ktime_get(), __end) > 0) {                        \
            (val) = op(args);                                                               \
            break;                                                                          \
        }                                                                                   \
        if (__delay_us) udelay(__delay_us);                                                 \
        cpu_relax();                                                                        \
    }                                                                                       \
    (cond) ? 0 : -ETIMEDOUT;                                                                \
})

#define readx_poll_timeout(op, addr, val, cond, sleep_us, timeout_us) \
    read_poll_timeout(op, val, cond, sleep_us, timeout_us, false, addr)
#define readx_poll_timeout_atomic(op, addr, val, cond, delay_us, timeout_us) \
    read_poll_timeout_atomic(op, val, cond, delay_us, timeout_us, false, addr)

#define readb_poll_timeout(addr, val, cond, delay_us, timeout_us)  readx_poll_timeout(readb, addr, val, cond, delay_us, timeout_us)
#define readw_poll_timeout(addr, val, cond, delay_us, timeout_us)  readx_poll_timeout(readw, addr, val, cond, delay_us, timeout_us)
#define readl_poll_timeout(addr, val, cond, delay_us, timeout_us)  readx_poll_timeout(readl, addr, val, cond, delay_us, timeout_us)
#define readl_poll_timeout_atomic(addr, val, cond, delay_us, timeout_us) \
    readx_poll_timeout_atomic(readl, addr, val, cond, delay_us, timeout_us)

#endif
