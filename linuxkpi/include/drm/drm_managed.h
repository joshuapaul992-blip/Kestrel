/* drm/drm_managed.h -- Kestrel's Linux KPI: resources freed with a DRM device (linuxkpi/src/drm.c)
 *
 * Each drmm_* allocation or action is recorded on the device and released,
 * newest first, when its last reference goes. */
#ifndef _LKPI_DRM_DRM_MANAGED_H
#define _LKPI_DRM_DRM_MANAGED_H

#include <linux/gfp.h>
#include <linux/overflow.h>
#include <linux/types.h>

struct drm_device;
struct mutex;

typedef void (*drmres_release_t)(struct drm_device *dev, void *res);

int __must_check __drmm_add_action(struct drm_device *dev, drmres_release_t action, void *data, const char *name);
int __must_check __drmm_add_action_or_reset(struct drm_device *dev, drmres_release_t action, void *data,
                                            const char *name);
#define drmm_add_action(dev, action, data)              __drmm_add_action(dev, action, data, #action)
#define drmm_add_action_or_reset(dev, action, data)     __drmm_add_action_or_reset(dev, action, data, #action)
void drmm_release_action(struct drm_device *dev, drmres_release_t action, void *data);

void *drmm_kmalloc(struct drm_device *dev, size_t size, gfp_t gfp) __malloc;
static inline void *drmm_kzalloc(struct drm_device *dev, size_t size, gfp_t gfp)
{
    return drmm_kmalloc(dev, size, gfp | __GFP_ZERO);
}
static inline void *drmm_kmalloc_array(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
    size_t bytes;
    if (check_mul_overflow(n, size, &bytes)) return NULL;
    return drmm_kmalloc(dev, bytes, flags);
}
static inline void *drmm_kcalloc(struct drm_device *dev, size_t n, size_t size, gfp_t flags)
{
    return drmm_kmalloc_array(dev, n, size, flags | __GFP_ZERO);
}
char *drmm_kstrdup(struct drm_device *dev, const char *s, gfp_t gfp);
void drmm_kfree(struct drm_device *dev, void *data);

void __drmm_mutex_release(struct drm_device *dev, void *res);
#define drmm_mutex_init(dev, lock) ({                                           \
    mutex_init(lock);                                                           \
    drmm_add_action_or_reset(dev, __drmm_mutex_release, lock);                  \
})

#endif
