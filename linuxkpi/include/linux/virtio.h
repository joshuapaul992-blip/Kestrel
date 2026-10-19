/* linux/virtio.h -- Kestrel's Linux KPI: virtio devices, drivers and queues
 * (linuxkpi/src/virtio.c)
 *
 * A virtio device sits on the "virtio" bus under its transport's device
 * (a PCI function: the KPI drives the modern virtio-pci interface). Its
 * queues are split virtqueues; the driver feeds them scatter-gather lists
 * and gets its tokens back from virtqueue_get_buf(). Written from the
 * virtio 1.x specification. */
#ifndef _LKPI_LINUX_VIRTIO_H
#define _LKPI_LINUX_VIRTIO_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/gfp_types.h>
#include <linux/scatterlist.h>

struct virtio_device;
struct virtio_config_ops;

struct virtqueue {
    struct list_head        list;
    void                  (*callback)(struct virtqueue *vq);
    const char             *name;
    struct virtio_device   *vdev;
    unsigned int            index;
    unsigned int            num_free;
    unsigned int            num_max;
    bool                    reset;
    void                   *priv;
};

int  virtqueue_add_sgs(struct virtqueue *vq, struct scatterlist *sgs[], unsigned int out_sgs,
                       unsigned int in_sgs, void *data, gfp_t gfp);
int  virtqueue_add_outbuf(struct virtqueue *vq, struct scatterlist sg[], unsigned int num, void *data, gfp_t gfp);
int  virtqueue_add_inbuf(struct virtqueue *vq, struct scatterlist sg[], unsigned int num, void *data, gfp_t gfp);
bool virtqueue_kick(struct virtqueue *vq);
bool virtqueue_kick_prepare(struct virtqueue *vq);
bool virtqueue_notify(struct virtqueue *vq);
void *virtqueue_get_buf(struct virtqueue *vq, unsigned int *len);
void virtqueue_disable_cb(struct virtqueue *vq);
bool virtqueue_enable_cb(struct virtqueue *vq);
bool virtqueue_poll(struct virtqueue *vq, unsigned int last_used_idx);
unsigned int virtqueue_enable_cb_prepare(struct virtqueue *vq);
void *virtqueue_detach_unused_buf(struct virtqueue *vq);
unsigned int virtqueue_get_vring_size(const struct virtqueue *vq);
bool virtqueue_is_broken(const struct virtqueue *vq);

struct virtio_device {
    int                             index;
    bool                            failed;
    bool                            config_core_enabled;
    bool                            config_driver_disabled;
    bool                            config_change_pending;
    spinlock_t                      config_lock;
    spinlock_t                      vqs_list_lock;
    struct device                   dev;
    struct virtio_device_id         id;
    const struct virtio_config_ops *config;
    struct list_head                vqs;
    u64                             features;
    void                           *priv;
};

#define dev_to_virtio(_dev)     container_of_const(_dev, struct virtio_device, dev)

int  register_virtio_device(struct virtio_device *dev);
void unregister_virtio_device(struct virtio_device *dev);
bool is_virtio_device(struct device *dev);
void virtio_break_device(struct virtio_device *dev);
void virtio_config_changed(struct virtio_device *dev);
void virtio_reset_device(struct virtio_device *dev);

#define virtio_device_for_each_vq(vdev, vq) \
    list_for_each_entry(vq, &(vdev)->vqs, list)

struct virtio_driver {
    struct device_driver            driver;
    const struct virtio_device_id  *id_table;
    const unsigned int             *feature_table;
    unsigned int                    feature_table_size;
    const unsigned int             *feature_table_legacy;
    unsigned int                    feature_table_size_legacy;
    int  (*validate)(struct virtio_device *dev);
    int  (*probe)(struct virtio_device *dev);
    void (*scan)(struct virtio_device *dev);
    void (*remove)(struct virtio_device *dev);
    void (*config_changed)(struct virtio_device *dev);
};

#define drv_to_virtio(__drv)    container_of_const(__drv, struct virtio_driver, driver)

int  __register_virtio_driver(struct virtio_driver *drv, struct module *owner);
#define register_virtio_driver(drv)     __register_virtio_driver(drv, THIS_MODULE)
void unregister_virtio_driver(struct virtio_driver *drv);

#define module_virtio_driver(__virtio_driver) \
    module_driver(__virtio_driver, register_virtio_driver, unregister_virtio_driver)

#endif
