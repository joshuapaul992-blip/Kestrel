/* mm/vmm.c -- page-table editing on top of the boot identity map.
 *
 * boot.asm identity-maps 0-4 GiB with 2 MiB pages, which covers the kernel,
 * all memory the PMM hands out, and the frame buffer on virtually every
 * BIOS/VBE system. Some UEFI firmware places the GOP frame buffer above
 * 4 GiB; vmm_identity_map() extends the mapping on demand. This is also the
 * hook where per-process address spaces will be built later. */
#include <kernel/smp.h>
#include <kernel/mm.h>
#include <kernel/cpu.h>
#include <kernel/klog.h>
#include <kernel/uvm.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

#define PTE_PRESENT 0x001
#define PTE_WRITE   0x002
#define PTE_HUGE    0x080
#define ADDR_MASK   0x000FFFFFFFFFF000ull

/* Always edit the kernel's tables, whichever process' CR3 is loaded: the
 * PDPTs under PML4[0..127] are shared by every address space (uvm.c). */
static uint64_t *kpml4(void)
{
    uint64_t k = uvm_kernel_pml4();
    return (uint64_t *)(k ? k : (read_cr3() & ADDR_MASK));
}

static uint64_t *table_for(uint64_t *parent, int idx)
{
    if (!(parent[idx] & PTE_PRESENT)) {
        uint64_t t = pmm_alloc();               /* zeroed, identity mapped */
        if (!t) return NULL;
        parent[idx] = t | PTE_PRESENT | PTE_WRITE;
    }
    return (uint64_t *)(parent[idx] & ADDR_MASK);
}

bool vmm_set_uncached(uint64_t phys, uint64_t size)
{
    if (!vmm_identity_map(phys, size, VMM_WRITE | VMM_PCD | VMM_PWT)) return false;
    uint64_t *pml4 = kpml4();
    for (uint64_t a = phys & ~0x1FFFFFull; a < phys + size; a += 0x200000) {
        uint64_t *pdpt = (uint64_t *)(pml4[(a >> 39) & 511] & ADDR_MASK);
        uint64_t *pd = (uint64_t *)(pdpt[(a >> 30) & 511] & ADDR_MASK);
        uint64_t *pde = &pd[(a >> 21) & 511];
        if ((*pde & PTE_HUGE) && (*pde & (VMM_PCD | VMM_PWT)) != (VMM_PCD | VMM_PWT)) {
            *pde |= VMM_PCD | VMM_PWT;
            tlb_shootdown_kernel(a, a + PAGE_SIZE);     /* one entry covers the large page */
        }
    }
    return true;
}

/* Write-combining for frame buffers. Firmware MTRRs normally make video
 * memory uncached, which turns every frame-buffer write into a single slow
 * bus transaction. A write-combining PAT type overrides an uncached MTRR, so
 * PAT entry 1 (PWT=1, PCD=0 -- unused otherwise) is reprogrammed from
 * write-through to write-combining, as Linux does, and frame-buffer pages
 * are mapped with PWT alone. */
#define MSR_PAT     0x277
#define PAT_WC      0x01

static bool pat_wc_ready(void)
{
    static int state;                               /* 0 untried, 1 ok, -1 no PAT */
    if (state) return state > 0;
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    if (!(d & (1u << 16))) { state = -1; return false; }
    uint64_t f = irq_save();
    uint64_t pat = (rdmsr(MSR_PAT) & ~(0xFFull << 8)) | ((uint64_t)PAT_WC << 8);
    __asm__ volatile("wbinvd" ::: "memory");
    wrmsr(MSR_PAT, pat);
    __asm__ volatile("wbinvd" ::: "memory");
    write_cr3(read_cr3());                          /* drop TLB entries with the old type */
    irq_restore(f);
    state = 1;
    return true;
}

static uint64_t *identity_pte(uint64_t phys);

bool vmm_set_write_combining(uint64_t phys, uint64_t size)
{
    if (!size || !pat_wc_ready() || !vmm_identity_map(phys, size, VMM_WRITE)) return false;
    uint64_t *pml4 = kpml4();
    int pages = 0;
    for (uint64_t a = phys & ~0x1FFFFFull; a < phys + size; a += 0x200000) {
        uint64_t *pdpt = (uint64_t *)(pml4[(a >> 39) & 511] & ADDR_MASK);
        uint64_t *pd = (uint64_t *)(pdpt[(a >> 30) & 511] & ADDR_MASK);
        uint64_t *pde = &pd[(a >> 21) & 511];
        /* MMIO made uncached stays so, unless the whole 2 MiB page is inside
         * the range asked for: a frame buffer a driver mapped both ways
         * (Linux's bochs ioremaps VRAM, then TTM ioremap_wc's it). */
        bool inside = a >= phys && a + 0x200000 <= phys + size;
        if ((*pde & PTE_HUGE) && (inside || !(*pde & VMM_PCD))) {
            *pde = (*pde & ~(uint64_t)VMM_PCD) | VMM_PWT;
            tlb_shootdown_kernel(a, a + PAGE_SIZE);
            pages++;
            continue;
        }
        /* An uncached large page only partly in the range (a frame buffer
         * that does not start or end on 2 MiB, e.g. after a mode switch), or
         * one already split: just the 4 KiB pages in the range. */
        uint64_t lo = a > (phys & ~0xFFFull) ? a : (phys & ~0xFFFull);
        uint64_t hi = a + 0x200000 < phys + size ? a + 0x200000 : phys + size;
        uint64_t f = irq_save();
        for (uint64_t p = lo; p < hi; p += PAGE_SIZE) {
            uint64_t *pte = identity_pte(p);
            if (!pte) break;
            *pte = (*pte & ~(uint64_t)(VMM_PCD | 0x80)) | VMM_PWT;      /* PAT entry 1 */
            pages++;
        }
        irq_restore(f);
        tlb_shootdown_kernel(lo, hi);
    }
    __asm__ volatile("wbinvd" ::: "memory");
    return pages > 0;
}

/* The 4 KiB PTE of identity-mapped `phys`, splitting its 2 MiB page into a
 * page table first if needed (same physical pages, same attributes). The
 * boot identity map lives in the shared kernel PDPTs, so the change is seen
 * by every address space. NULL if unmapped or out of memory. */
#define PTE_NX      (1ull << 63)
#define PDE_PAT     (1ull << 12)                        /* PAT bit of a 2 MiB PDE */
static uint64_t *identity_pte(uint64_t phys)
{
    uint64_t *pml4 = kpml4();
    if (!(pml4[(phys >> 39) & 511] & PTE_PRESENT)) return NULL;
    uint64_t *pdpt = (uint64_t *)(pml4[(phys >> 39) & 511] & ADDR_MASK);
    if (!(pdpt[(phys >> 30) & 511] & PTE_PRESENT) || (pdpt[(phys >> 30) & 511] & PTE_HUGE)) return NULL;
    uint64_t *pd = (uint64_t *)(pdpt[(phys >> 30) & 511] & ADDR_MASK);
    uint64_t *pde = &pd[(phys >> 21) & 511];
    if (!(*pde & PTE_PRESENT)) return NULL;
    if (*pde & PTE_HUGE) {
        uint64_t pt_phys = pmm_alloc();                 /* zeroed, identity mapped */
        if (!pt_phys) return NULL;
        uint64_t *pt = (uint64_t *)pt_phys;
        uint64_t base = *pde & 0x000FFFFFFFE00000ull;
        uint64_t attr = *pde & (PTE_PRESENT | PTE_WRITE | VMM_PWT | VMM_PCD | PTE_NX);
        if (*pde & PDE_PAT) attr |= 0x80;               /* PAT bit moves to bit 7 in a PTE */
        for (int i = 0; i < 512; i++) pt[i] = (base + (uint64_t)i * 4096) | attr;
        *pde = pt_phys | PTE_PRESENT | PTE_WRITE;
        /* the large page's TLB entries, on every CPU */
        tlb_shootdown_kernel(base, base + 0x200000);
    }
    uint64_t *pt = (uint64_t *)(*pde & ADDR_MASK);
    return &pt[(phys >> 12) & 511];
}

bool vmm_set_guard(uint64_t phys, bool guard)
{
    uint64_t f = irq_save();
    uint64_t *pte = identity_pte(phys & ~0xFFFull);
    if (pte) {
        if (guard) *pte &= ~(uint64_t)PTE_PRESENT; else *pte |= PTE_PRESENT;
        tlb_shootdown_kernel(phys & ~0xFFFull, (phys & ~0xFFFull) + PAGE_SIZE);
    }
    irq_restore(f);
    return pte != NULL;
}

bool vmm_identity_map(uint64_t phys, uint64_t size, uint64_t flags)
{
    uint64_t *pml4 = kpml4();
    if (phys + size > VMM_VMAP_BASE && phys < VMM_VMAP_BASE + VMM_VMAP_SIZE) {
        kprintf("vmm: %lx (+%lx) is in the vmap window, not identity-mapped\n", phys, size);
        return false;
    }
    uint64_t start = phys & ~0x1FFFFFull;
    uint64_t end = (phys + size + 0x1FFFFF) & ~0x1FFFFFull;

    for (uint64_t a = start; a < end; a += 0x200000) {
        uint64_t *pdpt = table_for(pml4, (a >> 39) & 511);
        if (!pdpt) return false;
        uint64_t *pd = table_for(pdpt, (a >> 30) & 511);
        if (!pd) return false;
        uint64_t *pde = &pd[(a >> 21) & 511];
        if (!(*pde & PTE_PRESENT)) {
            *pde = a | PTE_PRESENT | PTE_HUGE | (flags & (VMM_WRITE | VMM_PWT | VMM_PCD | VMM_USER));
            invlpg(a);
        }
    }
    return true;
}

/* ---- vmap window: scattered frames at consecutive kernel addresses --------
 * Linux drivers map buffers made of single pages (vmap()). The window is in
 * the last shared kernel PML4 slot, so every address space sees it; 4 KiB
 * pages, first fit. `used` marks pages in use and `head` the first page of
 * each mapping, so vmm_vunmap() needs only the address. */
#define VMAP_PAGES      (VMM_VMAP_SIZE / PAGE_SIZE)
static uint64_t vmap_used[VMAP_PAGES / 64], vmap_head[VMAP_PAGES / 64];
static struct spinlock vmap_lock;

static bool vbit(const uint64_t *map, uint64_t i) { return map[i / 64] >> (i % 64) & 1; }
static void vset(uint64_t *map, uint64_t i, bool on)
{
    if (on) map[i / 64] |= 1ull << (i % 64);
    else map[i / 64] &= ~(1ull << (i % 64));
}

static uint64_t *vmap_pte(uint64_t va)
{
    uint64_t *pdpt = table_for(kpml4(), (va >> 39) & 511);
    uint64_t *pd = pdpt ? table_for(pdpt, (va >> 30) & 511) : NULL;
    uint64_t *pt = pd ? table_for(pd, (va >> 21) & 511) : NULL;
    return pt ? &pt[(va >> 12) & 511] : NULL;
}

void *vmm_vmap(const uint64_t *frames, size_t n, uint64_t flags)
{
    if (!n || n > VMAP_PAGES) return NULL;
    uint64_t f = spin_lock_irqsave(&vmap_lock);
    uint64_t first = 0, run = 0;
    for (uint64_t i = 0; i < VMAP_PAGES && run < n; i++) {
        if (vbit(vmap_used, i)) { run = 0; continue; }
        if (!run) first = i;
        run++;
    }
    if (run < n) {
        spin_unlock_irqrestore(&vmap_lock, f);
        kprintf("vmm: vmap window full (%lu pages wanted)\n", (unsigned long)n);
        return NULL;
    }
    for (uint64_t i = first; i < first + n; i++) vset(vmap_used, i, true);
    vset(vmap_head, first, true);
    spin_unlock_irqrestore(&vmap_lock, f);

    uint64_t va = VMM_VMAP_BASE + first * PAGE_SIZE;
    for (size_t i = 0; i < n; i++) {
        uint64_t *pte = vmap_pte(va + i * PAGE_SIZE);
        if (!pte) {
            kprintf("vmm: out of memory for vmap page tables\n");
            vmm_vunmap((void *)(uintptr_t)va);
            return NULL;
        }
        *pte = (frames[i] & ADDR_MASK) | PTE_PRESENT | (flags & (VMM_WRITE | VMM_PWT | VMM_PCD));
    }
    return (void *)(uintptr_t)va;
}

void vmm_vunmap(void *addr)
{
    uint64_t va = (uint64_t)(uintptr_t)addr;
    if (va < VMM_VMAP_BASE || va >= VMM_VMAP_BASE + VMM_VMAP_SIZE || (va & (PAGE_SIZE - 1))) return;
    uint64_t first = (va - VMM_VMAP_BASE) / PAGE_SIZE, i = first;
    uint64_t f = spin_lock_irqsave(&vmap_lock);
    if (!vbit(vmap_head, first)) {
        spin_unlock_irqrestore(&vmap_lock, f);
        kprintf("vmm: vunmap of %lx, which is not a mapping\n", va);
        return;
    }
    spin_unlock_irqrestore(&vmap_lock, f);
    for (i = first; i < VMAP_PAGES && vbit(vmap_used, i) && (i == first || !vbit(vmap_head, i)); i++) {
        uint64_t *pte = vmap_pte(VMM_VMAP_BASE + i * PAGE_SIZE);
        if (pte) *pte = 0;
    }
    tlb_shootdown_kernel(va, VMM_VMAP_BASE + i * PAGE_SIZE);
    f = spin_lock_irqsave(&vmap_lock);
    vset(vmap_head, first, false);
    for (uint64_t j = first; j < i; j++) vset(vmap_used, j, false);
    spin_unlock_irqrestore(&vmap_lock, f);
}

/* The frame behind a vmap address (for vmalloc_to_page()), or 0. */
uint64_t vmm_vmap_phys(const void *addr)
{
    uint64_t va = (uint64_t)(uintptr_t)addr;
    if (va < VMM_VMAP_BASE || va >= VMM_VMAP_BASE + VMM_VMAP_SIZE) return 0;
    uint64_t *pte = vmap_pte(va & ~(uint64_t)(PAGE_SIZE - 1));
    return pte && (*pte & PTE_PRESENT) ? (*pte & ADDR_MASK) | (va & (PAGE_SIZE - 1)) : 0;
}
