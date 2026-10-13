/* linux/clk.h -- Kestrel's Linux KPI: common clock framework (no clock controllers on PCs: every lookup fails) */
#ifndef _LKPI_LINUX_CLK_H
#define _LKPI_LINUX_CLK_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

struct clk;
struct device;
static inline struct clk *clk_get(struct device *dev, const char *id) { return ERR_PTR(-ENOENT); }
static inline struct clk *devm_clk_get(struct device *dev, const char *id) { return ERR_PTR(-ENOENT); }
static inline struct clk *devm_clk_get_optional(struct device *dev, const char *id) { return NULL; }
static inline void clk_put(struct clk *clk) { }
static inline int clk_prepare_enable(struct clk *clk) { return 0; }
static inline void clk_disable_unprepare(struct clk *clk) { }
static inline int clk_enable(struct clk *clk) { return 0; }
static inline void clk_disable(struct clk *clk) { }
static inline unsigned long clk_get_rate(struct clk *clk) { return 0; }
static inline int clk_set_rate(struct clk *clk, unsigned long rate) { return -EINVAL; }
static inline long clk_round_rate(struct clk *clk, unsigned long rate) { return -EINVAL; }

#endif
