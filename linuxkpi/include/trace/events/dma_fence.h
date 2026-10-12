/* trace/events/dma_fence.h -- Kestrel's Linux KPI: dma-fence tracepoints (none: Kestrel has no tracing) */
#ifndef _LKPI_TRACE_EVENTS_DMA_FENCE_H
#define _LKPI_TRACE_EVENTS_DMA_FENCE_H

#include <linux/tracepoint.h>

struct dma_fence;

DECLARE_TRACE(dma_fence_emit, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_init, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_destroy, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_enable_signal, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_signaled, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_wait_start, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))
DECLARE_TRACE(dma_fence_wait_end, TP_PROTO(struct dma_fence *fence), TP_ARGS(fence))

#endif
