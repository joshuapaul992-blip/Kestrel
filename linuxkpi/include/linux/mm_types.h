/* linux/mm_types.h -- Kestrel's Linux KPI: struct page
 *
 * Kestrel keeps no per-frame structure; the KPI makes one on demand for
 * every frame Linux code allocates or asks about (linuxkpi/src/page.c),
 * from a table indexed by frame number. Memory is identity mapped, so a
 * page's address is its physical address. */
#ifndef _LKPI_LINUX_MM_TYPES_H
#define _LKPI_LINUX_MM_TYPES_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/list.h>

struct address_space;
struct task_struct;

struct page {
    unsigned long           flags;
    unsigned long           lkpi_pfn;
    struct list_head        lru;
    struct address_space   *mapping;
    unsigned long           index;
    unsigned long           private;
    atomic_t                _refcount;
    atomic_t                _mapcount;
    unsigned int            lkpi_order;     /* of the allocation it heads */
};

struct folio { struct page page; };
struct mm_struct { int dummy; };
typedef unsigned long vm_flags_t;
typedef unsigned int vm_fault_t;
#ifndef _LKPI_PGPROT_T
#define _LKPI_PGPROT_T
typedef struct { unsigned long pgprot; } pgprot_t;
#endif

/* mappings of a file (a DRM device) into a process: Kestrel does not map
 * driver memory into processes yet, so these only describe a request */
struct vm_operations_struct;
struct file;
struct vm_area_struct {
    unsigned long vm_start, vm_end;
    struct mm_struct *vm_mm;
    pgprot_t vm_page_prot;
    vm_flags_t vm_flags;
    unsigned long vm_pgoff;             /* offset in the file, in pages */
    struct file *vm_file;
    void *vm_private_data;
    const struct vm_operations_struct *vm_ops;
};
struct vm_fault {
    struct vm_area_struct *vma;
    unsigned int flags;                 /* FAULT_FLAG_* */
    pgoff_t pgoff;
    unsigned long address;
    struct page *page;
};
struct vm_operations_struct {
    void (*open)(struct vm_area_struct *area);
    void (*close)(struct vm_area_struct *area);
    vm_fault_t (*fault)(struct vm_fault *vmf);
    int (*access)(struct vm_area_struct *vma, unsigned long addr, void *buf, int len, int write);
};

/* Linux's maple tree (third_party/linux/lib/maple_tree.c) comes with the
 * memory types there; only builds that vendor it (DRM) have it. */
#if __has_include(<linux/maple_tree.h>)
#include <linux/maple_tree.h>
#endif

#endif
