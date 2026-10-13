/* linux/kmsg_dump.h -- Kestrel's Linux KPI: kernel log dumpers (registered, never called: panics go to Kestrel's own log) */
#ifndef _LKPI_LINUX_KMSG_DUMP_H
#define _LKPI_LINUX_KMSG_DUMP_H

#include <linux/list.h>
#include <linux/errno.h>

enum kmsg_dump_reason {
    KMSG_DUMP_UNDEF,
    KMSG_DUMP_PANIC,
    KMSG_DUMP_OOPS,
    KMSG_DUMP_EMERG,
    KMSG_DUMP_SHUTDOWN,
    KMSG_DUMP_MAX
};
struct kmsg_dump_detail {
    enum kmsg_dump_reason reason;
    const char *description;
};
struct kmsg_dumper {
    struct list_head list;
    void (*dump)(struct kmsg_dumper *dumper, struct kmsg_dump_detail *detail);
    enum kmsg_dump_reason max_reason;
    bool registered;
};
static inline int kmsg_dump_register(struct kmsg_dumper *d) { return -EINVAL; }
static inline int kmsg_dump_unregister(struct kmsg_dumper *d) { return -EINVAL; }

#endif
