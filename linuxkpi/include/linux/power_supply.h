/* linux/power_supply.h -- Kestrel's Linux KPI: power supplies (none known: report mains power, as a desktop) */
#ifndef _LKPI_LINUX_POWER_SUPPLY_H
#define _LKPI_LINUX_POWER_SUPPLY_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

static inline int power_supply_is_system_supplied(void) { return 1; }

#endif
