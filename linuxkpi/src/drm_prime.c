/* linuxkpi/src/drm_prime.c -- PRIME: GEM objects as dma-buf file
 * descriptors (include/drm/drm_prime.h)
 *
 * DRM_IOCTL_PRIME_HANDLE_TO_FD exports a GEM object as a dma-buf, and
 * DRM_IOCTL_PRIME_FD_TO_HANDLE imports one: a buffer of the same device
 * comes back as its own object, a foreign one through the driver's
 * gem_prime_import_sg_table. Mesa hands buffers to the X server this way
 * (DRI3), and gets them back.
 *
 * An exported object keeps its dma-buf in obj->dma_buf for as long as it
 * has handles, so exporting it again gives the same buffer. Each DRM file
 * remembers which handle names which dma-buf in both directions
 * (struct drm_prime_file_private: two red-black trees of the same
 * members), so importing a buffer twice gives the same handle, and the
 * entry goes when the handle is closed. */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/mutex.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>
#include <drm/drm_vma_manager.h>
#include "../../third_party/linux/drivers/gpu/drm/drm_internal.h"

/* ---- the per-file cache ------------------------------------------------------------------- */
struct drm_prime_member {
    struct dma_buf *dma_buf;            /* with a reference */
    uint32_t        handle;
    struct rb_node  dmabuf_rb;
    struct rb_node  handle_rb;
};

static int add_buf_handle(struct drm_prime_file_private *fp, struct dma_buf *dma_buf, uint32_t handle)
{
    struct drm_prime_member *m = kmalloc(sizeof(*m), GFP_KERNEL), *pos;
    struct rb_node **p, *parent;

    if (!m) return -ENOMEM;
    get_dma_buf(dma_buf);
    m->dma_buf = dma_buf;
    m->handle = handle;

    for (parent = NULL, p = &fp->dmabufs.rb_node; *p; ) {
        parent = *p;
        pos = rb_entry(parent, struct drm_prime_member, dmabuf_rb);
        p = (uintptr_t)dma_buf > (uintptr_t)pos->dma_buf ? &parent->rb_right : &parent->rb_left;
    }
    rb_link_node(&m->dmabuf_rb, parent, p);
    rb_insert_color(&m->dmabuf_rb, &fp->dmabufs);

    for (parent = NULL, p = &fp->handles.rb_node; *p; ) {
        parent = *p;
        pos = rb_entry(parent, struct drm_prime_member, handle_rb);
        p = handle > pos->handle ? &parent->rb_right : &parent->rb_left;
    }
    rb_link_node(&m->handle_rb, parent, p);
    rb_insert_color(&m->handle_rb, &fp->handles);
    return 0;
}

static struct drm_prime_member *find_handle(struct drm_prime_file_private *fp, uint32_t handle)
{
    struct rb_node *n = fp->handles.rb_node;
    while (n) {
        struct drm_prime_member *m = rb_entry(n, struct drm_prime_member, handle_rb);
        if (m->handle == handle) return m;
        n = handle < m->handle ? n->rb_left : n->rb_right;
    }
    return NULL;
}

static struct drm_prime_member *find_dmabuf(struct drm_prime_file_private *fp, struct dma_buf *dma_buf)
{
    struct rb_node *n = fp->dmabufs.rb_node;
    while (n) {
        struct drm_prime_member *m = rb_entry(n, struct drm_prime_member, dmabuf_rb);
        if (m->dma_buf == dma_buf) return m;
        n = (uintptr_t)dma_buf < (uintptr_t)m->dma_buf ? n->rb_left : n->rb_right;
    }
    return NULL;
}

void drm_prime_remove_buf_handle(struct drm_prime_file_private *fp, uint32_t handle)
{
    struct drm_prime_member *m;

    mutex_lock(&fp->lock);
    m = find_handle(fp, handle);
    if (m) {
        rb_erase(&m->handle_rb, &fp->handles);
        rb_erase(&m->dmabuf_rb, &fp->dmabufs);
        dma_buf_put(m->dma_buf);
        kfree(m);
    }
    mutex_unlock(&fp->lock);
}

void drm_prime_init_file_private(struct drm_prime_file_private *fp)
{
    mutex_init(&fp->lock);
    fp->dmabufs = RB_ROOT;
    fp->handles = RB_ROOT;
}

/* every handle is closed by now, and took its entry with it */
void drm_prime_destroy_file_private(struct drm_prime_file_private *fp)
{
    WARN_ON(!RB_EMPTY_ROOT(&fp->dmabufs));
}

/* ---- exporting ----------------------------------------------------------------------------- */
struct dma_buf *drm_gem_dmabuf_export(struct drm_device *dev, struct dma_buf_export_info *exp_info)
{
    struct drm_gem_object *obj = exp_info->priv;
    struct dma_buf *dma_buf = dma_buf_export(exp_info);

    if (IS_ERR(dma_buf)) return dma_buf;
    drm_dev_get(dev);
    drm_gem_object_get(obj);
    /* mappings of the buffer go when TTM moves the object, like the device's */
    dma_buf->file->f_mapping = obj->dev->anon_inode->i_mapping;
    return dma_buf;
}
EXPORT_SYMBOL(drm_gem_dmabuf_export);

void drm_gem_dmabuf_release(struct dma_buf *dma_buf)
{
    struct drm_gem_object *obj = dma_buf->priv;
    struct drm_device *dev = obj->dev;

    drm_gem_object_put(obj);
    drm_dev_put(dev);
}
EXPORT_SYMBOL(drm_gem_dmabuf_release);

int drm_gem_map_attach(struct dma_buf *dma_buf, struct dma_buf_attachment *attach)
{
    struct drm_gem_object *obj = dma_buf->priv;

    if (!obj->funcs->get_sg_table) return -ENOSYS;
    return drm_gem_pin(obj);
}
EXPORT_SYMBOL(drm_gem_map_attach);

void drm_gem_map_detach(struct dma_buf *dma_buf, struct dma_buf_attachment *attach)
{
    drm_gem_unpin(dma_buf->priv);
}
EXPORT_SYMBOL(drm_gem_map_detach);

struct sg_table *drm_gem_map_dma_buf(struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
    struct drm_gem_object *obj = attach->dmabuf->priv;
    struct sg_table *sgt;
    int ret;

    if (WARN_ON(dir == DMA_NONE)) return ERR_PTR(-EINVAL);
    if (!obj->funcs->get_sg_table) return ERR_PTR(-ENOSYS);
    sgt = obj->funcs->get_sg_table(obj);
    if (IS_ERR(sgt)) return sgt;
    ret = dma_map_sgtable(attach->dev, sgt, dir, DMA_ATTR_SKIP_CPU_SYNC);
    if (ret) {
        sg_free_table(sgt);
        kfree(sgt);
        return ERR_PTR(ret);
    }
    return sgt;
}
EXPORT_SYMBOL(drm_gem_map_dma_buf);

void drm_gem_unmap_dma_buf(struct dma_buf_attachment *attach, struct sg_table *sgt, enum dma_data_direction dir)
{
    if (!sgt) return;
    dma_unmap_sgtable(attach->dev, sgt, dir, DMA_ATTR_SKIP_CPU_SYNC);
    sg_free_table(sgt);
    kfree(sgt);
}
EXPORT_SYMBOL(drm_gem_unmap_dma_buf);

int drm_gem_dmabuf_vmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
    return drm_gem_vmap(dma_buf->priv, map);
}
EXPORT_SYMBOL(drm_gem_dmabuf_vmap);

void drm_gem_dmabuf_vunmap(struct dma_buf *dma_buf, struct iosys_map *map)
{
    drm_gem_vunmap(dma_buf->priv, map);
}
EXPORT_SYMBOL(drm_gem_dmabuf_vunmap);

/* Through the object's own mmap (TTM's, for nouveau and the VRAM helpers),
 * at the object's offset in the device's address space: the same pages as
 * a mapping of the DRM node. */
int drm_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
    int ret;

    if (!obj->funcs || !obj->funcs->mmap) return -EINVAL;   /* drivers with only an f_op->mmap: none here */
    vma->vm_pgoff += drm_vma_node_start(&obj->vma_node);
    vma->vm_ops = obj->funcs->vm_ops;
    drm_gem_object_get(obj);
    ret = obj->funcs->mmap(obj, vma);
    if (ret) {
        drm_gem_object_put(obj);
        return ret;
    }
    vma->vm_private_data = obj;
    return 0;
}
EXPORT_SYMBOL(drm_gem_prime_mmap);

int drm_gem_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
    return drm_gem_prime_mmap(dma_buf->priv, vma);
}
EXPORT_SYMBOL(drm_gem_dmabuf_mmap);

static const struct dma_buf_ops drm_gem_prime_dmabuf_ops = {
    .cache_sgt_mapping = true,
    .attach = drm_gem_map_attach,
    .detach = drm_gem_map_detach,
    .map_dma_buf = drm_gem_map_dma_buf,
    .unmap_dma_buf = drm_gem_unmap_dma_buf,
    .release = drm_gem_dmabuf_release,
    .mmap = drm_gem_dmabuf_mmap,
    .vmap = drm_gem_dmabuf_vmap,
    .vunmap = drm_gem_dmabuf_vunmap,
};

struct dma_buf *drm_gem_prime_export(struct drm_gem_object *obj, int flags)
{
    struct dma_buf_export_info exp_info = {
        .exp_name = KBUILD_MODNAME,
        .owner = obj->dev->driver->fops->owner,
        .ops = &drm_gem_prime_dmabuf_ops,
        .size = obj->size,
        .flags = flags,
        .priv = obj,
        .resv = obj->resv,
    };

    return drm_gem_dmabuf_export(obj->dev, &exp_info);
}
EXPORT_SYMBOL(drm_gem_prime_export);

/* the object's dma-buf, made on its first export (object_name_lock held) */
static struct dma_buf *export_object(struct drm_device *dev, struct drm_gem_object *obj, uint32_t flags)
{
    struct dma_buf *dma_buf;

    if (obj->handle_count == 0) return ERR_PTR(-ENOENT);    /* its last handle is being closed */
    if (obj->import_attach) {
        dma_buf = obj->import_attach->dmabuf;
        get_dma_buf(dma_buf);
        return dma_buf;
    }
    if (obj->dma_buf) {
        get_dma_buf(obj->dma_buf);
        return obj->dma_buf;
    }
    dma_buf = obj->funcs->export ? obj->funcs->export(obj, (int)flags) : drm_gem_prime_export(obj, (int)flags);
    if (IS_ERR(dma_buf)) return dma_buf;
    obj->dma_buf = dma_buf;             /* the object's reference, dropped with its last handle */
    get_dma_buf(dma_buf);
    return dma_buf;
}

struct dma_buf *drm_gem_prime_handle_to_dmabuf(struct drm_device *dev, struct drm_file *file_priv,
                                               uint32_t handle, uint32_t flags)
{
    struct drm_prime_member *m;
    struct drm_gem_object *obj;
    struct dma_buf *dma_buf;

    mutex_lock(&file_priv->prime.lock);
    obj = drm_gem_object_lookup(file_priv, handle);
    if (!obj) {
        dma_buf = ERR_PTR(-ENOENT);
        goto out_unlock;
    }
    m = find_handle(&file_priv->prime, handle);
    if (m) {                            /* exported (or imported) through this file before */
        dma_buf = m->dma_buf;
        get_dma_buf(dma_buf);
        goto out_put;
    }
    mutex_lock(&dev->object_name_lock);
    dma_buf = export_object(dev, obj, flags);
    if (!IS_ERR(dma_buf)) {
        int ret = add_buf_handle(&file_priv->prime, dma_buf, handle);
        if (ret) {
            dma_buf_put(dma_buf);
            dma_buf = ERR_PTR(ret);
        }
    }
    mutex_unlock(&dev->object_name_lock);
out_put:
    drm_gem_object_put(obj);
out_unlock:
    mutex_unlock(&file_priv->prime.lock);
    return dma_buf;
}
EXPORT_SYMBOL(drm_gem_prime_handle_to_dmabuf);

int drm_gem_prime_handle_to_fd(struct drm_device *dev, struct drm_file *file_priv, uint32_t handle,
                               uint32_t flags, int *prime_fd)
{
    struct dma_buf *dma_buf = drm_gem_prime_handle_to_dmabuf(dev, file_priv, handle, flags);
    int fd;

    if (IS_ERR(dma_buf)) return (int)PTR_ERR(dma_buf);
    fd = dma_buf_fd(dma_buf, (int)flags);
    if (fd < 0) {
        dma_buf_put(dma_buf);
        return fd;
    }
    *prime_fd = fd;
    return 0;
}
EXPORT_SYMBOL(drm_gem_prime_handle_to_fd);

int drm_prime_handle_to_fd_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
    struct drm_prime_handle *args = data;

    if (args->flags & ~(DRM_CLOEXEC | DRM_RDWR)) return -EINVAL;
    if (dev->driver->prime_handle_to_fd)
        return dev->driver->prime_handle_to_fd(dev, file_priv, args->handle, args->flags, &args->fd);
    return drm_gem_prime_handle_to_fd(dev, file_priv, args->handle, args->flags, &args->fd);
}

/* ---- importing ----------------------------------------------------------------------------- */
struct drm_gem_object *drm_gem_prime_import_dev(struct drm_device *dev, struct dma_buf *dma_buf,
                                                struct device *attach_dev)
{
    struct dma_buf_attachment *attach;
    struct drm_gem_object *obj;
    struct sg_table *sgt;
    int ret;

    if (dma_buf->ops == &drm_gem_prime_dmabuf_ops) {
        obj = dma_buf->priv;
        if (obj->dev == dev) {          /* our own: the same object */
            drm_gem_object_get(obj);
            return obj;
        }
    }
    if (!dev->driver->gem_prime_import_sg_table) return ERR_PTR(-EINVAL);

    attach = dma_buf_attach(dma_buf, attach_dev);
    if (IS_ERR(attach)) return ERR_CAST(attach);
    get_dma_buf(dma_buf);
    sgt = dma_buf_map_attachment_unlocked(attach, DMA_BIDIRECTIONAL);
    if (IS_ERR(sgt)) {
        ret = (int)PTR_ERR(sgt);
        goto fail_detach;
    }
    obj = dev->driver->gem_prime_import_sg_table(dev, attach, sgt);
    if (IS_ERR(obj)) {
        ret = (int)PTR_ERR(obj);
        goto fail_unmap;
    }
    obj->import_attach = attach;
    obj->resv = dma_buf->resv;
    return obj;

fail_unmap:
    dma_buf_unmap_attachment_unlocked(attach, sgt, DMA_BIDIRECTIONAL);
fail_detach:
    dma_buf_detach(dma_buf, attach);
    dma_buf_put(dma_buf);
    return ERR_PTR(ret);
}
EXPORT_SYMBOL(drm_gem_prime_import_dev);

struct drm_gem_object *drm_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
    return drm_gem_prime_import_dev(dev, dma_buf, dev->dev);
}
EXPORT_SYMBOL(drm_gem_prime_import);

int drm_gem_prime_fd_to_handle(struct drm_device *dev, struct drm_file *file_priv, int prime_fd,
                               uint32_t *handle)
{
    struct drm_prime_member *m;
    struct drm_gem_object *obj;
    struct dma_buf *dma_buf;
    int ret;

    dma_buf = dma_buf_get(prime_fd);
    if (IS_ERR(dma_buf)) return (int)PTR_ERR(dma_buf);

    mutex_lock(&file_priv->prime.lock);
    m = find_dmabuf(&file_priv->prime, dma_buf);
    if (m) {
        *handle = m->handle;
        ret = 0;
        goto out_put;
    }

    mutex_lock(&dev->object_name_lock);
    obj = dev->driver->gem_prime_import ? dev->driver->gem_prime_import(dev, dma_buf)
                                        : drm_gem_prime_import(dev, dma_buf);
    if (IS_ERR(obj)) {
        ret = (int)PTR_ERR(obj);
        mutex_unlock(&dev->object_name_lock);
        goto out_put;
    }
    if (obj->dma_buf) {
        WARN_ON(obj->dma_buf != dma_buf);
    } else {
        obj->dma_buf = dma_buf;
        get_dma_buf(dma_buf);
    }
    ret = drm_gem_handle_create_tail(file_priv, obj, handle);    /* unlocks object_name_lock */
    drm_gem_object_put(obj);
    if (ret) goto out_put;

    ret = add_buf_handle(&file_priv->prime, dma_buf, *handle);
    mutex_unlock(&file_priv->prime.lock);
    if (ret) {
        drm_gem_handle_delete(file_priv, *handle);
        dma_buf_put(dma_buf);
        return ret;
    }
    dma_buf_put(dma_buf);
    return 0;

out_put:
    mutex_unlock(&file_priv->prime.lock);
    dma_buf_put(dma_buf);
    return ret;
}
EXPORT_SYMBOL(drm_gem_prime_fd_to_handle);

int drm_prime_fd_to_handle_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
    struct drm_prime_handle *args = data;

    if (dev->driver->prime_fd_to_handle)
        return dev->driver->prime_fd_to_handle(dev, file_priv, args->fd, &args->handle);
    return drm_gem_prime_fd_to_handle(dev, file_priv, args->fd, &args->handle);
}

void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg)
{
    struct dma_buf_attachment *attach = obj->import_attach;
    struct dma_buf *dma_buf = attach->dmabuf;

    if (sg) dma_buf_unmap_attachment_unlocked(attach, sg, DMA_BIDIRECTIONAL);
    dma_buf_detach(dma_buf, attach);
    dma_buf_put(dma_buf);               /* the import's reference */
}
EXPORT_SYMBOL(drm_prime_gem_destroy);

/* ---- scatter lists ---------------------------------------------------------------------------- */
struct sg_table *drm_prime_pages_to_sg(struct drm_device *dev, struct page **pages, unsigned int nr_pages)
{
    struct sg_table *sg = kzalloc(sizeof(*sg), GFP_KERNEL);
    int ret;

    if (!sg) return ERR_PTR(-ENOMEM);
    ret = sg_alloc_table_from_pages(sg, pages, nr_pages, 0, (unsigned long)nr_pages << PAGE_SHIFT, GFP_KERNEL);
    if (ret) {
        kfree(sg);
        return ERR_PTR(ret);
    }
    return sg;
}
EXPORT_SYMBOL(drm_prime_pages_to_sg);

/* the length of the run of DMA addresses from the first, without gaps */
unsigned long drm_prime_get_contiguous_size(struct sg_table *sgt)
{
    dma_addr_t expected = sg_dma_address(sgt->sgl);
    struct scatterlist *sg;
    unsigned long size = 0;
    int i;

    for_each_sgtable_dma_sg(sgt, sg, i) {
        unsigned int len = sg_dma_len(sg);
        if (!len) break;
        if (sg_dma_address(sg) != expected) break;
        expected += len;
        size += len;
    }
    return size;
}
EXPORT_SYMBOL(drm_prime_get_contiguous_size);

int drm_prime_sg_to_page_array(struct sg_table *sgt, struct page **pages, int max_pages)
{
    struct sg_page_iter iter;
    int n = 0;

    for_each_sgtable_page(sgt, &iter, 0) {
        if (WARN_ON(n >= max_pages)) return -1;
        pages[n++] = sg_page_iter_page(&iter);
    }
    return 0;
}
EXPORT_SYMBOL(drm_prime_sg_to_page_array);

/* The DMA address of every page of an imported buffer. */
int drm_prime_sg_to_dma_addr_array(struct sg_table *sgt, dma_addr_t *addrs, int max_pages)
{
    struct sg_dma_page_iter iter;
    int n = 0;

    for_each_sgtable_dma_page(sgt, &iter, 0) {
        if (WARN_ON(n >= max_pages)) return -1;
        addrs[n++] = sg_page_iter_dma_address(&iter);
    }
    return 0;
}
EXPORT_SYMBOL(drm_prime_sg_to_dma_addr_array);
