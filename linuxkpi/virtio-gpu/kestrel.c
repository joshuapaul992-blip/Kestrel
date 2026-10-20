/* linuxkpi/virtio-gpu/kestrel.c -- what virtio-gpu's virtgpu_vram.c gives
 * the rest of the driver, for a device without host-visible memory
 *
 * Linux's virtgpu_vram.c (GPL-2.0-only) maps blob resources that live in
 * the device's shared memory window. The KPI's virtio-pci transport offers
 * no shared memory regions (linuxkpi/src/virtio.c), so the driver never
 * sets has_host_visible and refuses mappable host blobs itself; a host
 * blob that is not mappable gets -ENODEV here (QEMU offers blobs only
 * without virgl, which uses guest memory). No object is ever a "vram"
 * object. */
#include "virtgpu_drv.h"

bool virtio_gpu_is_vram(struct virtio_gpu_object *bo)
{
    return false;
}

int virtio_gpu_vram_create(struct virtio_gpu_device *vgdev, struct virtio_gpu_object_params *params,
                           struct virtio_gpu_object **bo_ptr)
{
    return -ENODEV;
}

struct sg_table *virtio_gpu_vram_map_dma_buf(struct virtio_gpu_object *bo, struct device *dev,
                                             enum dma_data_direction dir)
{
    return ERR_PTR(-ENODEV);
}

void virtio_gpu_vram_unmap_dma_buf(struct device *dev, struct sg_table *sgt, enum dma_data_direction dir)
{
}
