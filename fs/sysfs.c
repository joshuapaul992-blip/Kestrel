/* fs/sysfs.c -- /sys: one small attribute file per kernel object property
 *
 * Besides Kestrel's own entries, the parts of Linux's layout that libdrm,
 * libpciaccess, Mesa and X read to find a GPU:
 *
 *   devices/pci0000:00/0000:00:02.0/   one directory per PCI function (behind
 *                                      a bridge: inside the bridge's), with
 *                                      vendor, device, class, config, resource,
 *                                      uevent (PCI_SLOT_NAME), boot_vga ...
 *   bus/pci/devices/<slot>             links to those
 *   bus/pci/drivers/<driver>/          once a Linux driver binds; the device's
 *                                      "driver" link points here
 *   <device>/drm/card0/                a character device a driver registers
 *                                      (class "drm", dev "226:0", uevent with
 *                                      DEVNAME), with "device" and "subsystem"
 *                                      links; devices/virtual/<class>/<name>
 *                                      for one with no PCI device behind it
 *   class/drm/card0, dev/char/226:0    links to it
 *
 * Links are relative, as Linux's are. Entries are only ever added: a driver
 * that unbinds leaves its links behind (its /dev nodes stay too, and no
 * longer open). */
#include <kernel/vfs.h>
#include <kernel/fb.h>
#include <kernel/task.h>
#include <kernel/arch.h>
#include <kernel/string.h>
#include <kernel/bootinfo.h>
#include <kernel/smp.h>
#include <kernel/mm.h>
#include "../drivers/pci/pci.h"

/* A tiny attribute macro: each generator prints one value + newline. */
#define ATTR(fn, ...) \
    static size_t fn(char *buf, size_t cap, void *ctx) { (void)ctx; return (size_t)snprintf(buf, cap, __VA_ARGS__); }

ATTR(a_ostype,     "%s\n", KESTREL_NAME)
ATTR(a_osrelease,  "%s\n", KESTREL_VERSION)
ATTR(a_hostname,   "%s\n", KESTREL_HOSTNAME)
ATTR(a_fb_name,    "%s\n", g_fb.driver[0] ? g_fb.driver : g_boot.uefi ? "efi-gop" : "vesa-vbe")
ATTR(a_fb_size,    "%u,%u\n", g_fb.width, g_fb.height)
ATTR(a_fb_bpp,     "%u\n", g_fb.bpp)
ATTR(a_fb_stride,  "%u\n", g_fb.pitch)
ATTR(a_fb_phys,    "0x%lx\n", g_fb.phys)
ATTR(a_fb_dbl,     "%d\n", g_fb.double_buffered ? 1 : 0)
ATTR(a_loader,     "%s\n", g_boot.loader)
ATTR(a_bootmode,   "%s\n", g_boot.uefi ? "uefi" : "bios")
ATTR(a_bootentry,  "%s\n", g_boot.boot_entry_name)
ATTR(a_bootflags,  "0x%x\n", g_boot.boot_flags)
ATTR(a_cpu_vendor, "%s\n", g_boot.cpu_vendor)
ATTR(a_cpu_model,  "%s\n", g_boot.cpu_brand)
ATTR(a_preempt,    "%d\n", sched_preemption() ? 1 : 0)
ATTR(a_quantum,    "%d\n", SCHED_QUANTUM)
ATTR(a_ctxsw,      "%lu\n", sched_context_switches())
ATTR(a_hz,         "%d\n", PIT_HZ)
ATTR(a_pwr_state,  "freeze mem disk\n")

/* /sys/devices/system/cpu/{online,possible,present}: "0" or "0-N"
 * (sysconf(_SC_NPROCESSORS_ONLN) in glibc reads "online") */
static size_t a_cpu_range(char *buf, size_t cap, void *ctx)
{
    (void)ctx;
    return ncpus > 1 ? (size_t)snprintf(buf, cap, "0-%d\n", ncpus - 1) : (size_t)snprintf(buf, cap, "0\n");
}
ATTR(a_one, "1\n")

/* ---- links ----------------------------------------------------------------------------- */
static int link_read(struct vnode *vn, char *buf, size_t size)
{
    size_t n = strlen(vn->data);
    if (n > size) n = size;
    memcpy(buf, vn->data, n);
    return (int)n;
}
static const struct vnode_ops link_ops = { .readlink = link_read, .nolock = true };

/* A link in `dir`, `depth` levels below /sys, to `target` (a path from /sys). */
static void sys_link(struct vnode *dir, int depth, const char *name, const char *target)
{
    size_t n = (size_t)depth * 3 + strlen(target) + 1;
    char *t = kmalloc(n);
    if (!t) return;
    for (int i = 0; i < depth; i++) memcpy(t + 3 * i, "../", 3);
    memcpy(t + 3 * depth, target, strlen(target) + 1);
    struct vnode *l = vfs_node_new(dir->fs, name, VLNK, 0777, &link_ops);
    if (!l) { kfree(t); return; }
    l->data = t;
    l->size = n - 1;
    vfs_node_add(dir, l);
}

static struct vnode *child(struct vnode *dir, const char *name)
{
    for (struct vnode *c = dir->children; c; c = c->sibling)
        if (!strcmp(c->name, name)) return c;
    return NULL;
}

static struct vnode *subdir(struct vnode *dir, const char *name)
{
    struct vnode *c = child(dir, name);
    return c ? c : pseudo_dir(dir, name);
}

static struct vnode *sys_root, *sys_devices, *sys_pci_devs, *sys_pci_drivers, *sys_class, *sys_devchar;

/* ---- PCI functions ------------------------------------------------------------------------ */
struct sys_pci {
    struct pci_device *d;
    struct vnode *dir;
    int   depth;                    /* of dir below /sys */
    char  path[96];                 /* of dir from /sys */
    char  driver[32];
    bool  boot_vga;
};
static struct sys_pci sys_pci[PCI_MAX_TRACKED];
static size_t nsys_pci;

#define PCI_ATTR(fn, ...) \
    static size_t fn(char *buf, size_t cap, void *ctx) { const struct sys_pci *p = ctx; const struct pci_device *d = p->d; (void)d; return (size_t)snprintf(buf, cap, __VA_ARGS__); }
PCI_ATTR(a_pci_vendor,    "0x%04x\n", d->vendor_id)
PCI_ATTR(a_pci_device,    "0x%04x\n", d->device_id)
PCI_ATTR(a_pci_subvendor, "0x%04x\n", d->subsystem_vendor_id)
PCI_ATTR(a_pci_subdevice, "0x%04x\n", d->subsystem_id)
PCI_ATTR(a_pci_class,     "0x%02x%02x%02x\n", d->class_code, d->subclass, d->prog_if)
PCI_ATTR(a_pci_revision,  "0x%02x\n", d->revision)
PCI_ATTR(a_pci_irq,       "%u\n", d->interrupt_line)
PCI_ATTR(a_pci_boot_vga,  "%d\n", p->boot_vga ? 1 : 0)
PCI_ATTR(a_pci_modalias,  "pci:v%08Xd%08Xsv%08Xsd%08Xbc%02Xsc%02Xi%02X\n", d->vendor_id, d->device_id,
         d->subsystem_vendor_id, d->subsystem_id, d->class_code, d->subclass, d->prog_if)

static size_t a_pci_enable(char *buf, size_t cap, void *ctx)
{
    const struct pci_device *d = ((const struct sys_pci *)ctx)->d;
    unsigned cmd = pci_read16(d->bus, d->device, d->function, 0x04);
    return (size_t)snprintf(buf, cap, "%d\n", cmd & 7 ? 1 : 0);
}

/* the configuration space as it is now: 256 bytes */
static size_t a_pci_config(char *buf, size_t cap, void *ctx)
{
    const struct pci_device *d = ((const struct sys_pci *)ctx)->d;
    size_t n = cap < 256 ? cap & ~3ul : 256;
    for (size_t off = 0; off < n; off += 4) {
        uint32_t v = pci_read32(d->bus, d->device, d->function, (uint8_t)off);
        memcpy(buf + off, &v, 4);
    }
    return n;
}

/* first, last, flags per BAR and the ROM, with Linux's IORESOURCE_* flags (and
 * the BAR's own low bits, which libpciaccess reads) */
static size_t a_pci_resource(char *buf, size_t cap, void *ctx)
{
    const struct pci_device *d = ((const struct sys_pci *)ctx)->d;
    size_t n = 0;
    for (int i = 0; i < 7; i++) {
        uint64_t first = 0, last = 0, flags = 0;
        if (i < d->bar_count) {
            const struct pci_bar *b = &d->bars[i];
            if (b->size && b->kind != PCI_BAR_UNUSED && b->kind != PCI_BAR_MEM64_UPPER) {
                first = b->base;
                last = b->base + b->size - 1;
                if (b->kind == PCI_BAR_IO) flags = 0x101;
                else {
                    flags = 0x200;
                    if (b->kind == PCI_BAR_MEM64) flags |= 0x100004;
                    if (b->prefetchable) flags |= 0x2008;
                }
            }
        }
        n += (size_t)snprintf(buf + n, cap > n ? cap - n : 0, "0x%016lx 0x%016lx 0x%016lx\n",
                              (unsigned long)first, (unsigned long)last, (unsigned long)flags);
    }
    return n;
}

static size_t a_pci_uevent(char *buf, size_t cap, void *ctx)
{
    const struct sys_pci *p = ctx;
    const struct pci_device *d = p->d;
    size_t n = 0;
    if (p->driver[0]) n += (size_t)snprintf(buf, cap, "DRIVER=%s\n", p->driver);
    n += (size_t)snprintf(buf + n, cap > n ? cap - n : 0,
        "PCI_CLASS=%X\nPCI_ID=%04X:%04X\nPCI_SUBSYS_ID=%04X:%04X\nPCI_SLOT_NAME=0000:%02x:%02x.%x\n"
        "MODALIAS=pci:v%08Xd%08Xsv%08Xsd%08Xbc%02Xsc%02Xi%02X\n",
        (unsigned)d->class_code << 16 | (unsigned)d->subclass << 8 | d->prog_if,
        d->vendor_id, d->device_id, d->subsystem_vendor_id, d->subsystem_id,
        d->bus, d->device, d->function,
        d->vendor_id, d->device_id, d->subsystem_vendor_id, d->subsystem_id,
        d->class_code, d->subclass, d->prog_if);
    return n;
}

static void slot_name(char *out, size_t cap, const struct pci_device *d)
{
    snprintf(out, cap, "0000:%02x:%02x.%x", d->bus, d->device, d->function);
}

static struct sys_pci *pci_rec(const struct pci_device *d)
{
    for (size_t i = 0; i < nsys_pci; i++)
        if (sys_pci[i].d == d) return &sys_pci[i];
    return NULL;
}

/* The function's directory, its parent bridge's first. */
static struct sys_pci *pci_add(struct pci_device *d)
{
    struct sys_pci *p = pci_rec(d);
    if (p) return p;
    if (nsys_pci >= PCI_MAX_TRACKED) return NULL;
    struct vnode *parent;
    char slot[16];
    const char *ppath;
    int pdepth;
    char rootname[16];
    struct sys_pci *up = d->parent ? pci_add(d->parent) : NULL;
    if (up) {
        parent = up->dir; ppath = up->path; pdepth = up->depth;
    } else {
        snprintf(rootname, sizeof rootname, "pci0000:%02x", d->bus);
        parent = subdir(sys_devices, rootname);
        ppath = NULL; pdepth = 2;
    }
    p = &sys_pci[nsys_pci++];
    p->d = d;
    slot_name(slot, sizeof slot, d);
    if (ppath) snprintf(p->path, sizeof p->path, "%s/%s", ppath, slot);
    else snprintf(p->path, sizeof p->path, "devices/%s/%s", rootname, slot);
    p->depth = pdepth + 1;
    p->dir = pseudo_dir(parent, slot);
    pseudo_file(p->dir, "vendor",           a_pci_vendor, p);
    pseudo_file(p->dir, "device",           a_pci_device, p);
    pseudo_file(p->dir, "subsystem_vendor", a_pci_subvendor, p);
    pseudo_file(p->dir, "subsystem_device", a_pci_subdevice, p);
    pseudo_file(p->dir, "class",            a_pci_class, p);
    pseudo_file(p->dir, "revision",         a_pci_revision, p);
    pseudo_file(p->dir, "irq",              a_pci_irq, p);
    pseudo_file(p->dir, "enable",           a_pci_enable, p);
    pseudo_file(p->dir, "modalias",         a_pci_modalias, p);
    pseudo_file(p->dir, "config",           a_pci_config, p);
    pseudo_file(p->dir, "resource",         a_pci_resource, p);
    pseudo_file(p->dir, "uevent",           a_pci_uevent, p);
    if (d->class_code == 0x03 && d->subclass == 0x00)
        pseudo_file(p->dir, "boot_vga",     a_pci_boot_vga, p);
    sys_link(p->dir, p->depth, "subsystem", "bus/pci");
    sys_link(sys_pci_devs, 3, slot, p->path);
    return p;
}

/* boot_vga: the VGA function whose memory holds the boot frame buffer, else
 * the first VGA function (Linux's VGA arbiter picks the same way) */
static void pci_pick_boot_vga(void)
{
    struct sys_pci *first = NULL;
    for (size_t i = 0; i < nsys_pci; i++) {
        struct sys_pci *p = &sys_pci[i];
        const struct pci_device *d = p->d;
        if (d->class_code != 0x03 || d->subclass != 0x00) continue;
        if (!first) first = p;
        for (int b = 0; b < d->bar_count; b++)
            if (d->bars[b].kind != PCI_BAR_IO && d->bars[b].size && g_fb.phys >= d->bars[b].base &&
                g_fb.phys < d->bars[b].base + d->bars[b].size) {
                p->boot_vga = true;
                return;
            }
    }
    if (first) first->boot_vga = true;
}

static void pci_tree(void)
{
    struct vnode *bus = pseudo_dir(sys_root, "bus");
    struct vnode *pci = pseudo_dir(bus, "pci");
    sys_pci_devs = pseudo_dir(pci, "devices");
    sys_pci_drivers = pseudo_dir(pci, "drivers");
    for (size_t i = 0; i < pci_device_count(); i++) pci_add(pci_device_at(i));
    pci_pick_boot_vga();
}

void sysfs_pci_driver(const struct pci_device *d, const char *driver)
{
    vfs_tree_lock(true);
    struct sys_pci *p = pci_rec(d);
    if (p && !p->driver[0] && driver) {
        char slot[16], target[48];
        strlcpy(p->driver, driver, sizeof p->driver);
        slot_name(slot, sizeof slot, d);
        struct vnode *drv = subdir(sys_pci_drivers, p->driver);
        sys_link(drv, 4, slot, p->path);
        snprintf(target, sizeof target, "bus/pci/drivers/%s", p->driver);
        sys_link(p->dir, p->depth, "driver", target);
    }
    vfs_tree_unlock();
}

/* ---- character devices of a class --------------------------------------------------------- */
struct sys_cdev {
    unsigned major, minor;
    char devname[48];
};
static size_t a_cdev_dev(char *buf, size_t cap, void *ctx)
{
    const struct sys_cdev *c = ctx;
    return (size_t)snprintf(buf, cap, "%u:%u\n", c->major, c->minor);
}
static size_t a_cdev_uevent(char *buf, size_t cap, void *ctx)
{
    const struct sys_cdev *c = ctx;
    return (size_t)snprintf(buf, cap, "MAJOR=%u\nMINOR=%u\nDEVNAME=%s\n", c->major, c->minor, c->devname);
}

void sysfs_class_dev(const struct pci_device *d, const char *cls, const char *name, const char *devname,
                     unsigned major, unsigned minor)
{
    struct sys_cdev *c = kzalloc(sizeof *c);
    if (!c) return;
    c->major = major;
    c->minor = minor;
    strlcpy(c->devname, devname, sizeof c->devname);
    vfs_tree_lock(true);
    struct sys_pci *p = d ? pci_rec(d) : NULL;
    struct vnode *parent;
    char path[160], target[32];
    int depth;
    if (p) {
        parent = subdir(p->dir, cls);
        snprintf(path, sizeof path, "%s/%s/%s", p->path, cls, name);
        depth = p->depth + 2;
    } else {
        parent = subdir(subdir(sys_devices, "virtual"), cls);
        snprintf(path, sizeof path, "devices/virtual/%s/%s", cls, name);
        depth = 4;
    }
    if (child(parent, name)) { vfs_tree_unlock(); kfree(c); return; }
    struct vnode *dir = pseudo_dir(parent, name);
    pseudo_file(dir, "dev", a_cdev_dev, c);
    pseudo_file(dir, "uevent", a_cdev_uevent, c);
    snprintf(target, sizeof target, "class/%s", cls);
    sys_link(dir, depth, "subsystem", target);
    if (p) sys_link(dir, depth, "device", p->path);
    sys_link(subdir(sys_class, cls), 2, name, path);
    snprintf(target, sizeof target, "%u:%u", major, minor);
    sys_link(sys_devchar, 2, target, path);
    vfs_tree_unlock();
}

void sysfs_init(const char *mountpoint)
{
    struct mount *m;
    struct vnode *r = vfs_mkfs("sysfs", MNT_RDONLY | MNT_NOCREATE, &m);
    sys_root = r;

    struct vnode *kernel = pseudo_dir(r, "kernel");
    pseudo_file(kernel, "ostype",    a_ostype, NULL);
    pseudo_file(kernel, "osrelease", a_osrelease, NULL);
    pseudo_file(kernel, "hostname",  a_hostname, NULL);
    struct vnode *sched = pseudo_dir(kernel, "sched");
    pseudo_file(sched, "preemption",       a_preempt, NULL);
    pseudo_file(sched, "quantum_ms",       a_quantum, NULL);
    pseudo_file(sched, "context_switches", a_ctxsw, NULL);
    pseudo_file(sched, "tick_hz",          a_hz, NULL);

    struct vnode *cls = sys_class = pseudo_dir(r, "class");
    struct vnode *gfx = pseudo_dir(cls, "graphics");
    struct vnode *fb0 = pseudo_dir(gfx, "fb0");
    pseudo_file(fb0, "name",            a_fb_name, NULL);
    pseudo_file(fb0, "virtual_size",    a_fb_size, NULL);
    pseudo_file(fb0, "bits_per_pixel",  a_fb_bpp, NULL);
    pseudo_file(fb0, "stride",          a_fb_stride, NULL);
    pseudo_file(fb0, "phys_addr",       a_fb_phys, NULL);
    pseudo_file(fb0, "double_buffered", a_fb_dbl, NULL);

    struct vnode *fw = pseudo_dir(r, "firmware");
    pseudo_file(fw, "bootloader", a_loader, NULL);
    pseudo_file(fw, "boot_mode",  a_bootmode, NULL);
    pseudo_file(fw, "boot_entry", a_bootentry, NULL);
    pseudo_file(fw, "boot_flags", a_bootflags, NULL);

    struct vnode *dev = sys_devices = pseudo_dir(r, "devices");
    struct vnode *sys = pseudo_dir(dev, "system");
    struct vnode *cpu = pseudo_dir(sys, "cpu");
    pseudo_file(cpu, "online",   a_cpu_range, NULL);
    pseudo_file(cpu, "possible", a_cpu_range, NULL);
    pseudo_file(cpu, "present",  a_cpu_range, NULL);
    int listed = apic_enabled() ? apic_cpu_count() : 1;   /* the APs are not up yet */
    if (listed > MAX_CPUS) listed = MAX_CPUS;
    for (int i = 0; i < listed; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "cpu%d", i);
        struct vnode *c = pseudo_dir(cpu, nm);
        pseudo_file(c, "vendor",     a_cpu_vendor, NULL);
        pseudo_file(c, "model_name", a_cpu_model, NULL);
        pseudo_file(c, "online",     a_one, NULL);
    }

    pci_tree();
    struct vnode *devs = pseudo_dir(r, "dev");
    sys_devchar = pseudo_dir(devs, "char");
    pseudo_dir(devs, "block");

    struct vnode *pwr = pseudo_dir(r, "power");
    pseudo_file(pwr, "state", a_pwr_state, NULL);

    vfs_mount(m, mountpoint);
}
