/* mm/uvm.c -- per-process address spaces (see include/kernel/uvm.h) */
#include <kernel/uvm.h>
#include <kernel/mm.h>
#include <kernel/cpu.h>
#include <kernel/fpu.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/task.h>
#include <kernel/vm.h>

#define PTE_P       0x001ull
#define PTE_W       0x002ull
#define PTE_U       0x004ull
#define ADDR_MASK   0x000FFFFFFFFFF000ull
#define KERNEL_SLOTS 128                    /* PML4[0..127] */
#define DIRECT_SLOT  256                    /* PML4[256..383]: the same tables */

static uint64_t kernel_pml4;
uint64_t uvm_kernel_cr3;                    /* isr.asm: NMI, #DF and #MC run on it */

uint64_t uvm_kernel_pml4(void) { return kernel_pml4; }

void uvm_init(void)
{
    kernel_pml4 = read_cr3() & ADDR_MASK;
    uint64_t *pml4 = (uint64_t *)kernel_pml4;
    int added = 0;
    for (int i = 0; i < KERNEL_SLOTS; i++) {
        if (pml4[i] & PTE_P) continue;
        uint64_t t = pmm_alloc();           /* zeroed, identity mapped */
        if (!t) panic("uvm: out of memory for kernel PDPTs");
        pml4[i] = t | PTE_P | PTE_W;
        added++;
    }
    for (int i = 0; i < KERNEL_SLOTS; i++) pml4[DIRECT_SLOT + i] = pml4[i];   /* boot.asm set the first */
    write_cr3(read_cr3());
    uvm_kernel_cr3 = kernel_pml4;

    /* SSE for user code (the kernel itself is built without it):
     * CR0.EM off, CR0.MP on, CR4.OSFXSR + OSXMMEXCPT on, x87 reset. */
    uint64_t cr0 = read_cr0(), cr4 = read_cr4();
    cr0 = (cr0 & ~0x4ull) | 0x2ull;
    cr0 |= 1ull << 18;                      /* AM: user code that sets EFLAGS.AC gets #AC (SIGBUS) */
    cr4 |= (1ull << 9) | (1ull << 10);
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    __asm__ volatile("fninit");
    fpu_init();                             /* XSAVE and AVX where the CPU has them */
    kprintf("uvm: kernel PML4 %lx, %d shared PDPTs pre-allocated; user space %lx-%lx\n",
            kernel_pml4, added, (uint64_t)UVM_USER_START, (uint64_t)(UVM_USER_END - 1));
}

uint64_t uvm_create(void)
{
    uint64_t p = pmm_alloc();
    if (!p) return 0;
    uint64_t *n = (uint64_t *)p, *k = (uint64_t *)kernel_pml4;
    for (int i = 0; i < KERNEL_SLOTS; i++) n[i] = k[i];
    for (int i = DIRECT_SLOT; i < DIRECT_SLOT + KERNEL_SLOTS; i++) n[i] = k[i];
    return p;
}

uint64_t uvm_create_low(void)
{
    uint64_t p = uvm_create();
    if (p) ((uint64_t *)p)[0] = 0;                      /* the program's, filled on demand */
    return p;
}

static bool in(uint64_t va, uint64_t len, uint64_t lo, uint64_t hi)
{
    return va >= lo && va <= hi && len <= hi - va;
}

bool uvm_range_ok_mm(const struct mm *mm, uint64_t va, uint64_t len)
{
    return in(va, len, UVM_USER_START, UVM_USER_END) || (mm && mm->kpml4 && in(va, len, UVM_LOW_START, UVM_LOW_END));
}

bool uvm_range_ok(uint64_t va, uint64_t len)
{
    struct tcb *t = current_task();
    return uvm_range_ok_mm(t && t->user ? t->mm : NULL, va, len);
}

uint64_t uvm_region_end(uint64_t va)
{
    if (va >= UVM_USER_START && va < UVM_USER_END) return UVM_USER_END;
    if (va >= UVM_LOW_START && va < UVM_LOW_END && uvm_range_ok(va, 1)) return UVM_LOW_END;
    return 0;
}
