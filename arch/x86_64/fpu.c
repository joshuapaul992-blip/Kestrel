/* arch/x86_64/fpu.c -- XSAVE for user FPU state.
 *
 * With FXSAVE alone (512 bytes) CPUID cannot report AVX to programs: they
 * check CR4.OSXSAVE and XCR0 before using the YMM registers. Where the CPU
 * has XSAVE, CR4.OSXSAVE is set and XCR0 enables x87, SSE, AVX and, when the
 * area still fits FPU_AREA_MAX, AVX-512. The area is the standard
 * (uncompacted) layout, the one Linux puts in signal frames. */
#include <kernel/fpu.h>
#include <kernel/cpu.h>
#include <kernel/klog.h>
#include <kernel/string.h>

size_t   fpu_size = 512;
uint64_t fpu_xfeatures;
uint32_t fpu_mxcsr_mask = 0xffbf;

static uint8_t clean[FPU_AREA_MAX] __attribute__((aligned(64)));

static inline void xsetbv(uint32_t reg, uint64_t v)
{
    __asm__ volatile("xsetbv" :: "c"(reg), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

void fpu_save(void *area)
{
    if (fpu_xfeatures)
        __asm__ volatile("xsave64 %0" : "=m"(*(uint8_t (*)[FPU_AREA_MAX])area) : "a"(-1), "d"(-1));
    else
        __asm__ volatile("fxsave64 %0" : "=m"(*(uint8_t (*)[512])area));
}

void fpu_restore(const void *area)
{
    if (fpu_xfeatures)
        __asm__ volatile("xrstor64 %0" :: "m"(*(const uint8_t (*)[FPU_AREA_MAX])area), "a"(-1), "d"(-1));
    else
        __asm__ volatile("fxrstor64 %0" :: "m"(*(const uint8_t (*)[512])area));
}

const void *fpu_clean(void) { return clean; }

void fpu_init(void)
{
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    bool xsave = c & (1u << 26), avx = c & (1u << 28);
    if (xsave) {
        uint32_t sa, sb, sc, sd;
        cpuid(0xD, 0, &sa, &sb, &sc, &sd);
        uint64_t supported = sa | (uint64_t)sd << 32;
        cpuid(7, 0, &a, &b, &c, &d);
        uint64_t want = XFEATURE_X87 | XFEATURE_SSE;
        if (avx && (supported & XFEATURE_YMM)) want |= XFEATURE_YMM;
        if ((want & XFEATURE_YMM) && (b & (1u << 16)) && (supported & XFEATURE_AVX512) == XFEATURE_AVX512)
            want |= XFEATURE_AVX512;
        __asm__ volatile("mov %0, %%cr4" :: "r"(read_cr4() | (1ull << 18)));   /* OSXSAVE */
        for (;;) {
            xsetbv(0, want);
            cpuid(0xD, 0, &sa, &sb, &sc, &sd);
            if (sb <= FPU_AREA_MAX || !(want & XFEATURE_AVX512)) break;
            want &= ~(uint64_t)XFEATURE_AVX512;
        }
        if (sb <= FPU_AREA_MAX) {
            fpu_xfeatures = want;
            fpu_size = sb;
        } else {                                        /* cannot happen without AVX-512 */
            xsetbv(0, XFEATURE_X87 | XFEATURE_SSE);
            __asm__ volatile("mov %0, %%cr4" :: "r"(read_cr4() & ~(1ull << 18)));
        }
    }

    /* MXCSR_MASK from a save of the reset state; 0 there means 0xffbf */
    uint8_t probe[512] __attribute__((aligned(16)));
    __asm__ volatile("fninit; fxsave64 %0" : "=m"(probe));
    uint32_t mask = *(uint32_t *)(probe + 28);
    if (mask) fpu_mxcsr_mask = mask;

    /* a new program: x87 control word 0x37f, MXCSR 0x1f80, and with XSAVE an
     * empty XSTATE_BV, so XRSTOR puts every other component in its reset state */
    memset(clean, 0, sizeof clean);
    *(uint16_t *)(clean + 0) = 0x037f;
    *(uint32_t *)(clean + 24) = 0x1f80;
    kprintf("fpu: %s, %lu-byte state, features %lx\n", fpu_xfeatures ? "XSAVE" : "FXSAVE",
            (unsigned long)fpu_size, fpu_xfeatures);
}

void fpu_init_ap(void)
{
    if (fpu_xfeatures) xsetbv(0, fpu_xfeatures);
}

/* ---- /proc/cpuinfo's flags line --------------------------------------------
 * Linux's names for the CPUID bits programs look for. As on Linux, the AVX
 * and AVX-512 families are listed only if XCR0 enables their registers. */
struct cpu_flag { uint8_t reg, bit; uint8_t needs; const char *name; };
enum { R1D, R1C, R7B, R7C, R7D, R71A, RXD, RXC, RDA, NREGS };
enum { ANY, YMM, ZMM, XS };
static const struct cpu_flag flags[] = {
    { R1D, 0, ANY, "fpu" }, { R1D, 1, ANY, "vme" }, { R1D, 2, ANY, "de" }, { R1D, 3, ANY, "pse" },
    { R1D, 4, ANY, "tsc" }, { R1D, 5, ANY, "msr" }, { R1D, 6, ANY, "pae" }, { R1D, 7, ANY, "mce" },
    { R1D, 8, ANY, "cx8" }, { R1D, 9, ANY, "apic" }, { R1D, 11, ANY, "sep" }, { R1D, 12, ANY, "mtrr" },
    { R1D, 13, ANY, "pge" }, { R1D, 14, ANY, "mca" }, { R1D, 15, ANY, "cmov" }, { R1D, 16, ANY, "pat" },
    { R1D, 17, ANY, "pse36" }, { R1D, 19, ANY, "clflush" }, { R1D, 23, ANY, "mmx" }, { R1D, 24, ANY, "fxsr" },
    { R1D, 25, ANY, "sse" }, { R1D, 26, ANY, "sse2" }, { R1D, 28, ANY, "ht" },
    { RXD, 11, ANY, "syscall" }, { RXD, 20, ANY, "nx" }, { RXD, 26, ANY, "pdpe1gb" }, { RXD, 27, ANY, "rdtscp" },
    { RXD, 29, ANY, "lm" },
    { R1C, 0, ANY, "pni" }, { R1C, 1, ANY, "pclmulqdq" }, { R1C, 3, ANY, "monitor" }, { R1C, 9, ANY, "ssse3" },
    { R1C, 12, YMM, "fma" }, { R1C, 13, ANY, "cx16" }, { R1C, 17, ANY, "pcid" }, { R1C, 19, ANY, "sse4_1" },
    { R1C, 20, ANY, "sse4_2" }, { R1C, 21, ANY, "x2apic" }, { R1C, 22, ANY, "movbe" }, { R1C, 23, ANY, "popcnt" },
    { R1C, 25, ANY, "aes" }, { R1C, 26, XS, "xsave" }, { R1C, 28, YMM, "avx" }, { R1C, 29, YMM, "f16c" },
    { R1C, 30, ANY, "rdrand" }, { R1C, 31, ANY, "hypervisor" },
    { RXC, 0, ANY, "lahf_lm" }, { RXC, 5, ANY, "abm" }, { RXC, 6, ANY, "sse4a" }, { RXC, 8, ANY, "3dnowprefetch" },
    { RDA, 0, XS, "xsaveopt" }, { RDA, 1, XS, "xsavec" }, { RDA, 2, XS, "xgetbv1" },
    { R7B, 0, ANY, "fsgsbase" }, { R7B, 3, ANY, "bmi1" }, { R7B, 4, ANY, "hle" }, { R7B, 5, YMM, "avx2" },
    { R7B, 7, ANY, "smep" }, { R7B, 8, ANY, "bmi2" }, { R7B, 9, ANY, "erms" }, { R7B, 10, ANY, "invpcid" },
    { R7B, 11, ANY, "rtm" }, { R7B, 16, ZMM, "avx512f" }, { R7B, 17, ZMM, "avx512dq" }, { R7B, 18, ANY, "rdseed" },
    { R7B, 19, ANY, "adx" }, { R7B, 20, ANY, "smap" }, { R7B, 21, ZMM, "avx512ifma" },
    { R7B, 23, ANY, "clflushopt" }, { R7B, 24, ANY, "clwb" }, { R7B, 28, ZMM, "avx512cd" },
    { R7B, 29, ANY, "sha_ni" }, { R7B, 30, ZMM, "avx512bw" }, { R7B, 31, ZMM, "avx512vl" },
    { R71A, 4, YMM, "avx_vnni" }, { R71A, 5, ZMM, "avx512_bf16" },
    { R7C, 1, ZMM, "avx512vbmi" }, { R7C, 2, ANY, "umip" }, { R7C, 6, ZMM, "avx512_vbmi2" },
    { R7C, 8, ANY, "gfni" }, { R7C, 9, YMM, "vaes" }, { R7C, 10, YMM, "vpclmulqdq" },
    { R7C, 11, ZMM, "avx512_vnni" }, { R7C, 12, ZMM, "avx512_bitalg" }, { R7C, 14, ZMM, "avx512_vpopcntdq" },
    { R7C, 22, ANY, "rdpid" },
    { R7D, 4, ANY, "fsrm" }, { R7D, 8, ZMM, "avx512_vp2intersect" }, { R7D, 14, ANY, "serialize" },
};

size_t cpu_flags(char *buf, size_t cap)
{
    uint32_t r[NREGS] = { 0 }, a, b, c, d, max, maxx;
    cpuid(0, 0, &max, &b, &c, &d);
    cpuid(1, 0, &a, &b, &r[R1C], &r[R1D]);
    if (max >= 7) {
        cpuid(7, 0, &a, &r[R7B], &r[R7C], &r[R7D]);
        if (a >= 1) cpuid(7, 1, &r[R71A], &b, &c, &d);
    }
    if (max >= 0xD) cpuid(0xD, 1, &r[RDA], &b, &c, &d);
    cpuid(0x80000000u, 0, &maxx, &b, &c, &d);
    if (maxx >= 0x80000001u) cpuid(0x80000001u, 0, &a, &b, &r[RXC], &r[RXD]);
    size_t n = 0;
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        const struct cpu_flag *f = &flags[i];
        if (!(r[f->reg] >> f->bit & 1)) continue;
        if (f->needs == XS && !fpu_xfeatures) continue;
        if (f->needs == YMM && !(fpu_xfeatures & XFEATURE_YMM)) continue;
        if (f->needs == ZMM && (fpu_xfeatures & XFEATURE_AVX512) != XFEATURE_AVX512) continue;
        size_t len = strlen(f->name);
        if (n + len + 2 > cap) break;
        if (n) buf[n++] = ' ';
        memcpy(buf + n, f->name, len);
        n += len;
    }
    if (n < cap) buf[n] = 0;
    return n;
}
