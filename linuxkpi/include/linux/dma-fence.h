/* linux/dma-fence.h -- Kestrel's Linux KPI: fences (linuxkpi/src/dma-fence.c)
 *
 * A fence is a one-shot event: it starts unsignaled and is signaled once,
 * usually when a GPU finishes some work. It is reference counted and
 * belongs to a timeline (context); seqno orders the fences of a timeline.
 * Waiters either sleep in dma_fence_wait*() or register a callback that runs
 * at signal time, with the fence's lock held and interrupts off.
 *
 * The provider's ops say how: enable_signaling() asks it to signal from
 * an interrupt from now on, signaled() polls the hardware. */
#ifndef _LKPI_LINUX_DMA_FENCE_H
#define _LKPI_LINUX_DMA_FENCE_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/wordpart.h>

struct dma_fence;
struct dma_fence_ops;
struct dma_fence_cb;
struct seq_file;

struct dma_fence {
    spinlock_t                 *lock;
    const struct dma_fence_ops *ops;
    struct list_head            cb_list;    /* callbacks until signaled */
    ktime_t                     timestamp;  /* when it was signaled */
    struct rcu_head             rcu;
    u64                         context;
    u64                         seqno;
    unsigned long               flags;      /* DMA_FENCE_FLAG_* bits */
    struct kref                 refcount;
    int                         error;
};

enum dma_fence_flag_bits {
    DMA_FENCE_FLAG_SEQNO64_BIT,
    DMA_FENCE_FLAG_SIGNALED_BIT,
    DMA_FENCE_FLAG_TIMESTAMP_BIT,
    DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
    DMA_FENCE_FLAG_USER_BITS,               /* the first bit the provider may use */
};

typedef void (*dma_fence_func_t)(struct dma_fence *fence, struct dma_fence_cb *cb);

struct dma_fence_cb {
    struct list_head    node;
    dma_fence_func_t    func;
};

struct dma_fence_ops {
    bool use_64bit_seqno;
    const char *(*get_driver_name)(struct dma_fence *fence);
    const char *(*get_timeline_name)(struct dma_fence *fence);
    bool (*enable_signaling)(struct dma_fence *fence);  /* false: already signaled */
    bool (*signaled)(struct dma_fence *fence);
    signed long (*wait)(struct dma_fence *fence, bool intr, signed long timeout);
    void (*release)(struct dma_fence *fence);
    void (*fence_value_str)(struct dma_fence *fence, char *str, int size);
    void (*timeline_value_str)(struct dma_fence *fence, char *str, int size);
    void (*set_deadline)(struct dma_fence *fence, ktime_t deadline);
};

void dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
                    spinlock_t *lock, u64 context, u64 seqno);
void dma_fence_release(struct kref *kref);
void dma_fence_free(struct dma_fence *fence);
void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq);

static inline void dma_fence_put(struct dma_fence *fence)
{
    if (fence) kref_put(&fence->refcount, dma_fence_release);
}
static inline struct dma_fence *dma_fence_get(struct dma_fence *fence)
{
    if (fence) kref_get(&fence->refcount);
    return fence;
}
static inline struct dma_fence *dma_fence_get_rcu(struct dma_fence *fence)
{
    return kref_get_unless_zero(&fence->refcount) ? fence : NULL;
}
/* a counted reference to the fence *fencep points at, read under RCU */
static inline struct dma_fence *dma_fence_get_rcu_safe(struct dma_fence __rcu **fencep)
{
    for (;;) {
        struct dma_fence *fence = rcu_dereference(*fencep);
        if (!fence || !dma_fence_get_rcu(fence)) return NULL;
        if (fence == rcu_access_pointer(*fencep)) return fence;
        dma_fence_put(fence);
    }
}

int  dma_fence_signal(struct dma_fence *fence);
int  dma_fence_signal_locked(struct dma_fence *fence);
int  dma_fence_signal_timestamp(struct dma_fence *fence, ktime_t timestamp);
int  dma_fence_signal_timestamp_locked(struct dma_fence *fence, ktime_t timestamp);
signed long dma_fence_default_wait(struct dma_fence *fence, bool intr, signed long timeout);
int  dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb, dma_fence_func_t func);
bool dma_fence_remove_callback(struct dma_fence *fence, struct dma_fence_cb *cb);
void dma_fence_enable_sw_signaling(struct dma_fence *fence);
int  dma_fence_get_status(struct dma_fence *fence);
signed long dma_fence_wait_timeout(struct dma_fence *fence, bool intr, signed long timeout);
signed long dma_fence_wait_any_timeout(struct dma_fence **fences, uint32_t count, bool intr,
                                       signed long timeout, uint32_t *idx);
void dma_fence_set_deadline(struct dma_fence *fence, ktime_t deadline);
struct dma_fence *dma_fence_get_stub(void);
struct dma_fence *dma_fence_allocate_private_stub(ktime_t timestamp);
u64  dma_fence_context_alloc(unsigned num);

static inline bool dma_fence_is_signaled_locked(struct dma_fence *fence)
{
    if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) return true;
    if (fence->ops->signaled && fence->ops->signaled(fence)) {
        dma_fence_signal_locked(fence);
        return true;
    }
    return false;
}
static inline bool dma_fence_is_signaled(struct dma_fence *fence)
{
    if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) return true;
    if (fence->ops->signaled && fence->ops->signaled(fence)) {
        dma_fence_signal(fence);
        return true;
    }
    return false;
}

/* is seqno f1 after f2 on one timeline? (32-bit seqnos wrap) */
static inline bool __dma_fence_is_later(u64 f1, u64 f2, const struct dma_fence_ops *ops)
{
    if (ops->use_64bit_seqno) return f1 > f2;
    return (int)(lower_32_bits(f1) - lower_32_bits(f2)) > 0;
}
static inline bool dma_fence_is_later(struct dma_fence *f1, struct dma_fence *f2)
{
    if (WARN_ON(f1->context != f2->context)) return false;
    return __dma_fence_is_later(f1->seqno, f2->seqno, f1->ops);
}
static inline bool dma_fence_is_later_or_same(struct dma_fence *f1, struct dma_fence *f2)
{
    return f1 == f2 || dma_fence_is_later(f1, f2);
}
static inline struct dma_fence *dma_fence_later(struct dma_fence *f1, struct dma_fence *f2)
{
    if (WARN_ON(f1->context != f2->context)) return NULL;
    if (dma_fence_is_later(f1, f2)) return dma_fence_is_signaled(f1) ? NULL : f1;
    return dma_fence_is_signaled(f2) ? NULL : f2;
}

static inline int dma_fence_get_status_locked(struct dma_fence *fence)
{
    if (dma_fence_is_signaled_locked(fence)) return fence->error ?: 1;
    return 0;
}
static inline void dma_fence_set_error(struct dma_fence *fence, int error)
{
    WARN_ON(test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags));
    WARN_ON(error >= 0 || error < -MAX_ERRNO);
    fence->error = error;
}
static inline ktime_t dma_fence_timestamp(struct dma_fence *fence)
{
    if (WARN_ON(!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))) return ktime_get();
    while (!test_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags)) cpu_relax();
    return fence->timestamp;
}
static inline signed long dma_fence_wait(struct dma_fence *fence, bool intr)
{
    signed long ret = dma_fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);
    return ret < 0 ? ret : 0;
}

bool dma_fence_is_array(struct dma_fence *fence);
bool dma_fence_is_chain(struct dma_fence *fence);
static inline bool dma_fence_is_container(struct dma_fence *fence)
{
    return dma_fence_is_array(fence) || dma_fence_is_chain(fence);
}

/* fence-signalling critical sections: annotations only */
static inline bool dma_fence_begin_signalling(void) { return true; }
static inline void dma_fence_end_signalling(bool cookie) { }
#define __dma_fence_might_wait()        do { } while (0)

#define DMA_FENCE_TRACE(f, fmt, args...)        do { } while (0)
#define DMA_FENCE_WARN(f, fmt, args...) \
    pr_warn("f %llu#%llu: " fmt, (f)->context, (f)->seqno, ##args)
#define DMA_FENCE_ERR(f, fmt, args...) \
    pr_err("f %llu#%llu: " fmt, (f)->context, (f)->seqno, ##args)

#endif
