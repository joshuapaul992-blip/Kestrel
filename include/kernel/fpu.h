/* include/kernel/fpu.h -- user x87, SSE and AVX state: XSAVE where the CPU
 * has it, FXSAVE otherwise. The kernel itself is built without SSE, so only
 * user tasks own this state. */
#ifndef KESTREL_FPU_H
#define KESTREL_FPU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FPU_AREA_MAX    3072            /* per task: x87, SSE, AVX and AVX-512 */

#define XFEATURE_X87        0x001
#define XFEATURE_SSE        0x002
#define XFEATURE_YMM        0x004
#define XFEATURE_AVX512     0x0E0       /* opmask, ZMM_Hi256, Hi16_ZMM */

extern size_t   fpu_size;               /* bytes saved: 512 without XSAVE */
extern uint64_t fpu_xfeatures;          /* XCR0, 0 without XSAVE */
extern uint32_t fpu_mxcsr_mask;         /* MXCSR bits user code may set */

void fpu_init(void);                    /* boot CPU, after CR4.OSFXSR is on */
void fpu_init_ap(void);                 /* each other CPU, after the boot CPU's CR4 */
void fpu_save(void *area);              /* 64-byte aligned, fpu_size bytes */
void fpu_restore(const void *area);
const void *fpu_clean(void);            /* a new program's state, fpu_size bytes */
size_t cpu_flags(char *buf, size_t cap); /* /proc/cpuinfo's flags, NUL-terminated */

#endif
