/* linux/pagemap.h -- Kestrel's Linux KPI: page cache mappings (only shmem-backed GEM objects use them) */
#ifndef _LKPI_LINUX_PAGEMAP_H
#define _LKPI_LINUX_PAGEMAP_H

#include <linux/mm.h>
#include <linux/fs.h>

#define mapping_gfp_mask(m)             ((m)->gfp_mask)
#define mapping_set_gfp_mask(m, mask)   do { (m)->gfp_mask = (mask); } while (0)
#define mapping_gfp_constraint(m, gfp)  (mapping_gfp_mask(m) & (gfp))
#define mapping_set_unevictable(m)      do { } while (0)
#define mapping_clear_unevictable(m)    do { } while (0)
static inline void folio_mark_accessed(struct folio *f) { }
static inline void folio_mark_dirty(struct folio *f) { }
static inline long folio_nr_pages(struct folio *f) { return 1L << f->page.lkpi_order; }
static inline unsigned long folio_pfn(struct folio *f) { return f->page.lkpi_pfn; }
#define folio_file_page(f, index)       (&(f)->page)

#endif
