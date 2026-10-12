/* linux/dma-fence-chain.h -- Kestrel's Linux KPI: fences in a chain (linuxkpi/src/dma-fence.c)
 *
 * A chain node wraps one fence and points at the previous node; it signals
 * when its own fence and every earlier node have signaled. */
#ifndef _LKPI_LINUX_DMA_FENCE_CHAIN_H
#define _LKPI_LINUX_DMA_FENCE_CHAIN_H

#include <linux/dma-fence.h>
#include <linux/slab.h>

struct dma_fence_chain {
    struct dma_fence        base;
    struct dma_fence __rcu *prev;
    u64                     prev_seqno;
    struct dma_fence       *fence;
    struct dma_fence_cb     cb;
    spinlock_t              lock;
};

extern const struct dma_fence_ops dma_fence_chain_ops;

static inline struct dma_fence_chain *to_dma_fence_chain(struct dma_fence *fence)
{
    if (!fence || fence->ops != &dma_fence_chain_ops) return NULL;
    return container_of(fence, struct dma_fence_chain, base);
}
static inline struct dma_fence *dma_fence_chain_contained(struct dma_fence *fence)
{
    struct dma_fence_chain *chain = to_dma_fence_chain(fence);
    return chain ? chain->fence : fence;
}
static inline struct dma_fence_chain *dma_fence_chain_alloc(void)
{
    return kmalloc(sizeof(struct dma_fence_chain), GFP_KERNEL);
}
static inline void dma_fence_chain_free(struct dma_fence_chain *chain) { kfree(chain); }

#define dma_fence_chain_for_each(iter, head)                                    \
    for (iter = dma_fence_get(head); iter; iter = dma_fence_chain_walk(iter))

struct dma_fence *dma_fence_chain_walk(struct dma_fence *fence);
int dma_fence_chain_find_seqno(struct dma_fence **pfence, uint64_t seqno);
void dma_fence_chain_init(struct dma_fence_chain *chain, struct dma_fence *prev,
                          struct dma_fence *fence, uint64_t seqno);

#endif
