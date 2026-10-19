/* linux/dma-fence-unwrap.h -- Kestrel's Linux KPI: the fences inside
 * containers (linuxkpi/src/dma-fence.c)
 *
 * A fence may be a chain node or an array, or a chain of arrays; walking
 * one with dma_fence_unwrap_for_each() visits the plain fences inside, and
 * dma_fence_unwrap_merge() makes one fence of the unsignaled ones, the
 * latest of each context. */
#ifndef _LKPI_LINUX_DMA_FENCE_UNWRAP_H
#define _LKPI_LINUX_DMA_FENCE_UNWRAP_H

#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>

struct dma_fence_unwrap {
    struct dma_fence *chain;            /* the chain node being walked (a reference) */
    struct dma_fence *array;            /* the array inside it, or the plain fence */
    unsigned int      index;
};

struct dma_fence *dma_fence_unwrap_first(struct dma_fence *head, struct dma_fence_unwrap *cursor);
struct dma_fence *dma_fence_unwrap_next(struct dma_fence_unwrap *cursor);

#define dma_fence_unwrap_for_each(fence, cursor, head)                          \
    for (fence = dma_fence_unwrap_first(head, cursor); fence;                   \
         fence = dma_fence_unwrap_next(cursor))

struct dma_fence *__dma_fence_unwrap_merge(unsigned int num_fences, struct dma_fence **fences,
                                           struct dma_fence_unwrap *cursors);

#define dma_fence_unwrap_merge(...)                                             \
    ({                                                                          \
        struct dma_fence *__f[] = { __VA_ARGS__ };                              \
        struct dma_fence_unwrap __c[ARRAY_SIZE(__f)];                           \
        __dma_fence_unwrap_merge(ARRAY_SIZE(__f), __f, __c);                    \
    })

#endif
