/* linux/sched/mm.h -- Kestrel's Linux KPI: memory-allocation context of the current task */
#ifndef _LKPI_LINUX_SCHED_MM_H
#define _LKPI_LINUX_SCHED_MM_H

#include <linux/gfp.h>
#include <linux/kernel.h>

#define might_alloc(gfp)                do { if (gfpflags_allow_blocking(gfp)) might_sleep(); } while (0)
static inline unsigned int memalloc_noreclaim_save(void) { return 0; }
static inline void memalloc_noreclaim_restore(unsigned int flags) { }
static inline unsigned int memalloc_nofs_save(void) { return 0; }
static inline void memalloc_nofs_restore(unsigned int flags) { }
static inline unsigned int memalloc_noio_save(void) { return 0; }
static inline void memalloc_noio_restore(unsigned int flags) { }
static inline void fs_reclaim_acquire(gfp_t gfp) { }
static inline void fs_reclaim_release(gfp_t gfp) { }
static inline void mmgrab(struct mm_struct *mm) { }
static inline void mmdrop(struct mm_struct *mm) { }

#endif
