/* linux/vgaarb.h -- Kestrel's Linux KPI: VGA arbitration (none: one VGA owner, the boot device) */
#ifndef _LKPI_LINUX_VGAARB_H
#define _LKPI_LINUX_VGAARB_H

struct pci_dev;
static inline struct pci_dev *vga_default_device(void) { return NULL; }
static inline int vga_remove_vgacon(struct pci_dev *pdev) { return 0; }

#endif
