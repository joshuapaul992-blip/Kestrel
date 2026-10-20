/* linuxkpi/src/drm.c -- what the vendored DRM core needs that Linux keeps
 * under GPL-2.0 or outside DRM: managed resources (drm_managed.h), DRM's
 * sysfs and uevents, the parts that serve user space (PRIME, sync objects,
 * writeback connectors; none are offered yet), DRM's inode file system,
 * memory-backed files for GEM objects, and the CPU and memory facts DRM and
 * TTM ask about.
 *
 * Linux's DRM drivers (bochs, nouveau) bind by default; linuxkpi.drm=0 or
 * the boot manager's Safe Mode keep them off (display_linux_drm()), and the
 * firmware's frame buffer stays the only screen. video_firmware_drivers_only()
 * is how DRM drivers ask whether they may take over a display. */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>
#include <linux/anon_inodes.h>
#include <linux/shmem_fs.h>
#include <linux/shrinker.h>
#include <linux/sysfb.h>
#include <linux/mm.h>
#include <linux/xarray.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/pci.h>
#include <asm/cpufeature.h>
#include <asm/smp.h>
#include <video/nomodeset.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>
#include <drm/drm_sysfs.h>
#include <drm/drm_writeback.h>
#include <drm/drm_prime.h>
#include <drm/drm_syncobj.h>
#include <drm/drm_connector.h>
#include <drm/drm_utils.h>
#include "internal.h"
#include "../../third_party/linux/drivers/gpu/drm/drm_internal.h"
#include "../../third_party/linux/lib/radix-tree.h"

/* ---- the switch -------------------------------------------------------------------- */
static bool lkpi_drm_enabled;

bool video_firmware_drivers_only(void)
{
    return !lkpi_drm_enabled;
}

/* ---- managed resources --------------------------------------------------------------- */
struct drmres {
    struct list_head    entry;
    drmres_release_t    release;
    const char         *name;
    size_t              size;
    void               *data;               /* the action's argument, or the allocation below */
    u8                  alloc[] __aligned(ARCH_KMALLOC_MINALIGN);
};

static void add_dr(struct drm_device *dev, struct drmres *dr)
{
    unsigned long flags;
    spin_lock_irqsave(&dev->managed.lock, flags);
    list_add(&dr->entry, &dev->managed.resources);
    spin_unlock_irqrestore(&dev->managed.lock, flags);
}

int __drmm_add_action(struct drm_device *dev, drmres_release_t action, void *data, const char *name)
{
    struct drmres *dr = kzalloc(sizeof(*dr), GFP_KERNEL);
    if (!dr) return -ENOMEM;
    dr->release = action;
    dr->data = data;
    dr->name = name;
    add_dr(dev, dr);
    return 0;
}
EXPORT_SYMBOL(__drmm_add_action);

int __drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action, void *data, const char *name)
{
    int ret = __drmm_add_action(dev, action, data, name);
    if (ret) action(dev, data);
    return ret;
}
EXPORT_SYMBOL(__drmm_add_action_or_reset);

void drmm_release_action(struct drm_device *dev, drmres_release_t action, void *data)
{
    struct drmres *dr, *found = NULL;
    unsigned long flags;

    spin_lock_irqsave(&dev->managed.lock, flags);
    list_for_each_entry_reverse(dr, &dev->managed.resources, entry) {
        if (dr->release == action && dr->data == data) {
            list_del(&dr->entry);
            found = dr;
            break;
        }
    }
    spin_unlock_irqrestore(&dev->managed.lock, flags);
    if (WARN_ONCE(!found, "drm: no managed action %ps(%p) to release\n", action, data)) return;
    action(dev, data);
    kfree(found);
}
EXPORT_SYMBOL(drmm_release_action);

void *drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
    struct drmres *dr = kmalloc(sizeof(*dr) + size, gfp & ~__GFP_ZERO);
    if (!dr) {
        drm_dbg_driver(dev, "drmm_kmalloc of %zu bytes failed\n", size);
        return NULL;
    }
    memset(dr, 0, sizeof(*dr));
    if (gfp & __GFP_ZERO) memset(dr->alloc, 0, size);
    dr->size = size;
    dr->data = dr->alloc;
    dr->name = "kmalloc";
    add_dr(dev, dr);
    return dr->alloc;
}
EXPORT_SYMBOL(drmm_kmalloc);

char *drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp)
{
    size_t size;
    char *buf;
    if (!s) return NULL;
    size = strlen(s) + 1;
    buf = drmm_kmalloc(dev, size, gfp);
    if (buf) memcpy(buf, s, size);
    return buf;
}
EXPORT_SYMBOL(drmm_kstrdup);

void drmm_kfree(struct drm_device *dev, void *data)
{
    struct drmres *dr, *found = NULL;
    unsigned long flags;

    if (!data) return;
    spin_lock_irqsave(&dev->managed.lock, flags);
    list_for_each_entry(dr, &dev->managed.resources, entry) {
        if (!dr->release && dr->data == data) {
            list_del(&dr->entry);
            found = dr;
            break;
        }
    }
    spin_unlock_irqrestore(&dev->managed.lock, flags);
    if (WARN_ON(!found)) return;
    kfree(found);
}
EXPORT_SYMBOL(drmm_kfree);

void __drmm_mutex_release(struct drm_device *dev, void *res)
{
    mutex_destroy((struct mutex *)res);
}
EXPORT_SYMBOL(__drmm_mutex_release);

/* the structure embedding dev, freed last */
void drmm_add_final_kfree(struct drm_device *dev, void *container)
{
    WARN_ON(dev->managed.final_kfree);
    WARN_ON(dev < (struct drm_device *)container);
    WARN_ON(dev + 1 > (struct drm_device *)(container + ksize(container)));
    dev->managed.final_kfree = container;
}

/* release everything, newest first */
void drm_managed_release(struct drm_device *dev)
{
    struct drmres *dr, *tmp;
    list_for_each_entry_safe(dr, tmp, &dev->managed.resources, entry) {
        if (dr->release) dr->release(dev, dr->size ? dr->alloc : dr->data);
        list_del(&dr->entry);
        kfree(dr);
    }
}

/* ---- sysfs and uevents: none; a minor is a plain device -------------------------------- */
#define DRM_MAJOR               226

int drm_sysfs_init(void) { return 0; }
void drm_sysfs_destroy(void) { }
int drm_sysfs_connector_add(struct drm_connector *connector) { return 0; }
int drm_sysfs_connector_add_late(struct drm_connector *connector) { return 0; }
void drm_sysfs_connector_remove_early(struct drm_connector *connector) { }
void drm_sysfs_connector_remove(struct drm_connector *connector) { }
void drm_sysfs_lease_event(struct drm_device *dev) { }
void drm_sysfs_hotplug_event(struct drm_device *dev) { }
void drm_sysfs_connector_hotplug_event(struct drm_connector *connector) { }
void drm_sysfs_connector_property_event(struct drm_connector *connector, struct drm_property *property) { }
int drm_class_device_register(struct device *dev) { return 0; }
void drm_class_device_unregister(struct device *dev) { }
EXPORT_SYMBOL(drm_sysfs_hotplug_event);
EXPORT_SYMBOL(drm_sysfs_connector_hotplug_event);
EXPORT_SYMBOL(drm_sysfs_connector_property_event);

static void drm_sysfs_release(struct device *dev)
{
    kfree(dev);
}

/* /dev/dri/card0, /dev/dri/renderD128, as Linux's drm_devnode() names them */
static char *drm_devnode(const struct device *dev, umode_t *mode)
{
    const struct drm_minor *minor = dev_get_drvdata(dev);

    if (mode) *mode = minor && minor->type == DRM_MINOR_RENDER ? 0666 : 0660;
    return kasprintf(GFP_KERNEL, "dri/%s", dev_name(dev));
}

static const struct class drm_kpi_class = { .name = "drm", .devnode = drm_devnode };

struct device *drm_sysfs_minor_alloc(struct drm_minor *minor)
{
    const char *minor_str;
    struct device *kdev;
    int r;

    kdev = kzalloc(sizeof(*kdev), GFP_KERNEL);
    if (!kdev) return ERR_PTR(-ENOMEM);
    device_initialize(kdev);
    if (minor->type == DRM_MINOR_ACCEL) {
        minor_str = "accel%d";
    } else {
        minor_str = minor->type == DRM_MINOR_RENDER ? "renderD%d" : "card%d";
    }
    kdev->devt = MKDEV(DRM_MAJOR, minor->index);
    kdev->class = &drm_kpi_class;
    kdev->parent = minor->dev->dev;
    kdev->release = drm_sysfs_release;
    dev_set_drvdata(kdev, minor);
    r = dev_set_name(kdev, minor_str, minor->index);
    if (r < 0) {
        put_device(kdev);
        return ERR_PTR(r);
    }
    return kdev;
}

int drm_writeback_set_fb(struct drm_connector_state *conn_state, struct drm_framebuffer *fb) { return -EINVAL; }
int drm_writeback_prepare_job(struct drm_writeback_job *job) { return 0; }
void drm_writeback_cleanup_job(struct drm_writeback_job *job) { kfree(job); }
struct dma_fence *drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector) { return NULL; }
EXPORT_SYMBOL(drm_writeback_cleanup_job);

/* panels mounted sideways in tablets: no quirks known */
int drm_get_panel_orientation_quirk(int width, int height)
{
    return DRM_MODE_PANEL_ORIENTATION_UNKNOWN;
}
EXPORT_SYMBOL(drm_get_panel_orientation_quirk);

/* ---- DRM's inode file system: plain inodes ---------------------------------------------- */
static struct vfsmount drm_kpi_mnt;

int simple_pin_fs(struct file_system_type *type, struct vfsmount **mount, int *count)
{
    if (!*mount) *mount = &drm_kpi_mnt;
    ++*count;
    return 0;
}

void simple_release_fs(struct vfsmount **mount, int *count)
{
    if (--*count == 0) *mount = NULL;
}

void kern_unmount(struct vfsmount *mnt) { }

static struct inode *lkpi_new_inode(void)
{
    struct inode *inode = kzalloc(sizeof(*inode), GFP_KERNEL);
    if (!inode) return ERR_PTR(-ENOMEM);
    inode->i_mapping = &inode->i_data;
    inode->i_data.host = inode;
    inode->i_data.gfp_mask = GFP_HIGHUSER;
    return inode;
}

struct inode *alloc_anon_inode(struct super_block *sb)
{
    return lkpi_new_inode();
}

void iput(struct inode *inode)
{
    kfree(inode);
}

struct file *file_clone_open(struct file *file) { return ERR_PTR(-EOPNOTSUPP); }

/* GEM objects get one so they can be mapped by offset; it has no pages behind it
 * (objects that want shmem pages, rather than VRAM, are not supported yet) */
struct file *shmem_file_setup(const char *name, loff_t size, unsigned long flags)
{
    struct inode *inode = lkpi_new_inode();
    struct file *file;

    if (IS_ERR(inode)) return ERR_CAST(inode);
    inode->i_size = size;
    file = lkpi_file_new(NULL, inode, O_RDWR, NULL);
    if (IS_ERR(file)) iput(inode);
    return file;
}

struct page *shmem_read_mapping_page_gfp(struct address_space *mapping, pgoff_t index, gfp_t gfp)
{
    return ERR_PTR(-ENOSYS);
}

struct folio *shmem_read_folio_gfp(struct address_space *mapping, pgoff_t index, gfp_t gfp)
{
    return ERR_PTR(-ENOSYS);
}

/* ---- the firmware frame buffer ----------------------------------------------------------- */
/* Linux drivers call this (through aperture_remove_conflicting_*) before
 * they take over an adapter, so the firmware frame buffer's driver lets go.
 * Adapters Kestrel drives itself are never offered to Linux drivers
 * (pci_dev->lkpi_native), but the one behind the boot
 * frame buffer is: Kestrel stops drawing to it here, and the driver's first
 * monitor becomes monitor 0 (drm_kestrel.c). */
void sysfb_disable(struct device *dev)
{
    if (dev_is_pci(dev) && to_pci_dev(dev)->lkpi_handle)
        lkpi_display_handover(to_pci_dev(dev)->lkpi_handle);
}

/* ---- memory ---------------------------------------------------------------------------- */
struct shrinker *shrinker_alloc(unsigned int flags, const char *fmt, ...)
{
    return kzalloc(sizeof(struct shrinker), GFP_KERNEL);
}
void shrinker_register(struct shrinker *shrinker) { }
void shrinker_free(struct shrinker *shrinker) { kfree(shrinker); }

void si_meminfo(struct sysinfo *val)
{
    memset(val, 0, sizeof(*val));
    val->totalram = totalram_pages();
    val->freeram = val->totalram / 2;           /* Kestrel does not say: a guess */
    val->mem_unit = PAGE_SIZE;
}

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
    return vm_flags & VM_WRITE ? PAGE_SHARED : __pgprot(0x25UL);
}

/* xarray nodes: lib/xarray.c takes them as they come from the cache and
 * expects empty slots (Linux's cache has a constructor that clears them,
 * and frees them cleared), so each one is cleared when it is handed out */
struct kmem_cache *radix_tree_node_cachep;

static void radix_tree_node_ctor(void *p)
{
    struct xa_node *node = p;
    memset(node, 0, sizeof(*node));
    INIT_LIST_HEAD(&node->private_list);
}

void radix_tree_node_rcu_free(struct rcu_head *head)
{
    struct xa_node *node = container_of(head, struct xa_node, rcu_head);
    memset(node->slots, 0, sizeof(node->slots));
    memset(node->tags, 0, sizeof(node->tags));
    INIT_LIST_HEAD(&node->private_list);
    kmem_cache_free(radix_tree_node_cachep, node);
}

/* ---- the CPU ------------------------------------------------------------------------------ */
struct cpuinfo_x86 boot_cpu_data;

static void cpu_fill(void)
{
    u32 a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    boot_cpu_data.x86 = (a >> 8) & 0xf;
    boot_cpu_data.x86_model = (a >> 4) & 0xf;
    boot_cpu_data.x86_clflush_size = ((b >> 8) & 0xff) * 8;
    boot_cpu_data.x86_capability[0] = d;
    /* no SSE4.1: drm_memcpy_from_wc() would use SSE registers, which the
     * kernel does not save; it then keeps to plain copies */
    boot_cpu_data.x86_capability[4] = c & ~(1u << (X86_FEATURE_XMM4_1 % 32));
}

int wbinvd_on_all_cpus(void)
{
    /* only this CPU's caches: other CPUs keep their own (DRM uses this only
     * when a page lacks CLFLUSH, which every x86-64 CPU has) */
    asm volatile("wbinvd" ::: "memory");
    return 0;
}

/* ---- bring-up --------------------------------------------------------------------------- */
void lkpi_drm_core_init(void)
{
    cpu_fill();
    radix_tree_node_cachep = kmem_cache_create("radix_tree_node", sizeof(struct xa_node), 0, 0,
                                               radix_tree_node_ctor);
    lkpi_drm_enabled = lkpi_display_drm();
    printk(KERN_INFO "linuxkpi: DRM drivers %s\n", lkpi_drm_enabled ? "may take over displays"
           : "are off (linuxkpi.drm=0 or Safe Mode): the firmware's frame buffer only");
}
