/* linuxkpi/src/mmap.c -- Linux drivers' memory mapped into processes
 *
 * mmap() of a device node (a DRM file) reaches the driver's f_op->mmap with
 * a vm_area_struct of its own, kept for as long as Kestrel's region lasts.
 * The driver sets vm_ops; Kestrel's page faults in the region call
 * vm_ops->fault, which maps the frames with vmf_insert_pfn_prot() (TTM
 * inserts a few around the fault). When a driver moves a buffer (TTM
 * evicting it from VRAM) it calls unmap_mapping_range() on the file's
 * address_space: every region of that mapping over the range is unmapped,
 * and the next touch faults the new place in.
 *
 * Each address_space lists its regions. One mutex covers every list; it
 * is taken after the address space's lock (a region closes when Kestrel
 * frees it) and never around a fault. */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <lkpi/glue.h>
#include "internal.h"

#define LKPI_PROT_READ   0x1
#define LKPI_PROT_WRITE  0x2
#define LKPI_PROT_EXEC   0x4
#define LKPI_MAP_SHARED  0x01
#define LKPI_MAP_TYPE    0x0f

struct lkpi_vma {
    struct vm_area_struct vma;
    struct list_head      link;             /* on mapping->i_mmap */
    struct address_space *mapping;
};

static DEFINE_MUTEX(mmap_lock);

static void link_vma(struct lkpi_vma *lv)
{
    struct address_space *m = lv->vma.vm_file ? lv->vma.vm_file->f_mapping : NULL;
    INIT_LIST_HEAD(&lv->link);
    lv->mapping = m;
    if (!m) return;
    mutex_lock(&mmap_lock);
    if (!m->i_mmap.next) INIT_LIST_HEAD(&m->i_mmap);
    list_add_tail(&lv->link, &m->i_mmap);
    mutex_unlock(&mmap_lock);
}

static void unlink_vma(struct lkpi_vma *lv)
{
    if (!lv->mapping) return;
    mutex_lock(&mmap_lock);
    list_del(&lv->link);
    mutex_unlock(&mmap_lock);
}

static int kpi_fault(void *ctx, unsigned long va, int write)
{
    struct lkpi_vma *lv = ctx;
    struct vm_area_struct *vma = &lv->vma;
    struct vm_fault vmf = {
        .vma = vma,
        .flags = write ? FAULT_FLAG_WRITE : 0,
        .pgoff = vma->vm_pgoff + ((va - vma->vm_start) >> PAGE_SHIFT),
        .address = va & PAGE_MASK,
    };
    if (!vma->vm_ops || !vma->vm_ops->fault) return -EIO;
    vm_fault_t r = vma->vm_ops->fault(&vmf);
    if (r & VM_FAULT_OOM) return -ENOMEM;
    if (r & (VM_FAULT_SIGBUS | VM_FAULT_HWPOISON)) return -EIO;
    if (r & VM_FAULT_SIGSEGV) return -EFAULT;
    if (vmf.page) {                     /* a page the driver handed back instead of mapping it */
        int e = lkpi_vm_insert(vma->vm_mm, vmf.address, page_to_phys(vmf.page),
                               pgprot_val(vma->vm_page_prot) & 0x98);
        put_page(vmf.page);             /* the driver keeps the page while it is mapped */
        return e;
    }
    return 0;
}

static void *kpi_dup(void *ctx, void *mm, unsigned long start, unsigned long end, unsigned long off)
{
    struct lkpi_vma *lv = ctx, *n = kmalloc(sizeof(*n), GFP_KERNEL);

    if (!n) return NULL;
    *n = *lv;
    if (mm) n->vma.vm_mm = mm;
    n->vma.vm_start = start;
    n->vma.vm_end = end;
    n->vma.vm_pgoff = off >> PAGE_SHIFT;
    link_vma(n);
    if (n->vma.vm_ops && n->vma.vm_ops->open) n->vma.vm_ops->open(&n->vma);
    return n;
}

static void kpi_resize(void *ctx, unsigned long start, unsigned long end, unsigned long off)
{
    struct lkpi_vma *lv = ctx;

    lv->vma.vm_start = start;
    lv->vma.vm_end = end;
    lv->vma.vm_pgoff = off >> PAGE_SHIFT;
}

static void kpi_close(void *ctx)
{
    struct lkpi_vma *lv = ctx;

    unlink_vma(lv);
    if (lv->vma.vm_ops && lv->vma.vm_ops->close) lv->vma.vm_ops->close(&lv->vma);
    kfree(lv);
}

static const struct lkpi_mmap_ops kpi_mmap_ops = {
    .fault = kpi_fault, .dup = kpi_dup, .resize = kpi_resize, .close = kpi_close,
};

/* The cdev's mmap (chardev.c): set the region up through f_op->mmap. */
int lkpi_file_mmap(struct file *file, struct lkpi_mmap *m)
{
    struct lkpi_vma *lv;
    struct vm_area_struct *vma;
    int r;

    if (!file->f_op->mmap) return -ENODEV;
    if ((m->flags & LKPI_MAP_TYPE) != LKPI_MAP_SHARED && (m->flags & LKPI_MAP_TYPE) != 0x03) return -EINVAL;
    lv = kzalloc(sizeof(*lv), GFP_KERNEL);
    if (!lv) return -ENOMEM;
    vma = &lv->vma;
    vma->vm_start = m->start;
    vma->vm_end = m->end;
    vma->vm_pgoff = m->off >> PAGE_SHIFT;
    vma->vm_mm = m->mm;
    vma->vm_file = file;
    vma->vm_flags = VM_SHARED | VM_MAYSHARE | VM_MAYREAD | VM_MAYEXEC;
    if (file->f_mode & FMODE_WRITE) vma->vm_flags |= VM_MAYWRITE;
    if (m->prot & LKPI_PROT_READ) vma->vm_flags |= VM_READ;
    if (m->prot & LKPI_PROT_WRITE) vma->vm_flags |= VM_WRITE;
    if (m->prot & LKPI_PROT_EXEC) vma->vm_flags |= VM_EXEC;
    vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
    r = file->f_op->mmap(file, vma);
    if (r) { kfree(lv); return r; }
    link_vma(lv);
    m->ops = &kpi_mmap_ops;
    m->ctx = lv;
    return 0;
}

/* ---- what drivers call -------------------------------------------------------------------- */
vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn, pgprot_t pgprot)
{
    if (addr < vma->vm_start || addr >= vma->vm_end) return VM_FAULT_SIGBUS;
    int r = lkpi_vm_insert(vma->vm_mm, addr, pfn << PAGE_SHIFT, pgprot_val(pgprot) & 0x98);
    return r == -ENOMEM ? VM_FAULT_OOM : r ? VM_FAULT_SIGBUS : VM_FAULT_NOPAGE;
}
EXPORT_SYMBOL(vmf_insert_pfn_prot);

vm_fault_t vmf_insert_pfn(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn)
{
    return vmf_insert_pfn_prot(vma, addr, pfn, vma->vm_page_prot);
}
EXPORT_SYMBOL(vmf_insert_pfn);

vm_fault_t vmf_insert_page(struct vm_area_struct *vma, unsigned long addr, struct page *page)
{
    return vmf_insert_pfn(vma, addr, page_to_pfn(page));
}
EXPORT_SYMBOL(vmf_insert_page);

void unmap_mapping_range(struct address_space *mapping, loff_t const holebegin, loff_t const holelen,
                         int even_cows)
{
    unsigned long first = (unsigned long)holebegin >> PAGE_SHIFT;
    unsigned long last = holelen ? first + (((unsigned long)holelen + PAGE_SIZE - 1) >> PAGE_SHIFT) : ULONG_MAX;
    struct lkpi_vma *lv;

    mutex_lock(&mmap_lock);
    if (mapping->i_mmap.next) {
        list_for_each_entry(lv, &mapping->i_mmap, link) {
            struct vm_area_struct *vma = &lv->vma;
            unsigned long vfirst = vma->vm_pgoff, vlast = vfirst + vma_pages(vma);
            unsigned long s = max(first, vfirst), e = min(last, vlast);
            if (s >= e) continue;
            lkpi_vm_zap(vma->vm_mm, vma->vm_start + ((s - vfirst) << PAGE_SHIFT),
                        vma->vm_start + ((e - vfirst) << PAGE_SHIFT));
        }
    }
    mutex_unlock(&mmap_lock);
}
EXPORT_SYMBOL(unmap_mapping_range);
