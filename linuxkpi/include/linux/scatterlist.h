/* linux/scatterlist.h -- Kestrel's Linux KPI: scatter-gather lists (linuxkpi/src/dma.c)
 * A table is one flat array (no chaining); entries hold the page directly. */
#ifndef _LKPI_LINUX_SCATTERLIST_H
#define _LKPI_LINUX_SCATTERLIST_H

#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/gfp_types.h>
#include <asm/page.h>
#include <linux/mm.h>

struct scatterlist {
    struct page    *lkpi_page;
    unsigned int    offset;
    unsigned int    length;
    dma_addr_t      dma_address;
    unsigned int    dma_length;
    bool            lkpi_last;
};

struct sg_table {
    struct scatterlist *sgl;
    unsigned int        nents;          /* mapped */
    unsigned int        orig_nents;
};

#define sg_dma_address(sg)      ((sg)->dma_address)
#define sg_dma_len(sg)          ((sg)->dma_length)
static inline struct page *sg_page(struct scatterlist *sg) { return sg->lkpi_page; }
static inline bool sg_is_last(struct scatterlist *sg) { return sg->lkpi_last; }
static inline struct scatterlist *sg_next(struct scatterlist *sg) { return sg->lkpi_last ? NULL : sg + 1; }
static inline void sg_mark_end(struct scatterlist *sg) { sg->lkpi_last = true; }
static inline void sg_unmark_end(struct scatterlist *sg) { sg->lkpi_last = false; }
static inline void sg_assign_page(struct scatterlist *sg, struct page *page) { sg->lkpi_page = page; }
static inline void sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len, unsigned int offset)
{ sg->lkpi_page = page; sg->offset = offset; sg->length = len; }
void sg_init_table(struct scatterlist *sgl, unsigned int nents);
void sg_init_one(struct scatterlist *sg, const void *buf, unsigned int buflen);
void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);
static inline dma_addr_t sg_phys(struct scatterlist *sg) { return ((dma_addr_t)sg->lkpi_page->lkpi_pfn << PAGE_SHIFT) + sg->offset; }
static inline void *sg_virt(struct scatterlist *sg) { return (void *)(unsigned long)sg_phys(sg); }
int  sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp);
void sg_free_table(struct sg_table *table);
int  sg_alloc_table_from_pages(struct sg_table *sgt, struct page **pages, unsigned int n_pages,
                               unsigned int offset, unsigned long size, gfp_t gfp);
int  sg_nents(struct scatterlist *sg);
#define for_each_sg(sglist, sg, nr, __i) \
    for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))
#define for_each_sgtable_sg(sgt, sg, i)         for_each_sg((sgt)->sgl, sg, (sgt)->orig_nents, i)
#define for_each_sgtable_dma_sg(sgt, sg, i)     for_each_sg((sgt)->sgl, sg, (sgt)->nents, i)

/* walking a table page by page */
struct sg_page_iter {
    struct scatterlist *sg;
    unsigned int        sg_pgoffset;    /* page within sg */
    unsigned int        __nents;
    int                 __pg_advance;
};
struct sg_dma_page_iter { struct sg_page_iter base; };
static inline unsigned int __lkpi_sg_npages(struct scatterlist *sg)
{ return (sg->offset + sg->length + PAGE_SIZE - 1) >> PAGE_SHIFT; }
static inline void __sg_page_iter_start(struct sg_page_iter *piter, struct scatterlist *sglist,
                                        unsigned int nents, unsigned long pgoffset)
{
    piter->__pg_advance = 0;
    piter->__nents = nents;
    piter->sg = sglist;
    piter->sg_pgoffset = pgoffset;
}
static inline bool __sg_page_iter_next(struct sg_page_iter *piter)
{
    if (!piter->__nents || !piter->sg) return false;
    piter->sg_pgoffset += piter->__pg_advance;
    piter->__pg_advance = 1;
    while (piter->sg_pgoffset >= __lkpi_sg_npages(piter->sg)) {
        piter->sg_pgoffset -= __lkpi_sg_npages(piter->sg);
        piter->sg = sg_next(piter->sg);
        if (!--piter->__nents || !piter->sg) return false;
    }
    return true;
}
static inline struct page *sg_page_iter_page(struct sg_page_iter *piter)
{ return pfn_to_page(page_to_pfn(sg_page(piter->sg)) + piter->sg_pgoffset); }
static inline bool __sg_page_iter_dma_next(struct sg_dma_page_iter *dma_iter) { return __sg_page_iter_next(&dma_iter->base); }
static inline dma_addr_t sg_page_iter_dma_address(struct sg_dma_page_iter *dma_iter)
{ return sg_dma_address(dma_iter->base.sg) + ((dma_addr_t)dma_iter->base.sg_pgoffset << PAGE_SHIFT); }
#define for_each_sg_page(sglist, piter, nents, pgoffset) \
    for (__sg_page_iter_start((piter), (sglist), (nents), (pgoffset)); __sg_page_iter_next(piter);)
#define for_each_sg_dma_page(sglist, dma_iter, dma_nents, pgoffset) \
    for (__sg_page_iter_start(&(dma_iter)->base, sglist, dma_nents, pgoffset); __sg_page_iter_dma_next(dma_iter);)
#define for_each_sgtable_page(sgt, piter, pgoffset) for_each_sg_page((sgt)->sgl, piter, (sgt)->orig_nents, pgoffset)
#define for_each_sgtable_dma_page(sgt, dma_iter, pgoffset) \
    for_each_sg_dma_page((sgt)->sgl, dma_iter, (sgt)->nents, pgoffset)

#endif
