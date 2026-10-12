/* linux/shrinker.h -- Kestrel's Linux KPI: memory shrinkers (registered, never asked: Kestrel does not reclaim from drivers) */
#ifndef _LKPI_LINUX_SHRINKER_H
#define _LKPI_LINUX_SHRINKER_H

#include <linux/types.h>
#include <linux/gfp_types.h>

struct shrink_control {
    gfp_t gfp_mask;
    int nid;
    unsigned long nr_to_scan;
    unsigned long nr_scanned;
};
#define SHRINK_STOP             (~0UL)
#define SHRINK_EMPTY            (~0UL - 1)
#define DEFAULT_SEEKS           2
struct shrinker {
    unsigned long (*count_objects)(struct shrinker *, struct shrink_control *sc);
    unsigned long (*scan_objects)(struct shrinker *, struct shrink_control *sc);
    long batch;
    int seeks;
    unsigned flags;
    void *private_data;
};
struct shrinker *shrinker_alloc(unsigned int flags, const char *fmt, ...);
void shrinker_register(struct shrinker *shrinker);
void shrinker_free(struct shrinker *shrinker);

#endif
