/* drivers/virtio/virtio_blk.h -- virtio block devices (see virtio_blk.c) */
#ifndef KESTREL_VIRTIO_BLK_H
#define KESTREL_VIRTIO_BLK_H

void virtio_blk_init(void);             /* storage_init(): find and start the devices */
void virtio_blk_register(void);         /* storage_register_devices(): "vdN" block devices */
void virtio_blk_enable_irqs(void);      /* storage_enable_irqs(): MSI-X completion */
int  virtio_blk_count(void);

#endif
