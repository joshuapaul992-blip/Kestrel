/* kernel/time.c -- HPET / calibrated TSC clocksource (see time.h) */
#include <kernel/time.h>
#include <kernel/cpu.h>
#include <kernel/mm.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/multiboot2.h>
#include <kernel/bootinfo.h>
#include <kernel/vdso.h>

enum source { SRC_NONE, SRC_HPET, SRC_TSC };
static enum source src;
static char src_name[64];

/* HPET */
static volatile uint8_t *hpet;
static uint64_t hpet_period_fs;          /* femtoseconds per tick */
static bool     hpet_wide;               /* 64-bit main counter   */
static uint64_t hpet_start, hpet_ext, hpet_last32;

/* TSC */
static uint64_t tsc_freq, tsc_start;
static bool     tsc_invariant;           /* runs at one rate in every power state */

/* wall clock: CMOS RTC read once at boot, advanced by the clocksource */
static int64_t boot_epoch;

#define HPET_GCAP_ID    0x000
#define HPET_GEN_CONF   0x010
#define HPET_COUNTER    0x0F0

/* ======================================================================== */
/*  ACPI table lookup (RSDP from Multiboot2, else the BIOS search area)        */
/* ======================================================================== */
struct rsdp {
    char     sig[8];
    uint8_t  checksum;
    char     oem[6];
    uint8_t  revision;
    uint32_t rsdt;
    uint32_t length;
    uint64_t xsdt;
    uint8_t  xchecksum;
    uint8_t  reserved[3];
} __attribute__((packed));

struct sdt_header {
    char     sig[4];
    uint32_t length;
    uint8_t  revision, checksum;
    char     oem[6], oem_table[8];
    uint32_t oem_rev, creator, creator_rev;
} __attribute__((packed));

struct acpi_hpet {
    struct sdt_header h;
    uint32_t block_id;
    uint8_t  space_id, bit_width, bit_offset, access_size;   /* generic address */
    uint64_t address;
    uint8_t  number;
    uint16_t min_tick;
    uint8_t  page_prot;
} __attribute__((packed));

static bool checksum_ok(const void *p, size_t n)
{
    const uint8_t *b = p; uint8_t s = 0;
    while (n--) s = (uint8_t)(s + *b++);
    return s == 0;
}

static const struct rsdp *find_rsdp(uintptr_t mbi)
{
    for (struct mb2_tag *t = mb2_first_tag(mbi); t->type != MB2_TAG_END; t = mb2_next_tag(t))
        if (t->type == MB2_TAG_ACPI_NEW || t->type == MB2_TAG_ACPI_OLD) {
            const struct rsdp *r = (const struct rsdp *)((uint8_t *)t + 8);
            if (!memcmp(r->sig, "RSD PTR ", 8) && checksum_ok(r, 20)) return r;
        }
    /* legacy BIOS: EBDA first KiB, then 0xE0000-0xFFFFF, 16-byte aligned */
    uintptr_t bda = 0x40E;                       /* BIOS data area: EBDA segment */
    __asm__("" : "+r"(bda));                     /* low memory is mapped: hide the constant from -Warray-bounds */
    uintptr_t ebda = (uintptr_t)(*(volatile uint16_t *)bda) << 4;
    uintptr_t ranges[2][2] = { { ebda, ebda + 1024 }, { 0xE0000, 0x100000 } };
    for (int i = 0; i < 2; i++)
        for (uintptr_t a = ranges[i][0]; a && a < ranges[i][1]; a += 16) {
            const struct rsdp *r = (const struct rsdp *)a;
            if (!memcmp(r->sig, "RSD PTR ", 8) && checksum_ok(r, 20)) return r;
        }
    return NULL;
}

static const struct sdt_header *map_table(uint64_t phys)
{
    if (!phys) return NULL;
    if (phys + 4096 > BOOT_MAP_END) vmm_identity_map(phys, 4096, VMM_WRITE);
    const struct sdt_header *h = (const struct sdt_header *)(uintptr_t)phys;
    if (h->length > 4096 && phys + h->length > BOOT_MAP_END) vmm_identity_map(phys, h->length, VMM_WRITE);
    return h;
}

/* The RSDT/XSDT, found once at boot (the BIOS search reads page 0, which
 * becomes a guard page afterwards). */
static const struct sdt_header *acpi_root;
static bool acpi_x;

static void acpi_init(uintptr_t mbi)
{
    const struct rsdp *r = find_rsdp(mbi);
    if (!r) { kprintf("acpi: no RSDP found\n"); return; }
    bool x = r->revision >= 2 && r->xsdt;
    kprintf("acpi: RSDP rev %u, %s at 0x%lx\n", r->revision, x ? "XSDT" : "RSDT", x ? r->xsdt : (uint64_t)r->rsdt);
    const struct sdt_header *root = map_table(x ? r->xsdt : r->rsdt);
    if (!root || root->length < sizeof *root || !checksum_ok(root, root->length)) {
        kprintf("acpi: root table invalid\n");
        return;
    }
    acpi_root = root;
    acpi_x = x;
}

const void *acpi_map_table(uint64_t phys)
{
    const struct sdt_header *h = map_table(phys);
    return h && h->length >= sizeof *h && checksum_ok(h, h->length) ? h : NULL;
}

const void *acpi_find_table(const char sig[4])
{
    if (!acpi_root) return NULL;
    size_t n = (acpi_root->length - sizeof *acpi_root) / (acpi_x ? 8 : 4);
    const uint8_t *entries = (const uint8_t *)acpi_root + sizeof *acpi_root;
    for (size_t i = 0; i < n; i++) {
        uint64_t p = acpi_x ? *(const uint64_t *)(entries + i * 8) : *(const uint32_t *)(entries + i * 4);
        const struct sdt_header *h = map_table(p);
        if (h && !memcmp(h->sig, sig, 4) && h->length >= sizeof *h && checksum_ok(h, h->length)) return h;
    }
    return NULL;
}

static const struct acpi_hpet *find_hpet(uintptr_t mbi)
{
    acpi_init(mbi);
    return acpi_find_table("HPET");
}

/* ======================================================================== */
/*  HPET                                                                       */
/* ======================================================================== */
static uint64_t hpet_read(void)
{
    if (hpet_wide) return *(volatile uint64_t *)(hpet + HPET_COUNTER);
    /* 32-bit counter: extend in software; callers read far more often than
     * the ~42 s wrap period of a 100 MHz counter. */
    uint64_t f = irq_save();
    uint32_t now = *(volatile uint32_t *)(hpet + HPET_COUNTER);
    if (now < hpet_last32) hpet_ext += 1ull << 32;
    hpet_last32 = now;
    uint64_t v = hpet_ext | now;
    irq_restore(f);
    return v;
}

static bool hpet_init(uintptr_t mbi)
{
    const struct acpi_hpet *t = find_hpet(mbi);
    if (!t) return false;
    if (t->space_id != 0 || !t->address) { kprintf("time: HPET not memory mapped\n"); return false; }
    vmm_set_uncached(t->address, 4096);
    hpet = (volatile uint8_t *)(uintptr_t)t->address;

    kprintf("time: HPET at 0x%lx, checking it\n", t->address);
    uint64_t cap = *(volatile uint64_t *)(hpet + HPET_GCAP_ID);
    hpet_period_fs = cap >> 32;
    hpet_wide = (cap >> 13) & 1;
    if (hpet_period_fs == 0 || hpet_period_fs > 100000000ull) {   /* spec: <= 100 ns */
        kprintf("time: HPET period %lu fs invalid\n", hpet_period_fs);
        hpet = NULL;
        return false;
    }
    volatile uint64_t *conf = (volatile uint64_t *)(hpet + HPET_GEN_CONF);
    if (!(*conf & 1)) *conf |= 1;                   /* ENABLE_CNF (legacy routing untouched) */

    /* make sure it actually counts */
    uint64_t a = hpet_read();
    for (int i = 0; i < 100000 && hpet_read() == a; i++) cpu_relax();
    if (hpet_read() == a) { kprintf("time: HPET counter is stuck\n"); hpet = NULL; return false; }

    hpet_start = hpet_read();
    uint64_t khz = 1000000000000ull / hpet_period_fs;
    snprintf(src_name, sizeof src_name, "HPET %lu.%03lu MHz at 0x%lx (%s counter)",
             khz / 1000, khz % 1000, t->address, hpet_wide ? "64-bit" : "32-bit");
    return true;
}

/* ======================================================================== */
/*  TSC rate: measured against the HPET, the ACPI PM timer or PIT channel 2,  */
/*  else read from CPUID                                                       */
/* ======================================================================== */
/* Many UEFI machines stop the PIT's clock when no legacy BIOS (CSM) is
 * loaded ("8254 clock gating"), and some have no HPET either: then OUT2
 * never rises, or reads as risen at once. A rate outside 100 MHz-20 GHz
 * is a timer that does not count, and the next one is tried. */
#define TSC_MIN_HZ 100000000ull
#define TSC_MAX_HZ 20000000000ull
static bool tsc_plausible(uint64_t hz) { return hz >= TSC_MIN_HZ && hz <= TSC_MAX_HZ; }

static uint64_t pit_measure_tsc(uint16_t ticks)
{
    uint8_t p61 = inb(0x61);
    outb(0x61, (uint8_t)((p61 & ~0x03) ));          /* gate low, speaker off */
    outb(0x43, 0xB0);                                /* ch2, lo/hi, mode 0, binary */
    outb(0x42, ticks & 0xFF);
    outb(0x42, ticks >> 8);
    outb(0x61, (uint8_t)((p61 & ~0x02) | 0x01));    /* gate high: start counting */
    uint64_t t0 = rdtsc();
    uint64_t spins = 0;
    /* 10 ms is about 10 000 port reads; 2 000 000 is a stopped PIT. */
    while (!(inb(0x61) & 0x20))                      /* OUT2 rises at terminal count */
        if (++spins > 2000000ull) { outb(0x61, p61); return 0; }
    uint64_t t1 = rdtsc();
    outb(0x61, p61);
    return t1 - t0;
}

static uint64_t tsc_hz_pit(void)
{
    const uint16_t ticks = 11932;                    /* 10 ms at 1.193182 MHz */
    uint64_t best = 0;
    for (int i = 0; i < 5; i++) {                    /* minimum: least disturbed by SMIs */
        uint64_t d = pit_measure_tsc(ticks);
        if (!d) return 0;                            /* stopped: no point trying again */
        if (!best || d < best) best = d;
    }
    return best * 1193182ull / ticks;
}

/* The ACPI PM timer: 3.579545 MHz, 24 or 32 bits, an I/O port in the FADT.
 * Every ACPI PC but the "hardware-reduced" ones has it. */
static uint16_t pmtmr_port;
static uint32_t pmtmr_mask;

static bool pmtmr_find(void)
{
    const uint8_t *f = acpi_find_table("FACP");
    if (!f) return false;
    uint32_t len = *(const uint32_t *)(f + 4);
    uint32_t flags = len >= 116 ? *(const uint32_t *)(f + 112) : 0;
    if (flags & (1u << 20)) return false;           /* HW_REDUCED_ACPI */
    uint64_t port = len >= 80 ? *(const uint32_t *)(f + 76) : 0;            /* PM_TMR_BLK */
    if (len >= 220 && f[208] == 1 && *(const uint64_t *)(f + 212))         /* X_PM_TMR_BLK, I/O */
        port = *(const uint64_t *)(f + 212);
    if (!port || port > 0xFFFC) return false;
    pmtmr_port = (uint16_t)port;
    pmtmr_mask = flags & (1u << 8) ? 0xFFFFFFFFu : 0xFFFFFFu;               /* TMR_VAL_EXT */
    return true;
}

static uint64_t tsc_hz_pmtmr(void)
{
    if (!pmtmr_find()) return 0;
    uint32_t a = inl(pmtmr_port) & pmtmr_mask;
    int i;
    for (i = 0; i < 100000 && (inl(pmtmr_port) & pmtmr_mask) == a; i++) cpu_relax();
    if (i == 100000) return 0;                       /* not counting */
    const uint32_t span = 178977;                    /* 50 ms */
    uint32_t p0 = inl(pmtmr_port) & pmtmr_mask, p1;
    uint64_t t0 = rdtsc();
    uint64_t spins = 0;
    do {
        p1 = inl(pmtmr_port) & pmtmr_mask;
        if (++spins > 20000000ull) return 0;
    } while (((p1 - p0) & pmtmr_mask) < span);
    uint64_t t1 = rdtsc();
    return (t1 - t0) * 3579545ull / ((p1 - p0) & pmtmr_mask);
}

/* CPUID 0x15 (TSC / crystal ratio, crystal rate) or 0x16 (base MHz):
 * what the CPU says, when nothing can measure it. */
static uint64_t tsc_hz_cpuid(void)
{
    uint32_t a, b, c, d, max;
    cpuid(0, 0, &max, &b, &c, &d);
    if (max >= 0x15) {
        cpuid(0x15, 0, &a, &b, &c, &d);
        if (a && b && c) return (uint64_t)c * b / a;
    }
    if (max >= 0x16) {
        cpuid(0x16, 0, &a, &b, &c, &d);
        if (a & 0xFFFF) return (uint64_t)(a & 0xFFFF) * 1000000ull;
    }
    return 0;
}

static void tsc_set(uint64_t hz, const char *how)
{
    tsc_freq = hz;
    tsc_start = rdtsc();
    snprintf(src_name, sizeof src_name, "TSC %lu.%03lu GHz (%s)",
             (unsigned long)(tsc_freq / 1000000000ull), (unsigned long)((tsc_freq / 1000000ull) % 1000), how);
}

/* A finer TSC rate against the HPET (50 ms, the HPET's own period). */
static uint64_t tsc_hz_hpet(void)
{
    uint64_t h0 = hpet_read(), t0 = rdtsc();
    uint64_t span = 50000000000000ull / hpet_period_fs;         /* 50 ms in HPET ticks */
    uint64_t spins = 0;
    while (hpet_read() - h0 < span)                             /* 50 ms is ~100 000 reads */
        if (++spins > 20000000ull) return 0;                    /* the counter stopped */
    uint64_t h1 = hpet_read(), t1 = rdtsc();
    uint64_t fs = (h1 - h0) * hpet_period_fs;                   /* femtoseconds elapsed */
    uint64_t ns = fs / 1000000ull;
    return ns ? (t1 - t0) * 1000000000ull / ns : 0;
}

/* The TSC rate from the best timer that works; false only if none does. */
static bool tsc_init(bool hpet_ok)
{
    static const char *const how[] = { "HPET-calibrated", "PM-timer-calibrated", "PIT-calibrated", "CPUID" };
    for (int i = 0; i < 4; i++) {
        if (i == 0 && !hpet_ok) continue;
        kprintf("time: TSC rate: trying %s\n", how[i]);
        uint64_t hz = i == 0 ? (hpet_ok ? tsc_hz_hpet() : 0) : i == 1 ? tsc_hz_pmtmr()
                    : i == 2 ? tsc_hz_pit() : tsc_hz_cpuid();
        if (tsc_plausible(hz)) { tsc_set(hz, how[i]); return true; }
        if (hz) kprintf("time: %s TSC rate %lu Hz is not believable, ignored\n", how[i], (unsigned long)hz);
    }
    return false;
}

/* ======================================================================== */
/*  CMOS real-time clock (MC146818 compatible)                                 */
/* ======================================================================== */
static uint8_t cmos(uint8_t reg) { outb(0x70, reg); return inb(0x71); }

/* days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant) */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? (unsigned)-3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void rtc_snapshot(uint8_t r[6])
{
    for (int spin = 0; spin < 100000 && (cmos(0x0A) & 0x80); spin++) cpu_relax();   /* update in progress */
    static const uint8_t regs[6] = { 0x00, 0x02, 0x04, 0x07, 0x08, 0x09 };
    for (int i = 0; i < 6; i++) r[i] = cmos(regs[i]);
}

static int64_t rtc_read_epoch(void)
{
    uint8_t a[6], b[6];
    rtc_snapshot(a);
    for (int tries = 0; tries < 5; tries++) {           /* two equal reads: no rollover mid-read */
        rtc_snapshot(b);
        if (!memcmp(a, b, 6)) break;
        memcpy(a, b, 6);
    }
    uint8_t status_b = cmos(0x0B);
    bool pm = !(status_b & 0x02) && (a[2] & 0x80);
    a[2] &= 0x7F;
    if (!(status_b & 0x04))                             /* BCD -> binary */
        for (int i = 0; i < 6; i++) a[i] = (uint8_t)((a[i] & 0x0F) + (a[i] >> 4) * 10);
    if (!(status_b & 0x02)) { if (a[2] == 12) a[2] = 0; if (pm) a[2] = (uint8_t)(a[2] + 12); }
    unsigned sec = a[0], min = a[1], hour = a[2], day = a[3], mon = a[4];
    int64_t year = 2000 + a[5];
    if (mon < 1 || mon > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 60) {
        kprintf("time: RTC returned an invalid date, wall clock starts at the epoch\n");
        return 0;
    }
    return days_from_civil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec;
}

int64_t time_realtime_sec(void) { return boot_epoch + (int64_t)(time_ns() / 1000000000ull); }

void time_realtime(int64_t *sec, int64_t *nsec)
{
    uint64_t ns = time_ns();
    *sec = boot_epoch + (int64_t)(ns / 1000000000ull);
    *nsec = (int64_t)(ns % 1000000000ull);
}

/* ======================================================================== */
/*  public API                                                                 */
/* ======================================================================== */
/* CPUID 0x80000007 EDX bit 8: the TSC ticks at a constant rate, also in
 * deep C-states (every Intel CPU since Nehalem, AMD since Barcelona). */
static bool cpu_tsc_invariant(void)
{
    uint32_t a, b, c, d;
    cpuid(0x80000000, 0, &a, &b, &c, &d);
    if (a < 0x80000007) return false;
    cpuid(0x80000007, 0, &a, &b, &c, &d);
    return d & (1u << 8);
}

void time_init(uintptr_t mbi)
{
    /* The TSC's rate, from the HPET if there is one: it backs udelay()
     * when the HPET is the clock too. */
    /* kestrel.hpet=0: leave the HPET alone (one that hangs or misreads) */
    bool hpet_ok;
    if (strstr(g_boot.cmdline, "kestrel.hpet=0")) {
        acpi_init(mbi);
        hpet_ok = false;
        kprintf("time: HPET not used (kestrel.hpet=0)\n");
    } else hpet_ok = hpet_init(mbi);
    char hpet_desc[64];
    strlcpy(hpet_desc, src_name, sizeof hpet_desc);
    if (!tsc_init(hpet_ok)) {
        /* No timer counts and the CPU does not say: a guess keeps the
         * timeouts finite, if wrong. */
        tsc_set(2000000000ull, "guessed, no timer counts");
        kprintf("time: WARNING no working HPET, PM timer or PIT: timing is guessed\n");
    }
    char tsc_desc[64];
    tsc_invariant = cpu_tsc_invariant();
    snprintf(tsc_desc, sizeof tsc_desc, "%s%s", src_name, tsc_invariant ? ", invariant" : "");

    /* The clock: an invariant TSC if there is one (cheap to read, and the
     * vDSO can read it in user mode), else the HPET, else a TSC as it is.
     * kestrel.clocksource=hpet or =tsc decides instead. */
    bool want_hpet = strstr(g_boot.cmdline, "kestrel.clocksource=hpet") != NULL;
    bool want_tsc = strstr(g_boot.cmdline, "kestrel.clocksource=tsc") != NULL;
    if (hpet_ok && (want_hpet || (!tsc_invariant && !want_tsc))) {
        src = SRC_HPET;
        strlcpy(src_name, hpet_desc, sizeof src_name);
    } else {
        tsc_start = rdtsc();
        src = SRC_TSC;
        strlcpy(src_name, tsc_desc, sizeof src_name);
    }

    kprintf("time: reading the RTC\n");
    boot_epoch = rtc_read_epoch();
    kprintf("time: RTC wall clock %ld (Unix seconds)\n", (long)boot_epoch);
    kprintf("time: clocksource %s%s%s\n", src_name, src == SRC_HPET ? "; " : "",
            src == SRC_HPET ? tsc_desc : "");
}

uint64_t time_ns(void)
{
    switch (src) {
    case SRC_HPET: {
        uint64_t ticks = hpet_read() - hpet_start;
        /* ticks * period_fs / 1e6, split to avoid 64-bit overflow */
        return (ticks / 1000000ull) * hpet_period_fs + (ticks % 1000000ull) * hpet_period_fs / 1000000ull;
    }
    case SRC_TSC: {
        uint64_t t = rdtsc() - tsc_start;
        return (t / tsc_freq) * 1000000000ull + (t % tsc_freq) * 1000000000ull / tsc_freq;
    }
    default:
        return 0;
    }
}

uint64_t time_us(void) { return time_ns() / 1000; }
uint64_t time_ms(void) { return time_ns() / 1000000; }

void udelay(uint32_t us)
{
    if (src == SRC_NONE) { while (us--) outb(0x80, 0); return; }
    uint64_t end = time_ns() + (uint64_t)us * 1000;
    while (time_ns() < end) cpu_relax();
}

void mdelay(uint32_t ms) { while (ms--) udelay(1000); }

const char *time_source(void) { return src_name; }
uint64_t tsc_hz(void) { return tsc_freq; }

bool time_tsc_clock(uint64_t *start, uint64_t *hz)
{
    *start = tsc_start;
    *hz = tsc_freq;
    return src == SRC_TSC;
}

int64_t time_boot_epoch(void) { return boot_epoch; }

/* The CPUs' TSCs are not in step (smp.c): go back to the HPET, carrying the
 * time on so the clock never jumps (boot time only). */
void time_tsc_unstable(void)
{
    if (src != SRC_TSC || !hpet) return;
    uint64_t f = irq_save();
    uint64_t ns = time_ns();
    hpet_start = hpet_read() - ns * 1000000ull / hpet_period_fs;
    src = SRC_HPET;
    uint64_t khz = 1000000000000ull / hpet_period_fs;
    snprintf(src_name, sizeof src_name, "HPET %lu.%03lu MHz (the TSCs differ between CPUs)", khz / 1000, khz % 1000);
    irq_restore(f);
    kprintf("time: the CPUs' TSCs are not in step: clocksource %s\n", src_name);
    vdso_update();
}
