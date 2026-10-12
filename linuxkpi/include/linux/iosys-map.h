/* linux/iosys-map.h -- Kestrel's Linux KPI: pointers to memory that may be I/O memory
 *
 * A struct iosys_map is an address and whether it is I/O memory (a frame
 * buffer in VRAM, say), so code can copy to either without knowing which. */
#ifndef _LKPI_LINUX_IOSYS_MAP_H
#define _LKPI_LINUX_IOSYS_MAP_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/compiler.h>
#include <linux/stddef.h>

struct iosys_map {
    union {
        void __iomem *vaddr_iomem;
        void *vaddr;
    };
    bool is_iomem;
};

#define IOSYS_MAP_INIT_VADDR(vaddr_)            { .vaddr = (vaddr_), .is_iomem = false }
#define IOSYS_MAP_INIT_VADDR_IOMEM(vaddr_iomem_) { .vaddr_iomem = (vaddr_iomem_), .is_iomem = true }
#define IOSYS_MAP_INIT_OFFSET(map_, offset_) ({                                 \
    struct iosys_map __copy = *(map_);                                          \
    iosys_map_incr(&__copy, offset_);                                           \
    __copy; })

static inline void iosys_map_set_vaddr(struct iosys_map *map, void *vaddr)
{ map->vaddr = vaddr; map->is_iomem = false; }
static inline void iosys_map_set_vaddr_iomem(struct iosys_map *map, void __iomem *vaddr_iomem)
{ map->vaddr_iomem = vaddr_iomem; map->is_iomem = true; }
static inline bool iosys_map_is_equal(const struct iosys_map *lhs, const struct iosys_map *rhs)
{
    if (lhs->is_iomem != rhs->is_iomem) return false;
    return lhs->is_iomem ? lhs->vaddr_iomem == rhs->vaddr_iomem : lhs->vaddr == rhs->vaddr;
}
static inline bool iosys_map_is_null(const struct iosys_map *map)
{ return map->is_iomem ? !map->vaddr_iomem : !map->vaddr; }
static inline bool iosys_map_is_set(const struct iosys_map *map) { return !iosys_map_is_null(map); }
static inline void iosys_map_clear(struct iosys_map *map)
{ if (map->is_iomem) { map->vaddr_iomem = NULL; map->is_iomem = false; } else map->vaddr = NULL; }

static inline void iosys_map_memcpy_to(struct iosys_map *dst, size_t dst_offset, const void *src, size_t len)
{
    if (dst->is_iomem) memcpy_toio(dst->vaddr_iomem + dst_offset, src, len);
    else memcpy(dst->vaddr + dst_offset, src, len);
}
static inline void iosys_map_memcpy_from(void *dst, const struct iosys_map *src, size_t src_offset, size_t len)
{
    if (src->is_iomem) memcpy_fromio(dst, src->vaddr_iomem + src_offset, len);
    else memcpy(dst, src->vaddr + src_offset, len);
}
static inline void iosys_map_incr(struct iosys_map *map, size_t incr)
{
    if (map->is_iomem) map->vaddr_iomem += incr;
    else map->vaddr += incr;
}
static inline void iosys_map_memset(struct iosys_map *dst, size_t offset, int value, size_t len)
{
    if (dst->is_iomem) memset_io(dst->vaddr_iomem + offset, value, len);
    else memset(dst->vaddr + offset, value, len);
}

/* typed reads and writes of one value; u64 accesses to I/O memory are two 32-bit ones */
#define __iosys_map_rd_io(val__, vaddr_iomem__, type__) do {                    \
    type__ __v;                                                                 \
    memcpy_fromio(&__v, (vaddr_iomem__), sizeof(type__));                       \
    val__ = __v; } while (0)
#define __iosys_map_wr_io(val__, vaddr_iomem__, type__) do {                    \
    type__ __v = (val__);                                                       \
    memcpy_toio((vaddr_iomem__), &__v, sizeof(type__)); } while (0)
#define iosys_map_rd(map__, offset__, type__) ({                                \
    type__ val_;                                                                \
    if ((map__)->is_iomem) {                                                    \
        __iosys_map_rd_io(val_, (map__)->vaddr_iomem + (offset__), type__);     \
    } else {                                                                    \
        val_ = READ_ONCE(*(type__ *)((map__)->vaddr + (offset__)));             \
    }                                                                           \
    val_; })
#define iosys_map_wr(map__, offset__, type__, val__) ({                         \
    type__ val_ = (val__);                                                      \
    if ((map__)->is_iomem) {                                                    \
        __iosys_map_wr_io(val_, (map__)->vaddr_iomem + (offset__), type__);     \
    } else {                                                                    \
        WRITE_ONCE(*(type__ *)((map__)->vaddr + (offset__)), val_);             \
    }                                                                           \
})
#define iosys_map_rd_field(map__, struct_offset__, struct_type__, field__) ({   \
    struct_type__ *s_;                                                          \
    iosys_map_rd(map__, struct_offset__ + offsetof(struct_type__, field__),     \
                 typeof(s_->field__)); })
#define iosys_map_wr_field(map__, struct_offset__, struct_type__, field__, val__) ({ \
    struct_type__ *s_;                                                          \
    iosys_map_wr(map__, struct_offset__ + offsetof(struct_type__, field__),     \
                 typeof(s_->field__), val__); })

#endif
