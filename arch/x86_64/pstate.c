/* arch/x86_64/pstate.c -- the processor's clock: turbo, and how fast it ran
 *
 * Firmware hands over an Intel processor at a fixed clock, usually its base
 * frequency, and leaves the choice to the operating system. Since Skylake
 * the processor picks its own clock once the system turns on hardware
 * P-states (HWP, CPUID.06H:EAX[7]): IA32_PM_ENABLE enables them, and each
 * CPU's IA32_HWP_REQUEST gives the range to pick from and how much to
 * favour speed over power (the energy-performance preference). Kestrel
 * asks for the whole range, turbo included, with the preference Linux's
 * intel_pstate uses by default ("balance_performance", 0x80): the clock
 * rises under load and falls back when idle. kestrel.pstate=0 leaves the
 * firmware's clock alone. Processors without HWP, and AMD's, which boost
 * on their own, are left as they are.
 *
 * APERF and MPERF (CPUID.06H:ECX[0]) count, while the CPU is not halted,
 * at its actual clock and at the TSC's rate. Once a second each CPU turns
 * their change into the average clock it ran at while busy; /proc/cpuinfo
 * shows it as "cpu MHz".
 *
 * Under a hypervisor neither is touched: the MSRs may not be emulated.
 *
 * Kestrel's own code, from the Intel SDM (vol. 3B, 15.4): MIT licence. */
#include <kernel/smp.h>
#include <kernel/arch.h>
#include <kernel/cpu.h>
#include <kernel/time.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/bootinfo.h>
#include <kernel/pstate.h>

#define MSR_MPERF               0x0E7
#define MSR_APERF               0x0E8
#define MSR_PM_ENABLE           0x770
#define MSR_HWP_CAPABILITIES    0x771
#define MSR_HWP_REQUEST         0x774

#define HWP_EPP_BALANCE_PERFORMANCE 0x80

static bool use_hwp, use_aperf;
static uint64_t hwp_request;

/* The boot CPU decides, and asks for, what every CPU will do. */
static void pstate_probe(void)
{
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    if (c & (1u << 31)) {
        kprintf("cpu: clock left to the hypervisor\n");
        return;
    }
    cpuid(0, 0, &a, &b, &c, &d);
    if (a < 6) return;
    cpuid(6, 0, &a, &b, &c, &d);
    use_aperf = c & 1;
    if (!(a & (1u << 7))) {
        if (!strcmp(g_boot.cpu_vendor, "GenuineIntel"))
            kprintf("cpu: no hardware P-states: the clock stays where the firmware set it\n");
        return;
    }
    if (strstr(g_boot.cmdline, "kestrel.pstate=0")) {
        kprintf("cpu: hardware P-states off (kestrel.pstate=0)\n");
        return;
    }
    bool epp = a & (1u << 10);
    wrmsr(MSR_PM_ENABLE, 1);
    uint64_t caps = rdmsr(MSR_HWP_CAPABILITIES);
    uint32_t highest = caps & 0xff, guaranteed = (caps >> 8) & 0xff, lowest = (caps >> 24) & 0xff;
    hwp_request = lowest | (uint64_t)highest << 8 |                 /* min, max; desired 0: the CPU picks */
                  (epp ? (uint64_t)HWP_EPP_BALANCE_PERFORMANCE << 24 : 0);
    use_hwp = true;
    kprintf("cpu: hardware P-states on: %u to %u MHz, %u guaranteed%s\n",
            lowest * 100, highest * 100, guaranteed * 100, epp ? ", balance_performance" : "");
}

void pstate_init_cpu(void)
{
    struct percpu *c = this_cpu();
    if (c->id == 0) pstate_probe();
    if (use_hwp) {
        wrmsr(MSR_PM_ENABLE, 1);                        /* per package; again on each is harmless */
        wrmsr(MSR_HWP_REQUEST, hwp_request);
    }
    if (use_aperf) {
        c->aperf = rdmsr(MSR_APERF);
        c->mperf = rdmsr(MSR_MPERF);
    }
}

/* every local timer tick, on each CPU */
void pstate_tick(void)
{
    struct percpu *c = this_cpu();
    if (!use_aperf || c->ticks % PIT_HZ) return;
    uint64_t ap = rdmsr(MSR_APERF), mp = rdmsr(MSR_MPERF);
    uint64_t dap = ap - c->aperf, dmp = mp - c->mperf;
    c->aperf = ap;
    c->mperf = mp;
    uint64_t khz = tsc_hz() / 1000;                     /* a second's APERF times this fits in 64 bits */
    if (dmp > 1000 && khz)                              /* halted all second: keep the last value */
        c->mhz = (uint32_t)(khz * dap / dmp / 1000);
}

/* the CPU's clock while it last ran, in MHz; 0 if unknown */
uint32_t pstate_mhz(int cpu)
{
    return use_aperf ? cpus[cpu].mhz : 0;
}
