/* linux/vgaarb.h -- Kestrel's Linux KPI: VGA arbitration (none: one VGA owner, the boot device) */
#ifndef _LKPI_LINUX_VGAARB_H
#define _LKPI_LINUX_VGAARB_H

#include <linux/types.h>

struct pci_dev;

#define VGA_RSRC_NONE           0x00
#define VGA_RSRC_LEGACY_IO      0x01
#define VGA_RSRC_LEGACY_MEM     0x02
#define VGA_RSRC_LEGACY_MASK    (VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM)
#define VGA_RSRC_NORMAL_IO      0x04
#define VGA_RSRC_NORMAL_MEM     0x08

static inline struct pci_dev *vga_default_device(void) { return NULL; }
static inline int vga_remove_vgacon(struct pci_dev *pdev) { return 0; }
static inline int vga_client_register(struct pci_dev *pdev,
                                      unsigned int (*set_decode)(struct pci_dev *pdev, bool state)) { return 0; }
static inline void vga_client_unregister(struct pci_dev *pdev) { }
static inline int vga_get_interruptible(struct pci_dev *pdev, unsigned int rsrc) { return 0; }
static inline void vga_put(struct pci_dev *pdev, unsigned int rsrc) { }
static inline void vga_set_legacy_decoding(struct pci_dev *pdev, unsigned int decodes) { }

#endif
