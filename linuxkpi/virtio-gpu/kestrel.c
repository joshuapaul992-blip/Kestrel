/* linuxkpi/virtio-gpu/kestrel.c -- host-visible blob objects ("vram"),
 * what virtio-gpu's virtgpu_vram.c gives the rest of the driver
 *
 * Linux's virtgpu_vram.c is GPL-2.0-only, so this is written from the
 * virtio-gpu specification ("Device Operation: Shared memory" and the blob
 * commands) and the MIT-licensed parts of the driver that call it.
 *
 * A device with VIRTIO_GPU_F_RESOURCE_BLOB and a host-visible shared
 * memory region (VIRTIO_GPU_SHM_ID_HOST_VISIBLE, which the KPI's
 * virtio-pci transport finds in the device's PCI capabilities) lets a
 * program create blobs that live on the host (BLOB_MEM_HOST3D): Venus,
 * Mesa's Vulkan driver for virtual machines, allocates its device memory
 * this way. A mappable blob gets a range of the region (host_visible_mm,
 * under host_visible_lock), RESOURCE_MAP_BLOB asks the host to place the
 * blob there, and its answer (virtio_gpu_cmd_resource_map_cb in
 * virtgpu_vq.c) says how the guest must cache it. A program's mmap() of
 * the object waits for that answer and maps the region's pages with that
 * caching, page by page as they are touched. Freeing unmaps the blob and
 * drops the resource; the resource's completion handler gives the range
 * back (virtio_gpu_cleanup_object).
 *
 * Such a blob has no pages in guest memory, so it is not exported to other
 * devices as a dma-buf (the same device importing it again, as an X
 * server on the same GPU does, gets the object itself). */
#include "virtgpu_drv.h"
#include <linux/mm.h>

static void virtio_gpu_vram_free(struct drm_gem_object *obj)
{
    struct virtio_gpu_object *bo = gem_to_virtio_gpu_obj(obj);
    struct virtio_gpu_device *vgdev = obj->dev->dev_private;
    struct virtio_gpu_object_vram *vram = to_virtio_gpu_vram(bo);
    bool mapped;

    if (!bo->created) {
        virtio_gpu_cleanup_object(bo);
        return;
    }
    spin_lock(&vgdev->host_visible_lock);
    mapped = drm_mm_node_allocated(&vram->vram_node);
    spin_unlock(&vgdev->host_visible_lock);
    if (mapped) virtio_gpu_cmd_unmap(vgdev, bo);
    virtio_gpu_cmd_unref_resource(vgdev, bo);     /* its completion calls virtio_gpu_cleanup_object() */
    virtio_gpu_notify(vgdev);
}

static vm_fault_t virtio_gpu_vram_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct drm_gem_object *obj = vma->vm_private_data;
    struct virtio_gpu_object_vram *vram = to_virtio_gpu_vram(gem_to_virtio_gpu_obj(obj));
    unsigned long page = vmf->pgoff - drm_vma_node_start(&obj->vma_node);

    if (page >= vram->vram_node.size >> PAGE_SHIFT) return VM_FAULT_SIGBUS;
    return vmf_insert_pfn(vma, vmf->address, (vram->vram_node.start >> PAGE_SHIFT) + page);
}

static const struct vm_operations_struct virtio_gpu_vram_vm_ops = {
    .fault = virtio_gpu_vram_fault,
    .open  = drm_gem_vm_open,
    .close = drm_gem_vm_close,
};

static int virtio_gpu_vram_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
    struct virtio_gpu_device *vgdev = obj->dev->dev_private;
    struct virtio_gpu_object *bo = gem_to_virtio_gpu_obj(obj);
    struct virtio_gpu_object_vram *vram = to_virtio_gpu_vram(bo);
    unsigned long size = vma->vm_end - vma->vm_start;
    int ret;

    if (!(bo->blob_flags & VIRTGPU_BLOB_FLAG_USE_MAPPABLE)) return -EINVAL;
    ret = wait_event_interruptible(vgdev->resp_wq, vram->map_state != STATE_INITIALIZING);
    if (ret) return ret;
    if (vram->map_state != STATE_OK) return -EINVAL;
    if (size > vram->vram_node.size) return -EINVAL;

    switch (vram->map_info & VIRTIO_GPU_MAP_CACHE_MASK) {
    case VIRTIO_GPU_MAP_CACHE_CACHED:   break;
    case VIRTIO_GPU_MAP_CACHE_WC:       vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot); break;
    case VIRTIO_GPU_MAP_CACHE_UNCACHED: vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); break;
    default:                            return -EINVAL;
    }
    vm_flags_set(vma, VM_PFNMAP | VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
    return 0;
}

static const struct drm_gem_object_funcs virtio_gpu_vram_funcs = {
    .open    = virtio_gpu_gem_object_open,
    .close   = virtio_gpu_gem_object_close,
    .free    = virtio_gpu_vram_free,
    .mmap    = virtio_gpu_vram_mmap,
    .export  = virtgpu_gem_prime_export,
    .vm_ops  = &virtio_gpu_vram_vm_ops,
};

bool virtio_gpu_is_vram(struct virtio_gpu_object *bo)
{
    return bo->base.base.funcs == &virtio_gpu_vram_funcs;
}

int virtio_gpu_vram_create(struct virtio_gpu_device *vgdev, struct virtio_gpu_object_params *params,
                           struct virtio_gpu_object **bo_ptr)
{
    struct virtio_gpu_object_vram *vram;
    struct virtio_gpu_object_array *objs;
    struct drm_gem_object *obj;
    int ret;

    *bo_ptr = NULL;
    vram = kzalloc(sizeof(*vram), GFP_KERNEL);
    if (!vram) return -ENOMEM;
    obj = &vram->base.base.base;
    obj->funcs = &virtio_gpu_vram_funcs;
    params->size = PAGE_ALIGN(params->size);
    drm_gem_private_object_init(vgdev->ddev, obj, params->size);
    ret = drm_gem_create_mmap_offset(obj);
    if (ret) goto free;
    ret = virtio_gpu_resource_id_get(vgdev, &vram->base.hw_res_handle);
    if (ret) goto offset;

    /* the blob flags decide whether it gets a place in the window */
    vram->base.blob_flags = params->blob_flags;
    if (params->blob_flags & VIRTGPU_BLOB_FLAG_USE_MAPPABLE) {
        if (!vgdev->has_host_visible) ret = -EINVAL;
        else {
            spin_lock(&vgdev->host_visible_lock);
            ret = drm_mm_insert_node_generic(&vgdev->host_visible_mm, &vram->vram_node, params->size,
                                             PAGE_SIZE, 0, DRM_MM_INSERT_BEST);
            spin_unlock(&vgdev->host_visible_lock);
        }
        if (ret) {
            virtio_gpu_cleanup_object(&vram->base);     /* the id, the offset and the object */
            return ret;
        }
    }

    virtio_gpu_cmd_resource_create_blob(vgdev, &vram->base, params, NULL, 0);
    if (params->blob_flags & VIRTGPU_BLOB_FLAG_USE_MAPPABLE) {
        objs = virtio_gpu_array_alloc(1);
        if (!objs) { ret = -ENOMEM; goto created; }
        virtio_gpu_array_add_obj(objs, obj);
        ret = virtio_gpu_cmd_map(vgdev, objs, vram->vram_node.start - vgdev->host_visible_region.addr);
        if (ret) { virtio_gpu_array_put_free(objs); goto created; }
    }
    *bo_ptr = &vram->base;
    return 0;

created:
    /* the host has the resource: freeing the object unmaps and drops it */
    drm_gem_object_put(obj);
    return ret;
offset:
    drm_gem_free_mmap_offset(obj);
free:
    drm_gem_object_release(obj);
    kfree(vram);
    return ret;
}

struct sg_table *virtio_gpu_vram_map_dma_buf(struct virtio_gpu_object *bo, struct device *dev,
                                             enum dma_data_direction dir)
{
    return ERR_PTR(-EOPNOTSUPP);                /* see the top of this file */
}

void virtio_gpu_vram_unmap_dma_buf(struct device *dev, struct sg_table *sgt, enum dma_data_direction dir)
{
}
