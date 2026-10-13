/* linux/property.h -- Kestrel's Linux KPI: firmware node properties (none) */
#ifndef _LKPI_LINUX_PROPERTY_H
#define _LKPI_LINUX_PROPERTY_H

#include <linux/types.h>

struct fwnode_handle {
    struct fwnode_handle *secondary;
    const void *ops;
    struct device *dev;
};
static inline void fwnode_handle_put(struct fwnode_handle *f) { }
static inline struct fwnode_handle *fwnode_handle_get(struct fwnode_handle *f) { return f; }
static inline struct fwnode_handle *dev_fwnode(const struct device *dev) { return NULL; }

#endif
