/* include/kernel/uvm.h -- per-process (user) address spaces
 *
 * Layout of every address space (4-level paging, 48-bit):
 *
 *   PML4[0..127]    0 .. 64 TiB          kernel: the boot identity map (0-4 GiB)
 *                                        plus MMIO the kernel maps later. The
 *                                        PDPTs are allocated once at boot and
 *                                        shared by every address space, so a
 *                                        mapping the kernel adds later is seen
 *                                        everywhere. Supervisor-only.
 *   PML4[128..255]  64 TiB .. 128 TiB    user, private to the process:
 *     UVM_IMAGE_BASE   0x4000_0000_0000  ET_DYN programs (PIE) load here
 *     UVM_INTERP_BASE  0x5000_0000_0000  their ELF interpreter (ld.so)
 *     UVM_MMAP_BASE    0x6000_0000_0000  mmap() region, first fit, grows up
 *     UVM_STACK_TOP    0x7fff_ff00_0000  main-thread stack (8 MiB), grows down
 *
 * The regions, demand paging and copy-on-write of the user half are in
 * mm/vm.c (vm.h); this file only sets up the shared kernel half.
 *   PML4[256..383]  UVM_DIRECT_BASE      the same PDPTs as PML4[0..127]: a
 *                                        second view of the kernel half, which
 *                                        the entry code runs on (below)
 *   PML4[511]       UVM_LOW_ALIAS        the low window's tables, in the
 *                                        kernel's view of a low address space
 *
 * Programs that are not position independent (ET_EXEC) are linked at low
 * addresses, usually 0x400000, where the kernel's identity map is. Their
 * address space gets a low window, [UVM_LOW_START, UVM_LOW_END) =
 * PML4[0], and two roots: the user's, whose PML4[0] is the program's own,
 * and the kernel's, whose PML4[0] is the identity map as everywhere else
 * and whose PML4[511] holds the program's PML4[0] table. Ring 3 runs on the
 * user's root and the kernel on its own. The interrupt and system-call entry
 * code starts on the UVM_DIRECT_BASE view (IDT, GDT, TSS, percpu block and
 * stacks are all reached through it), which both roots have, switches CR3
 * (percpu kern_cr3/user_cr3) and then continues at the kernel's usual
 * addresses; the way out does the reverse. uaccess.c moves user pointers in
 * the window to UVM_LOW_ALIAS. Every other address space has no window and
 * one root, and its entries and exits only pass through the second view.
 *
 * Kernel code reaches user memory only through uaccess.h. */
#ifndef KESTREL_UVM_H
#define KESTREL_UVM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define UVM_USER_START      0x0000400000000000ul
/* One page short of the canonical boundary, as on Linux (TASK_SIZE): if user
 * code could sit in the last page, a `syscall` there would make SYSRET
 * return to a non-canonical RIP, which Intel CPUs fault on in ring 0 with
 * the user's stack (CVE-2012-0217). */
#define UVM_USER_END        0x00007ffffffff000ul
#define UVM_IMAGE_BASE      0x0000400000000000ull
#define UVM_MMAP_BASE       0x0000600000000000ull
#define UVM_STACK_TOP       0x00007fffff000000ull
#define UVM_STACK_SIZE      (8ull << 20)    /* main-thread stack (RLIMIT_STACK), demand paged */
#define UVM_INTERP_BASE     0x0000500000000000ull   /* the ELF interpreter (ld.so) */
#define UVM_PAGE            4096ull
#define UVM_LOW_START       0x0000000000010000ull   /* low window (ET_EXEC programs): vm.mmap_min_addr */
#define UVM_LOW_END         0x0000008000000000ull   /* the end of PML4[0] */
#define UVM_LOW_ALIAS       0xFFFFFF8000000000ull   /* PML4[511]: the window, in the kernel's root */
#define UVM_DIRECT_BASE     0xFFFF800000000000ull   /* PML4[256]: the kernel half, again */
#define UVM_LOW_SLOT        511

/* An identity-mapped kernel address in the second view, and back. */
static inline uint64_t uvm_direct(uint64_t a) { return a + UVM_DIRECT_BASE; }

void     uvm_init(void);                    /* after pmm_init, before any user process */
uint64_t uvm_kernel_pml4(void);
uint64_t uvm_create(void);                  /* new PML4 (phys) sharing the kernel half; 0 on OOM */
/* A user root without the identity map's PML4[0] (the low window). */
uint64_t uvm_create_low(void);
/* [va, va+len) inside the current process's user space: the user half, or
 * the low window if it has one. */
bool     uvm_range_ok(uint64_t va, uint64_t len);
/* The same for the address space mm (vm.h). */
struct mm;
bool     uvm_range_ok_mm(const struct mm *mm, uint64_t va, uint64_t len);
/* The end of the user region va is in (0 if none): ranges never span two. */
uint64_t uvm_region_end(uint64_t va);

#endif
