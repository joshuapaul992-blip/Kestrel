/* linux/mm.h -- Kestrel's Linux KPI: pages and addresses */
#ifndef _LKPI_LINUX_MM_H
#define _LKPI_LINUX_MM_H

#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/mm_types.h>
#include <linux/pfn.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <asm/page.h>
#include <linux/pgtable.h>
#include <linux/errno.h>
#include <linux/shrinker.h>
#include <linux/sizes.h>

struct page *pfn_to_page(unsigned long pfn);
static inline unsigned long page_to_pfn(const struct page *p) { return p->lkpi_pfn; }
#define page_to_phys(page)      ((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(phys)      pfn_to_page((phys) >> PAGE_SHIFT)
static inline void *page_address(const struct page *p) { return (void *)(p->lkpi_pfn << PAGE_SHIFT); }
#define lowmem_page_address(p)  page_address(p)
#define virt_to_page(addr)      pfn_to_page(__pa(addr) >> PAGE_SHIFT)
#define page_to_virt(p)         page_address(p)
#define virt_to_pfn(addr)       (__pa(addr) >> PAGE_SHIFT)
#define pfn_valid(pfn)          ((pfn) != 0)
#define offset_in_page(p)       ((unsigned long)(p) & ~PAGE_MASK)
#define page_folio(p)           ((struct folio *)(p))
#define folio_page(f, n)        (&(f)->page + (n))

static inline void get_page(struct page *p) { atomic_inc(&p->_refcount); }
void put_page(struct page *p);
static inline int page_count(const struct page *p) { return atomic_read(&p->_refcount); }
static inline void set_page_private(struct page *p, unsigned long v) { p->private = v; }
static inline unsigned long page_private(const struct page *p) { return p->private; }
static inline void set_page_dirty(struct page *p) { }
static inline int set_page_dirty_lock(struct page *p) { return 0; }
#define SetPageReserved(p)      do { } while (0)
#define ClearPageReserved(p)    do { } while (0)
#define PageHighMem(p)          0
#define is_vmalloc_addr(a)      lkpi_is_vmalloc_addr(a)
bool lkpi_is_vmalloc_addr(const void *a);
struct page *vmalloc_to_page(const void *addr);
unsigned long vmalloc_to_pfn(const void *addr);

unsigned long lkpi_totalram_pages(void);
#define totalram_pages()        lkpi_totalram_pages()

#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004
#define VM_SHARED               0x00000008
#define VM_MAYWRITE             0x00000020
#define VM_PFNMAP               0x00000400
#define VM_IO                   0x00004000
#define VM_DONTCOPY             0x00020000
#define VM_DONTEXPAND           0x00040000
#define VM_NORESERVE            0x00200000
#define VM_MIXEDMAP             0x10000000
#define VM_DONTDUMP             0x04000000

#define VM_FAULT_OOM            0x000001
#define VM_FAULT_SIGBUS         0x000002
#define VM_FAULT_MAJOR          0x000004
#define VM_FAULT_HWPOISON       0x000010
#define VM_FAULT_SIGSEGV        0x000040
#define VM_FAULT_NOPAGE         0x000100
#define VM_FAULT_LOCKED         0x000200
#define VM_FAULT_RETRY          0x000400
#define VM_FAULT_ERROR          (VM_FAULT_OOM | VM_FAULT_SIGBUS | VM_FAULT_SIGSEGV | VM_FAULT_HWPOISON)

#define FAULT_FLAG_WRITE            0x01
#define FAULT_FLAG_MKWRITE          0x02
#define FAULT_FLAG_ALLOW_RETRY      0x04
#define FAULT_FLAG_RETRY_NOWAIT     0x08
#define FAULT_FLAG_KILLABLE         0x10
#define FAULT_FLAG_TRIED            0x20

static inline unsigned long vma_pages(const struct vm_area_struct *vma) { return (vma->vm_end - vma->vm_start) >> PAGE_SHIFT; }
static inline void vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags) { vma->vm_flags |= flags; }
static inline void vm_flags_clear(struct vm_area_struct *vma, vm_flags_t flags) { vma->vm_flags &= ~flags; }
static inline void vm_flags_mod(struct vm_area_struct *vma, vm_flags_t set, vm_flags_t clear)
{ vma->vm_flags = (vma->vm_flags | set) & ~clear; }
pgprot_t vm_get_page_prot(unsigned long vm_flags);
/* inserting pages into a process: not available, every fault fails */
static inline vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn, pgprot_t pgprot)
{ return VM_FAULT_SIGBUS; }
static inline vm_fault_t vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn)
{ return VM_FAULT_SIGBUS; }
static inline vm_fault_t vmf_insert_page(struct vm_area_struct *vma, unsigned long addr, struct page *page)
{ return VM_FAULT_SIGBUS; }
static inline int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn, unsigned long size,
                                  pgprot_t prot) { return -ENOSYS; }
#define io_remap_pfn_range              remap_pfn_range
static inline void unmap_mapping_range(struct address_space *mapping, loff_t const holebegin,
                                       loff_t const holelen, int even_cows) { }
static inline int access_process_vm(struct task_struct *tsk, unsigned long addr, void *buf, int len,
                                    unsigned int gup_flags) { return -ENOSYS; }


static inline bool want_init_on_free(void) { return false; }
static inline void mark_page_accessed(struct page *page) { }
static inline bool is_cow_mapping(vm_flags_t flags) { return (flags & (VM_SHARED | VM_MAYWRITE)) == VM_MAYWRITE; }
static inline bool fault_flag_allow_retry_first(unsigned int flags)
{ return (flags & FAULT_FLAG_ALLOW_RETRY) && !(flags & FAULT_FLAG_TRIED); }
static inline void mmap_read_lock(struct mm_struct *mm) { }
static inline void mmap_read_unlock(struct mm_struct *mm) { }

#include <linux/highmem.h>      /* kmap() and friends, which need page_address() above */

#endif
