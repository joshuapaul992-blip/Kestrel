/* drm/drm_sysfs.h -- Kestrel's Linux KPI: DRM's sysfs entries and uevents (none; linuxkpi/src/drm.c) */
#ifndef _LKPI_DRM_DRM_SYSFS_H
#define _LKPI_DRM_DRM_SYSFS_H

struct drm_device;
struct device;
struct drm_connector;
struct drm_property;

int  drm_class_device_register(struct device *dev);
void drm_class_device_unregister(struct device *dev);
void drm_sysfs_hotplug_event(struct drm_device *dev);
void drm_sysfs_connector_hotplug_event(struct drm_connector *connector);
void drm_sysfs_connector_property_event(struct drm_connector *connector, struct drm_property *property);

#endif
