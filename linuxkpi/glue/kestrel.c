/* linuxkpi/glue/kestrel.c -- the Linux KPI's view of Kestrel (lkpi/glue.h)
 *
 * Everything the Linux side needs from the kernel goes through here:
 * memory, the clock, threads and their sleep/wake protocol, PCI, MSI and
 * files. This file is ordinary Kestrel code (Kestrel headers, Kestrel
 * semantics); it is the one place where both worlds meet. */
#include <kernel/mm.h>
#include <kernel/time.h>
#include <kernel/task.h>
#include <kernel/smp.h>
#include <kernel/cpu.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/bootinfo.h>
#include <kernel/process.h>
#include <kernel/uaccess.h>
#include <kernel/power.h>
#include <kernel/msi.h>
#include <kernel/vfs.h>
#include <kernel/posix.h>
#include <kernel/display.h>
#include <kernel/tty.h>
#include <kernel/report.h>
#include "pci.h"
#include "../include/lkpi/glue.h"

/* ---- memory ---------------------------------------------------------------- */
void *lkpi_heap_alloc(unsigned long n) { return kmalloc(n); }
void  lkpi_heap_free(void *p) { kfree(p); }

unsigned long lkpi_frames_alloc(unsigned long n, int dma32)
{
    if (!n) return 0;
    if (dma32) return pmm_alloc_dma32(n);
    return n == 1 ? pmm_alloc() : pmm_alloc_contig(n);
}

unsigned long lkpi_frames_alloc_order(unsigned int order, int dma32)
{
    return pmm_alloc_order(order, dma32);
}

void lkpi_frames_free(unsigned long phys, unsigned long n)
{
    if (n == 1) pmm_free(phys);
    else pmm_free_contig(phys, n);
}

unsigned long lkpi_ram_total(void) { return pmm_total_bytes(); }
unsigned long lkpi_ram_free(void) { return pmm_free_bytes(); }

int lkpi_map_mmio(unsigned long phys, unsigned long size, int wc)
{
    if (wc && vmm_set_write_combining(phys, size)) return 0;
    return vmm_set_uncached(phys, size) ? 0 : -1;
}

void *lkpi_vmap(const unsigned long *frames, unsigned long n, unsigned long cache)
{
    _Static_assert(sizeof(unsigned long) == sizeof(uint64_t), "frame addresses");
    return vmm_vmap((const uint64_t *)frames, n, VMM_WRITE | (cache & (VMM_PWT | VMM_PCD)));
}
void lkpi_vunmap(const void *addr) { vmm_vunmap((void *)addr); }
unsigned long lkpi_vmap_phys(const void *addr) { return vmm_vmap_phys(addr); }

/* ---- time ------------------------------------------------------------------ */
unsigned long long lkpi_time_ns(void) { return time_ns(); }
unsigned long long lkpi_time_ms(void) { return time_ms(); }
void lkpi_udelay(unsigned int us) { udelay(us); }

long long lkpi_realtime_ns(void)
{
    int64_t s, ns;
    time_realtime(&s, &ns);
    return s * 1000000000ll + ns;
}

/* ---- CPUs and threads -------------------------------------------------------- */
int lkpi_cpu_id(void) { return this_cpu()->id; }
int lkpi_cpu_count(void) { return ncpus; }
void *lkpi_task_self(void) { return current_task(); }
int lkpi_task_slots(void) { return MAX_TASKS; }

int lkpi_task_slot(void *task)
{
    struct tcb *t = task, *first = task_slot(0);
    if (t < first || t > task_slot(MAX_TASKS - 1)) return -1;     /* an idle thread */
    return (int)(t - first);
}

int lkpi_task_pid(void *task) { return ((struct tcb *)task)->pid; }
const char *lkpi_task_name(void *task) { return ((struct tcb *)task)->name; }
int lkpi_task_is_user(void *task) { return ((struct tcb *)task)->user; }
void *lkpi_task_create(const char *name, int (*fn)(void *), void *arg) { return task_create(name, fn, arg); }
void lkpi_task_exit(int code) { task_exit(code); }
void lkpi_task_block(void *chan, unsigned long long deadline_ms)
{
    if (current_task()->pid != 0) task_block_self(chan, deadline_ms);     /* see lkpi_task_switch */
}
void lkpi_task_wake(void *task) { task_wake(task); }
void lkpi_task_yield(void) { sched_yield(); }
int lkpi_task_signal_pending(void) { return current_task()->user && signal_pending(); }

void lkpi_task_switch(void)
{
    struct tcb *self = current_task();
    if (self->pid == 0) {                       /* the boot thread cannot block: just wait a tick */
        self->wait_chan = NULL;
        sti(); hlt(); cli();
        return;
    }
    schedule();
    self->wait_chan = NULL;
}

/* The boot thread (pid 0) only runs when nothing else is ready, and a
 * thread that becomes ready does not preempt it: yield explicitly, and
 * halt until the next interrupt when there was nothing to yield to. */
void lkpi_boot_wait(volatile int *flag)
{
    while (!__atomic_load_n(flag, __ATOMIC_ACQUIRE)) {
        sched_yield();
        uint64_t f = irq_save();
        if (!__atomic_load_n(flag, __ATOMIC_ACQUIRE)) { sti(); hlt(); cli(); }
        irq_restore(f);
    }
}

/* ---- logging, boot parameters, hooks ------------------------------------------ */
void lkpi_log(const char *s) { kprintf("%s", s); }
const char *lkpi_cmdline(void) { return g_boot.cmdline; }

void lkpi_set_hooks(void (*panic_fn)(const char *msg), void (*power_fn)(int what))
{
    panic_set_hook(panic_fn);
    power_set_hook(power_fn);
}

void lkpi_die(const char *msg) { panic("%s", msg); }

void lkpi_power(int what)
{
    if (what == POWER_HOOK_REBOOT) power_reboot();
    if (what == POWER_HOOK_HALT) power_halt();
    power_off();
}

/* ---- user memory ---------------------------------------------------------- */
int lkpi_copy_from_user_glue(void *dst, const void *usrc, unsigned long n) { return copy_from_user(dst, usrc, n); }
int lkpi_copy_to_user_glue(void *udst, const void *src, unsigned long n) { return copy_to_user(udst, src, n); }
int lkpi_user_range_ok(unsigned long addr, unsigned long n) { return user_range_ok(addr, n); }

/* ---- PCI ----------------------------------------------------------------- */
int lkpi_pci_count(void) { return (int)pci_device_count(); }

int lkpi_pci_get(int index, struct lkpi_pci_info *out)
{
    struct pci_device *d = pci_device_at((size_t)index);
    if (!d) return -1;
    memset(out, 0, sizeof *out);
    out->handle = d;
    out->bus = d->bus; out->dev = d->device; out->fn = d->function;
    out->vendor = d->vendor_id; out->device = d->device_id;
    out->subvendor = d->subsystem_vendor_id; out->subdevice = d->subsystem_id;
    out->class_code = d->class_code; out->subclass = d->subclass;
    out->prog_if = d->prog_if; out->revision = d->revision;
    out->irq_line = d->interrupt_line; out->irq_pin = d->interrupt_pin;
    out->header_type = d->header_type;
    for (int i = 0; i < d->bar_count && i < 6; i++) {
        out->bar[i].base = d->bars[i].base;
        out->bar[i].size = d->bars[i].size;
        out->bar[i].kind = d->bars[i].kind;
        out->bar[i].prefetch = d->bars[i].prefetchable;
    }
    out->parent = d->parent;
    out->native = display_pci_owner(d);
    return 0;
}

/* ---- displays ------------------------------------------------------------- */
/* set_mode() of each monitor a Linux driver lit, behind the head's own */
static struct lkpi_mode_switch {
    int  (*fn)(void *ctx, int monitor, const struct lkpi_display_mode *mode);
    void  *ctx;
} mode_switches[MAX_MONITORS * 2];
static int nmode_switches;

static int lkpi_switch_mode(void *ctx, int index, const struct display_mode *m)
{
    struct lkpi_mode_switch *sw = ctx;
    struct lkpi_display_mode lm = { m->width, m->height, m->refresh_mhz, m->preferred };
    return sw->fn(sw->ctx, index, &lm);
}

int lkpi_display_add(const struct lkpi_display *d)
{
    struct display_head h = {
        .phys = d->phys, .vram = d->vram, .width = d->width, .height = d->height, .pitch = d->pitch,
        .bytes_pp = 4, .r_pos = 16, .g_pos = 8, .b_pos = 0,                 /* x8r8g8b8 */
        .write_span = d->write_span, .write_ctx = d->ctx,
        .refresh_mhz = d->refresh_mhz, .width_mm = d->width_mm, .height_mm = d->height_mm,
    };
    if (d->width <= 0 || d->height <= 0 || d->pitch < d->width * 4) return -1;
    if (d->set_mode && d->nmodes > 0 && nmode_switches < (int)(sizeof mode_switches / sizeof *mode_switches)) {
        for (int i = 0; i < d->nmodes && i < DISPLAY_MAX_MODES; i++)
            h.modes[h.nmodes++] = (struct display_mode){ d->modes[i].width, d->modes[i].height,
                                                         d->modes[i].refresh_mhz, d->modes[i].preferred != 0 };
        mode_switches[nmode_switches] = (struct lkpi_mode_switch){ d->set_mode, d->ctx };
        h.set_mode = lkpi_switch_mode;
        h.mode_ctx = &mode_switches[nmode_switches++];
    }
    size_t frames = ((size_t)d->pitch * (size_t)d->height + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t shadow = pmm_alloc_contig(frames);          /* zeroed, identity mapped */
    if (!shadow) return -1;
    h.shadow = (uint32_t *)(uintptr_t)shadow;
    strlcpy(h.name, d->name, sizeof h.name);
    strlcpy(h.monitor, d->monitor ? d->monitor : "", sizeof h.monitor);
    int idx = display_replace(&h);                      /* the monitor the driver took over */
    if (idx >= 0) return idx;
    idx = display_register(&h);
    if (idx < 0) { pmm_free_contig(shadow, frames); return -1; }
    if (tty_add_monitor(idx) < 0) kprintf("linuxkpi: monitor %d has no terminal\n", idx);
    return idx;
}

int lkpi_display_update(int monitor, const struct lkpi_display *d)
{
    const struct display_head *cur = display_get(monitor);
    if (!cur || d->width <= 0 || d->height <= 0 || d->pitch < d->width * 4) return -EINVAL;
    struct display_head h = *cur;
    h.phys = d->phys; h.vram = d->vram;
    h.width = d->width; h.height = d->height; h.pitch = d->pitch;
    h.write_span = d->write_span; h.write_ctx = d->ctx;
    h.refresh_mhz = d->refresh_mhz;
    size_t frames = ((size_t)d->pitch * (size_t)d->height + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t old_frames = ((size_t)cur->pitch * (size_t)cur->height + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t *old = cur->shadow;
    uint64_t shadow = pmm_alloc_contig(frames);
    if (!shadow) return -ENOMEM;
    h.shadow = (uint32_t *)(uintptr_t)shadow;
    int rc = display_update(monitor, &h);
    if (rc) { pmm_free_contig(shadow, frames); return rc; }
    if (old) pmm_free_contig((uint64_t)(uintptr_t)old, old_frames);
    return 0;
}

void lkpi_display_reclaim(void *handle)
{
    display_reclaim(handle);
}

/* While monitor 0 waits for the driver's first monitor the screen may stay
 * dark, or the driver may hang: refresh REPORT.TXT every 10 s for two
 * minutes so the log of a failed takeover reaches the boot stick. */
static int handover_watch(void *arg)
{
    static char reason[64];
    for (int s = 10; s <= 120; s += 10) {
        task_sleep_ms(10000);
        if (!display_handover_pending()) return 0;
        snprintf(reason, sizeof reason, "monitor 0 still dark %d s after a Linux driver took it over", s);
        report_save(reason);
    }
    return 0;
}

bool lkpi_display_drm(void) { return display_linux_drm(); }

int lkpi_display_handover(void *handle)
{
    int idx = display_handover(handle);
    /* A dark screen from here on leaves only the report on the boot stick */
    if (idx >= 0 && !strstr(g_boot.cmdline, "kestrel.report=0") && report_locate()) {
        report_save("a Linux driver takes over monitor 0");
        task_create("handover-watch", handover_watch, NULL);
    }
    return idx;
}

unsigned int lkpi_pci_read(void *handle, int off, int size)
{
    struct pci_device *d = handle;
    if (off < 0 || off + size > 256) return ~0u;
    switch (size) {
    case 1:  return pci_read8(d->bus, d->device, d->function, (uint8_t)off);
    case 2:  return pci_read16(d->bus, d->device, d->function, (uint8_t)off);
    default: return pci_read32(d->bus, d->device, d->function, (uint8_t)off);
    }
}

/* Narrow writes are real 8- and 16-bit configuration cycles, not a
 * read-modify-write of the dword: devices decode the access size (QEMU's
 * i6300esb takes its lock register only as a byte), and rewriting the
 * neighbouring bytes would clear write-1-to-clear bits such as Status. */
void lkpi_pci_write(void *handle, int off, int size, unsigned int v)
{
    struct pci_device *d = handle;
    if (off < 0 || off + size > 256) return;
    pci_cfg_write(d->bus, d->device, d->function, (uint8_t)off, size, v);
}

int lkpi_pci_msi_enable(void *handle, void (*fn)(void *), void *arg, const char *name)
{
    return msi_enable(handle, fn, arg, name);
}

void lkpi_pci_msi_disable(void *handle, int vector) { msi_disable(handle, vector); }
int lkpi_pci_msi_is_msix(int vector) { return msi_is_msix(vector); }

/* ---- files ------------------------------------------------------------------ */
long lkpi_file_read_all(const char *path, void **data, unsigned long max)
{
    struct stat st;
    int r = vfs_stat(path, &st);
    if (r < 0) return r;
    if (!S_ISREG(st.st_mode)) return -EINVAL;
    if ((unsigned long)st.st_size > max) return -EFBIG;
    struct file *f;
    if ((r = vfs_open(path, O_RDONLY, 0, &f)) < 0) return r;
    size_t size = (size_t)st.st_size;
    uint8_t *buf = kmalloc(size ? size : 1);
    if (!buf) { vfs_close(f); return -ENOMEM; }
    size_t got = 0;
    while (got < size) {
        ssize_t n = vfs_read(f, buf + got, size - got);
        if (n < 0) { kfree(buf); vfs_close(f); return n; }
        if (n == 0) break;
        got += (size_t)n;
    }
    vfs_close(f);
    *data = buf;
    return (long)got;
}
