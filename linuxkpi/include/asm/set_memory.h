/* asm/set_memory.h -- Kestrel's Linux KPI: page caching attributes
 *
 * Kestrel maps RAM write-back and does not change it per page yet, so these
 * report success and leave the pages as they are; frame buffers in VRAM are
 * mapped write-combined through ioremap_wc() instead. */
#ifndef _LKPI_ASM_SET_MEMORY_H
#define _LKPI_ASM_SET_MEMORY_H

struct page;
static inline int set_memory_wb(unsigned long addr, int numpages) { return 0; }
static inline int set_memory_wc(unsigned long addr, int numpages) { return 0; }
static inline int set_memory_uc(unsigned long addr, int numpages) { return 0; }
static inline int set_pages_wb(struct page *page, int numpages) { return 0; }
static inline int set_pages_uc(struct page *page, int numpages) { return 0; }
static inline int set_pages_array_wb(struct page **pages, int addrinarray) { return 0; }
static inline int set_pages_array_wc(struct page **pages, int addrinarray) { return 0; }
static inline int set_pages_array_uc(struct page **pages, int addrinarray) { return 0; }

#endif
