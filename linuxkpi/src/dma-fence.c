/* linuxkpi/src/dma-fence.c -- fences, fence arrays and fence chains
 * (include/linux/dma-fence.h, dma-fence-array.h, dma-fence-chain.h) */
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/dma-fence-chain.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include <linux/kernel.h>

static atomic64_t dma_fence_context_counter = ATOMIC64_INIT(1);

u64 dma_fence_context_alloc(unsigned num)
{
    return atomic64_fetch_add(num, &dma_fence_context_counter);
}

void dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
                    spinlock_t *lock, u64 context, u64 seqno)
{
    kref_init(&fence->refcount);
    fence->ops = ops;
    INIT_LIST_HEAD(&fence->cb_list);
    fence->lock = lock;
    fence->context = context;
    fence->seqno = seqno;
    fence->flags = 0;
    fence->error = 0;
    fence->timestamp = 0;
}

void dma_fence_free(struct dma_fence *fence)
{
    kfree_rcu(fence, rcu);
}

void dma_fence_release(struct kref *kref)
{
    struct dma_fence *fence = container_of(kref, struct dma_fence, refcount);

    if (WARN(!list_empty(&fence->cb_list) && !test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags),
             "fence %llu#%llu released with callbacks pending\n", fence->context, fence->seqno)) {
        unsigned long flags;
        spin_lock_irqsave(fence->lock, flags);
        fence->error = -EDEADLK;
        dma_fence_signal_locked(fence);
        spin_unlock_irqrestore(fence->lock, flags);
    }
    if (fence->ops->release) fence->ops->release(fence);
    else dma_fence_free(fence);
}

int dma_fence_signal_timestamp_locked(struct dma_fence *fence, ktime_t timestamp)
{
    struct dma_fence_cb *cur, *tmp;
    struct list_head cb_list;

    if (test_and_set_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) return -EINVAL;
    list_replace_init(&fence->cb_list, &cb_list);
    fence->timestamp = timestamp;
    set_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fence->flags);
    list_for_each_entry_safe(cur, tmp, &cb_list, node) {
        INIT_LIST_HEAD(&cur->node);
        cur->func(fence, cur);
    }
    return 0;
}

int dma_fence_signal_timestamp(struct dma_fence *fence, ktime_t timestamp)
{
    unsigned long flags;
    int ret;
    if (!fence) return -EINVAL;
    spin_lock_irqsave(fence->lock, flags);
    ret = dma_fence_signal_timestamp_locked(fence, timestamp);
    spin_unlock_irqrestore(fence->lock, flags);
    return ret;
}

int dma_fence_signal_locked(struct dma_fence *fence)
{
    return dma_fence_signal_timestamp_locked(fence, ktime_get());
}

int dma_fence_signal(struct dma_fence *fence)
{
    return dma_fence_signal_timestamp(fence, ktime_get());
}

/* ask the provider to signal; with the lock held. False when already signaled. */
static bool __dma_fence_enable_signaling(struct dma_fence *fence)
{
    if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) return false;
    if (!test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
        fence->ops->enable_signaling && !fence->ops->enable_signaling(fence)) {
        dma_fence_signal_locked(fence);
        return false;
    }
    return true;
}

void dma_fence_enable_sw_signaling(struct dma_fence *fence)
{
    unsigned long flags;
    spin_lock_irqsave(fence->lock, flags);
    __dma_fence_enable_signaling(fence);
    spin_unlock_irqrestore(fence->lock, flags);
}

int dma_fence_add_callback(struct dma_fence *fence, struct dma_fence_cb *cb, dma_fence_func_t func)
{
    unsigned long flags;
    int ret = 0;

    if (WARN_ON(!fence || !func)) return -EINVAL;
    if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
        INIT_LIST_HEAD(&cb->node);
        return -ENOENT;
    }
    spin_lock_irqsave(fence->lock, flags);
    if (__dma_fence_enable_signaling(fence)) {
        cb->func = func;
        list_add_tail(&cb->node, &fence->cb_list);
    } else {
        INIT_LIST_HEAD(&cb->node);
        ret = -ENOENT;
    }
    spin_unlock_irqrestore(fence->lock, flags);
    return ret;
}

bool dma_fence_remove_callback(struct dma_fence *fence, struct dma_fence_cb *cb)
{
    unsigned long flags;
    bool ret;
    spin_lock_irqsave(fence->lock, flags);
    ret = !list_empty(&cb->node);
    if (ret) list_del_init(&cb->node);
    spin_unlock_irqrestore(fence->lock, flags);
    return ret;
}

int dma_fence_get_status(struct dma_fence *fence)
{
    unsigned long flags;
    int status;
    spin_lock_irqsave(fence->lock, flags);
    status = dma_fence_get_status_locked(fence);
    spin_unlock_irqrestore(fence->lock, flags);
    return status;
}

struct default_wait_cb {
    struct dma_fence_cb base;
    struct task_struct *task;
};

static void dma_fence_default_wait_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
    wake_up_state(container_of(cb, struct default_wait_cb, base)->task, TASK_NORMAL);
}

signed long dma_fence_default_wait(struct dma_fence *fence, bool intr, signed long timeout)
{
    struct default_wait_cb cb;
    unsigned long flags;
    signed long ret = timeout ? timeout : 1;

    spin_lock_irqsave(fence->lock, flags);
    if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) goto out;
    if (intr && signal_pending(current)) { ret = -ERESTARTSYS; goto out; }
    if (!timeout) { ret = 0; goto out; }
    if (!__dma_fence_enable_signaling(fence)) goto out;

    cb.base.func = dma_fence_default_wait_cb;
    cb.task = current;
    list_add(&cb.base.node, &fence->cb_list);
    while (!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags) && ret > 0) {
        set_current_state(intr ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE);
        spin_unlock_irqrestore(fence->lock, flags);
        ret = schedule_timeout(ret);
        spin_lock_irqsave(fence->lock, flags);
        if (ret > 0 && intr && signal_pending(current)) ret = -ERESTARTSYS;
    }
    if (!list_empty(&cb.base.node)) list_del(&cb.base.node);
    __set_current_state(TASK_RUNNING);
    if (ret == 0 && test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) ret = 1;
out:
    spin_unlock_irqrestore(fence->lock, flags);
    return ret;
}

signed long dma_fence_wait_timeout(struct dma_fence *fence, bool intr, signed long timeout)
{
    if (WARN_ON(timeout < 0)) return -EINVAL;
    might_sleep();
    if (fence->ops->wait) return fence->ops->wait(fence, intr, timeout);
    return dma_fence_default_wait(fence, intr, timeout);
}

signed long dma_fence_wait_any_timeout(struct dma_fence **fences, uint32_t count, bool intr,
                                       signed long timeout, uint32_t *idx)
{
    struct default_wait_cb *cb;
    signed long ret = timeout;
    unsigned i;

    if (WARN_ON(!fences || !count || timeout < 0)) return -EINVAL;
    for (i = 0; i < count; i++)
        if (dma_fence_is_signaled(fences[i])) { if (idx) *idx = i; return timeout ? timeout : 1; }
    if (!timeout) return 0;
    cb = kcalloc(count, sizeof(*cb), GFP_KERNEL);
    if (!cb) return -ENOMEM;
    for (i = 0; i < count; i++) {
        cb[i].task = current;
        if (dma_fence_add_callback(fences[i], &cb[i].base, dma_fence_default_wait_cb)) {
            if (idx) *idx = i;
            goto fence_rm_cb;
        }
    }
    while (ret > 0) {
        set_current_state(intr ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE);
        for (i = 0; i < count; i++)
            if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fences[i]->flags)) { if (idx) *idx = i; break; }
        if (i < count) break;
        ret = schedule_timeout(ret);
        if (ret > 0 && intr && signal_pending(current)) ret = -ERESTARTSYS;
    }
    __set_current_state(TASK_RUNNING);
fence_rm_cb:
    while (i-- > 0) dma_fence_remove_callback(fences[i], &cb[i].base);
    kfree(cb);
    return ret;
}

void dma_fence_set_deadline(struct dma_fence *fence, ktime_t deadline)
{
    if (fence->ops->set_deadline && !dma_fence_is_signaled(fence))
        fence->ops->set_deadline(fence, deadline);
}

void dma_fence_describe(struct dma_fence *fence, struct seq_file *seq)
{
    seq_printf(seq, "%s %s seq %llu %ssignalled\n", fence->ops->get_driver_name(fence),
               fence->ops->get_timeline_name(fence), fence->seqno,
               dma_fence_is_signaled(fence) ? "" : "un");
}

/* ---- the stub fence: always signaled ---------------------------------------------- */
static const char *dma_fence_stub_get_name(struct dma_fence *fence) { return "stub"; }
static const struct dma_fence_ops dma_fence_stub_ops = {
    .get_driver_name = dma_fence_stub_get_name,
    .get_timeline_name = dma_fence_stub_get_name,
};
static DEFINE_SPINLOCK(dma_fence_stub_lock);
static struct dma_fence dma_fence_stub;

struct dma_fence *dma_fence_get_stub(void)
{
    spin_lock(&dma_fence_stub_lock);
    if (!dma_fence_stub.ops) {
        dma_fence_init(&dma_fence_stub, &dma_fence_stub_ops, &dma_fence_stub_lock, 0, 0);
        set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &dma_fence_stub.flags);
        dma_fence_signal_locked(&dma_fence_stub);
    }
    spin_unlock(&dma_fence_stub_lock);
    return dma_fence_get(&dma_fence_stub);
}

struct dma_fence *dma_fence_allocate_private_stub(ktime_t timestamp)
{
    struct dma_fence *fence = kzalloc(sizeof(*fence), GFP_KERNEL);
    if (!fence) return NULL;
    dma_fence_init(fence, &dma_fence_stub_ops, &dma_fence_stub_lock, 0, 0);
    set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);
    dma_fence_signal_timestamp(fence, timestamp);
    return fence;
}

/* ---- arrays ---------------------------------------------------------------------- */
static const char *dma_fence_array_get_driver_name(struct dma_fence *fence) { return "dma_fence_array"; }
static const char *dma_fence_array_get_timeline_name(struct dma_fence *fence) { return "unbound"; }

static void dma_fence_array_signal_work(struct work_struct *work)
{
    struct dma_fence_array *array = container_of(work, struct dma_fence_array, work);
    dma_fence_signal(&array->base);
    dma_fence_put(&array->base);
}

static void dma_fence_array_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
    struct dma_fence_array *array = container_of(cb, struct dma_fence_array_cb, cb)->array;

    if (f->error && !array->base.error) array->base.error = f->error;
    if (atomic_dec_and_test(&array->num_pending))
        queue_work(system_unbound_wq, &array->work);    /* the work drops the reference */
    else
        dma_fence_put(&array->base);
}

static bool dma_fence_array_enable_signaling(struct dma_fence *fence)
{
    struct dma_fence_array *array = to_dma_fence_array(fence);
    unsigned i;

    for (i = 0; i < array->num_fences; i++) {
        array->callbacks[i].array = array;
        dma_fence_get(&array->base);            /* dropped by the callback */
        if (dma_fence_add_callback(array->fences[i], &array->callbacks[i].cb, dma_fence_array_cb_func)) {
            int error = array->fences[i]->error;
            if (error && !array->base.error) array->base.error = error;
            dma_fence_put(&array->base);
            if (atomic_dec_and_test(&array->num_pending)) return false;
        }
    }
    return true;
}

static bool dma_fence_array_signaled(struct dma_fence *fence)
{
    struct dma_fence_array *array = to_dma_fence_array(fence);
    unsigned i, done = 0;

    if (atomic_read(&array->num_pending) <= 0) return true;
    for (i = 0; i < array->num_fences; i++)
        if (dma_fence_is_signaled(array->fences[i])) done++;
    return array->signal_on_any ? done > 0 : done == array->num_fences;
}

static void dma_fence_array_release(struct dma_fence *fence)
{
    struct dma_fence_array *array = to_dma_fence_array(fence);
    unsigned i;
    for (i = 0; i < array->num_fences; i++) dma_fence_put(array->fences[i]);
    kfree(array->fences);
    dma_fence_free(fence);
}

const struct dma_fence_ops dma_fence_array_ops = {
    .get_driver_name = dma_fence_array_get_driver_name,
    .get_timeline_name = dma_fence_array_get_timeline_name,
    .enable_signaling = dma_fence_array_enable_signaling,
    .signaled = dma_fence_array_signaled,
    .release = dma_fence_array_release,
};

struct dma_fence_array *dma_fence_array_create(int num_fences, struct dma_fence **fences,
                                               u64 context, unsigned seqno, bool signal_on_any)
{
    struct dma_fence_array *array;

    WARN_ON(!num_fences || !fences);
    array = kzalloc(struct_size(array, callbacks, num_fences), GFP_KERNEL);
    if (!array) return NULL;
    spin_lock_init(&array->lock);
    dma_fence_init(&array->base, &dma_fence_array_ops, &array->lock, context, seqno);
    INIT_WORK(&array->work, dma_fence_array_signal_work);
    array->num_fences = num_fences;
    atomic_set(&array->num_pending, signal_on_any ? 1 : num_fences);
    array->signal_on_any = signal_on_any;
    array->fences = fences;
    return array;
}

bool dma_fence_match_context(struct dma_fence *fence, u64 context)
{
    struct dma_fence_array *array = to_dma_fence_array(fence);
    unsigned i;
    if (!array) return fence->context == context;
    for (i = 0; i < array->num_fences; i++)
        if (array->fences[i]->context != context) return false;
    return true;
}

struct dma_fence *dma_fence_array_first(struct dma_fence *head)
{
    struct dma_fence_array *array;
    if (!head) return NULL;
    array = to_dma_fence_array(head);
    if (!array) return head;
    return array->num_fences ? array->fences[0] : NULL;
}

struct dma_fence *dma_fence_array_next(struct dma_fence *head, unsigned int index)
{
    struct dma_fence_array *array = to_dma_fence_array(head);
    if (!array || index >= array->num_fences) return NULL;
    return array->fences[index];
}

bool dma_fence_is_array(struct dma_fence *fence) { return fence->ops == &dma_fence_array_ops; }

/* ---- chains ---------------------------------------------------------------------- */
static const char *dma_fence_chain_get_driver_name(struct dma_fence *fence) { return "dma_fence_chain"; }
static const char *dma_fence_chain_get_timeline_name(struct dma_fence *fence) { return "unbound"; }

/* a counted reference to the previous node, or NULL */
static struct dma_fence *dma_fence_chain_get_prev(struct dma_fence_chain *chain)
{
    struct dma_fence *prev;
    rcu_read_lock();
    prev = dma_fence_get_rcu_safe(&chain->prev);
    rcu_read_unlock();
    return prev;
}

/* the oldest unsignaled fence of the chain from fence on (chain nodes or
 * their contained fences), with a reference; NULL when all have signaled */
static struct dma_fence *dma_fence_chain_first_pending(struct dma_fence *fence)
{
    struct dma_fence *iter, *pending = NULL;

    dma_fence_chain_for_each(iter, fence) {
        struct dma_fence *f = dma_fence_chain_contained(iter);
        if (!dma_fence_is_signaled(f)) {
            dma_fence_put(pending);
            pending = dma_fence_get(f);
        }
    }
    return pending;
}

static void dma_fence_chain_cb(struct dma_fence *f, struct dma_fence_cb *cb);

/* wait on the next unsignaled fence, or signal; consumes one reference to the node */
static void dma_fence_chain_arm(struct dma_fence_chain *chain)
{
    for (;;) {
        struct dma_fence *pending = dma_fence_chain_first_pending(&chain->base);
        if (!pending) {
            dma_fence_signal(&chain->base);
            dma_fence_put(&chain->base);
            return;
        }
        if (!dma_fence_add_callback(pending, &chain->cb, dma_fence_chain_cb)) {
            dma_fence_put(pending);
            return;                             /* the callback keeps the reference */
        }
        dma_fence_put(pending);
    }
}

static void dma_fence_chain_work(struct work_struct *work);
struct dma_fence_chain_work { struct work_struct work; struct dma_fence_chain *chain; };

static void dma_fence_chain_cb(struct dma_fence *f, struct dma_fence_cb *cb)
{
    struct dma_fence_chain *chain = container_of(cb, struct dma_fence_chain, cb);
    struct dma_fence_chain_work *w = kmalloc(sizeof(*w), GFP_ATOMIC);

    /* re-arming takes other fences' locks: do it outside this callback */
    if (!w) { dma_fence_signal(&chain->base); dma_fence_put(&chain->base); return; }
    INIT_WORK(&w->work, dma_fence_chain_work);
    w->chain = chain;
    queue_work(system_unbound_wq, &w->work);
}

static void dma_fence_chain_work(struct work_struct *work)
{
    struct dma_fence_chain_work *w = container_of(work, struct dma_fence_chain_work, work);
    struct dma_fence_chain *chain = w->chain;
    kfree(w);
    dma_fence_chain_arm(chain);
}

static bool dma_fence_chain_enable_signaling(struct dma_fence *fence)
{
    struct dma_fence_chain *chain = to_dma_fence_chain(fence);
    struct dma_fence *pending;

    pending = dma_fence_chain_first_pending(fence);
    if (!pending) return false;
    dma_fence_get(&chain->base);
    if (dma_fence_add_callback(pending, &chain->cb, dma_fence_chain_cb)) {
        /* it signaled meanwhile: look again from a worker */
        dma_fence_chain_cb(pending, &chain->cb);
    }
    dma_fence_put(pending);
    return true;
}

static bool dma_fence_chain_signaled(struct dma_fence *fence)
{
    struct dma_fence *pending = dma_fence_chain_first_pending(fence);
    dma_fence_put(pending);
    return !pending;
}

static void dma_fence_chain_release(struct dma_fence *fence)
{
    struct dma_fence_chain *chain = to_dma_fence_chain(fence);
    dma_fence_put(rcu_dereference_protected(chain->prev, true));
    dma_fence_put(chain->fence);
    dma_fence_free(fence);
}

const struct dma_fence_ops dma_fence_chain_ops = {
    .use_64bit_seqno = true,
    .get_driver_name = dma_fence_chain_get_driver_name,
    .get_timeline_name = dma_fence_chain_get_timeline_name,
    .enable_signaling = dma_fence_chain_enable_signaling,
    .signaled = dma_fence_chain_signaled,
    .release = dma_fence_chain_release,
};

bool dma_fence_is_chain(struct dma_fence *fence) { return fence->ops == &dma_fence_chain_ops; }

/* the next node down the chain, dropping the reference to fence; the walk
 * does not unlink signaled nodes as Linux's does */
struct dma_fence *dma_fence_chain_walk(struct dma_fence *fence)
{
    struct dma_fence_chain *chain = to_dma_fence_chain(fence);
    struct dma_fence *prev = chain ? dma_fence_chain_get_prev(chain) : NULL;
    dma_fence_put(fence);
    return prev;
}

int dma_fence_chain_find_seqno(struct dma_fence **pfence, uint64_t seqno)
{
    struct dma_fence_chain *chain;

    if (!seqno) return 0;
    chain = to_dma_fence_chain(*pfence);
    if (!chain || chain->base.seqno < seqno) return -EINVAL;
    dma_fence_chain_for_each(*pfence, &chain->base) {
        if ((*pfence)->context != chain->base.context ||
            to_dma_fence_chain(*pfence)->prev_seqno < seqno)
            break;
    }
    dma_fence_put(&chain->base);
    return 0;
}

void dma_fence_chain_init(struct dma_fence_chain *chain, struct dma_fence *prev,
                          struct dma_fence *fence, uint64_t seqno)
{
    struct dma_fence_chain *prev_chain = to_dma_fence_chain(prev);
    uint64_t context;

    spin_lock_init(&chain->lock);
    rcu_assign_pointer(chain->prev, prev);
    chain->fence = fence;
    chain->prev_seqno = 0;
    if (prev_chain && __dma_fence_is_later(seqno, prev->seqno, prev->ops)) {
        context = prev->context;
        chain->prev_seqno = prev->seqno;
    } else {
        context = dma_fence_context_alloc(1);
        if (prev_chain) seqno = max(prev->seqno, seqno);
    }
    dma_fence_init(&chain->base, &dma_fence_chain_ops, &chain->lock, context, seqno);
}
