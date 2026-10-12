/* linux/average.h -- Kestrel's Linux KPI: exponentially weighted moving averages
 *
 * DECLARE_EWMA(name, precision, weight_rcp) makes struct ewma_<name> and
 * ewma_<name>_init/_read/_add. The average is kept with <precision> fraction
 * bits; each new value moves it 1/<weight_rcp> of the way (a power of two). */
#ifndef _LKPI_LINUX_AVERAGE_H
#define _LKPI_LINUX_AVERAGE_H

#include <linux/build_bug.h>
#include <linux/compiler.h>
#include <linux/log2.h>

#define DECLARE_EWMA(name, _precision, _weight_rcp)                                    \
    struct ewma_##name { unsigned long internal; };                                     \
    static inline void ewma_##name##_init(struct ewma_##name *e)                        \
    {                                                                                   \
        BUILD_BUG_ON(!__builtin_constant_p(_precision));                                \
        BUILD_BUG_ON(!__builtin_constant_p(_weight_rcp));                               \
        BUILD_BUG_ON(!is_power_of_2(_weight_rcp));                                      \
        e->internal = 0;                                                                \
    }                                                                                   \
    static inline unsigned long ewma_##name##_read(struct ewma_##name *e)               \
    {                                                                                   \
        return e->internal >> (_precision);                                             \
    }                                                                                   \
    static inline void ewma_##name##_add(struct ewma_##name *e, unsigned long val)      \
    {                                                                                   \
        unsigned long cur = READ_ONCE(e->internal);                                     \
        unsigned long shift = ilog2(_weight_rcp);                                       \
        WRITE_ONCE(e->internal, cur ? (((cur << shift) - cur) + (val << (_precision))) >> shift \
                                    : (val << (_precision)));                           \
    }

#endif
