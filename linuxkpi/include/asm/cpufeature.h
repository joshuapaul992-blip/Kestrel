/* asm/cpufeature.h -- Kestrel's Linux KPI: CPU features */
#ifndef _LKPI_ASM_CPUFEATURE_H
#define _LKPI_ASM_CPUFEATURE_H

#include <linux/types.h>
#include <linux/jump_label.h>

#define X86_FEATURE_CLFLUSH     (0 * 32 + 19)   /* CPUID 1, EDX bit 19 */
#define X86_FEATURE_PAT         (0 * 32 + 16)
#define X86_FEATURE_XMM4_1      (4 * 32 + 19)   /* CPUID 1, ECX bit 19: SSE4.1 */
#define X86_FEATURE_HYPERVISOR  (4 * 32 + 31)   /* CPUID 1, ECX bit 31 */
struct cpuinfo_x86 {
    u8 x86;                     /* family */
    u8 x86_vendor;
    u8 x86_model;
    u16 x86_clflush_size;
    u32 x86_capability[8];      /* [0] CPUID 1 EDX, [4] CPUID 1 ECX */
};
extern struct cpuinfo_x86 boot_cpu_data;
#define boot_cpu_has(bit)       (!!(boot_cpu_data.x86_capability[(bit) / 32] & (1u << ((bit) % 32))))
#define static_cpu_has(bit)     boot_cpu_has(bit)
#define cpu_feature_enabled(bit) boot_cpu_has(bit)

/* the FPU in kernel code: never entered, since boot_cpu_data reports no SSE4.1
 * and DRM then keeps to plain copies (linuxkpi/src/drm.c) */
static inline void kernel_fpu_begin(void) { }
static inline void kernel_fpu_end(void) { }
static inline void clflushopt(volatile void *p) { asm volatile("clflush %0" : "+m"(*(volatile char *)p)); }
static inline void clflush(volatile void *p) { asm volatile("clflush %0" : "+m"(*(volatile char *)p)); }

#endif
