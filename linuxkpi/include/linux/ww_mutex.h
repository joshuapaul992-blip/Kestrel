/* linux/ww_mutex.h -- Kestrel's Linux KPI: wound/wait mutexes (linuxkpi/src/ww_mutex.c)
 *
 * A set of locks taken together under one acquire context (struct
 * ww_acquire_ctx) never deadlocks: each context has a ticket, and a context
 * that already holds locks and finds one held by an older context gets
 * -EDEADLK. It must then drop everything it holds, take the contended lock
 * with ww_mutex_lock_slow() and try again. Without a context a ww_mutex is
 * a plain mutex.
 *
 * Both classes are run as wait-die here: the younger context backs off.
 * Linux's wound-wait class (DEFINE_WW_CLASS) instead makes the older one
 * take the lock away; callers handle -EDEADLK the same way in both. */
#ifndef _LKPI_LINUX_WW_MUTEX_H
#define _LKPI_LINUX_WW_MUTEX_H

#include <linux/mutex.h>
#include <linux/atomic.h>

struct ww_class {
    atomic_long_t       stamp;              /* the next ticket */
    const char         *name;
    bool                is_wait_die;
};

struct ww_acquire_ctx {
    struct task_struct *task;
    unsigned long       stamp;              /* ticket: lower is older */
    unsigned int        acquired;           /* locks held through this context */
    unsigned short      wounded;
    unsigned short      is_wait_die;
    struct ww_mutex    *contending;         /* the lock it got -EDEADLK on */
    bool                done;
};

struct ww_mutex {
    struct mutex            base;
    struct ww_acquire_ctx  *ctx;            /* the holder's context, NULL without one */
    struct ww_class        *ww_class;
};

#define __WW_CLASS_INITIALIZER(cls, wd)   { .stamp = ATOMIC_LONG_INIT(0), .name = #cls, .is_wait_die = (wd) }
#define DEFINE_WW_CLASS(name)           struct ww_class name = __WW_CLASS_INITIALIZER(name, 0)
#define DEFINE_WD_CLASS(name)           struct ww_class name = __WW_CLASS_INITIALIZER(name, 1)

static inline void ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class)
{
    mutex_init(&lock->base);
    lock->ctx = NULL;
    lock->ww_class = ww_class;
}
static inline void ww_mutex_destroy(struct ww_mutex *lock) { }
static inline bool ww_mutex_is_locked(struct ww_mutex *lock) { return mutex_is_locked(&lock->base); }

static inline void ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class)
{
    ctx->task = current;
    ctx->stamp = atomic_long_inc_return(&ww_class->stamp);
    ctx->acquired = 0;
    ctx->wounded = 0;
    ctx->is_wait_die = ww_class->is_wait_die;
    ctx->contending = NULL;
    ctx->done = false;
}
static inline void ww_acquire_done(struct ww_acquire_ctx *ctx) { ctx->done = true; }
static inline void ww_acquire_fini(struct ww_acquire_ctx *ctx) { }

int  ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);
int  __must_check ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);
int  ww_mutex_trylock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx);   /* 1 if taken */
void ww_mutex_unlock(struct ww_mutex *lock);

/* after -EDEADLK, with nothing else held: wait for the contended lock */
static inline void ww_mutex_lock_slow(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    ww_mutex_lock(lock, ctx);
}
static inline int __must_check ww_mutex_lock_slow_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
    return ww_mutex_lock_interruptible(lock, ctx);
}

#endif
