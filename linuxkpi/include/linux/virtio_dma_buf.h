/* linux/virtio_dma_buf.h -- Kestrel's Linux KPI: dma-bufs a virtio device
 * exports, which other virtio devices can name by UUID (linuxkpi/src/virtio.c) */
#ifndef _LKPI_LINUX_VIRTIO_DMA_BUF_H
#define _LKPI_LINUX_VIRTIO_DMA_BUF_H

#include <linux/dma-buf.h>
#include <linux/uuid.h>

struct virtio_dma_buf_ops {
    struct dma_buf_ops ops;                     /* first: its attach is virtio_dma_buf_attach */
    int (*device_attach)(struct dma_buf *dma_buf, struct dma_buf_attachment *attach);
    int (*get_uuid)(struct dma_buf *dma_buf, uuid_t *uuid);
};

struct dma_buf *virtio_dma_buf_export(const struct dma_buf_export_info *exp_info);
int  virtio_dma_buf_attach(struct dma_buf *dma_buf, struct dma_buf_attachment *attach);
bool is_virtio_dma_buf(struct dma_buf *dma_buf);
int  virtio_dma_buf_get_uuid(struct dma_buf *dma_buf, uuid_t *uuid);

#endif
