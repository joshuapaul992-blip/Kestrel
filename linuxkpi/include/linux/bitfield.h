/* linux/bitfield.h -- Kestrel's Linux KPI: register fields described by a mask
 *
 * FIELD_PREP(mask, v) shifts v into the field, FIELD_GET(mask, reg) takes it
 * out; the mask must be a contiguous run of bits. */
#ifndef _LKPI_LINUX_BITFIELD_H
#define _LKPI_LINUX_BITFIELD_H

#include <linux/types.h>
#include <linux/bits.h>

#define __lkpi_bf_shift(mask)           (__builtin_ffsll(mask) - 1)
#define FIELD_PREP(mask, val)           (((typeof(mask))(val) << __lkpi_bf_shift(mask)) & (mask))
#define FIELD_GET(mask, reg)            ((typeof(mask))(((reg) & (mask)) >> __lkpi_bf_shift(mask)))
#define FIELD_FIT(mask, val)            (!((((typeof(mask))(val)) << __lkpi_bf_shift(mask)) & ~(mask)))
#define FIELD_MAX(mask)                 ((typeof(mask))((mask) >> __lkpi_bf_shift(mask)))
#define FIELD_PREP_CONST(mask, val)     (((val) << __lkpi_bf_shift(mask)) & (mask))

#define __LKPI_BF_FUNCS(type)                                                           \
    static inline type type##_encode_bits(type v, type field)                          \
    { return (v << __lkpi_bf_shift(field)) & field; }                                   \
    static inline type type##_get_bits(type v, type field)                             \
    { return (v & field) >> __lkpi_bf_shift(field); }                                   \
    static inline type type##_replace_bits(type old, type v, type field)               \
    { return (old & ~field) | type##_encode_bits(v, field); }
__LKPI_BF_FUNCS(u8)
__LKPI_BF_FUNCS(u16)
__LKPI_BF_FUNCS(u32)
__LKPI_BF_FUNCS(u64)

#endif
