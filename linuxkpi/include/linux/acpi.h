/* linux/acpi.h -- Kestrel's Linux KPI: ACPI (not used: Kestrel builds Linux drivers without CONFIG_ACPI) */
#ifndef _LKPI_LINUX_ACPI_H
#define _LKPI_LINUX_ACPI_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>

typedef void *acpi_handle;
struct acpi_device;
#define ACPI_HANDLE(dev)                ((acpi_handle)NULL)
#define ACPI_COMPANION(dev)             ((struct acpi_device *)NULL)
#define acpi_disabled                   1
static inline bool has_acpi_companion(struct device *dev) { return false; }

#endif
