/* linux/string_helpers.h -- Kestrel's Linux KPI: strings for booleans */
#ifndef _LKPI_LINUX_STRING_HELPERS_H
#define _LKPI_LINUX_STRING_HELPERS_H

#include <linux/types.h>
#include <linux/string.h>

static inline const char *str_high_low(bool v) { return v ? "high" : "low"; }
static inline const char *str_read_write(bool v) { return v ? "read" : "write"; }

#endif
