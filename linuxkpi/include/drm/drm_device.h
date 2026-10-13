/* drm/drm_device.h -- Kestrel's Linux KPI: struct drm_device
 *
 * Written for the KPI (Linux's own header has no license line, so it is
 * GPL-2.0-only); the fields are the ones the vendored DRM core and drivers
 * use, with the same names and types. */
#ifndef _LKPI_DRM_DRM_DEVICE_H
#define _LKPI_DRM_DRM_DEVICE_H

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/idr.h>

#include <drm/drm_mode_config.h>
#include <linux/completion.h>
#include <linux/fs.h>

struct drm_driver;
struct drm_minor;
struct drm_master;
struct drm_vblank_crtc;
struct drm_vma_offset_manager;
struct drm_vram_mm;
struct drm_fb_helper;
struct inode;
struct pci_dev;
struct pci_controller;
struct dentry;

enum switch_power_state {
    DRM_SWITCH_POWER_ON = 0,
    DRM_SWITCH_POWER_OFF = 1,
    DRM_SWITCH_POWER_CHANGING = 2,
    DRM_SWITCH_POWER_DYNAMIC_OFF = 3,
};

struct drm_device {
    int if_version;                         /* the DRM interface version user space set */
    struct kref ref;
    struct device *dev;                     /* the bus device (a PCI function) */

    struct {                                /* drmm_* resources, released with the device */
        struct list_head resources;
        void *final_kfree;                  /* the structure embedding this one */
        spinlock_t lock;
    } managed;

    const struct drm_driver *driver;
    void *dev_private;
    struct drm_minor *primary;              /* /dev/dri/cardN */
    struct drm_minor *render;               /* /dev/dri/renderDN */
    struct drm_minor *accel;
    bool registered;
    struct drm_master *master;
    u32 driver_features;                    /* DRIVER_* bits this instance supports */
    bool unplugged;
    struct inode *anon_inode;
    char *unique;

    struct mutex struct_mutex;
    struct mutex master_mutex;
    atomic_t open_count;
    struct mutex filelist_mutex;
    struct list_head filelist;
    struct list_head filelist_internal;     /* in-kernel clients' files */
    struct mutex clientlist_mutex;
    struct list_head clientlist;

    bool vblank_disable_immediate;
    struct drm_vblank_crtc *vblank;         /* one per CRTC */
    spinlock_t vblank_time_lock;
    spinlock_t vbl_lock;
    u32 max_vblank_count;
    struct list_head vblank_event_list;
    spinlock_t event_lock;
    unsigned int num_crtcs;

    struct drm_mode_config mode_config;

    struct mutex object_name_lock;
    struct idr object_name_idr;
    struct drm_vma_offset_manager *vma_offset_manager;
    struct drm_vram_mm *vram_mm;
    enum switch_power_state switch_power_state;
    struct drm_fb_helper *fb_helper;
    struct dentry *debugfs_root;
};

#endif
