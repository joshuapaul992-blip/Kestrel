/* linuxkpi/src/virtio.c -- the virtio bus, split virtqueues and the modern
 * virtio-pci transport (linux/virtio.h, linux/virtio_config.h)
 *
 * Written from the virtio 1.x specification ("Virtio Over PCI Bus", "Split
 * Virtqueues", "Device Initialization"). Only what Linux's virtio drivers
 * in third_party need: a device found on PCI gets a struct virtio_device on
 * the "virtio" bus, the driver bound to it negotiates features through the
 * bus's probe, and find_vqs() sets up split rings in DMA-able memory.
 *
 *   - Modern devices only (VIRTIO_F_VERSION_1), little endian like x86.
 *   - No indirect descriptors and no event index: the driver sees neither
 *     feature, so a ring holds one descriptor per buffer piece and
 *     interrupts are switched with the avail ring's NO_INTERRUPT flag.
 *   - Kestrel gives a device one MSI vector (MSI-X entry 0, or MSI): every
 *     queue and the configuration report there, and the handler looks at
 *     each queue's used ring. With MSI the ISR byte tells a configuration
 *     change; with MSI-X the configuration generation does.
 *   - Kestrel drives virtio-net and virtio-blk itself, so the PCI driver
 *     here takes only the device types that have a Linux driver: the GPU.
 *   - No shared memory regions are offered (virtio_get_shm_region() says
 *     none), so virtio-gpu runs without host-visible blob memory.
 *
 * The rings are the driver's to lock (virtio-gpu holds its queue's
 * spinlock around add, kick and get_buf), as on Linux. The interrupt runs
 * from the MSI without the big kernel lock and only reads the used index
 * before calling the queue's callback. */
#include <linux/kernel.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_dma_buf.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/printk.h>

/* ---- the bus -------------------------------------------------------------------------- */
static atomic_t virtio_index = ATOMIC_INIT(0);

static int virtio_id_match(const struct virtio_device *dev, const struct virtio_device_id *id)
{
    if (id->device != dev->id.device && id->device != VIRTIO_DEV_ANY_ID) return 0;
    return id->vendor == VIRTIO_DEV_ANY_ID || id->vendor == dev->id.vendor;
}

static int virtio_dev_match(struct device *_dv, const struct device_driver *_dr)
{
    struct virtio_device *dev = dev_to_virtio(_dv);
    const struct virtio_device_id *ids = drv_to_virtio(_dr)->id_table;
    for (unsigned int i = 0; ids && ids[i].device; i++)
        if (virtio_id_match(dev, &ids[i])) return 1;
    return 0;
}

void virtio_add_status(struct virtio_device *dev, unsigned int status)
{
    dev->config->set_status(dev, dev->config->get_status(dev) | status);
}
EXPORT_SYMBOL_GPL(virtio_add_status);

void virtio_device_ready(struct virtio_device *dev)
{
    unsigned int status = dev->config->get_status(dev);
    WARN_ON(status & VIRTIO_CONFIG_S_DRIVER_OK);
    dev->config->set_status(dev, status | VIRTIO_CONFIG_S_DRIVER_OK);
}
EXPORT_SYMBOL_GPL(virtio_device_ready);

void virtio_reset_device(struct virtio_device *dev)
{
    dev->config->reset(dev);
}
EXPORT_SYMBOL_GPL(virtio_reset_device);

void virtio_break_device(struct virtio_device *dev)
{
    struct virtqueue *vq;
    spin_lock(&dev->vqs_list_lock);
    list_for_each_entry(vq, &dev->vqs, list) WRITE_ONCE(vq->reset, true);
    spin_unlock(&dev->vqs_list_lock);
}
EXPORT_SYMBOL_GPL(virtio_break_device);

static void config_changed_locked(struct virtio_device *dev)
{
    struct virtio_driver *drv = dev->dev.driver ? drv_to_virtio(dev->dev.driver) : NULL;
    if (!dev->config_core_enabled || dev->config_driver_disabled) dev->config_change_pending = true;
    else if (drv && drv->config_changed) drv->config_changed(dev);
}

void virtio_config_changed(struct virtio_device *dev)
{
    unsigned long flags;
    spin_lock_irqsave(&dev->config_lock, flags);
    config_changed_locked(dev);
    spin_unlock_irqrestore(&dev->config_lock, flags);
}
EXPORT_SYMBOL_GPL(virtio_config_changed);

static void config_enable(struct virtio_device *dev, bool on)
{
    spin_lock_irq(&dev->config_lock);
    dev->config_core_enabled = on;
    if (on && dev->config_change_pending) {
        dev->config_change_pending = false;
        config_changed_locked(dev);
    }
    spin_unlock_irq(&dev->config_lock);
}

/* Device initialization (spec 3.1.1): ACKNOWLEDGE and DRIVER are set; the
 * features both sides know, FEATURES_OK, the driver's probe, DRIVER_OK. */
static int virtio_dev_probe(struct device *_d)
{
    struct virtio_device *dev = dev_to_virtio(_d);
    struct virtio_driver *drv = drv_to_virtio(dev->dev.driver);
    u64 device_features, driver_features = 0;
    int err;

    virtio_add_status(dev, VIRTIO_CONFIG_S_DRIVER);
    device_features = dev->config->get_features(dev);
    for (unsigned int i = 0; i < drv->feature_table_size; i++)
        if (drv->feature_table[i] < 64) driver_features |= 1ULL << drv->feature_table[i];
    driver_features |= 1ULL << VIRTIO_F_VERSION_1;      /* the transport's own */
    dev->features = device_features & driver_features;
    if (!virtio_has_feature(dev, VIRTIO_F_VERSION_1)) {
        dev_err(&dev->dev, "not a modern (virtio 1.0) device\n");
        err = -ENODEV;
        goto err;
    }
    if (drv->validate) {
        u64 features = dev->features;
        err = drv->validate(dev);
        if (err) goto err;
        if (features != dev->features) dev->features &= features;   /* validate may only clear */
    }
    err = dev->config->finalize_features(dev);
    if (err) goto err;
    virtio_add_status(dev, VIRTIO_CONFIG_S_FEATURES_OK);
    if (!(dev->config->get_status(dev) & VIRTIO_CONFIG_S_FEATURES_OK)) {
        dev_err(&dev->dev, "the device refused the features %#llx\n", (unsigned long long)dev->features);
        err = -ENODEV;
        goto err;
    }
    err = drv->probe(dev);
    if (err) goto err;
    if (!(dev->config->get_status(dev) & VIRTIO_CONFIG_S_DRIVER_OK)) virtio_device_ready(dev);
    if (drv->scan) drv->scan(dev);
    config_enable(dev, true);
    return 0;
err:
    virtio_add_status(dev, VIRTIO_CONFIG_S_FAILED);
    return err;
}

static void virtio_dev_remove(struct device *_d)
{
    struct virtio_device *dev = dev_to_virtio(_d);
    struct virtio_driver *drv = drv_to_virtio(dev->dev.driver);

    config_enable(dev, false);
    drv->remove(dev);
    WARN_ON_ONCE(dev->config->get_status(dev));         /* the driver resets the device */
    virtio_add_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE);
}

static const struct bus_type virtio_bus = {
    .name   = "virtio",
    .match  = virtio_dev_match,
    .probe  = virtio_dev_probe,
    .remove = virtio_dev_remove,
};

bool is_virtio_device(struct device *dev) { return dev->bus == &virtio_bus; }
EXPORT_SYMBOL_GPL(is_virtio_device);

int __register_virtio_driver(struct virtio_driver *driver, struct module *owner)
{
    BUG_ON(driver->feature_table_size && !driver->feature_table);
    driver->driver.bus = &virtio_bus;
    driver->driver.owner = owner;
    return driver_register(&driver->driver);
}
EXPORT_SYMBOL_GPL(__register_virtio_driver);

void unregister_virtio_driver(struct virtio_driver *driver)
{
    driver_unregister(&driver->driver);
}
EXPORT_SYMBOL_GPL(unregister_virtio_driver);

int register_virtio_device(struct virtio_device *dev)
{
    int err;

    dev->dev.bus = &virtio_bus;
    device_initialize(&dev->dev);
    dev->index = atomic_inc_return(&virtio_index) - 1;
    err = dev_set_name(&dev->dev, "virtio%u", dev->index);
    if (err) return err;
    spin_lock_init(&dev->config_lock);
    spin_lock_init(&dev->vqs_list_lock);
    INIT_LIST_HEAD(&dev->vqs);
    dev->config_core_enabled = false;
    dev->config_change_pending = false;
    virtio_reset_device(dev);                           /* whatever the firmware left */
    virtio_add_status(dev, VIRTIO_CONFIG_S_ACKNOWLEDGE);
    return device_add(&dev->dev);
}
EXPORT_SYMBOL_GPL(register_virtio_device);

void unregister_virtio_device(struct virtio_device *dev)
{
    device_unregister(&dev->dev);
}
EXPORT_SYMBOL_GPL(unregister_virtio_device);

static int __init virtio_init(void)
{
    return bus_register(&virtio_bus);
}
core_initcall(virtio_init);

/* ---- split virtqueues (spec 2.7) ------------------------------------------------------- */
#define VRING_DESC_F_NEXT           1
#define VRING_DESC_F_WRITE          2
#define VRING_AVAIL_F_NO_INTERRUPT  1
#define VRING_USED_F_NO_NOTIFY      1
#define VQ_MAX_SIZE                 1024

struct vring_desc  { __le64 addr; __le32 len; __le16 flags; __le16 next; };
struct vring_avail { __le16 flags; __le16 idx; __le16 ring[]; };
struct vring_used_elem { __le32 id; __le32 len; };
struct vring_used  { __le16 flags; __le16 idx; struct vring_used_elem ring[]; };

struct vp_vq {
    struct virtqueue    vq;
    u16                 num;
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    dma_addr_t          desc_dma, avail_dma, used_dma;
    size_t              desc_size, avail_size, used_size;
    u16                 free_head;
    u16                 avail_idx;          /* the next avail->idx */
    u16                 last_used;          /* used entries taken so far */
    u16                 added;              /* since the last kick */
    void              **data;               /* by head descriptor */
    void __iomem       *notify;
    struct device      *dma_dev;
};

#define to_vp_vq(_vq) container_of(_vq, struct vp_vq, vq)

static unsigned int count_sg(struct scatterlist *sg)
{
    unsigned int n = 0;
    for (; sg; sg = sg_next(sg)) n++;
    return n;
}

int virtqueue_add_sgs(struct virtqueue *_vq, struct scatterlist *sgs[], unsigned int out_sgs,
                      unsigned int in_sgs, void *data, gfp_t gfp)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    unsigned int total = 0, i;
    u16 head, cur, prev = 0;

    if (WARN_ON(!data)) return -EINVAL;
    if (READ_ONCE(_vq->reset)) return -EIO;
    for (i = 0; i < out_sgs + in_sgs; i++) total += count_sg(sgs[i]);
    if (!total || total > _vq->num_free) return -ENOSPC;

    head = cur = vq->free_head;
    for (i = 0; i < out_sgs + in_sgs; i++) {
        for (struct scatterlist *sg = sgs[i]; sg; sg = sg_next(sg)) {
            struct vring_desc *d = &vq->desc[cur];
            d->addr = cpu_to_le64(sg_phys(sg));
            d->len = cpu_to_le32(sg->length);
            d->flags = cpu_to_le16(VRING_DESC_F_NEXT | (i >= out_sgs ? VRING_DESC_F_WRITE : 0));
            prev = cur;
            cur = le16_to_cpu(d->next);
        }
    }
    vq->desc[prev].flags &= cpu_to_le16(~VRING_DESC_F_NEXT);
    vq->free_head = cur;
    _vq->num_free -= total;
    vq->data[head] = data;

    vq->avail->ring[vq->avail_idx & (vq->num - 1)] = cpu_to_le16(head);
    vq->avail_idx++;
    virt_wmb();                                         /* the descriptors before the index */
    WRITE_ONCE(vq->avail->idx, cpu_to_le16(vq->avail_idx));
    vq->added++;
    return 0;
}
EXPORT_SYMBOL_GPL(virtqueue_add_sgs);

int virtqueue_add_outbuf(struct virtqueue *vq, struct scatterlist sg[], unsigned int num, void *data, gfp_t gfp)
{
    struct scatterlist *sgs[1] = { sg };
    if (num) sg_mark_end(&sg[num - 1]);
    return virtqueue_add_sgs(vq, sgs, 1, 0, data, gfp);
}
EXPORT_SYMBOL_GPL(virtqueue_add_outbuf);

int virtqueue_add_inbuf(struct virtqueue *vq, struct scatterlist sg[], unsigned int num, void *data, gfp_t gfp)
{
    struct scatterlist *sgs[1] = { sg };
    if (num) sg_mark_end(&sg[num - 1]);
    return virtqueue_add_sgs(vq, sgs, 0, 1, data, gfp);
}
EXPORT_SYMBOL_GPL(virtqueue_add_inbuf);

bool virtqueue_kick_prepare(struct virtqueue *_vq)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    virt_mb();                                          /* the new index before the device's flag */
    bool notify = vq->added && !(le16_to_cpu(READ_ONCE(vq->used->flags)) & VRING_USED_F_NO_NOTIFY);
    vq->added = 0;
    return notify;
}
EXPORT_SYMBOL_GPL(virtqueue_kick_prepare);

bool virtqueue_notify(struct virtqueue *_vq)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    if (READ_ONCE(_vq->reset)) return false;
    iowrite16(_vq->index, vq->notify);
    return true;
}
EXPORT_SYMBOL_GPL(virtqueue_notify);

bool virtqueue_kick(struct virtqueue *vq)
{
    return virtqueue_kick_prepare(vq) ? virtqueue_notify(vq) : true;
}
EXPORT_SYMBOL_GPL(virtqueue_kick);

static bool more_used(struct vp_vq *vq)
{
    return vq->last_used != le16_to_cpu(READ_ONCE(vq->used->idx));
}

void *virtqueue_get_buf(struct virtqueue *_vq, unsigned int *len)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    struct vring_used_elem *e;
    void *data;
    u16 id, i;

    if (!more_used(vq)) return NULL;
    virt_rmb();                                         /* the entry after the index */
    e = &vq->used->ring[vq->last_used & (vq->num - 1)];
    id = (u16)le32_to_cpu(e->id);
    if (id >= vq->num || !vq->data[id]) {
        pr_err("virtio: %s: the device returned descriptor %u, which is not in use\n", _vq->name, id);
        WRITE_ONCE(_vq->reset, true);
        return NULL;
    }
    if (len) *len = le32_to_cpu(e->len);
    data = vq->data[id];
    vq->data[id] = NULL;

    /* the chain goes back on the free list */
    for (i = id;; i = le16_to_cpu(vq->desc[i].next)) {
        _vq->num_free++;
        if (!(vq->desc[i].flags & cpu_to_le16(VRING_DESC_F_NEXT))) break;
    }
    vq->desc[i].next = cpu_to_le16(vq->free_head);
    vq->desc[i].flags = 0;
    vq->free_head = id;
    vq->last_used++;
    return data;
}
EXPORT_SYMBOL_GPL(virtqueue_get_buf);

void virtqueue_disable_cb(struct virtqueue *_vq)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    WRITE_ONCE(vq->avail->flags, cpu_to_le16(VRING_AVAIL_F_NO_INTERRUPT));
}
EXPORT_SYMBOL_GPL(virtqueue_disable_cb);

unsigned int virtqueue_enable_cb_prepare(struct virtqueue *_vq)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    WRITE_ONCE(vq->avail->flags, 0);
    return vq->last_used;
}
EXPORT_SYMBOL_GPL(virtqueue_enable_cb_prepare);

bool virtqueue_poll(struct virtqueue *_vq, unsigned int last_used_idx)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    virt_mb();                                          /* the flag before the index */
    return (u16)last_used_idx != le16_to_cpu(READ_ONCE(vq->used->idx));
}
EXPORT_SYMBOL_GPL(virtqueue_poll);

/* false: buffers came back meanwhile, the caller looks again */
bool virtqueue_enable_cb(struct virtqueue *vq)
{
    return !virtqueue_poll(vq, virtqueue_enable_cb_prepare(vq));
}
EXPORT_SYMBOL_GPL(virtqueue_enable_cb);

void *virtqueue_detach_unused_buf(struct virtqueue *_vq)
{
    struct vp_vq *vq = to_vp_vq(_vq);
    for (unsigned int i = 0; i < vq->num; i++) {
        void *data = vq->data[i];
        if (!data) continue;
        vq->data[i] = NULL;
        u16 j;
        for (j = i;; j = le16_to_cpu(vq->desc[j].next)) {
            _vq->num_free++;
            if (!(vq->desc[j].flags & cpu_to_le16(VRING_DESC_F_NEXT))) break;
        }
        vq->desc[j].next = cpu_to_le16(vq->free_head);
        vq->desc[j].flags = 0;
        vq->free_head = i;
        return data;
    }
    return NULL;
}
EXPORT_SYMBOL_GPL(virtqueue_detach_unused_buf);

unsigned int virtqueue_get_vring_size(const struct virtqueue *_vq)
{
    return container_of(_vq, struct vp_vq, vq)->num;
}
EXPORT_SYMBOL_GPL(virtqueue_get_vring_size);

bool virtqueue_is_broken(const struct virtqueue *vq) { return READ_ONCE(vq->reset); }
EXPORT_SYMBOL_GPL(virtqueue_is_broken);

static bool vring_interrupt(struct vp_vq *vq)
{
    if (!more_used(vq)) return false;
    if (vq->vq.callback) vq->vq.callback(&vq->vq);
    return true;
}

static void vring_free(struct vp_vq *vq)
{
    if (vq->desc) dma_free_coherent(vq->dma_dev, vq->desc_size, vq->desc, vq->desc_dma);
    if (vq->avail) dma_free_coherent(vq->dma_dev, vq->avail_size, vq->avail, vq->avail_dma);
    if (vq->used) dma_free_coherent(vq->dma_dev, vq->used_size, vq->used, vq->used_dma);
    kfree(vq->data);
    kfree(vq);
}

static struct vp_vq *vring_new(struct device *dma_dev, unsigned int index, u16 num, const char *name,
                               vq_callback_t *callback)
{
    struct vp_vq *vq = kzalloc(sizeof(*vq), GFP_KERNEL);
    if (!vq) return NULL;
    vq->dma_dev = dma_dev;
    vq->num = num;
    vq->desc_size = sizeof(struct vring_desc) * num;
    vq->avail_size = sizeof(struct vring_avail) + sizeof(__le16) * (num + 1);
    vq->used_size = sizeof(struct vring_used) + sizeof(struct vring_used_elem) * num + sizeof(__le16);
    vq->desc = dma_alloc_coherent(dma_dev, vq->desc_size, &vq->desc_dma, GFP_KERNEL);
    vq->avail = dma_alloc_coherent(dma_dev, vq->avail_size, &vq->avail_dma, GFP_KERNEL);
    vq->used = dma_alloc_coherent(dma_dev, vq->used_size, &vq->used_dma, GFP_KERNEL);
    vq->data = kcalloc(num, sizeof(void *), GFP_KERNEL);
    if (!vq->desc || !vq->avail || !vq->used || !vq->data) {
        vring_free(vq);
        return NULL;
    }
    memset(vq->desc, 0, vq->desc_size);
    memset(vq->avail, 0, vq->avail_size);
    memset(vq->used, 0, vq->used_size);
    for (u16 i = 0; i < num - 1; i++) vq->desc[i].next = cpu_to_le16(i + 1);
    vq->vq.callback = callback;
    vq->vq.name = name;
    vq->vq.index = index;
    vq->vq.num_free = num;
    vq->vq.num_max = num;
    if (!callback) vq->avail->flags = cpu_to_le16(VRING_AVAIL_F_NO_INTERRUPT);
    return vq;
}

/* ---- dma-bufs of virtio devices --------------------------------------------------------- */
struct dma_buf *virtio_dma_buf_export(const struct dma_buf_export_info *exp_info)
{
    const struct virtio_dma_buf_ops *ops = container_of(exp_info->ops, const struct virtio_dma_buf_ops, ops);
    if (!exp_info->ops || exp_info->ops->attach != &virtio_dma_buf_attach || !ops->get_uuid)
        return ERR_PTR(-EINVAL);
    return dma_buf_export(exp_info);
}
EXPORT_SYMBOL(virtio_dma_buf_export);

int virtio_dma_buf_attach(struct dma_buf *dma_buf, struct dma_buf_attachment *attach)
{
    const struct virtio_dma_buf_ops *ops = container_of(dma_buf->ops, const struct virtio_dma_buf_ops, ops);
    return ops->device_attach ? ops->device_attach(dma_buf, attach) : 0;
}
EXPORT_SYMBOL(virtio_dma_buf_attach);

bool is_virtio_dma_buf(struct dma_buf *dma_buf)
{
    return dma_buf->ops->attach == &virtio_dma_buf_attach;
}
EXPORT_SYMBOL(is_virtio_dma_buf);

int virtio_dma_buf_get_uuid(struct dma_buf *dma_buf, uuid_t *uuid)
{
    const struct virtio_dma_buf_ops *ops = container_of(dma_buf->ops, const struct virtio_dma_buf_ops, ops);
    if (!is_virtio_dma_buf(dma_buf)) return -EINVAL;
    return ops->get_uuid(dma_buf, uuid);
}
EXPORT_SYMBOL(virtio_dma_buf_get_uuid);

/* ---- virtio over PCI, the modern interface (spec 4.1) ------------------------------------ */
#define VIRTIO_PCI_VENDOR           0x1af4
#define VIRTIO_PCI_MODERN_BASE      0x1040          /* + the device type */

#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4

/* struct virtio_pci_cap: offsets in the vendor capability */
#define CAP_CFG_TYPE    3
#define CAP_BAR         4
#define CAP_OFFSET      8
#define CAP_LENGTH      12
#define CAP_NOTIFY_MULT 16

/* struct virtio_pci_common_cfg */
#define CC_DFSELECT     0
#define CC_DF           4
#define CC_GFSELECT     8
#define CC_GF           12
#define CC_MSIX         16
#define CC_NUMQ         18
#define CC_STATUS       20
#define CC_CFGGEN       21
#define CC_Q_SELECT     22
#define CC_Q_SIZE       24
#define CC_Q_MSIX       26
#define CC_Q_ENABLE     28
#define CC_Q_NOFF       30
#define CC_Q_DESCLO     32
#define CC_Q_DESCHI     36
#define CC_Q_AVAILLO    40
#define CC_Q_AVAILHI    44
#define CC_Q_USEDLO     48
#define CC_Q_USEDHI     52

#define VIRTIO_MSI_NO_VECTOR        0xffff
#define VIRTIO_PCI_ISR_CONFIG       0x2

struct vp_dev {
    struct virtio_device    vdev;
    struct pci_dev         *pci;
    void __iomem           *common;
    void __iomem           *isr;
    void __iomem           *device;
    size_t                  device_len;
    void __iomem           *notify_base;
    size_t                  notify_len;
    u32                     notify_mult;
    struct vp_vq          **vqs;
    unsigned int            nvqs;
    bool                    irq_on;
    u8                      config_gen;         /* the last generation seen (MSI-X) */
};

#define to_vp(_vdev) container_of(_vdev, struct vp_dev, vdev)

static void vp_get(struct virtio_device *vdev, unsigned int offset, void *buf, unsigned int len)
{
    struct vp_dev *vp = to_vp(vdev);
    u8 *p = buf;
    if (offset + len > vp->device_len) return;
    switch (len) {
    case 1: *p = ioread8(vp->device + offset); break;
    case 2: { u16 v = ioread16(vp->device + offset); memcpy(buf, &v, 2); break; }
    case 4: { u32 v = ioread32(vp->device + offset); memcpy(buf, &v, 4); break; }
    case 8: {
        u32 lo = ioread32(vp->device + offset), hi = ioread32(vp->device + offset + 4);
        memcpy(p, &lo, 4);
        memcpy(p + 4, &hi, 4);
        break;
    }
    default:
        for (unsigned int i = 0; i < len; i++) p[i] = ioread8(vp->device + offset + i);
    }
}

static void vp_set(struct virtio_device *vdev, unsigned int offset, const void *buf, unsigned int len)
{
    struct vp_dev *vp = to_vp(vdev);
    const u8 *p = buf;
    if (offset + len > vp->device_len) return;
    switch (len) {
    case 1: iowrite8(*p, vp->device + offset); break;
    case 2: { u16 v; memcpy(&v, buf, 2); iowrite16(v, vp->device + offset); break; }
    case 4: { u32 v; memcpy(&v, buf, 4); iowrite32(v, vp->device + offset); break; }
    case 8: {
        u32 lo, hi;
        memcpy(&lo, p, 4);
        memcpy(&hi, p + 4, 4);
        iowrite32(lo, vp->device + offset);
        iowrite32(hi, vp->device + offset + 4);
        break;
    }
    default:
        for (unsigned int i = 0; i < len; i++) iowrite8(p[i], vp->device + offset + i);
    }
}

static u32 vp_generation(struct virtio_device *vdev) { return ioread8(to_vp(vdev)->common + CC_CFGGEN); }
static u8 vp_get_status(struct virtio_device *vdev) { return ioread8(to_vp(vdev)->common + CC_STATUS); }

static void vp_set_status(struct virtio_device *vdev, u8 status)
{
    WARN_ON(status == 0);                               /* reset() does that */
    iowrite8(status, to_vp(vdev)->common + CC_STATUS);
}

static void vp_reset(struct virtio_device *vdev)
{
    struct vp_dev *vp = to_vp(vdev);
    iowrite8(0, vp->common + CC_STATUS);
    /* the reset is done when the device reads back 0 (spec 4.1.4.3.2) */
    for (int i = 0; i < 10000 && ioread8(vp->common + CC_STATUS); i++) udelay(100);
    if (vp->irq_on) synchronize_irq(vp->pci->irq);
}

static u64 vp_get_features(struct virtio_device *vdev)
{
    struct vp_dev *vp = to_vp(vdev);
    iowrite32(0, vp->common + CC_DFSELECT);
    u64 f = ioread32(vp->common + CC_DF);
    iowrite32(1, vp->common + CC_DFSELECT);
    return f | (u64)ioread32(vp->common + CC_DF) << 32;
}

static int vp_finalize_features(struct virtio_device *vdev)
{
    struct vp_dev *vp = to_vp(vdev);
    iowrite32(0, vp->common + CC_GFSELECT);
    iowrite32((u32)vdev->features, vp->common + CC_GF);
    iowrite32(1, vp->common + CC_GFSELECT);
    iowrite32((u32)(vdev->features >> 32), vp->common + CC_GF);
    return 0;
}

static const char *vp_bus_name(struct virtio_device *vdev) { return pci_name(to_vp(vdev)->pci); }

static irqreturn_t vp_interrupt(int irq, void *opaque)
{
    struct vp_dev *vp = opaque;
    irqreturn_t ret = IRQ_NONE;
    u8 isr = ioread8(vp->isr);                          /* reading clears it */
    bool config = isr & VIRTIO_PCI_ISR_CONFIG;

    if (vp->pci->msix_enabled) {
        u8 gen = ioread8(vp->common + CC_CFGGEN);
        if (gen != vp->config_gen) {
            vp->config_gen = gen;
            config = true;
        }
    }
    if (config) {
        virtio_config_changed(&vp->vdev);
        ret = IRQ_HANDLED;
    }
    for (unsigned int i = 0; i < vp->nvqs; i++)
        if (vp->vqs[i] && vring_interrupt(vp->vqs[i])) ret = IRQ_HANDLED;
    return vp->pci->msix_enabled ? IRQ_HANDLED : ret;
}

static void vp_del_vqs(struct virtio_device *vdev)
{
    struct vp_dev *vp = to_vp(vdev);

    if (vp->irq_on) {
        free_irq(vp->pci->irq, vp);
        pci_free_irq_vectors(vp->pci);
        vp->irq_on = false;
    }
    for (unsigned int i = 0; i < vp->nvqs; i++) {
        struct vp_vq *vq = vp->vqs[i];
        if (!vq) continue;
        spin_lock(&vdev->vqs_list_lock);
        list_del(&vq->vq.list);
        spin_unlock(&vdev->vqs_list_lock);
        vring_free(vq);
    }
    kfree(vp->vqs);
    vp->vqs = NULL;
    vp->nvqs = 0;
}

static int vp_setup_vq(struct vp_dev *vp, unsigned int index, struct virtqueue_info *info)
{
    struct vp_vq *vq;
    u16 num, off;

    iowrite16(index, vp->common + CC_Q_SELECT);
    num = ioread16(vp->common + CC_Q_SIZE);
    if (!num || ioread16(vp->common + CC_Q_ENABLE)) return -ENOENT;
    if (num & (num - 1)) return -EINVAL;                /* a split ring's size is a power of 2 */
    if (num > VQ_MAX_SIZE) num = VQ_MAX_SIZE;
    vq = vring_new(&vp->pci->dev, index, num, info->name, info->callback);
    if (!vq) return -ENOMEM;
    vq->vq.vdev = &vp->vdev;

    off = ioread16(vp->common + CC_Q_NOFF);
    if ((size_t)off * vp->notify_mult + 2 > vp->notify_len) {
        dev_err(&vp->pci->dev, "virtio: queue %u notifies outside its window\n", index);
        vring_free(vq);
        return -EINVAL;
    }
    vq->notify = vp->notify_base + (size_t)off * vp->notify_mult;

    iowrite16(num, vp->common + CC_Q_SIZE);
    iowrite32((u32)vq->desc_dma, vp->common + CC_Q_DESCLO);
    iowrite32((u32)((u64)vq->desc_dma >> 32), vp->common + CC_Q_DESCHI);
    iowrite32((u32)vq->avail_dma, vp->common + CC_Q_AVAILLO);
    iowrite32((u32)((u64)vq->avail_dma >> 32), vp->common + CC_Q_AVAILHI);
    iowrite32((u32)vq->used_dma, vp->common + CC_Q_USEDLO);
    iowrite32((u32)((u64)vq->used_dma >> 32), vp->common + CC_Q_USEDHI);
    if (vp->pci->msix_enabled) {
        iowrite16(0, vp->common + CC_Q_MSIX);
        if (ioread16(vp->common + CC_Q_MSIX) == VIRTIO_MSI_NO_VECTOR) {
            vring_free(vq);
            return -EBUSY;
        }
    }
    iowrite16(1, vp->common + CC_Q_ENABLE);

    vp->vqs[index] = vq;
    spin_lock(&vp->vdev.vqs_list_lock);
    list_add_tail(&vq->vq.list, &vp->vdev.vqs);
    spin_unlock(&vp->vdev.vqs_list_lock);
    return 0;
}

static int vp_find_vqs(struct virtio_device *vdev, unsigned int nvqs, struct virtqueue *vqs[],
                       struct virtqueue_info vqs_info[], struct irq_affinity *desc)
{
    struct vp_dev *vp = to_vp(vdev);
    unsigned int i;
    int err;

    if (nvqs > ioread16(vp->common + CC_NUMQ)) return -ENOENT;
    vp->vqs = kcalloc(nvqs, sizeof(*vp->vqs), GFP_KERNEL);
    if (!vp->vqs) return -ENOMEM;
    vp->nvqs = nvqs;

    err = pci_alloc_irq_vectors(vp->pci, 1, 1, PCI_IRQ_MSIX | PCI_IRQ_MSI);
    if (err < 0) goto fail;
    err = request_irq(pci_irq_vector(vp->pci, 0), vp_interrupt, 0, dev_name(&vdev->dev), vp);
    if (err) {
        pci_free_irq_vectors(vp->pci);
        goto fail;
    }
    vp->irq_on = true;
    if (vp->pci->msix_enabled) {
        vp->config_gen = ioread8(vp->common + CC_CFGGEN);
        iowrite16(0, vp->common + CC_MSIX);
        if (ioread16(vp->common + CC_MSIX) == VIRTIO_MSI_NO_VECTOR) {
            err = -EBUSY;
            goto fail;
        }
    }

    for (i = 0; i < nvqs; i++) {
        if (!vqs_info[i].name) {                        /* an unused slot */
            vqs[i] = NULL;
            continue;
        }
        err = vp_setup_vq(vp, i, &vqs_info[i]);
        if (err) goto fail;
        vqs[i] = &vp->vqs[i]->vq;
    }
    return 0;
fail:
    vp_del_vqs(vdev);
    return err;
}

static bool vp_get_shm_region(struct virtio_device *vdev, struct virtio_shm_region *region, u8 id)
{
    return false;                                       /* see the top of this file */
}

static const struct virtio_config_ops vp_config_ops = {
    .get                = vp_get,
    .set                = vp_set,
    .generation         = vp_generation,
    .get_status         = vp_get_status,
    .set_status         = vp_set_status,
    .reset              = vp_reset,
    .find_vqs           = vp_find_vqs,
    .del_vqs            = vp_del_vqs,
    .get_features       = vp_get_features,
    .finalize_features  = vp_finalize_features,
    .bus_name           = vp_bus_name,
    .get_shm_region     = vp_get_shm_region,
};

/* The first capability of `type` whose BAR is a memory BAR: its mapping
 * (at least `minlen` long), or NULL. */
static void __iomem *vp_map_cap(struct pci_dev *pdev, u8 type, size_t minlen, size_t *len, u8 *at)
{
    for (u8 pos = pci_find_capability(pdev, PCI_CAP_ID_VNDR); pos;
         pos = pci_find_next_capability(pdev, pos, PCI_CAP_ID_VNDR)) {
        u8 cfg_type, bar;
        u32 offset, length;
        pci_read_config_byte(pdev, pos + CAP_CFG_TYPE, &cfg_type);
        pci_read_config_byte(pdev, pos + CAP_BAR, &bar);
        if (cfg_type != type || bar > 5 || !(pci_resource_flags(pdev, bar) & IORESOURCE_MEM)) continue;
        pci_read_config_dword(pdev, pos + CAP_OFFSET, &offset);
        pci_read_config_dword(pdev, pos + CAP_LENGTH, &length);
        if (length < minlen || (u64)offset + length > pci_resource_len(pdev, bar)) continue;
        if (len) *len = length;
        if (at) *at = pos;
        return pci_iomap_range(pdev, bar, offset, length);
    }
    return NULL;
}

static void vp_release_dev(struct device *_d)
{
    kfree(to_vp(dev_to_virtio(_d)));
}

static int vp_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct vp_dev *vp;
    u8 notify_cap;
    int err;

    err = pci_enable_device(pdev);
    if (err) return err;
    vp = kzalloc(sizeof(*vp), GFP_KERNEL);
    if (!vp) {
        err = -ENOMEM;
        goto err_disable;
    }
    vp->pci = pdev;
    vp->common = vp_map_cap(pdev, VIRTIO_PCI_CAP_COMMON_CFG, 56, NULL, NULL);
    vp->isr = vp_map_cap(pdev, VIRTIO_PCI_CAP_ISR_CFG, 1, NULL, NULL);
    vp->notify_base = vp_map_cap(pdev, VIRTIO_PCI_CAP_NOTIFY_CFG, 2, &vp->notify_len, &notify_cap);
    vp->device = vp_map_cap(pdev, VIRTIO_PCI_CAP_DEVICE_CFG, 0, &vp->device_len, NULL);
    if (!vp->common || !vp->isr || !vp->notify_base) {
        dev_err(&pdev->dev, "virtio: no modern interface (common, ISR and notify capabilities)\n");
        kfree(vp);
        err = -ENODEV;
        goto err_disable;
    }
    pci_read_config_dword(pdev, notify_cap + CAP_NOTIFY_MULT, &vp->notify_mult);
    pci_set_master(pdev);
    /* the rings and the buffers a driver hands over may be anywhere in memory */
    err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (err) {
        kfree(vp);
        goto err_disable;
    }

    vp->vdev.id.device = pdev->device - VIRTIO_PCI_MODERN_BASE;
    vp->vdev.id.vendor = pdev->subsystem_vendor;
    vp->vdev.config = &vp_config_ops;
    vp->vdev.dev.parent = &pdev->dev;
    vp->vdev.dev.release = vp_release_dev;
    pci_set_drvdata(pdev, vp);
    err = register_virtio_device(&vp->vdev);
    if (err) {
        put_device(&vp->vdev.dev);
        goto err_disable;
    }
    return 0;
err_disable:
    pci_disable_device(pdev);
    return err;
}

static void vp_remove(struct pci_dev *pdev)
{
    struct vp_dev *vp = pci_get_drvdata(pdev);
    unregister_virtio_device(&vp->vdev);
    pci_disable_device(pdev);
}

static const struct pci_device_id vp_ids[] = {
    { PCI_DEVICE(VIRTIO_PCI_VENDOR, VIRTIO_PCI_MODERN_BASE + VIRTIO_ID_GPU) },
    { }
};

static struct pci_driver vp_driver = {
    .name       = "virtio-pci",
    .id_table   = vp_ids,
    .probe      = vp_probe,
    .remove     = vp_remove,
};

/* after the drivers' module_init: a device found here binds at once */
static int __init vp_init(void) { return pci_register_driver(&vp_driver); }
late_initcall(vp_init);
