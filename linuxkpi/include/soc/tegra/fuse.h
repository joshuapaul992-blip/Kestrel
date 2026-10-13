/* soc/tegra/fuse.h -- Kestrel's Linux KPI: Tegra fuses (not a Tegra) */
#ifndef _LKPI_SOC_TEGRA_FUSE_H
#define _LKPI_SOC_TEGRA_FUSE_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

static inline u32 tegra_fuse_readl(unsigned long offset, u32 *value) { return -ENODEV; }
static inline u32 tegra_sku_info_dummy(void) { return 0; }

#endif
