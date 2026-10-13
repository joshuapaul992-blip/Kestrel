/* linux/vga_switcheroo.h -- Kestrel's Linux KPI: GPU switching on hybrid laptops (none) */
#ifndef _LKPI_LINUX_VGA_SWITCHEROO_H
#define _LKPI_LINUX_VGA_SWITCHEROO_H

#include <linux/errno.h>

struct pci_dev;
static inline void vga_switcheroo_lock_ddc(struct pci_dev *pdev) { }
static inline int vga_switcheroo_unlock_ddc(struct pci_dev *pdev) { return -ENODEV; }
static inline int vga_switcheroo_process_delayed_switch(void) { return 0; }

#endif
