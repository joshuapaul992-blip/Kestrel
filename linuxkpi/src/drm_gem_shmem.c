/* linuxkpi/src/drm_gem_shmem.c -- GEM objects backed by system memory
 * (drm/drm_gem_shmem_helper.h)
 *
 * Linux's helper is GPL-2.0-only; this is the KPI's own, with the same
 * interface. An object's pages are allocated one by one, zeroed, when
 * something first needs them: a pin, a kernel mapping, a user mapping or
 * the scatter-gather table a device reads them through. They stay while
 * any of those holds them (pages_use_count) and are freed with the object.
 * A user mapping inserts the page under each fault. The counts change
 * under the object's reservation lock, as in Linux.
 *
 * Imported objects (a dma-buf from another device) keep the exporter's
 * table; their mappings and kernel maps go through the dma-buf. */
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <drm/drm.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_prime.h>
#include <drm/drm_print.h>
#include <drm/drm_vma_manager.h>

static const struct drm_gem_object_funcs drm_gem_shmem_funcs = {
    .free           = drm_gem_shmem_object_free,
    .print_info     = drm_gem_shmem_object_print_info,
    .pin            = drm_gem_shmem_object_pin,
    .unpin          = drm_gem_shmem_object_unpin,
    .get_sg_table   = drm_gem_shmem_object_get_sg_table,
    .vmap           = drm_gem_shmem_object_vmap,
    .vunmap         = drm_gem_shmem_object_vunmap,
    .mmap           = drm_gem_shmem_object_mmap,
    .vm_ops         = &drm_gem_shmem_vm_ops,
};

static struct drm_gem_shmem_object *shmem_alloc(struct drm_device *dev, size_t size, bool private)
{
    struct drm_gem_shmem_object *shmem;
    struct drm_gem_object *obj;
    int ret;

    size = PAGE_ALIGN(size);
    if (dev->driver->gem_create_object) {
        obj = dev->driver->gem_create_object(dev, size);
        if (IS_ERR(obj)) return ERR_CAST(obj);
        shmem = to_drm_gem_shmem_obj(obj);
    } else {
        shmem = kzalloc(sizeof(*shmem), GFP_KERNEL);
        if (!shmem) return ERR_PTR(-ENOMEM);
        obj = &shmem->base;
    }
    if (!obj->funcs) obj->funcs = &drm_gem_shmem_funcs;

    drm_gem_private_object_init(dev, obj, size);        /* no shmem file: the pages are our own */
    shmem->map_wc = false;
    INIT_LIST_HEAD(&shmem->madv_list);
    if (!private) {
        ret = drm_gem_create_mmap_offset(obj);
        if (ret) {
            drm_gem_object_release(obj);
            kfree(obj);
            return ERR_PTR(ret);
        }
    }
    return shmem;
}

struct drm_gem_shmem_object *drm_gem_shmem_create(struct drm_device *dev, size_t size)
{
    return shmem_alloc(dev, size, false);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_create);

struct drm_gem_shmem_object *drm_gem_shmem_create_with_mnt(struct drm_device *dev, size_t size,
                                                           struct vfsmount *gemfs)
{
    return shmem_alloc(dev, size, false);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_create_with_mnt);

/* ---- pages ------------------------------------------------------------------------------- */
static void free_pages_array(struct page **pages, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (pages[i]) __free_page(pages[i]);
    kvfree(pages);
}

static int get_pages_locked(struct drm_gem_shmem_object *shmem)
{
    struct drm_gem_object *obj = &shmem->base;
    size_t n = obj->size >> PAGE_SHIFT;

    dma_resv_assert_held(obj->resv);
    if (shmem->pages_use_count++ > 0) return 0;

    struct page **pages = kvcalloc(n, sizeof(*pages), GFP_KERNEL);
    if (!pages) goto fail;
    for (size_t i = 0; i < n; i++) {
        pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!pages[i]) {
            free_pages_array(pages, n);
            goto fail;
        }
    }
    shmem->pages = pages;
    return 0;
fail:
    shmem->pages_use_count = 0;
    drm_dbg_kms(obj->dev, "no pages for a %zu-byte object\n", obj->size);
    return -ENOMEM;
}

void drm_gem_shmem_put_pages_locked(struct drm_gem_shmem_object *shmem)
{
    struct drm_gem_object *obj = &shmem->base;

    dma_resv_assert_held(obj->resv);
    if (drm_WARN_ON_ONCE(obj->dev, !shmem->pages_use_count)) return;
    if (--shmem->pages_use_count > 0) return;
    free_pages_array(shmem->pages, obj->size >> PAGE_SHIFT);
    shmem->pages = NULL;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_put_pages_locked);

int drm_gem_shmem_pin_locked(struct drm_gem_shmem_object *shmem)
{
    int ret;
    dma_resv_assert_held(shmem->base.resv);
    if (shmem->base.import_attach) return 0;            /* the exporter's pages */
    ret = get_pages_locked(shmem);
    if (!ret) shmem->pages_pin_count++;
    return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_pin_locked);

void drm_gem_shmem_unpin_locked(struct drm_gem_shmem_object *shmem)
{
    dma_resv_assert_held(shmem->base.resv);
    if (shmem->base.import_attach || !shmem->pages_pin_count) return;
    shmem->pages_pin_count--;
    drm_gem_shmem_put_pages_locked(shmem);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_unpin_locked);

int drm_gem_shmem_pin(struct drm_gem_shmem_object *shmem)
{
    int ret = dma_resv_lock_interruptible(shmem->base.resv, NULL);
    if (ret) return ret;
    ret = drm_gem_shmem_pin_locked(shmem);
    dma_resv_unlock(shmem->base.resv);
    return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_pin);

void drm_gem_shmem_unpin(struct drm_gem_shmem_object *shmem)
{
    dma_resv_lock(shmem->base.resv, NULL);
    drm_gem_shmem_unpin_locked(shmem);
    dma_resv_unlock(shmem->base.resv);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_unpin);

int drm_gem_shmem_madvise(struct drm_gem_shmem_object *shmem, int madv)
{
    dma_resv_assert_held(shmem->base.resv);
    if (shmem->madv >= 0) shmem->madv = madv;
    return shmem->madv >= 0;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_madvise);

/* ---- tables for devices -------------------------------------------------------------- */
struct sg_table *drm_gem_shmem_get_sg_table(struct drm_gem_shmem_object *shmem)
{
    struct drm_gem_object *obj = &shmem->base;
    if (drm_WARN_ON(obj->dev, obj->import_attach || !shmem->pages)) return ERR_PTR(-EINVAL);
    return drm_prime_pages_to_sg(obj->dev, shmem->pages, obj->size >> PAGE_SHIFT);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_get_sg_table);

/* The object's own table, mapped for its device; the pages stay until the
 * object is freed. */
struct sg_table *drm_gem_shmem_get_pages_sgt(struct drm_gem_shmem_object *shmem)
{
    struct drm_gem_object *obj = &shmem->base;
    struct sg_table *sgt;
    int ret;

    ret = dma_resv_lock_interruptible(obj->resv, NULL);
    if (ret) return ERR_PTR(ret);
    if (shmem->sgt) {
        sgt = shmem->sgt;
        goto out;
    }
    if (drm_WARN_ON(obj->dev, obj->import_attach)) {
        sgt = ERR_PTR(-EINVAL);
        goto out;
    }
    ret = get_pages_locked(shmem);
    if (ret) {
        sgt = ERR_PTR(ret);
        goto out;
    }
    sgt = drm_gem_shmem_get_sg_table(shmem);
    if (IS_ERR(sgt)) goto err_put;
    ret = dma_map_sgtable(obj->dev->dev, sgt, DMA_BIDIRECTIONAL, 0);
    if (ret) {
        sg_free_table(sgt);
        kfree(sgt);
        sgt = ERR_PTR(ret);
        goto err_put;
    }
    shmem->sgt = sgt;
    goto out;
err_put:
    drm_gem_shmem_put_pages_locked(shmem);
out:
    dma_resv_unlock(obj->resv);
    return sgt;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_get_pages_sgt);

/* ---- kernel mappings -------------------------------------------------------------------- */
int drm_gem_shmem_vmap(struct drm_gem_shmem_object *shmem, struct iosys_map *map)
{
    struct drm_gem_object *obj = &shmem->base;
    int ret;

    if (obj->import_attach) {
        ret = dma_buf_vmap(obj->import_attach->dmabuf, map);
        if (!ret && drm_WARN_ON(obj->dev, map->is_iomem)) {
            dma_buf_vunmap(obj->import_attach->dmabuf, map);
            return -EIO;
        }
        return ret;
    }
    dma_resv_assert_held(obj->resv);
    if (shmem->vmap_use_count++ > 0) {
        iosys_map_set_vaddr(map, shmem->vaddr);
        return 0;
    }
    ret = get_pages_locked(shmem);
    if (ret) goto err;
    pgprot_t prot = PAGE_KERNEL;
    if (shmem->map_wc) prot = pgprot_writecombine(prot);
    shmem->vaddr = vmap(shmem->pages, obj->size >> PAGE_SHIFT, VM_MAP, prot);
    if (!shmem->vaddr) {
        drm_gem_shmem_put_pages_locked(shmem);
        ret = -ENOMEM;
        goto err;
    }
    iosys_map_set_vaddr(map, shmem->vaddr);
    return 0;
err:
    shmem->vmap_use_count = 0;
    return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_vmap);

void drm_gem_shmem_vunmap(struct drm_gem_shmem_object *shmem, struct iosys_map *map)
{
    struct drm_gem_object *obj = &shmem->base;

    if (obj->import_attach) {
        dma_buf_vunmap(obj->import_attach->dmabuf, map);
        return;
    }
    dma_resv_assert_held(obj->resv);
    if (drm_WARN_ON_ONCE(obj->dev, !shmem->vmap_use_count)) return;
    if (--shmem->vmap_use_count > 0) return;
    vunmap(shmem->vaddr);
    shmem->vaddr = NULL;
    drm_gem_shmem_put_pages_locked(shmem);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_vunmap);

/* ---- user mappings ----------------------------------------------------------------------- */
static vm_fault_t shmem_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct drm_gem_object *obj = vma->vm_private_data;
    struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);
    pgoff_t page = vmf->pgoff - drm_vma_node_start(&obj->vma_node);
    vm_fault_t ret;

    dma_resv_lock(obj->resv, NULL);
    if (page >= (obj->size >> PAGE_SHIFT) || !shmem->pages || shmem->madv < 0)
        ret = VM_FAULT_SIGBUS;
    else
        ret = vmf_insert_pfn(vma, vmf->address, page_to_pfn(shmem->pages[page]));
    dma_resv_unlock(obj->resv);
    return ret;
}

/* a copy of the mapping (fork, a split) holds the pages too */
static void shmem_vm_open(struct vm_area_struct *vma)
{
    struct drm_gem_object *obj = vma->vm_private_data;
    struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);

    drm_WARN_ON(obj->dev, obj->import_attach);
    dma_resv_lock(obj->resv, NULL);
    if (!drm_WARN_ON_ONCE(obj->dev, !shmem->pages_use_count)) shmem->pages_use_count++;
    dma_resv_unlock(obj->resv);
    drm_gem_vm_open(vma);
}

static void shmem_vm_close(struct vm_area_struct *vma)
{
    struct drm_gem_object *obj = vma->vm_private_data;
    struct drm_gem_shmem_object *shmem = to_drm_gem_shmem_obj(obj);

    dma_resv_lock(obj->resv, NULL);
    drm_gem_shmem_put_pages_locked(shmem);
    dma_resv_unlock(obj->resv);
    drm_gem_vm_close(vma);
}

const struct vm_operations_struct drm_gem_shmem_vm_ops = {
    .fault = shmem_fault,
    .open  = shmem_vm_open,
    .close = shmem_vm_close,
};
EXPORT_SYMBOL_GPL(drm_gem_shmem_vm_ops);

int drm_gem_shmem_mmap(struct drm_gem_shmem_object *shmem, struct vm_area_struct *vma)
{
    struct drm_gem_object *obj = &shmem->base;
    int ret;

    if (obj->import_attach) {
        /* the exporter maps it: drop what drm_gem_mmap_obj took for us */
        vma->vm_private_data = NULL;
        vma->vm_ops = NULL;
        ret = dma_buf_mmap(obj->dma_buf, vma, 0);
        if (!ret) drm_gem_object_put(obj);
        return ret;
    }
    if (is_cow_mapping(vma->vm_flags)) return -EINVAL;

    dma_resv_lock(obj->resv, NULL);
    ret = get_pages_locked(shmem);
    dma_resv_unlock(obj->resv);
    if (ret) return ret;

    vm_flags_set(vma, VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
    vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
    if (shmem->map_wc) vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    return 0;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_mmap);

/* ---- the object's end -------------------------------------------------------------------- */
void drm_gem_shmem_free(struct drm_gem_shmem_object *shmem)
{
    struct drm_gem_object *obj = &shmem->base;

    if (obj->import_attach) {
        drm_prime_gem_destroy(obj, shmem->sgt);
    } else {
        dma_resv_lock(obj->resv, NULL);
        drm_WARN_ON(obj->dev, shmem->vmap_use_count);
        if (shmem->sgt) {
            dma_unmap_sgtable(obj->dev->dev, shmem->sgt, DMA_BIDIRECTIONAL, 0);
            sg_free_table(shmem->sgt);
            kfree(shmem->sgt);
            shmem->sgt = NULL;
            drm_gem_shmem_put_pages_locked(shmem);      /* the table's */
        }
        while (shmem->pages_pin_count) {
            shmem->pages_pin_count--;
            drm_gem_shmem_put_pages_locked(shmem);
        }
        drm_WARN_ON(obj->dev, shmem->pages_use_count);
        dma_resv_unlock(obj->resv);
    }
    drm_gem_object_release(obj);
    kfree(shmem);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_free);

void drm_gem_shmem_print_info(const struct drm_gem_shmem_object *shmem, struct drm_printer *p,
                              unsigned int indent)
{
    if (shmem->base.import_attach) return;
    drm_printf_indent(p, indent, "pages_use_count=%u\n", shmem->pages_use_count);
    drm_printf_indent(p, indent, "pages_pin_count=%u\n", shmem->pages_pin_count);
    drm_printf_indent(p, indent, "vmap_use_count=%u\n", shmem->vmap_use_count);
    drm_printf_indent(p, indent, "vaddr=%p\n", shmem->vaddr);
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_print_info);

/* ---- for drivers built on the helper alone ----------------------------------------------- */
struct drm_gem_object *drm_gem_shmem_prime_import_sg_table(struct drm_device *dev,
                                                           struct dma_buf_attachment *attach,
                                                           struct sg_table *sgt)
{
    struct drm_gem_shmem_object *shmem = shmem_alloc(dev, attach->dmabuf->size, true);
    if (IS_ERR(shmem)) return ERR_CAST(shmem);
    shmem->sgt = sgt;
    return &shmem->base;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_prime_import_sg_table);

int drm_gem_shmem_dumb_create(struct drm_file *file, struct drm_device *dev, struct drm_mode_create_dumb *args)
{
    struct drm_gem_shmem_object *shmem;
    int ret;

    if (!args->width || !args->height || !args->bpp) return -EINVAL;
    args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
    args->size = PAGE_ALIGN((u64)args->pitch * args->height);
    shmem = drm_gem_shmem_create(dev, args->size);
    if (IS_ERR(shmem)) return PTR_ERR(shmem);
    ret = drm_gem_handle_create(file, &shmem->base, &args->handle);
    drm_gem_object_put(&shmem->base);
    return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_shmem_dumb_create);
