/* drm/drm_accel.h -- Kestrel's Linux KPI: compute accelerators (none) */
#ifndef _LKPI_DRM_DRM_ACCEL_H
#define _LKPI_DRM_DRM_ACCEL_H

#define ACCEL_MAJOR             261
#define ACCEL_MAX_MINORS        256

struct device;
struct drm_device;
struct inode;
struct file;

static inline void accel_core_exit(void) { }
static inline int accel_core_init(void) { return 0; }
static inline void accel_set_device_instance_params(struct device *kdev, int index) { }
static inline void accel_debugfs_init(struct drm_device *dev) { }
static inline void accel_debugfs_register(struct drm_device *dev) { }

#endif
