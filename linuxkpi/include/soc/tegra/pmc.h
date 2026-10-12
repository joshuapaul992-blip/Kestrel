/* soc/tegra/pmc.h -- Kestrel's Linux KPI: Tegra power management controller (not a Tegra) */
#ifndef _LKPI_SOC_TEGRA_PMC_H
#define _LKPI_SOC_TEGRA_PMC_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

static inline int tegra_powergate_remove_clamping(unsigned int id) { return -ENODEV; }

#endif
