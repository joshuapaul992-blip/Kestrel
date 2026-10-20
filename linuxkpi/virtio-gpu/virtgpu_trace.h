/* linuxkpi/virtio-gpu/virtgpu_trace.h -- virtio-gpu's two trace points,
 * which the KPI does not record (Linux's header is GPL-2.0-only) */
#ifndef _LKPI_VIRTGPU_TRACE_H_
#define _LKPI_VIRTGPU_TRACE_H_

struct virtqueue;
struct virtio_gpu_ctrl_hdr;

static inline void trace_virtio_gpu_cmd_queue(struct virtqueue *vq, struct virtio_gpu_ctrl_hdr *hdr, u32 seqno) { }
static inline void trace_virtio_gpu_cmd_response(struct virtqueue *vq, struct virtio_gpu_ctrl_hdr *hdr, u32 seqno) { }

#endif
