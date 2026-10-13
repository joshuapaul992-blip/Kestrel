/* linux/pagevec.h -- Kestrel's Linux KPI: batches of folios */
#ifndef _LKPI_LINUX_PAGEVEC_H
#define _LKPI_LINUX_PAGEVEC_H

#include <linux/mm.h>

#define PAGEVEC_SIZE                    31
struct folio_batch {
    unsigned char nr;
    struct folio *folios[PAGEVEC_SIZE];
};
static inline void folio_batch_init(struct folio_batch *fbatch) { fbatch->nr = 0; }
static inline unsigned int folio_batch_count(struct folio_batch *fbatch) { return fbatch->nr; }
static inline unsigned folio_batch_add(struct folio_batch *fbatch, struct folio *folio)
{
    fbatch->folios[fbatch->nr++] = folio;
    return PAGEVEC_SIZE - fbatch->nr;
}
static inline void check_move_unevictable_folios(struct folio_batch *fbatch) { }

static inline void __folio_batch_release(struct folio_batch *fbatch)
{
    unsigned i;
    for (i = 0; i < fbatch->nr; i++) put_page(&fbatch->folios[i]->page);
    fbatch->nr = 0;
}

#endif
