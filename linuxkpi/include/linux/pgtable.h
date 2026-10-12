/* linux/pgtable.h -- Kestrel's Linux KPI: page protection values
 *
 * Kestrel maps kernel memory itself; these values only travel through
 * Linux code (TTM picks one per buffer) and are never written to a page table. */
#ifndef _LKPI_LINUX_PGTABLE_H
#define _LKPI_LINUX_PGTABLE_H

#include <linux/types.h>

#ifndef _LKPI_PGPROT_T
#define _LKPI_PGPROT_T
typedef struct { unsigned long pgprot; } pgprot_t;
#endif
#define __pgprot(x)                     ((pgprot_t) { (x) })
#define pgprot_val(x)                   ((x).pgprot)
#define _LKPI_PROT_WC                   0x08UL      /* PAT index 1: write-combining */
#define _LKPI_PROT_UC                   0x18UL      /* PCD | PWT: uncached */
#define PAGE_KERNEL                     __pgprot(0x163UL)
#define PAGE_KERNEL_IO                  PAGE_KERNEL
#define PAGE_SHARED                     __pgprot(0x67UL)
#define pgprot_writecombine(p)          __pgprot(pgprot_val(p) | _LKPI_PROT_WC)
#define pgprot_noncached(p)             __pgprot(pgprot_val(p) | _LKPI_PROT_UC)

#define pgprot_decrypted(prot)          (prot)     /* no memory encryption */
#define pgprot_encrypted(prot)          (prot)

#endif
