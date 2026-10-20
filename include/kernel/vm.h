/* include/kernel/vm.h -- user address spaces: regions, demand paging, COW
 *
 * Every user process has a struct mm: its page tables (PML4) and a sorted
 * list of regions (struct vma), each a page-aligned range with protection
 * bits and a backing:
 *
 *   private anonymous   zero-filled on first touch (heap, stack, bss, malloc)
 *   private file        filled from the file on first touch, then private
 *                       (ELF segments, MAP_PRIVATE of a file); bytes past
 *                       `file_end` read as zeros
 *   shared              pages of a vm_object (MAP_SHARED of anonymous memory,
 *                       of a ramfs file or memfd): every mapping sees the
 *                       same frames
 *   device              memory a driver hands out page by page as it is
 *                       touched (a GPU's buffers, MAP_SHARED of /dev/dri):
 *                       the driver's fault callback inserts the frames with
 *                       vm_dev_insert() and takes them away again with
 *                       vm_dev_zap() when it moves the memory. The frames
 *                       are the driver's (not reference counted), they may
 *                       be I/O memory with its own caching, and fork() does
 *                       not copy them: the child faults them in again.
 *
 * Nothing is mapped until it is touched: vm_fault() resolves page faults
 * from user mode and from uaccess copies. fork() copies the regions and
 * shares every page; private writable pages are made read-only and marked
 * copy-on-write (PTE bit 9) in both processes, and the first write copies
 * the page (or just takes it back if nobody else has it any more).
 *
 * Page table entries mirror the region's protection exactly: the U/S bit
 * on every user entry, R/W only for writable pages that are not shared
 * copy-on-write, and NX (EFER.NXE) on everything not executable. Pages are
 * reference counted (mm.h: page_ref/page_unref).
 *
 * Out of memory: a fault that cannot get a frame asks the OOM killer to end
 * the largest process, waits for memory, and retries; the faulting process
 * gets SIGKILL only if it is the victim or memory does not come back.
 */
#ifndef KESTREL_VM_H
#define KESTREL_VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <kernel/task.h>
#include <kernel/spinlock.h>

/* Page-fault error code bits */
#define PF_PRESENT  0x01        /* protection violation (else: not present) */
#define PF_WRITE    0x02
#define PF_USER     0x04
#define PF_RSVD     0x08
#define PF_INSN     0x10        /* instruction fetch (NX) */

/* mmap(2) protection and flags (Linux values) */
#define PROT_NONE       0x0
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_SHARED_VALIDATE 0x03
#define MAP_TYPE        0x0f
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_GROWSDOWN   0x0100
#define MAP_DENYWRITE   0x0800
#define MAP_EXECUTABLE  0x1000
#define MAP_LOCKED      0x2000
#define MAP_NORESERVE   0x4000
#define MAP_POPULATE    0x8000
#define MAP_NONBLOCK    0x10000
#define MAP_STACK       0x20000
#define MAP_HUGETLB     0x40000
#define MAP_FIXED_NOREPLACE 0x100000

/* region flags */
#define VMA_SHARED      0x1
#define VMA_STACK       0x2
#define VMA_HEAP        0x4
#define VMA_VVAR        0x8             /* the vDSO's pages (vdso.h): never writable */
#define VMA_VDSO        0x10
#define VMA_DEVICE      0x20            /* with VMA_SHARED: dops/dctx (above) */

struct file;
struct vm_object;
struct mm;

/* A device mapping's driver side. Each region has its own ctx: dup()
 * makes one for a copy (fork, in the child's mm; or the upper part when a
 * region is split, mm NULL, with resize() shrinking the original), close()
 * ends it. fault() runs with
 * the address space locked and may sleep; it maps at least `va` with
 * vm_dev_insert(), or returns -EIO (SIGBUS), -ENOMEM or -EFAULT. Unless
 * `nolock`, the callbacks run under the big kernel lock. */
struct vm_devmap_ops {
    int   (*fault)(void *ctx, uint64_t va, bool write);
    void *(*dup)(void *ctx, struct mm *mm, uint64_t start, uint64_t end, uint64_t off);   /* NULL on OOM */
    void  (*resize)(void *ctx, uint64_t start, uint64_t end, uint64_t off);
    void  (*close)(void *ctx);
    bool  nolock;
};

/* What vnode_ops.mmap gets: the region's place, and it fills in ops/ctx. */
struct vm_devmap {
    uint64_t start, end, off;
    int      prot, flags;
    struct mm *mm;
    const struct vm_devmap_ops *ops;
    void    *ctx;
};

struct vma {
    uint64_t start, end;        /* [start, end), page aligned               */
    uint32_t prot;              /* PROT_*                                    */
    uint32_t flags;             /* VMA_*                                     */
    struct vm_object *obj;      /* shared: the pages                         */
    struct file *file;          /* private file: where the bytes come from;
                                   device: the device file                  */
    uint64_t off;               /* object / file offset of `start` (bytes)   */
    uint64_t file_end;          /* private file: VA where the file data ends */
    const struct vm_devmap_ops *dops;   /* device mappings */
    void    *dctx;
    struct vma *next;
};

struct mm {
    uint64_t     pml4;          /* physical address of the top table        */
    struct vma  *vmas;          /* sorted, non-overlapping                  */
    int          users;         /* tasks running in it (threads, vfork)     */
    struct kmutex lock;         /* regions and page tables                  */
    struct spinlock fast;       /* held by faults resolved without the big
                                 * kernel lock (vm_fault_fast); see vm.c   */
    uint64_t     brk_start, brk;
    uint64_t     rss;           /* user pages mapped (statistics, OOM)      */
    bool         oom_victim;    /* the OOM killer is waiting for it to go   */
};

/* With interrupts on; may sleep. Needs no lock held (it takes the big
 * kernel lock where it must, see vm.c). */
int vm_fault(uint64_t addr, uint64_t err, bool user_mode);   /* 0, -EFAULT, -EACCES, -EIO, -ENOMEM */
/* A user-mode fault resolved with interrupts off and no sleeping: true if
 * done, false to go through vm_fault(). */
bool vm_fault_fast(uint64_t addr, uint64_t err);

struct mm *mm_create(void);                 /* empty address space; NULL on OOM */
void       mm_get(struct mm *mm);
void       mm_put(struct mm *mm);           /* destroys at 0 users              */
struct mm *mm_fork(struct mm *src);         /* copy-on-write copy; NULL on OOM  */

/* mmap(): place (or, with MAP_FIXED, replace at) a region. `file` (private
 * mappings) or `obj` (shared) supply the contents; neither: anonymous.
 * Returns the address or -errno. `file_end` limits private file data. */
int64_t vm_mmap(struct mm *mm, uint64_t addr, uint64_t len, int prot, int flags,
                struct file *file, struct vm_object *obj, uint64_t off, uint64_t file_end);
/* mmap() of a device file whose vnode has an mmap callback (MAP_SHARED). */
int64_t vm_mmap_dev(struct mm *mm, uint64_t addr, uint64_t len, int prot, int flags, struct file *f, uint64_t off);
/* From a device mapping's fault(): map `phys` at `va`, which must lie in a
 * device region of mm (the faulting process's, already locked). `cache`
 * holds the PTE's PWT/PCD/PAT bits (0x08 write-combining, 0x18 uncached).
 * 0, -EFAULT or -ENOMEM. */
int     vm_dev_insert(struct mm *mm, uint64_t va, uint64_t phys, uint64_t cache);
/* The driver moved or freed the memory behind [start, end) of a device
 * region: unmap it, so the next touch faults again. Callable without the
 * address space's lock; the driver keeps its own faults out meanwhile. */
void    vm_dev_zap(struct mm *mm, uint64_t start, uint64_t end);
int     vm_munmap(struct mm *mm, uint64_t addr, uint64_t len);
int     vm_mprotect(struct mm *mm, uint64_t addr, uint64_t len, int prot);
int64_t vm_mremap(struct mm *mm, uint64_t old, uint64_t olen, uint64_t nlen, int flags, uint64_t naddr);
int     vm_madvise_dontneed(struct mm *mm, uint64_t addr, uint64_t len);
void    vm_madvise_willneed(struct mm *mm, uint64_t addr, uint64_t len);   /* file pages: read ahead */
int     vm_mincore(struct mm *mm, uint64_t addr, uint64_t len, uint8_t *vec);
int64_t vm_brk(struct mm *mm, uint64_t addr);
/* Fault in [addr, addr+len) now (MAP_POPULATE, exec). */
int     vm_populate(struct mm *mm, uint64_t addr, uint64_t len, bool write);
void    vm_mark(struct mm *mm, uint64_t addr, uint32_t flag);     /* VMA_STACK, ... */
void    vm_init(void);                      /* EFER.NXE, before any user mapping */
/* Futex key: for a shared region, its object and the offset of addr in it
 * (*obj = NULL for private memory). -EFAULT if nothing is mapped there. */
int     vm_shared_key(struct mm *mm, uint64_t addr, struct vm_object **obj, uint64_t *off);
/* /proc/PID/maps */
size_t  vm_maps(struct mm *mm, char *buf, size_t cap);

/* OOM: end the largest user process (not `spare`); true if one was chosen. */
bool    oom_kill(struct tcb *spare);

#endif
