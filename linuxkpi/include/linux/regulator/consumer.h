/* linux/regulator/consumer.h -- Kestrel's Linux KPI: voltage regulators (none on PCs) */
#ifndef _LKPI_LINUX_REGULATOR_CONSUMER_H
#define _LKPI_LINUX_REGULATOR_CONSUMER_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

struct regulator;
struct device;
static inline struct regulator *devm_regulator_get(struct device *dev, const char *id) { return ERR_PTR(-ENODEV); }
static inline struct regulator *regulator_get(struct device *dev, const char *id) { return ERR_PTR(-ENODEV); }
static inline void regulator_put(struct regulator *r) { }
static inline int regulator_enable(struct regulator *r) { return 0; }
static inline int regulator_disable(struct regulator *r) { return 0; }
static inline int regulator_get_voltage(struct regulator *r) { return -EINVAL; }
static inline int regulator_set_voltage(struct regulator *r, int min_uV, int max_uV) { return -EINVAL; }

#endif
