/* linuxkpi/src/ww_mutex.c -- wound/wait mutexes (include/linux/ww_mutex.h)
 *
 * The lock is the base mutex; ctx records the holder's acquire context.
 * A context that holds other locks gives up with -EDEADLK when the lock is
 * held by an older context (wait-die); otherwise it sleeps on the base
 * mutex's wait queue until the lock is free. Taking a lock its own context
 * already holds is -EALREADY. Whenever the holder changes, every waiter is
 * woken to check again, so a waiter that holds locks never sleeps behind an
 * older context. */
#include <linux/ww_mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/printk.h>

/* should ctx back off rather than wait for lock? (read without the lock: a
 * stale answer only means one more pass round the loop) */
static bool ww_must_die(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    struct ww_acquire_ctx *holder = READ_ONCE(lock->ctx);
    return ctx && ctx->acquired > 0 && holder && holder != ctx &&
           (long)(holder->stamp - ctx->stamp) < 0;
}

static int ww_lock_common(struct ww_mutex *lock, struct ww_acquire_ctx *ctx, unsigned int state)
{
    if (ctx && READ_ONCE(lock->ctx) == ctx)
        return -EALREADY;
    for (;;) {
        if (mutex_trylock(&lock->base)) {
            WRITE_ONCE(lock->ctx, ctx);
            if (ctx) {
                ctx->acquired++;
                if (ctx->contending == lock) ctx->contending = NULL;
                /* waiters that hold locks may now have to back off: one of
                 * them could hold a lock this context will wait for */
                if (waitqueue_active(&lock->base.wait)) wake_up_all(&lock->base.wait);
            }
            return 0;
        }
        if (ww_must_die(lock, ctx)) {
            ctx->contending = lock;
            return -EDEADLK;
        }
        if (state == TASK_UNINTERRUPTIBLE)
            wait_event(lock->base.wait, !mutex_is_locked(&lock->base) || ww_must_die(lock, ctx));
        else if (wait_event_interruptible(lock->base.wait,
                                          !mutex_is_locked(&lock->base) || ww_must_die(lock, ctx)))
            return -EINTR;
    }
}

int ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    might_sleep();
    return ww_lock_common(lock, ctx, TASK_UNINTERRUPTIBLE);
}

int ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    might_sleep();
    return ww_lock_common(lock, ctx, TASK_INTERRUPTIBLE);
}

int ww_mutex_trylock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    if (!mutex_trylock(&lock->base)) return 0;
    WRITE_ONCE(lock->ctx, ctx);
    if (ctx) ctx->acquired++;
    return 1;
}

void ww_mutex_unlock(struct ww_mutex *lock)
{
    struct ww_acquire_ctx *ctx = lock->ctx;
    if (ctx) ctx->acquired--;
    WRITE_ONCE(lock->ctx, NULL);
    mutex_unlock(&lock->base);
    /* waiters that hold other locks re-check whether they must back off */
    if (waitqueue_active(&lock->base.wait)) wake_up_all(&lock->base.wait);
}
