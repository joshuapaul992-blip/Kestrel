/* kernel/display.c -- display head registry and boot-time probing
 *
 *   head 0     the boot frame buffer (VBE or GOP mode chosen by GRUB), drawn
 *              through its off-screen copy when double buffering is on
 *   head 1..   every secondary display adapter a driver can program; today
 *              QEMU/Bochs DISPI adapters (bochs-display, secondary-vga),
 *              each set to its monitor's EDID-preferred resolution
 *
 * A native GPU driver adds its monitors the same way, through
 * display_register(). With linuxkpi.drm=1 the DISPI adapters are left to
 * Linux's bochs driver instead: the Linux KPI registers their heads after
 * boot (linuxkpi/src/drm_kestrel.c), and tty_add_monitor() gives each one a
 * terminal.
 *
 * linuxkpi.drm=1 also lets a Linux driver take over the adapter behind the
 * boot frame buffer (nouveau on a primary NVIDIA card, bochs on QEMU's std
 * VGA). When the driver claims it, display_handover() stops every write to
 * the old frame buffer: the terminal and the panic screen keep drawing into
 * RAM. The driver's first monitor then takes monitor 0's place
 * (display_replace()), so tty1, the system console and /dev/fb0 move to it. */
#include <kernel/display.h>
#include <kernel/fb.h>
#include <kernel/mm.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/gpu.h>
#include <kernel/bootinfo.h>
#include <kernel/tty.h>
#include <kernel/posix.h>
#include "pci.h"
#include "edid.h"
#include "bochs_dispi.h"

#define DISPI_FALLBACK_W    1024        /* no EDID: a mode every monitor accepts */
#define DISPI_FALLBACK_H    768

static struct display_head heads[MAX_MONITORS];
static int nheads;
static int boot_head = -1;          /* the boot frame buffer's monitor, while it is one */
static int handed_over = -1;        /* a monitor a Linux driver took over, until it replaces it */
static const struct pci_device *handed_to;          /* ... the adapter            */
static struct display_head boot_saved;              /* ... and how it was, for display_reclaim() */
static struct framebuffer fb_saved;

static bool head_valid(const struct display_head *h)
{
    if ((!h->vram && !(h->write_span && h->shadow)) || h->width <= 0 || h->height <= 0 || (h->bytes_pp != 3 && h->bytes_pp != 4) ||
        h->pitch < h->width * h->bytes_pp) {
        kprintf("display: %s ignored, bad geometry %dx%d pitch %d\n", h->name, h->width, h->height, h->pitch);
        return false;
    }
    return true;
}

static void head_store(int index, const struct display_head *h)
{
    bool wc = h->vram && h->phys &&                 /* CPU-mapped VRAM: write-combining */
              vmm_set_write_combining(h->phys, (uint64_t)h->pitch * (uint64_t)h->height);
    heads[index] = *h;
    __atomic_thread_fence(__ATOMIC_RELEASE);        /* late heads (Linux DRM drivers) are read by other threads */
    kprintf("display: monitor %d: %dx%dx%d, pitch %d, VRAM %lx%s%s, %s%s%s%s\n", index, h->width, h->height,
            h->bytes_pp * 8, h->pitch, h->phys, wc ? " (write-combining)" : "", h->shadow ? " + RAM shadow" : "", h->name,
            h->monitor[0] ? " (\"" : "", h->monitor, h->monitor[0] ? "\")" : "");
}

int display_register(const struct display_head *h)
{
    if (nheads >= MAX_MONITORS) {
        kprintf("display: %s ignored, %d monitors already registered\n", h->name, MAX_MONITORS);
        return -1;
    }
    if (!head_valid(h)) return -1;
    head_store(nheads, h);
    return nheads++;
}

int display_count(void) { return nheads; }

const struct display_head *display_get(int index)
{
    return index >= 0 && index < nheads ? &heads[index] : NULL;
}

/* The head whose frame buffer lies in one of the device's memory BARs: that
 * adapter is driven here, so the Linux KPI does not offer it to its drivers. */
static bool in_mem_bar(const struct pci_device *dev, uint64_t phys)
{
    for (int b = 0; b < dev->bar_count && b < 6; b++) {
        const struct pci_bar *bar = &dev->bars[b];
        if ((bar->kind == PCI_BAR_MEM32 || bar->kind == PCI_BAR_MEM64) && bar->size &&
            phys >= bar->base && phys < bar->base + bar->size)
            return true;
    }
    return false;
}

/* linuxkpi.drm=1: secondary adapters, and the boot adapter, are left to
 * Linux DRM drivers */
static bool linux_drm_wanted(void)
{
    const char *o = strstr(g_boot.cmdline, "linuxkpi.drm");
    if (!o || (o != g_boot.cmdline && o[-1] != ' ')) return false;
    o += 12;
    return !*o || *o == ' ' || (*o == '=' && (o[1] == '1' || o[1] == 'y' || o[1] == 'Y'));
}

/* The firmware's frame buffer is not Kestrel driving the adapter: with
 * linuxkpi.drm=1 a Linux driver may take it over (display_handover()). */
const char *display_pci_owner(const struct pci_device *dev)
{
    for (int i = 0; i < nheads; i++) {
        if (!heads[i].phys || (i == boot_head && linux_drm_wanted())) continue;
        if (in_mem_bar(dev, heads[i].phys)) return heads[i].name;
    }
    return NULL;
}

/* A Linux driver claims `dev`. If the boot frame buffer lives in one of its
 * BARs, nothing may write there any more (the driver is about to reprogram
 * the card, and the old frame buffer's VRAM gets other uses): the terminal
 * and the panic screen draw into RAM until display_replace(). Returns the
 * monitor given up, or -1. */
int display_handover(const struct pci_device *dev)
{
    if (boot_head < 0 || !in_mem_bar(dev, heads[boot_head].phys)) return -1;
    int i = boot_head;
    uint64_t size = (uint64_t)g_fb.pitch * g_fb.height;
    void *ram = g_fb.double_buffered ? g_fb.back : (void *)(uintptr_t)pmm_alloc_contig((size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!ram) {
        kprintf("display: no memory to give up monitor %d\n", i);
        return -1;
    }
    if (!g_fb.double_buffered) memcpy(ram, g_fb.front, size);
    if (tty_detach_monitor(i, ram) < 0) {
        kprintf("display: the terminal on monitor %d is busy, not giving it up\n", i);
        if (!g_fb.double_buffered) pmm_free_contig((uint64_t)(uintptr_t)ram, (size + PAGE_SIZE - 1) / PAGE_SIZE);
        return -1;
    }
    boot_saved = heads[i];
    fb_saved = g_fb;
    g_fb.front = g_fb.back = ram;               /* the panic screen too */
    g_fb.double_buffered = false;
    heads[i].vram = NULL;
    heads[i].shadow = ram;
    heads[i].phys = 0;
    boot_head = -1;
    handed_over = i;
    handed_to = dev;
    kprintf("display: %02x:%02x.%x taken over by a Linux driver; monitor %d waits for its first monitor\n",
            dev->bus, dev->device, dev->function, i);
    return i;
}

bool display_handover_pending(void) { return __atomic_load_n(&handed_over, __ATOMIC_ACQUIRE) >= 0; }

/* The driver that took the boot adapter over failed to probe: draw on the
 * boot frame buffer again. It is the best there is, though the driver may
 * have reprogrammed the card before giving up. Returns the monitor, or -1. */
int display_reclaim(const struct pci_device *dev)
{
    int i = handed_over;
    if (i < 0 || dev != handed_to) return -1;
    void *ram = heads[i].shadow;
    handed_over = -1;
    boot_saved.shadow = ram;                    /* a RAM copy now, if it had none */
    head_store(i, &boot_saved);
    boot_head = i;
    if (tty_replace_monitor(i) < 0) {
        kprintf("display: monitor %d has no terminal any more\n", i);
        return i;
    }
    g_fb = fb_saved;
    g_fb.double_buffered = false;
    g_fb.back = ram;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    g_fb.double_buffered = true;
    kprintf("display: %02x:%02x.%x given back, monitor %d is the boot frame buffer again\n",
            dev->bus, dev->device, dev->function, i);
    return i;
}

/* The panic screen (g_fb, see fb.h) draws on monitor `h` from now on: its
 * CPU-mapped VRAM, through its RAM shadow. */
static void panic_screen_to(const struct display_head *h)
{
    g_fb.double_buffered = false;
    g_fb.phys = h->phys;
    g_fb.front = (uint8_t *)h->vram;
    g_fb.back = (uint8_t *)h->shadow;
    g_fb.width = (uint32_t)h->width; g_fb.height = (uint32_t)h->height; g_fb.pitch = (uint32_t)h->pitch;
    g_fb.bpp = 32; g_fb.bytes_pp = 4;
    g_fb.r_pos = h->r_pos; g_fb.g_pos = h->g_pos; g_fb.b_pos = h->b_pos;
    g_fb.r_size = g_fb.g_size = g_fb.b_size = 8;
    const char *sp = strchr(h->name, ' ');         /* "nouveau 0000:01:00.0" -> "nouveaudrmfb", as Linux */
    size_t n = sp ? (size_t)(sp - h->name) : strlen(h->name);
    if (n > sizeof g_fb.driver - 6) n = sizeof g_fb.driver - 6;
    memcpy(g_fb.driver, h->name, n);
    strlcpy(g_fb.driver + n, "drmfb", sizeof g_fb.driver - n);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    g_fb.double_buffered = true;
}

/* The first monitor a driver lights after display_handover() takes the
 * place of the monitor given up, with its terminal (tty_replace_monitor()).
 * When the new frame buffer is CPU-mapped VRAM with a RAM shadow, the panic
 * screen moves to it too and the old RAM copy is freed. Returns that monitor
 * number, or -1 when nothing is waiting to be replaced. */
int display_replace(const struct display_head *h)
{
    int i = handed_over;
    if (i < 0 || !head_valid(h)) return -1;
    handed_over = -1;
    uint8_t *old = (uint8_t *)heads[i].shadow;
    uint64_t old_frames = ((uint64_t)heads[i].pitch * (uint64_t)heads[i].height + PAGE_SIZE - 1) / PAGE_SIZE;
    head_store(i, h);
    if (tty_replace_monitor(i) < 0) {
        kprintf("display: monitor %d has no terminal any more\n", i);
        return i;                               /* the old copy may still be drawn into */
    }
    if (h->vram && h->shadow && h->bytes_pp == 4) {
        panic_screen_to(h);
        if (old != (uint8_t *)h->shadow) pmm_free_contig((uint64_t)(uintptr_t)old, old_frames);
    }
    return i;
}

/* ---- mode switching (KFB_SET_MODE) ----------------------------------------- */
static int switching = -1;              /* the monitor being switched */

int display_set_mode(int index, const struct display_mode *m)
{
    struct display_head *h = index >= 0 && index < nheads ? &heads[index] : NULL;
    if (!h) return -ENODEV;
    if (!h->set_mode || !h->nmodes) return -EOPNOTSUPP;
    const struct display_mode *want = NULL;
    for (int i = 0; i < h->nmodes && !want; i++)
        if (h->modes[i].width == m->width && h->modes[i].height == m->height &&
            (!m->refresh_mhz || h->modes[i].refresh_mhz == m->refresh_mhz))
            want = &h->modes[i];
    if (!want) return -EINVAL;
    if (want->width == h->width && want->height == h->height &&
        (!want->refresh_mhz || want->refresh_mhz == h->refresh_mhz))
        return 0;                                   /* already there */
    int idle = -1;
    if (!__atomic_compare_exchange_n(&switching, &idle, index, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return -EBUSY;
    struct display_mode copy = *want;
    kprintf("display: monitor %d: switching to %ux%u @ %u.%03u Hz\n", index, copy.width, copy.height,
            copy.refresh_mhz / 1000, copy.refresh_mhz % 1000);
    int rc = h->set_mode(h->mode_ctx, index, &copy);
    if (rc) kprintf("display: monitor %d: mode switch failed (%d)\n", index, rc);
    __atomic_store_n(&switching, -1, __ATOMIC_RELEASE);
    return rc;
}

int display_update(int index, const struct display_head *h)
{
    if (index < 0 || index >= nheads || !head_valid(h)) return -EINVAL;
    const struct display_head old = heads[index];
    bool panic_here = g_fb.ready && ((old.vram && g_fb.front == (uint8_t *)old.vram) ||
                                     (old.shadow && g_fb.back == (uint8_t *)old.shadow));
    head_store(index, h);
    if (tty_replace_monitor(index) < 0) {
        kprintf("display: monitor %d keeps its old terminal\n", index);
        heads[index] = old;
        return -ENOMEM;
    }
    if (panic_here) {
        if (h->vram && h->shadow && h->bytes_pp == 4) panic_screen_to(h);
        else {                                      /* nowhere to draw directly: RAM only */
            g_fb.double_buffered = false;
            g_fb.front = g_fb.back = (uint8_t *)h->shadow;
            g_fb.width = (uint32_t)h->width; g_fb.height = (uint32_t)h->height; g_fb.pitch = (uint32_t)h->pitch;
            __atomic_thread_fence(__ATOMIC_RELEASE);
        }
    }
    return 0;
}

/* ---- head 0: the boot frame buffer ---------------------------------------- */
static void probe_boot_fb(void)
{
    if (!g_fb.ready) return;
    if (gpu_replaced_boot_fb(g_fb.phys)) {          /* nvidia.modeset=2: not on screen any more */
        kprintf("display: boot frame buffer replaced by the NVIDIA modeset, no terminal on it\n");
        return;
    }
    if ((g_fb.bpp != 32 && g_fb.bpp != 24) || g_fb.r_size != 8 || g_fb.g_size != 8 || g_fb.b_size != 8) {
        kprintf("display: boot frame buffer is %u bpp (%u:%u:%u), no text terminal on it\n",
                g_fb.bpp, g_fb.r_size, g_fb.g_size, g_fb.b_size);
        return;
    }
    struct display_head h = {
        .phys   = g_fb.phys,
        .vram   = (uint32_t *)g_fb.front,
        .shadow = g_fb.double_buffered ? (uint32_t *)g_fb.back : NULL,
        .width  = (int)g_fb.width,
        .height = (int)g_fb.height,
        .pitch  = (int)g_fb.pitch,
        .bytes_pp = (int)g_fb.bytes_pp,
        .r_pos  = g_fb.r_pos, .g_pos = g_fb.g_pos, .b_pos = g_fb.b_pos,
    };
    strlcpy(h.name, g_fb.phys ? "boot frame buffer" : "frame buffer", sizeof h.name);
    int i = display_register(&h);
    if (i >= 0 && g_fb.phys) boot_head = i;
}

/* ---- secondary heads: Bochs/QEMU DISPI adapters --------------------------- */
/* The adapter takes any size; offer the usual ones that fit the monitor. */
static const uint16_t dispi_sizes[][2] = {
    { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 800 },
    { 1280, 1024 }, { 1366, 768 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 }, { 1680, 1050 },
    { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 }, { 2560, 1600 }, { 3840, 2160 },
};

struct dispi_adapter {
    volatile void *mmio;
    uint64_t lfb, lfb_size;
};
static struct dispi_adapter dispi_adapters[MAX_MONITORS];
static int ndispi;

static bool dispi_fits(const struct dispi_adapter *a, int w, int h, uint32_t vram)
{
    return w <= DISPI_MAX_XRES && h <= DISPI_MAX_YRES && (uint64_t)w * h * 4 <= a->lfb_size &&
           (!vram || (uint64_t)w * h * 4 <= vram);
}

static int dispi_switch(void *ctx, int index, const struct display_mode *m)
{
    struct dispi_adapter *a = ctx;
    const struct display_head *cur = display_get(index);
    struct dispi_mode mode;
    if (!cur) return -ENODEV;
    if (!dispi_set_mode(a->mmio, m->width, m->height, &mode) || (uint64_t)mode.pitch * mode.height > a->lfb_size) {
        dispi_set_mode(a->mmio, cur->width, cur->height, &mode);       /* back to what works */
        return -EINVAL;
    }
    struct display_head h = *cur;
    h.width = mode.width; h.height = mode.height; h.pitch = mode.pitch;
    h.refresh_mhz = m->refresh_mhz;
    size_t frames = ((size_t)mode.pitch * mode.height + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t old_frames = ((size_t)cur->pitch * cur->height + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t shadow = pmm_alloc_contig(frames);             /* zeroed, identity mapped */
    uint32_t *old = cur->shadow;
    h.shadow = (uint32_t *)(uintptr_t)shadow;
    int rc = display_update(index, &h);
    if (rc) {
        if (shadow) pmm_free_contig(shadow, frames);
        dispi_set_mode(a->mmio, cur->width, cur->height, &mode);
        return rc;
    }
    if (old) pmm_free_contig((uint64_t)(uintptr_t)old, old_frames);
    return 0;
}

static void probe_dispi(struct pci_device *dev)
{
    const struct pci_bar *lfb = &dev->bars[0], *mmio_bar = &dev->bars[2];
    if ((lfb->kind != PCI_BAR_MEM32 && lfb->kind != PCI_BAR_MEM64) || !lfb->base || !lfb->size) return;
    /* The adapter GRUB already drives is head 0. */
    if (g_fb.phys >= lfb->base && g_fb.phys < lfb->base + lfb->size) return;
    if (linux_drm_wanted()) {
        kprintf("display: %02x:%02x.%x left to Linux's bochs driver (linuxkpi.drm=1)\n",
                dev->bus, dev->device, dev->function);
        return;
    }
    if ((mmio_bar->kind != PCI_BAR_MEM32 && mmio_bar->kind != PCI_BAR_MEM64) || !mmio_bar->base) {
        kprintf("display: %02x:%02x.%x has no MMIO BAR2 (ISA-style VGA), skipped\n",
                dev->bus, dev->device, dev->function);
        return;
    }

    pci_enable(dev, false, true, false);
    if (!vmm_set_uncached(mmio_bar->base, 4096)) return;
    if (lfb->base + lfb->size > PMM_MAX_PHYS && !vmm_identity_map(lfb->base, lfb->size, VMM_WRITE)) return;
    volatile void *mmio = (volatile void *)(uintptr_t)mmio_bar->base;
    if (!dispi_present(mmio)) {
        kprintf("display: %02x:%02x.%x: no DISPI interface\n", dev->bus, dev->device, dev->function);
        return;
    }

    uint8_t raw[EDID_BLOCK_SIZE];
    struct edid_info edid;
    dispi_read_edid(mmio, raw);
    int w = DISPI_FALLBACK_W, h = DISPI_FALLBACK_H;
    if (edid_parse(raw, &edid)) {
        kprintf("display: %02x:%02x.%x: EDID %u.%u %s %04x \"%s\", native %dx%d @ %d Hz, %dx%d mm\n",
                dev->bus, dev->device, dev->function, edid.version, edid.revision, edid.vendor,
                edid.product, edid.name, edid.native_width, edid.native_height, edid.refresh_hz,
                edid.width_mm, edid.height_mm);
        w = edid.native_width;
        h = edid.native_height;
    } else {
        kprintf("display: %02x:%02x.%x: no valid EDID, using %dx%d\n",
                dev->bus, dev->device, dev->function, w, h);
    }

    struct dispi_mode mode;
    if (!dispi_set_mode(mmio, w, h, &mode) || (uint64_t)mode.pitch * (uint64_t)mode.height > lfb->size) {
        kprintf("display: %02x:%02x.%x: cannot set %dx%dx32\n", dev->bus, dev->device, dev->function, w, h);
        return;
    }

    struct display_head head = {
        .phys = lfb->base, .vram = (uint32_t *)(uintptr_t)lfb->base,
        .width = mode.width, .height = mode.height, .pitch = mode.pitch, .bytes_pp = 4,
        .r_pos = 16, .g_pos = 8, .b_pos = 0,                /* x8r8g8b8 */
    };
    size_t frames = ((size_t)mode.pitch * (size_t)mode.height + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t shadow = pmm_alloc_contig(frames);             /* zeroed, identity mapped */
    head.shadow = (uint32_t *)(uintptr_t)shadow;
    snprintf(head.name, sizeof head.name, "DISPI adapter %02x:%02x.%x", dev->bus, dev->device, dev->function);
    strlcpy(head.monitor, edid.name, sizeof head.monitor);
    head.refresh_mhz = edid.refresh_hz > 0 ? (uint32_t)edid.refresh_hz * 1000 : 60000;
    head.width_mm = edid.width_mm;
    head.height_mm = edid.height_mm;
    if (ndispi < MAX_MONITORS) {                            /* the modes xrandr may switch to */
        struct dispi_adapter *a = &dispi_adapters[ndispi];
        uint32_t vram = mode.vram_bytes;
        a->mmio = mmio;
        a->lfb = lfb->base;
        a->lfb_size = lfb->size;
        head.modes[head.nmodes++] = (struct display_mode){ (uint16_t)w, (uint16_t)h, head.refresh_mhz, true };
        for (size_t i = 0; i < sizeof dispi_sizes / sizeof dispi_sizes[0] && head.nmodes < DISPLAY_MAX_MODES; i++) {
            int mw = dispi_sizes[i][0], mh = dispi_sizes[i][1];
            if ((mw == w && mh == h) || mw > w || mh > h || !dispi_fits(a, mw, mh, vram)) continue;
            head.modes[head.nmodes++] = (struct display_mode){ (uint16_t)mw, (uint16_t)mh, 60000, false };
        }
        head.set_mode = dispi_switch;
        head.mode_ctx = a;
        ndispi++;
    }
    if (display_register(&head) < 0 && shadow) pmm_free_contig(shadow, frames);
}

void display_probe(void)
{
    gpu_probe();                    /* native GPU drivers: NVIDIA detection (stage 1) */
    probe_boot_fb();
    gpu_register_displays();        /* monitors lit by nvidia.modeset=1 */
    for (size_t i = 0; i < pci_device_count(); i++) {
        struct pci_device *dev = pci_device_at(i);
        if (dev->vendor_id == DISPI_PCI_VENDOR && dev->device_id == DISPI_PCI_DEVICE &&
            dev->class_code == PCI_CLASS_DISPLAY)
            probe_dispi(dev);
    }
    kprintf("display: %d monitor(s)\n", nheads);
}
