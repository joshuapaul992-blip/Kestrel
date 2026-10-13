/* linux/dma-fence-array.h -- Kestrel's Linux KPI: a fence for several fences (linuxkpi/src/dma-fence.c)
 *
 * It signals when all of its fences have signaled, or when any one has if
 * it was made with signal_on_any. It owns the fences and the array. */
#ifndef _LKPI_LINUX_DMA_FENCE_ARRAY_H
#define _LKPI_LINUX_DMA_FENCE_ARRAY_H

#include <linux/dma-fence.h>
#include <linux/workqueue.h>

struct dma_fence_array;
struct dma_fence_array_cb {
    struct dma_fence_cb     cb;
    struct dma_fence_array *array;
};

struct dma_fence_array {
    struct dma_fence    base;
    spinlock_t          lock;
    unsigned            num_fences;
    atomic_t            num_pending;
    bool                signal_on_any;
    struct dma_fence  **fences;
    struct work_struct  work;               /* signals base outside the callback */
    struct dma_fence_array_cb callbacks[];
};

extern const struct dma_fence_ops dma_fence_array_ops;

static inline struct dma_fence_array *to_dma_fence_array(struct dma_fence *fence)
{
    if (!fence || fence->ops != &dma_fence_array_ops) return NULL;
    return container_of(fence, struct dma_fence_array, base);
}

#define dma_fence_array_for_each(fence, index, head)                            \
    for (index = 0, fence = dma_fence_array_first(head); fence;                 \
         ++(index), fence = dma_fence_array_next(head, index))

struct dma_fence_array *dma_fence_array_create(int num_fences, struct dma_fence **fences,
                                               u64 context, unsigned seqno, bool signal_on_any);
bool dma_fence_match_context(struct dma_fence *fence, u64 context);
struct dma_fence *dma_fence_array_first(struct dma_fence *head);
struct dma_fence *dma_fence_array_next(struct dma_fence *head, unsigned int index);

#endif
