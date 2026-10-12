/* linux/nospec.h -- Kestrel's Linux KPI: bounds-checked array indices
 *
 * array_index_nospec(i, n) is i when i < n, else 0, computed without a
 * branch so a mispredicted bounds check cannot read past the array. */
#ifndef _LKPI_LINUX_NOSPEC_H
#define _LKPI_LINUX_NOSPEC_H

#include <linux/types.h>

static inline unsigned long array_index_mask_nospec(unsigned long index, unsigned long size)
{
    unsigned long mask;
    asm volatile("cmp %1,%2; sbb %0,%0" : "=r"(mask) : "g"(size), "r"(index) : "cc");
    return mask;
}
#define array_index_nospec(index, size)                                         \
    ({ typeof(index) __i = (index); typeof(size) __s = (size);                  \
       unsigned long __m = array_index_mask_nospec(__i, __s);                   \
       (typeof(__i))(__i & __m); })

#endif
