/* drm/drm_gem_shmem_helper.h -- Kestrel's Linux KPI: GEM objects backed by
 * pages of system memory (linuxkpi/src/drm_gem_shmem.c)
 *
 * Linux's header is GPL-2.0-only, so this one is written in the KPI with
 * the names drivers use. The pages are allocated when first needed (a
 * pin, a mapping, the scatter-gather table) and kept while anything uses
 * them; the counts change under the object's reservation lock. Kestrel
 * has no swap, so nothing is purged: madvise is recorded and ignored. */
#ifndef _LKPI_DRM_GEM_SHMEM_HELPER_H
#define _LKPI_DRM_GEM_SHMEM_HELPER_H

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/iosys-map.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_prime.h>

struct dma_buf_attachment;
struct drm_mode_create_dumb;
struct drm_printer;
struct sg_table;
struct vfsmount;

struct drm_gem_shmem_object {
    struct drm_gem_object   base;
    struct page           **pages;
    unsigned int            pages_use_count;
    unsigned int            pages_pin_count;
    int                     madv;
    struct list_head        madv_list;
    struct sg_table        *sgt;
    void                   *vaddr;
    unsigned int            vmap_use_count;
    bool                    pages_mark_dirty_on_put : 1;
    bool                    pages_mark_accessed_on_put : 1;
    bool                    map_wc : 1;
};

#define to_drm_gem_shmem_obj(obj) container_of(obj, struct drm_gem_shmem_object, base)

struct drm_gem_shmem_object *drm_gem_shmem_create(struct drm_device *dev, size_t size);
struct drm_gem_shmem_object *drm_gem_shmem_create_with_mnt(struct drm_device *dev, size_t size,
                                                           struct vfsmount *gemfs);
void drm_gem_shmem_free(struct drm_gem_shmem_object *shmem);

void drm_gem_shmem_put_pages_locked(struct drm_gem_shmem_object *shmem);
int  drm_gem_shmem_pin(struct drm_gem_shmem_object *shmem);
void drm_gem_shmem_unpin(struct drm_gem_shmem_object *shmem);
int  drm_gem_shmem_pin_locked(struct drm_gem_shmem_object *shmem);
void drm_gem_shmem_unpin_locked(struct drm_gem_shmem_object *shmem);
int  drm_gem_shmem_vmap(struct drm_gem_shmem_object *shmem, struct iosys_map *map);
void drm_gem_shmem_vunmap(struct drm_gem_shmem_object *shmem, struct iosys_map *map);
int  drm_gem_shmem_mmap(struct drm_gem_shmem_object *shmem, struct vm_area_struct *vma);
int  drm_gem_shmem_madvise(struct drm_gem_shmem_object *shmem, int madv);

static inline bool drm_gem_shmem_is_purgeable(struct drm_gem_shmem_object *shmem) { return false; }
static inline void drm_gem_shmem_purge(struct drm_gem_shmem_object *shmem) { }

struct sg_table *drm_gem_shmem_get_sg_table(struct drm_gem_shmem_object *shmem);
struct sg_table *drm_gem_shmem_get_pages_sgt(struct drm_gem_shmem_object *shmem);
void drm_gem_shmem_print_info(const struct drm_gem_shmem_object *shmem, struct drm_printer *p,
                              unsigned int indent);

extern const struct vm_operations_struct drm_gem_shmem_vm_ops;

/* the drm_gem_object_funcs of a shmem object */
static inline void drm_gem_shmem_object_free(struct drm_gem_object *obj)
{
    drm_gem_shmem_free(to_drm_gem_shmem_obj(obj));
}
static inline void drm_gem_shmem_object_print_info(struct drm_printer *p, unsigned int indent,
                                                   const struct drm_gem_object *obj)
{
    drm_gem_shmem_print_info(to_drm_gem_shmem_obj(obj), p, indent);
}
static inline int drm_gem_shmem_object_pin(struct drm_gem_object *obj)
{
    return drm_gem_shmem_pin_locked(to_drm_gem_shmem_obj(obj));
}
static inline void drm_gem_shmem_object_unpin(struct drm_gem_object *obj)
{
    drm_gem_shmem_unpin_locked(to_drm_gem_shmem_obj(obj));
}
static inline struct sg_table *drm_gem_shmem_object_get_sg_table(struct drm_gem_object *obj)
{
    return drm_gem_shmem_get_sg_table(to_drm_gem_shmem_obj(obj));
}
static inline int drm_gem_shmem_object_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
    return drm_gem_shmem_vmap(to_drm_gem_shmem_obj(obj), map);
}
static inline void drm_gem_shmem_object_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
    drm_gem_shmem_vunmap(to_drm_gem_shmem_obj(obj), map);
}
static inline int drm_gem_shmem_object_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
    return drm_gem_shmem_mmap(to_drm_gem_shmem_obj(obj), vma);
}

struct drm_gem_object *drm_gem_shmem_prime_import_sg_table(struct drm_device *dev,
                                                           struct dma_buf_attachment *attach,
                                                           struct sg_table *sgt);
int drm_gem_shmem_dumb_create(struct drm_file *file, struct drm_device *dev, struct drm_mode_create_dumb *args);

#define DRM_GEM_SHMEM_DRIVER_OPS \
    .gem_prime_import_sg_table = drm_gem_shmem_prime_import_sg_table, \
    .dumb_create               = drm_gem_shmem_dumb_create

#endif
