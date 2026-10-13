/* linux/reset.h -- Kestrel's Linux KPI: reset controllers (none on PCs) */
#ifndef _LKPI_LINUX_RESET_H
#define _LKPI_LINUX_RESET_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

struct reset_control;
struct device;
static inline struct reset_control *devm_reset_control_get(struct device *dev, const char *id) { return ERR_PTR(-ENODEV); }
static inline struct reset_control *reset_control_get(struct device *dev, const char *id) { return ERR_PTR(-ENODEV); }
static inline int reset_control_assert(struct reset_control *r) { return 0; }
static inline int reset_control_deassert(struct reset_control *r) { return 0; }
static inline void reset_control_put(struct reset_control *r) { }

#endif
