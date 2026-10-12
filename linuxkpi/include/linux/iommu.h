/* linux/iommu.h -- Kestrel's Linux KPI: IOMMU API (Kestrel programs no IOMMU: DMA addresses are physical) */
#ifndef _LKPI_LINUX_IOMMU_H
#define _LKPI_LINUX_IOMMU_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/gfp_types.h>

struct device;
struct iommu_domain { unsigned type; unsigned long pgsize_bitmap; };
struct bus_type;
#define IOMMU_READ      (1 << 0)
#define IOMMU_WRITE     (1 << 1)
#define IOMMU_CACHE     (1 << 2)
#define IOMMU_NOEXEC    (1 << 3)
#define IOMMU_MMIO      (1 << 4)
static inline struct iommu_domain *iommu_get_domain_for_dev(struct device *dev) { return NULL; }
static inline struct iommu_domain *iommu_domain_alloc(const struct bus_type *bus) { return NULL; }
static inline void iommu_domain_free(struct iommu_domain *domain) { }
static inline int iommu_attach_device(struct iommu_domain *domain, struct device *dev) { return -ENODEV; }
static inline void iommu_detach_device(struct iommu_domain *domain, struct device *dev) { }
static inline int iommu_map(struct iommu_domain *domain, unsigned long iova, phys_addr_t paddr, size_t size,
                            int prot, gfp_t gfp) { return -ENODEV; }
static inline size_t iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size) { return 0; }
static inline bool device_iommu_mapped(struct device *dev) { return false; }
static inline bool tegra_dev_iommu_get_stream_id(struct device *dev, u32 *stream_id) { return false; }

#endif
