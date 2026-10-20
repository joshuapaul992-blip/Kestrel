/* linux/virtio_config.h -- Kestrel's Linux KPI: a virtio device's
 * configuration, features and status (linuxkpi/src/virtio.c)
 *
 * The constants are the virtio 1.x specification's. The KPI's transport is
 * modern virtio-pci on x86, little endian like the devices' fields. */
#ifndef _LKPI_LINUX_VIRTIO_CONFIG_H
#define _LKPI_LINUX_VIRTIO_CONFIG_H

#include <linux/types.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/stddef.h>
#include <linux/virtio.h>

/* device status */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1
#define VIRTIO_CONFIG_S_DRIVER          2
#define VIRTIO_CONFIG_S_DRIVER_OK       4
#define VIRTIO_CONFIG_S_FEATURES_OK     8
#define VIRTIO_CONFIG_S_NEEDS_RESET     0x40
#define VIRTIO_CONFIG_S_FAILED          0x80

/* feature bits of the transport and the rings */
#define VIRTIO_TRANSPORT_F_START        28
#define VIRTIO_TRANSPORT_F_END          42
#define VIRTIO_F_NOTIFY_ON_EMPTY        24
#define VIRTIO_F_ANY_LAYOUT             27
#define VIRTIO_RING_F_INDIRECT_DESC     28
#define VIRTIO_RING_F_EVENT_IDX         29
#define VIRTIO_F_VERSION_1              32
#define VIRTIO_F_ACCESS_PLATFORM        33
#define VIRTIO_F_IOMMU_PLATFORM         VIRTIO_F_ACCESS_PLATFORM
#define VIRTIO_F_RING_PACKED            34
#define VIRTIO_F_IN_ORDER               35
#define VIRTIO_F_ORDER_PLATFORM         36
#define VIRTIO_F_SR_IOV                 37
#define VIRTIO_F_NOTIFICATION_DATA      38
#define VIRTIO_F_RING_RESET             40

typedef void vq_callback_t(struct virtqueue *);

struct virtqueue_info {
    const char      *name;
    vq_callback_t   *callback;
    bool             ctx;
};

struct virtio_shm_region {
    u64 addr;
    u64 len;
};

struct irq_affinity;
struct cpumask;

struct virtio_config_ops {
    void  (*get)(struct virtio_device *vdev, unsigned int offset, void *buf, unsigned int len);
    void  (*set)(struct virtio_device *vdev, unsigned int offset, const void *buf, unsigned int len);
    u32   (*generation)(struct virtio_device *vdev);
    u8    (*get_status)(struct virtio_device *vdev);
    void  (*set_status)(struct virtio_device *vdev, u8 status);
    void  (*reset)(struct virtio_device *vdev);
    int   (*find_vqs)(struct virtio_device *vdev, unsigned int nvqs, struct virtqueue *vqs[],
                      struct virtqueue_info vqs_info[], struct irq_affinity *desc);
    void  (*del_vqs)(struct virtio_device *vdev);
    void  (*synchronize_cbs)(struct virtio_device *vdev);
    u64   (*get_features)(struct virtio_device *vdev);
    int   (*finalize_features)(struct virtio_device *vdev);
    const char *(*bus_name)(struct virtio_device *vdev);
    int   (*set_vq_affinity)(struct virtqueue *vq, const struct cpumask *cpu_mask);
    const struct cpumask *(*get_vq_affinity)(struct virtio_device *vdev, int index);
    bool  (*get_shm_region)(struct virtio_device *vdev, struct virtio_shm_region *region, u8 id);
    int   (*disable_vq_and_reset)(struct virtqueue *vq);
    int   (*enable_vq_after_reset)(struct virtqueue *vq);
};

static inline bool __virtio_test_bit(const struct virtio_device *vdev, unsigned int fbit)
{
    return fbit < 64 && (vdev->features & (1ULL << fbit));
}
static inline void __virtio_set_bit(struct virtio_device *vdev, unsigned int fbit) { vdev->features |= 1ULL << fbit; }
static inline void __virtio_clear_bit(struct virtio_device *vdev, unsigned int fbit) { vdev->features &= ~(1ULL << fbit); }

static inline bool virtio_has_feature(const struct virtio_device *vdev, unsigned int fbit)
{
    return __virtio_test_bit(vdev, fbit);
}

/* the device uses physical addresses: no IOMMU between it and memory */
static inline bool virtio_has_dma_quirk(const struct virtio_device *vdev)
{
    return !virtio_has_feature(vdev, VIRTIO_F_ACCESS_PLATFORM);
}

static inline int virtio_find_vqs(struct virtio_device *vdev, unsigned int nvqs, struct virtqueue *vqs[],
                                  struct virtqueue_info vqs_info[], struct irq_affinity *desc)
{
    return vdev->config->find_vqs(vdev, nvqs, vqs, vqs_info, desc);
}

static inline struct virtqueue *virtio_find_single_vq(struct virtio_device *vdev, vq_callback_t *c, const char *n)
{
    struct virtqueue_info info = { n, c, false };
    struct virtqueue *vq;
    int err = virtio_find_vqs(vdev, 1, &vq, &info, NULL);
    return err ? ERR_PTR(err) : vq;
}

static inline bool virtio_get_shm_region(struct virtio_device *vdev, struct virtio_shm_region *region, u8 id)
{
    return vdev->config->get_shm_region ? vdev->config->get_shm_region(vdev, region, id) : false;
}

void virtio_device_ready(struct virtio_device *dev);
void virtio_add_status(struct virtio_device *dev, unsigned int status);

static inline bool virtio_is_little_endian(struct virtio_device *vdev) { return true; }

/* Configuration fields: read again while the device changed them meanwhile */
static inline void __virtio_cread_many(struct virtio_device *vdev, unsigned int offset, void *buf,
                                       size_t count, size_t bytes)
{
    u32 old, gen = vdev->config->generation ? vdev->config->generation(vdev) : 0;
    do {
        old = gen;
        for (size_t i = 0; i < count; i++)
            vdev->config->get(vdev, offset + bytes * i, (char *)buf + i * bytes, bytes);
        gen = vdev->config->generation ? vdev->config->generation(vdev) : 0;
    } while (gen != old);
}

static inline void virtio_cread_bytes(struct virtio_device *vdev, unsigned int offset, void *buf, size_t len)
{
    __virtio_cread_many(vdev, offset, buf, len, 1);
}

#define virtio_cread_le(vdev, structname, member, ptr)                                  \
    do {                                                                                \
        typeof(((structname *)0)->member) virtio_cread_v;                               \
        __virtio_cread_many(vdev, offsetof(structname, member), &virtio_cread_v, 1,     \
                            sizeof(virtio_cread_v));                                    \
        *(ptr) = (__force typeof(*(ptr)))virtio_cread_v;                                \
    } while (0)

#define virtio_cwrite_le(vdev, structname, member, ptr)                                 \
    do {                                                                                \
        typeof(((structname *)0)->member) virtio_cwrite_v = (__force typeof(virtio_cwrite_v))*(ptr); \
        (vdev)->config->set(vdev, offsetof(structname, member), &virtio_cwrite_v,      \
                            sizeof(virtio_cwrite_v));                                   \
    } while (0)

#define virtio_cread(vdev, structname, member, ptr)     virtio_cread_le(vdev, structname, member, ptr)
#define virtio_cwrite(vdev, structname, member, ptr)    virtio_cwrite_le(vdev, structname, member, ptr)

static inline u8 virtio_cread8(struct virtio_device *vdev, unsigned int offset)
{
    u8 v;
    vdev->config->get(vdev, offset, &v, 1);
    return v;
}
static inline u32 virtio_cread32(struct virtio_device *vdev, unsigned int offset)
{
    u32 v;
    __virtio_cread_many(vdev, offset, &v, 1, 4);
    return v;
}
static inline void virtio_cwrite32(struct virtio_device *vdev, unsigned int offset, u32 val)
{
    vdev->config->set(vdev, offset, &val, 4);
}

#endif
