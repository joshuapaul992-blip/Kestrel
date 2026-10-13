/* linux/mmu_notifier.h -- Kestrel's Linux KPI: MMU notifiers
 *
 * Linux code never sees user page tables change under it here (no SVM, no
 * userptr): a notifier can be embedded and is never called. */
#ifndef _LKPI_LINUX_MMU_NOTIFIER_H
#define _LKPI_LINUX_MMU_NOTIFIER_H

#include <linux/types.h>
#include <linux/list.h>

struct mm_struct;
struct mmu_notifier;
struct mmu_notifier_range { unsigned long start, end; unsigned int flags; int event; void *owner; };
struct mmu_notifier_ops {
    void (*release)(struct mmu_notifier *subscription, struct mm_struct *mm);
    int  (*invalidate_range_start)(struct mmu_notifier *subscription, const struct mmu_notifier_range *range);
    void (*invalidate_range_end)(struct mmu_notifier *subscription, const struct mmu_notifier_range *range);
};
struct mmu_notifier {
    struct hlist_node hlist;
    const struct mmu_notifier_ops *ops;
    struct mm_struct *mm;
};
struct mmu_interval_notifier;
struct mmu_interval_notifier_ops {
    bool (*invalidate)(struct mmu_interval_notifier *interval_sub, const struct mmu_notifier_range *range,
                       unsigned long cur_seq);
};
struct mmu_interval_notifier {
    const struct mmu_interval_notifier_ops *ops;
    struct mm_struct *mm;
    unsigned long invalidate_seq;
};

static inline void mmu_notifier_synchronize(void) { }
static inline int mmu_notifier_register(struct mmu_notifier *subscription, struct mm_struct *mm) { return -ENOSYS; }
static inline void mmu_notifier_unregister(struct mmu_notifier *subscription, struct mm_struct *mm) { }
static inline void mmu_notifier_put(struct mmu_notifier *subscription) { }

#endif
