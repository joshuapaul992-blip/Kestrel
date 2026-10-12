/* linux/seqlock.h -- Kestrel's Linux KPI: sequence counters and sequence locks
 *
 * A writer makes the count odd while it writes and even again after;
 * a reader retries when the count was odd or changed under it. */
#ifndef _LKPI_LINUX_SEQLOCK_H
#define _LKPI_LINUX_SEQLOCK_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <asm/barrier.h>
#include <asm/processor.h>

typedef struct seqcount { unsigned sequence; } seqcount_t;
typedef seqcount_t seqcount_spinlock_t;
typedef seqcount_t seqcount_mutex_t;
typedef seqcount_t seqcount_ww_mutex_t;
typedef struct { seqcount_t seqcount; spinlock_t lock; } seqlock_t;

#define SEQCNT_ZERO(name)               { .sequence = 0 }
#define seqcount_init(s)                do { (s)->sequence = 0; } while (0)
#define seqcount_spinlock_init(s, l)    seqcount_init(s)
#define seqcount_mutex_init(s, l)       seqcount_init(s)
#define seqcount_ww_mutex_init(s, l)    seqcount_init(s)

static inline unsigned read_seqcount_begin(const seqcount_t *s)
{
    unsigned seq;
    while ((seq = READ_ONCE(s->sequence)) & 1) cpu_relax();
    smp_rmb();
    return seq;
}
#define raw_read_seqcount_begin(s)      read_seqcount_begin(s)
#define __read_seqcount_begin(s)        read_seqcount_begin(s)
static inline unsigned raw_read_seqcount(const seqcount_t *s)
{
    unsigned seq = READ_ONCE(s->sequence);
    smp_rmb();
    return seq;
}
static inline int read_seqcount_retry(const seqcount_t *s, unsigned start)
{
    smp_rmb();
    return READ_ONCE(s->sequence) != start;
}
#define __read_seqcount_retry(s, start) read_seqcount_retry(s, start)
static inline void write_seqcount_begin(seqcount_t *s) { WRITE_ONCE(s->sequence, s->sequence + 1); smp_wmb(); }
static inline void write_seqcount_end(seqcount_t *s) { smp_wmb(); WRITE_ONCE(s->sequence, s->sequence + 1); }
#define raw_write_seqcount_begin(s)     write_seqcount_begin(s)
#define raw_write_seqcount_end(s)       write_seqcount_end(s)
static inline void write_seqcount_invalidate(seqcount_t *s) { smp_wmb(); WRITE_ONCE(s->sequence, s->sequence + 2); }

#define __SEQLOCK_UNLOCKED(name)        { .seqcount = SEQCNT_ZERO(name), .lock = __SPIN_LOCK_UNLOCKED(name.lock) }
#define DEFINE_SEQLOCK(name)            seqlock_t name = __SEQLOCK_UNLOCKED(name)
#define seqlock_init(sl)                do { seqcount_init(&(sl)->seqcount); spin_lock_init(&(sl)->lock); } while (0)
static inline unsigned read_seqbegin(const seqlock_t *sl) { return read_seqcount_begin(&sl->seqcount); }
static inline unsigned read_seqretry(const seqlock_t *sl, unsigned start) { return read_seqcount_retry(&sl->seqcount, start); }
static inline void write_seqlock(seqlock_t *sl) { spin_lock(&sl->lock); write_seqcount_begin(&sl->seqcount); }
static inline void write_sequnlock(seqlock_t *sl) { write_seqcount_end(&sl->seqcount); spin_unlock(&sl->lock); }
#define write_seqlock_irqsave(sl, flags) \
    do { spin_lock_irqsave(&(sl)->lock, flags); write_seqcount_begin(&(sl)->seqcount); } while (0)
#define write_sequnlock_irqrestore(sl, flags) \
    do { write_seqcount_end(&(sl)->seqcount); spin_unlock_irqrestore(&(sl)->lock, flags); } while (0)
static inline void write_seqlock_irq(seqlock_t *sl) { spin_lock_irq(&sl->lock); write_seqcount_begin(&sl->seqcount); }
static inline void write_sequnlock_irq(seqlock_t *sl) { write_seqcount_end(&sl->seqcount); spin_unlock_irq(&sl->lock); }

#endif
