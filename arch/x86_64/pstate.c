/* arch/x86_64/pstate.c -- the processor's clock: turbo, and how fast it ran
 *
 * Firmware hands over an Intel processor at a fixed clock, usually its base
 * frequency, sometimes lower, and leaves the choice to the operating
 * system. Since Skylake the processor picks its own clock once the system
 * turns on hardware P-states (HWP, CPUID.06H:EAX[7]): IA32_PM_ENABLE
 * enables them, and each CPU's IA32_HWP_REQUEST gives the range to pick
 * from and how much to favour speed over power (the energy-performance
 * preference, EPP). Kestrel asks, as Linux's "performance" policy does,
 * for no less than the guaranteed (base) clock and up to the highest
 * turbo, with EPP 0: a busy CPU runs as fast as the power and heat limits
 * allow. Linux's default preference ("balance_performance", 0x80, from the
 * lowest clock up) kept an i7-8700 near 2.3 GHz in a game that waits on
 * the GPU many times a frame; kestrel.pstate=balance asks for that one.
 *
 * Without HWP but with Enhanced SpeedStep (CPUID.01H:ECX[7]) the system
 * sets the clock itself through IA32_PERF_CTL: Kestrel asks for the
 * single-core turbo ratio (MSR_TURBO_RATIO_LIMIT), when CPUID says turbo
 * is on (firmware that turns it off hides it there), and idle CPUs still
 * halt. kestrel.pstate=0 leaves the firmware's clock alone. AMD's
 * processors, which boost on their own, are left as they are.
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
#define MSR_PLATFORM_INFO       0x0CE
#define MSR_PERF_CTL            0x199
#define MSR_MISC_ENABLE         0x1A0
#define MSR_TURBO_RATIO_LIMIT   0x1AD
#define MSR_PM_ENABLE           0x770
#define MSR_HWP_CAPABILITIES    0x771
#define MSR_HWP_REQUEST         0x774

#define HWP_EPP_PERFORMANCE         0x00
#define HWP_EPP_BALANCE_PERFORMANCE 0x80
#define MISC_ENABLE_EIST            (1ull << 16)

static bool use_hwp, use_perf_ctl, use_aperf;
static uint64_t hwp_request, perf_ctl;
static char desc[96] = "left to the firmware";

/* The boot CPU decides, and asks for, what every CPU will do. */
static void pstate_probe(void)
{
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    if (c & (1u << 31)) {
        snprintf(desc, sizeof desc, "left to the hypervisor");
        kprintf("cpu: clock left to the hypervisor\n");
        return;
    }
    cpuid(0, 0, &a, &b, &c, &d);
    if (a < 6) return;
    cpuid(6, 0, &a, &b, &c, &d);
    use_aperf = c & 1;
    if (strcmp(g_boot.cpu_vendor, "GenuineIntel")) return;
    if (strstr(g_boot.cmdline, "kestrel.pstate=0")) {
        kprintf("cpu: clock left to the firmware (kestrel.pstate=0)\n");
        return;
    }
    bool balance = strstr(g_boot.cmdline, "kestrel.pstate=balance");
    bool turbo = a & (1u << 1);
    if (a & (1u << 7)) {
        bool epp = a & (1u << 10);
        wrmsr(MSR_PM_ENABLE, 1);
        uint64_t caps = rdmsr(MSR_HWP_CAPABILITIES);
        uint32_t highest = caps & 0xff, guaranteed = (caps >> 8) & 0xff, lowest = (caps >> 24) & 0xff;
        uint32_t min = balance || !guaranteed ? lowest : guaranteed;
        uint64_t pref = balance ? HWP_EPP_BALANCE_PERFORMANCE : HWP_EPP_PERFORMANCE;
        hwp_request = min | (uint64_t)highest << 8 |                    /* min, max; desired 0: the CPU picks */
                      (epp ? pref << 24 : 0);
        use_hwp = true;
        snprintf(desc, sizeof desc, "hardware P-states, %u to %u MHz (%u guaranteed), %s",
                 min * 100, highest * 100, guaranteed * 100, balance ? "balance_performance" : "performance");
    } else {
        cpuid(1, 0, &a, &b, &c, &d);
        uint32_t family = ((a >> 8) & 0xf) + ((a >> 20) & 0xff);
        /* turbo (CPUID.06H:EAX[1]) marks Nehalem and later, which have both MSRs */
        if (!(c & (1u << 7)) || family != 6 || !turbo) {
            kprintf("cpu: no SpeedStep with turbo: the clock stays where the firmware set it\n");
            return;
        }
        if (balance) {
            kprintf("cpu: no hardware P-states; clock left to the firmware (kestrel.pstate=balance)\n");
            return;
        }
        uint64_t misc = rdmsr(MSR_MISC_ENABLE);
        if (!(misc & MISC_ENABLE_EIST)) {
            kprintf("cpu: SpeedStep turned off by the firmware: the clock stays where it set it\n");
            return;
        }
        uint32_t ratio = (rdmsr(MSR_PLATFORM_INFO) >> 8) & 0xff;   /* highest non-turbo */
        uint32_t t = rdmsr(MSR_TURBO_RATIO_LIMIT) & 0xff;           /* one core busy */
        if (t > ratio) ratio = t;
        if (!ratio) return;
        perf_ctl = (uint64_t)ratio << 8;
        use_perf_ctl = true;
        snprintf(desc, sizeof desc, "SpeedStep, %u MHz (turbo) requested", ratio * 100);
    }
    kprintf("cpu: clock: %s\n", desc);
}

void pstate_init_cpu(void)
{
    struct percpu *c = this_cpu();
    if (c->id == 0) pstate_probe();
    if (use_hwp) {
        wrmsr(MSR_PM_ENABLE, 1);                        /* per package; again on each is harmless */
        wrmsr(MSR_HWP_REQUEST, hwp_request);
    }
    if (use_perf_ctl) wrmsr(MSR_PERF_CTL, perf_ctl);
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

/* how the clock is set, for REPORT.TXT */
const char *pstate_desc(void) { return desc; }

/* the CPU's clock while it last ran, in MHz; 0 if unknown */
uint32_t pstate_mhz(int cpu)
{
    return use_aperf ? cpus[cpu].mhz : 0;
}
