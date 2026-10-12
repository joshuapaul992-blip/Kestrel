/* linux/io-mapping.h -- Kestrel's Linux KPI: a write-combined window onto device memory
 *
 * The memory is mapped once, whole, with ioremap_wc(); mapping a page of it
 * is then just an address. */
#ifndef _LKPI_LINUX_IO_MAPPING_H
#define _LKPI_LINUX_IO_MAPPING_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pgtable.h>
#include <linux/preempt.h>

struct io_mapping {
    resource_size_t base;
    unsigned long size;
    pgprot_t prot;
    void __iomem *iomem;
};

static inline struct io_mapping *io_mapping_init_wc(struct io_mapping *iomap, resource_size_t base, unsigned long size)
{
    iomap->iomem = ioremap_wc(base, size);
    if (!iomap->iomem) return NULL;
    iomap->base = base;
    iomap->size = size;
    iomap->prot = pgprot_writecombine(PAGE_KERNEL);
    return iomap;
}
static inline void io_mapping_fini(struct io_mapping *mapping) { iounmap(mapping->iomem); }
static inline struct io_mapping *io_mapping_create_wc(resource_size_t base, unsigned long size)
{
    struct io_mapping *iomap = kmalloc(sizeof(*iomap), GFP_KERNEL);
    if (iomap && !io_mapping_init_wc(iomap, base, size)) { kfree(iomap); iomap = NULL; }
    return iomap;
}
static inline void io_mapping_free(struct io_mapping *iomap) { io_mapping_fini(iomap); kfree(iomap); }
static inline void __iomem *io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset, unsigned long size)
{ return mapping->iomem + offset; }
static inline void io_mapping_unmap(void __iomem *vaddr) { }
static inline void __iomem *io_mapping_map_local_wc(struct io_mapping *mapping, unsigned long offset)
{ return mapping->iomem + offset; }
static inline void io_mapping_unmap_local(void __iomem *vaddr) { }
static inline void __iomem *io_mapping_map_atomic_wc(struct io_mapping *mapping, unsigned long offset)
{ preempt_disable(); return mapping->iomem + offset; }
static inline void io_mapping_unmap_atomic(void __iomem *vaddr) { preempt_enable(); }

#endif
