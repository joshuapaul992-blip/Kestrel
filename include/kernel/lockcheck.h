/* include/kernel/lockcheck.h -- the lock-order checker (kernel/lockcheck.c)
 *
 * Built in with `make LOCKCHECK=1`; otherwise every hook below is an empty
 * inline and the lock structures keep their size.
 *
 * Locks are grouped in classes: all the pipes' locks are one class, the
 * page cache's lock another. The checker records, for each lock taken,
 * which classes the taker already held ("A, then B"), and reports the first
 * time a new pair closes a cycle (A then B somewhere, B then A somewhere
 * else): two CPUs taking them at once could each wait for the other. Each
 * report names the classes and where each pair was first seen. It also
 * reports, once per place:
 *   - a sleeping lock (kmutex, krwlock) or schedule() under a spinlock;
 *   - two locks of one class taken in both orders (lower address first,
 *     and higher first): code that holds two at once must order them, as
 *     the futex buckets are ordered by address; and a lock taken again by
 *     the CPU or task that holds it;
 *   - a task that exits holding a sleeping lock.
 * Reports go to the kernel log ("lockcheck: ..."), with a count in
 * /proc/kstat ("lockcheck classes=... pairs=... reports=...").
 *
 * The big kernel lock is one class. Waiting for a kmutex or a krwlock lets
 * go of the big lock, so no pair of the big lock and a sleeping lock can
 * deadlock and none is recorded.
 *
 * A lock's class is its name, when the code that sets the lock up gives it
 * one (lockcheck_name), and otherwise the function that took it first:
 * locks first taken in the same function share a class. */
#ifndef KESTREL_LOCKCHECK_H
#define KESTREL_LOCKCHECK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef LOCKCHECK
/* The class tag inside each lock: LC_MAGIC in the high half guards against
 * a lock in memory that was not zeroed, the class number in the low half. */
#define LC_MAGIC        0x10c0u
#define LOCKCHECK_TAG   uint32_t lc;

/* The sleeping locks a task holds (struct tcb) */
#define LC_TASK_DEPTH   8
struct lc_held_task { void *lock; uint16_t cls; uint64_t site; };

enum lc_kind { LC_SPIN, LC_MUTEX, LC_RWLOCK, LC_BKL };

void lockcheck_acquire(void *lock, uint32_t *tag, enum lc_kind kind, bool try, uint64_t site);
void lockcheck_release(void *lock, enum lc_kind kind);
void lockcheck_set_name(uint32_t *tag, const char *name);
void lockcheck_schedule(uint64_t site);     /* schedule(): no spinlock may be held */
void lockcheck_task_exit(void);             /* the task leaves: nothing may be held */
size_t lockcheck_gen_stat(char *buf, size_t cap);
void lockcheck_selftest(void);               /* boot: one cycle on purpose, not reported */

#define lockcheck_name(l, nm)   lockcheck_set_name(&(l)->lc, (nm))
/* for locks in static arrays, named at their first use */
#define lockcheck_name_once(l, nm)  do { if (!(l)->lc) lockcheck_set_name(&(l)->lc, (nm)); } while (0)
#else
#define LOCKCHECK_TAG
#define lockcheck_name(l, nm)   ((void)(l), (void)(nm))
#define lockcheck_name_once(l, nm)  ((void)(l), (void)(nm))
static inline void lockcheck_schedule(uint64_t site) { (void)site; }
static inline void lockcheck_task_exit(void) { }
static inline size_t lockcheck_gen_stat(char *buf, size_t cap) { (void)buf; (void)cap; return 0; }
static inline void lockcheck_selftest(void) { }
#endif

#endif
