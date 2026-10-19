/* kernel/vbe_emu.c -- VBE mode switches through the video BIOS, run in the
 * x86emu real-mode emulator (third_party/x86emu, as X.org's int10 does).
 *
 * Calling the BIOS for real means leaving long mode for real mode and
 * coming back, with nothing of Kestrel's (IDT, interrupt routing, SMM's
 * idea of the machine) valid in between: on some PCs that resets the
 * machine. Emulated, the CPU never changes mode. The BIOS code still runs
 * against the real hardware:
 *
 *   - memory: the low megabyte and everything below 4 GiB is the boot
 *     identity map, read and written directly (the option ROM at C0000,
 *     the VGA window, the EBDA, and the frame buffer itself). Page 0, the
 *     interrupt vectors and BIOS data area, is the kernel's NULL guard page
 *     by then, so it is a copy taken at boot; its timer tick count at 46C
 *     is worked out from the clock, as the BIOS timer interrupt would keep
 *     it (some BIOSes wait on it).
 *   - port I/O: passed straight through.
 *   - interrupts the BIOS calls: INT 15h (wait) and INT 1Ah (tick count,
 *     PCI BIOS) are answered here, since the system BIOS behind them may
 *     rely on real mode or on interrupts arriving; the rest (INT 10h
 *     itself, INT 6Dh, INT 42h, ...) run through the BIOS's vectors.
 *
 * A run that has not finished after a few seconds is given up. */
#include <kernel/vbe_emu.h>
#include <stdarg.h>
#include <kernel/bootinfo.h>
#include <kernel/cpu.h>
#include <kernel/time.h>
#include <kernel/string.h>
#include <kernel/serial.h>
#include <kernel/klog.h>
#include "pci.h"
#include "x86emu.h"

#define LOW_SIZE    0x1000              /* page 0: the copy */
#define BDA_TICKS   0x46C
#define RET_ADDR    0x5F0               /* a HLT: where the INT 10h call returns to */
#define STACK_TOP   0x7000
#define INFO_BUF    0x6000              /* VbeInfoBlock, 512 bytes */
#define MODE_BUF    0x6400              /* ModeInfoBlock, 256 bytes */
#define TIMEOUT_MS  5000

/* to the serial port only: a panic may hold the log's lock */
#define note(...) do { char nb_[128]; snprintf(nb_, sizeof nb_, __VA_ARGS__); serial_write(nb_); } while (0)

static uint8_t  low[LOW_SIZE];
static bool     have_low;
static uint32_t ticks0;                 /* BIOS ticks at the copy */
static uint64_t ms0;                    /* and the clock then */
static uint64_t deadline;
static bool     timed_out;

void vbe_emu_save_low(void)
{
    if (g_boot.uefi) return;            /* no BIOS vectors on a UEFI boot */
    const volatile uint8_t *page0;
    __asm__("" : "=r"(page0) : "0"((uintptr_t)0));    /* not a NULL dereference to the compiler */
    for (int i = 0; i < LOW_SIZE; i++) low[i] = page0[i];
    memcpy(&ticks0, &low[BDA_TICKS], 4);
    ms0 = time_ms();
    have_low = true;
}

/* x86emu's own messages (its debug and "unhandled opcode" reports) */
void printk(const char *fmt, ...)
{
    char b[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    serial_write(b);
}

/* ---- memory ---------------------------------------------------------------- */
static u8 m_rdb(u32 a)
{
    if (a >= LOW_SIZE) return *(volatile u8 *)(uintptr_t)a;
    if (a >= BDA_TICKS && a < BDA_TICKS + 4) {
        /* 1193182 / 65536 ticks a second, counted from the copy */
        uint32_t t = ticks0 + (uint32_t)((time_ms() - ms0) * 1193182 / 65536000);
        memcpy(&low[BDA_TICKS], &t, 4);
    }
    return low[a];
}

static void m_wrb(u32 a, u8 v)
{
    if (a >= LOW_SIZE) *(volatile u8 *)(uintptr_t)a = v;
    else low[a] = v;
}

/* wider accesses: directly when clear of page 0, else byte by byte */
static u16 m_rdw(u32 a)
{
    if (a >= LOW_SIZE) return *(volatile u16 *)(uintptr_t)a;
    return (u16)(m_rdb(a) | m_rdb(a + 1) << 8);
}

static u32 m_rdl(u32 a)
{
    if (a >= LOW_SIZE) return *(volatile u32 *)(uintptr_t)a;
    return (u32)m_rdw(a) | (u32)m_rdw(a + 2) << 16;
}

static void m_wrw(u32 a, u16 v)
{
    if (a >= LOW_SIZE) { *(volatile u16 *)(uintptr_t)a = v; return; }
    m_wrb(a, (u8)v); m_wrb(a + 1, (u8)(v >> 8));
}

static void m_wrl(u32 a, u32 v)
{
    if (a >= LOW_SIZE) { *(volatile u32 *)(uintptr_t)a = v; return; }
    m_wrw(a, (u16)v); m_wrw(a + 2, (u16)(v >> 16));
}

/* ---- port I/O ---------------------------------------------------------------- */
static u8  p_inb(X86EMU_pioAddr p) { return inb(p); }
static u32 p_inl(X86EMU_pioAddr p) { return inl(p); }
static void p_outb(X86EMU_pioAddr p, u8 v) { outb(p, v); }
static void p_outl(X86EMU_pioAddr p, u32 v) { outl(p, v); }
static u16 p_inw(X86EMU_pioAddr p)
{
    u16 v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}
static void p_outw(X86EMU_pioAddr p, u16 v) { __asm__ volatile("outw %0, %1" :: "a"(v), "Nd"(p)); }

/* ---- interrupts ---------------------------------------------------------------- */
static void set_cf(bool on)
{
    if (on) M.x86.R_FLG |= F_CF;
    else M.x86.R_FLG &= ~F_CF;
}

/* through the vector in the copied table, as the CPU would */
static void chain(int n)
{
    u16 off = m_rdw((u32)n * 4), seg = m_rdw((u32)n * 4 + 2);
    if (!off && !seg) return;           /* nobody there: return at once */
    M.x86.R_SP -= 2; m_wrw(((u32)M.x86.R_SS << 4) + M.x86.R_SP, (u16)M.x86.R_FLG);
    M.x86.R_SP -= 2; m_wrw(((u32)M.x86.R_SS << 4) + M.x86.R_SP, M.x86.R_CS);
    M.x86.R_SP -= 2; m_wrw(((u32)M.x86.R_SS << 4) + M.x86.R_SP, M.x86.R_IP);
    M.x86.R_FLG &= ~(F_IF | F_TF);
    M.x86.R_CS = seg;
    M.x86.R_IP = off;
}

/* PCI BIOS (INT 1Ah AH=B1h), configuration mechanism #1 */
static void pci_bios(void)
{
    int bus = M.x86.R_BH, devfn = M.x86.R_BL, reg = M.x86.R_DI & 0xFF;
    uint8_t status = 0;                 /* SUCCESSFUL */
    switch (M.x86.R_AL) {
    case 0x01: {                        /* installation check */
        int last = 0;
        for (size_t i = 0; i < pci_device_count(); i++)
            if (pci_device_at(i)->bus > last) last = pci_device_at(i)->bus;
        M.x86.R_EDX = 0x20494350;       /* "PCI " */
        M.x86.R_AL = 0x01;              /* mechanism #1 */
        M.x86.R_BX = 0x0210;
        M.x86.R_CL = (u8)last;
        M.x86.R_EDI = 0;
        break;
    }
    case 0x02:                          /* find device */
    case 0x03: {                        /* find class code */
        int want = M.x86.R_SI;
        status = 0x86;                  /* DEVICE_NOT_FOUND */
        for (size_t i = 0; i < pci_device_count(); i++) {
            struct pci_device *d = pci_device_at(i);
            bool hit = M.x86.R_AL == 0x02
                ? d->vendor_id == M.x86.R_DX && d->device_id == M.x86.R_CX
                : ((uint32_t)d->class_code << 16 | d->subclass << 8 | d->prog_if) == (M.x86.R_ECX & 0xFFFFFF);
            if (hit && want-- == 0) {
                M.x86.R_BH = d->bus;
                M.x86.R_BL = (u8)(d->device << 3 | d->function);
                status = 0;
                break;
            }
        }
        break;
    }
    case 0x08: M.x86.R_CL = (u8)pci_cfg_read(bus, devfn >> 3, devfn & 7, reg, 1); break;
    case 0x09: M.x86.R_CX = (u16)pci_cfg_read(bus, devfn >> 3, devfn & 7, reg, 2); break;
    case 0x0A: M.x86.R_ECX = pci_cfg_read(bus, devfn >> 3, devfn & 7, reg, 4); break;
    case 0x0B: pci_cfg_write(bus, devfn >> 3, devfn & 7, reg, 1, M.x86.R_CL); break;
    case 0x0C: pci_cfg_write(bus, devfn >> 3, devfn & 7, reg, 2, M.x86.R_CX); break;
    case 0x0D: pci_cfg_write(bus, devfn >> 3, devfn & 7, reg, 4, M.x86.R_ECX); break;
    default:   status = 0x81;           /* FUNC_NOT_SUPPORTED */
    }
    M.x86.R_AH = status;
    set_cf(status != 0);
}

static void intr(int n)
{
    if (n == 0x15) {
        if (M.x86.R_AH == 0x86) {       /* wait CX:DX microseconds */
            uint32_t us = (uint32_t)M.x86.R_CX << 16 | M.x86.R_DX;
            mdelay(us / 1000 > 1000 ? 1000 : us / 1000);
            udelay(us % 1000);
            M.x86.R_AH = 0;
            set_cf(false);
        } else {
            M.x86.R_AH = 0x86;          /* not supported */
            set_cf(true);
        }
        return;
    }
    if (n == 0x1A && M.x86.R_AH == 0x00) {   /* tick count */
        u32 t = m_rdl(BDA_TICKS);
        M.x86.R_CX = (u16)(t >> 16);
        M.x86.R_DX = (u16)t;
        M.x86.R_AL = 0;
        return;
    }
    if (n == 0x1A && M.x86.R_AH == 0xB1) { pci_bios(); return; }
    chain(n);
}

static int timeout(void)
{
    if (time_ms() < deadline) return 0;
    timed_out = true;
    return 1;
}

/* ---- calls ---------------------------------------------------------------------- */
static void setup(void)
{
    static X86EMU_memFuncs mem = { m_rdb, m_rdw, m_rdl, m_wrb, m_wrw, m_wrl };
    static X86EMU_pioFuncs pio = { p_inb, p_inw, p_inl, p_outb, p_outw, p_outl };
    static X86EMU_intrFuncs tab[256];
    memset(&M, 0, sizeof M);
    X86EMU_setupMemFuncs(&mem);
    X86EMU_setupPioFuncs(&pio);
    for (int i = 0; i < 256; i++) tab[i] = intr;
    X86EMU_setupIntrFuncs(tab);
    X86EMU_timeout = timeout;
    low[RET_ADDR] = 0xF4;               /* HLT */
}

/* INT 10h with AX, BX, CX, ES:DI; returns AX, or 0 if the run was cut short */
static u16 int10(u16 ax, u16 bx, u16 cx, u16 di)
{
    M.x86.R_EAX = ax; M.x86.R_EBX = bx; M.x86.R_ECX = cx; M.x86.R_EDX = 0;
    M.x86.R_ESI = 0;  M.x86.R_EDI = di; M.x86.R_EBP = 0;
    M.x86.R_DS = M.x86.R_ES = M.x86.R_FS = M.x86.R_GS = 0;
    M.x86.R_SS = 0; M.x86.R_ESP = STACK_TOP;
    M.x86.R_CS = 0; M.x86.R_EIP = RET_ADDR;
    M.x86.R_FLG = 0x0002 | F_IF;        /* as a real-mode caller has them */
    X86EMU_prepareForInt(0x10);
    X86EMU_exec();
    return timed_out ? 0 : M.x86.R_AX;
}

bool vbe_emu_set_mode(uint16_t w, uint16_t h, struct vbe_mode *out)
{
    if (!have_low) return false;
    u16 seg = m_rdw(0x10 * 4 + 2);
    if (seg < 0xC000) {                 /* INT 10h is not in a ROM: no video BIOS */
        note("vbe_emu: INT 10h at %04x:%04x, not a video BIOS\n", seg, m_rdw(0x10 * 4));
        return false;
    }
    setup();
    timed_out = false;
    deadline = time_ms() + TIMEOUT_MS;

    memcpy((void *)INFO_BUF, "VBE2", 4);
    u16 ax = int10(0x4F00, 0, 0, INFO_BUF);
    if (ax != 0x004F || memcmp((void *)INFO_BUF, "VESA", 4)) {
        note("vbe_emu: 4F00 failed (AX=%04x%s)\n", ax, timed_out ? ", timed out" : "");
        return false;
    }
    u32 list = m_rdl(INFO_BUF + 0x0E);
    u32 list_addr = (list >> 16 << 4) + (list & 0xFFFF);
    /* the list may be in the info block itself, which 4F01 calls must not
     * overwrite: copy it first */
    u16 modes[256];
    int n = 0;
    while (n < 256) {
        u16 m = m_rdw(list_addr + 2 * (u32)n);
        if (m == 0xFFFF) break;
        modes[n++] = m;
    }

    struct vbe_mode best = {0};
    for (int i = 0; i < n; i++) {
        if (int10(0x4F01, 0, modes[i], MODE_BUF) != 0x004F) {
            if (timed_out) break;
            continue;
        }
        const volatile u8 *mi = (const volatile u8 *)MODE_BUF;
        u16 attr = m_rdw(MODE_BUF), mw = m_rdw(MODE_BUF + 0x12), mh = m_rdw(MODE_BUF + 0x14);
        u8 bpp = mi[0x19], model = mi[0x1B];
        u32 phys = m_rdl(MODE_BUF + 0x28);
        if ((attr & 0x99) != 0x99 || mw != w || mh != h || model != 6 || !phys) continue;
        if (bpp != 32 && bpp != 24) continue;
        if (best.bpp == 32 || (best.bpp == 24 && bpp == 24)) continue;
        u16 lin_pitch = m_rdw(MODE_BUF + 0x32);
        best = (struct vbe_mode){
            .number = modes[i], .width = mw, .height = mh,
            .pitch = lin_pitch ? lin_pitch : m_rdw(MODE_BUF + 0x10), .bpp = bpp,
            .r_size = mi[0x1F], .r_pos = mi[0x20], .g_size = mi[0x21], .g_pos = mi[0x22],
            .b_size = mi[0x23], .b_pos = mi[0x24], .phys = phys,
        };
    }
    if (!best.bpp) {
        note("vbe_emu: no %ux%u mode with a linear frame buffer%s\n", w, h, timed_out ? " (timed out)" : "");
        return false;
    }

    /* linear frame buffer, contents not cleared: the caller draws all of it */
    ax = int10(0x4F02, (u16)(best.number | 0x4000 | 0x8000), 0, 0);
    if (ax != 0x004F) {
        note("vbe_emu: 4F02 mode %04x failed (AX=%04x%s)\n", best.number, ax, timed_out ? ", timed out" : "");
        return false;
    }
    note("vbe_emu: mode %04x, %ux%u %ubpp at %x\n", best.number, best.width, best.height, best.bpp, best.phys);
    *out = best;
    return true;
}
