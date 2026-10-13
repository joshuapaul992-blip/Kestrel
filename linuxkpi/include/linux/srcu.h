/* linux/srcu.h -- Kestrel's Linux KPI: sleepable RCU (linuxkpi/src/ww_mutex.c)
 *
 * Readers may sleep. Two counters, one per phase: a reader counts itself
 * in the current phase; synchronize_srcu() flips the phase and waits for
 * the old phase's count to drain, twice, so every reader that started
 * before it has finished. */
#ifndef _LKPI_LINUX_SRCU_H
#define _LKPI_LINUX_SRCU_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/wait.h>

struct srcu_struct {
    atomic_t            readers[2];
    unsigned int        idx;                /* the phase new readers join */
    struct mutex        sync;
    wait_queue_head_t   wait;
};
#define __SRCU_INITIALIZER(name) {                                              \
    .readers = { ATOMIC_INIT(0), ATOMIC_INIT(0) }, .idx = 0,                    \
    .sync = __MUTEX_INITIALIZER(name.sync),                                     \
    .wait = __WAIT_QUEUE_HEAD_INITIALIZER(name.wait) }
#define DEFINE_SRCU(name)               struct srcu_struct name = __SRCU_INITIALIZER(name)
#define DEFINE_STATIC_SRCU(name)        static struct srcu_struct name = __SRCU_INITIALIZER(name)
int  init_srcu_struct(struct srcu_struct *ssp);
void cleanup_srcu_struct(struct srcu_struct *ssp);
int  srcu_read_lock(struct srcu_struct *ssp);
void srcu_read_unlock(struct srcu_struct *ssp, int idx);
void synchronize_srcu(struct srcu_struct *ssp);
#define srcu_dereference(p, ssp)        READ_ONCE(p)
#define srcu_read_lock_held(ssp)        1

#endif
