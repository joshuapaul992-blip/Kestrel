/* linux/vga_switcheroo.h -- Kestrel's Linux KPI: GPU switching on hybrid laptops (none)
 *
 * Drivers register as clients and are never switched: there is no
 * handler, so every GPU stays on. */
#ifndef _LKPI_LINUX_VGA_SWITCHEROO_H
#define _LKPI_LINUX_VGA_SWITCHEROO_H

#include <linux/types.h>
#include <linux/errno.h>

struct pci_dev;
struct device;
struct dev_pm_domain;

enum vga_switcheroo_state { VGA_SWITCHEROO_OFF, VGA_SWITCHEROO_ON, VGA_SWITCHEROO_NOT_FOUND };
enum vga_switcheroo_handler_flags_t {
    VGA_SWITCHEROO_CAN_SWITCH_DDC   = (1 << 0),
    VGA_SWITCHEROO_NEEDS_EDP_CONFIG = (1 << 1),
};

struct vga_switcheroo_client_ops {
    void (*set_gpu_state)(struct pci_dev *dev, enum vga_switcheroo_state);
    void (*reprobe)(struct pci_dev *dev);
    bool (*can_switch)(struct pci_dev *dev);
    void (*gpu_bound)(struct pci_dev *dev, int id);
};

static inline int vga_switcheroo_register_client(struct pci_dev *dev, const struct vga_switcheroo_client_ops *ops,
                                                 bool driver_power_control) { return 0; }
static inline void vga_switcheroo_unregister_client(struct pci_dev *dev) { }
static inline bool vga_switcheroo_client_probe_defer(struct pci_dev *pdev) { return false; }
static inline enum vga_switcheroo_handler_flags_t vga_switcheroo_handler_flags(void) { return 0; }
static inline int vga_switcheroo_init_domain_pm_ops(struct device *dev, struct dev_pm_domain *domain) { return -EINVAL; }
static inline void vga_switcheroo_fini_domain_pm_ops(struct device *dev) { }
static inline void vga_switcheroo_lock_ddc(struct pci_dev *pdev) { }
static inline int vga_switcheroo_unlock_ddc(struct pci_dev *pdev) { return -ENODEV; }
static inline int vga_switcheroo_process_delayed_switch(void) { return 0; }

#endif
