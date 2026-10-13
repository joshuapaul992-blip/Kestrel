/* linux/of.h -- Kestrel's Linux KPI: device tree (none on PCs) */
#ifndef _LKPI_LINUX_OF_H
#define _LKPI_LINUX_OF_H

#include <linux/types.h>
#include <linux/errno.h>

struct device_node;
struct of_device_id;
struct property;
static inline struct device_node *of_node_get(struct device_node *n) { return n; }
static inline void of_node_put(struct device_node *n) { }
static inline bool of_device_is_available(const struct device_node *n) { return false; }
static inline int of_property_read_u32(const struct device_node *n, const char *name, u32 *v) { return -ENOSYS; }
#define for_each_child_of_node(parent, child)   for ((child) = NULL; (child); )

#endif
