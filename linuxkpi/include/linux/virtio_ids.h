/* linux/virtio_ids.h -- Kestrel's Linux KPI: virtio device types (the
 * virtio 1.x specification, "Device Types") */
#ifndef _LKPI_LINUX_VIRTIO_IDS_H
#define _LKPI_LINUX_VIRTIO_IDS_H

#define VIRTIO_ID_NET           1
#define VIRTIO_ID_BLOCK         2
#define VIRTIO_ID_CONSOLE       3
#define VIRTIO_ID_RNG           4
#define VIRTIO_ID_BALLOON       5
#define VIRTIO_ID_SCSI          8
#define VIRTIO_ID_9P            9
#define VIRTIO_ID_GPU           16
#define VIRTIO_ID_INPUT         18
#define VIRTIO_ID_VSOCK         19
#define VIRTIO_ID_CRYPTO        20
#define VIRTIO_ID_SOUND         25
#define VIRTIO_ID_FS            26

#endif
