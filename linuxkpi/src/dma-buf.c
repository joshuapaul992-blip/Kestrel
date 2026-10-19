/* linuxkpi/src/dma-buf.c -- buffers shared between drivers and with user
 * space (include/linux/dma-buf.h)
 *
 * After NetBSD's linux_dma_buf.c (sys/external/bsd/drm2/linux), whose
 * notice follows; written here for the KPI's files, where the buffer lives
 * as long as its struct file (as on Linux, so get_dma_buf is get_file),
 * with the parts drm_prime and user space use: attachments and their
 * mappings, vmap, CPU access, mmap, poll, lseek for the size, and the
 * ioctls (DMA_BUF_IOCTL_SYNC, DMA_BUF_SET_NAME, and moving fences in and
 * out as sync files). Kestrel's poll() asks again at every wakeup and every
 * 100 ms, so polling only tests the reservation's fences.
 *
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-mapping.h>
#include <linux/sync_file.h>
#include <linux/pseudo_fs.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include "internal.h"

/* user space's view (Linux's include/uapi/linux/dma-buf.h) */
struct dma_buf_sync { __u64 flags; };
#define DMA_BUF_SYNC_READ       (1 << 0)
#define DMA_BUF_SYNC_WRITE      (2 << 0)
#define DMA_BUF_SYNC_RW         (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START      (0 << 2)
#define DMA_BUF_SYNC_END        (1 << 2)
#define DMA_BUF_SYNC_VALID_FLAGS_MASK (DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END)
struct dma_buf_export_sync_file { __u32 flags; __s32 fd; };
struct dma_buf_import_sync_file { __u32 flags; __s32 fd; };
#define DMA_BUF_NAME_LEN        32
#define DMA_BUF_BASE            'b'
#define DMA_BUF_IOCTL_SYNC      _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)
#define DMA_BUF_SET_NAME_A      _IOW(DMA_BUF_BASE, 1, __u32)
#define DMA_BUF_SET_NAME_B      _IOW(DMA_BUF_BASE, 1, __u64)
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE _IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#define DMA_BUF_IOCTL_IMPORT_SYNC_FILE _IOW(DMA_BUF_BASE, 3, struct dma_buf_import_sync_file)

static const struct file_operations dma_buf_fops;

/* a buffer whose exporter gave no reservation object has its own, after it */
struct dma_buf_own {
    struct dma_buf  buf;
    struct dma_resv resv;
};

struct dma_buf *dma_buf_export(const struct dma_buf_export_info *info)
{
    struct dma_buf *dmabuf;
    struct inode *inode;
    struct file *file;

    if (WARN_ON(!info->priv || !info->ops || !info->ops->map_dma_buf || !info->ops->unmap_dma_buf ||
                !info->ops->release))
        return ERR_PTR(-EINVAL);
    if (WARN_ON(info->ops->cache_sgt_mapping && (info->ops->pin || info->ops->unpin)))
        return ERR_PTR(-EINVAL);
    if (!info->resv) {
        struct dma_buf_own *own = kzalloc(sizeof(*own), GFP_KERNEL);
        if (!own) return ERR_PTR(-ENOMEM);
        dma_resv_init(&own->resv);
        own->buf.resv = &own->resv;
        dmabuf = &own->buf;
    } else {
        dmabuf = kzalloc(sizeof(*dmabuf), GFP_KERNEL);
        if (!dmabuf) return ERR_PTR(-ENOMEM);
        dmabuf->resv = info->resv;
    }
    dmabuf->priv = info->priv;
    dmabuf->ops = info->ops;
    dmabuf->size = info->size;
    dmabuf->exp_name = info->exp_name;
    dmabuf->owner = info->owner;
    spin_lock_init(&dmabuf->name_lock);
    INIT_LIST_HEAD(&dmabuf->attachments);

    /* an inode of its own, so lseek and fstat see the size */
    inode = alloc_anon_inode(NULL);
    if (IS_ERR(inode)) { file = ERR_CAST(inode); goto fail; }
    inode->i_size = (loff_t)info->size;
    file = lkpi_file_new(&dma_buf_fops, inode, (unsigned int)info->flags & (O_ACCMODE | O_NONBLOCK), dmabuf);
    if (IS_ERR(file)) { iput(inode); goto fail; }
    lkpi_file_set_name(file, "dmabuf");
    dmabuf->file = file;
    return dmabuf;

fail:
    if (!info->resv) dma_resv_fini(dmabuf->resv);
    kfree(info->resv ? dmabuf : (void *)container_of(dmabuf, struct dma_buf_own, buf));
    return ERR_CAST(file);
}
EXPORT_SYMBOL(dma_buf_export);

/* the descriptor takes the caller's reference */
int dma_buf_fd(struct dma_buf *dmabuf, int flags)
{
    int fd;

    if (!dmabuf || !dmabuf->file) return -EINVAL;
    fd = get_unused_fd_flags((unsigned int)flags);
    if (fd < 0) return fd;
    fd_install((unsigned int)fd, dmabuf->file);
    return fd;
}
EXPORT_SYMBOL(dma_buf_fd);

struct dma_buf *dma_buf_get(int fd)
{
    struct file *file = fget((unsigned int)fd);

    if (!file) return ERR_PTR(-EBADF);
    if (file->f_op != &dma_buf_fops) {
        fput(file);
        return ERR_PTR(-EINVAL);
    }
    return file->private_data;
}
EXPORT_SYMBOL(dma_buf_get);

void dma_buf_put(struct dma_buf *dmabuf)
{
    if (WARN_ON(!dmabuf || !dmabuf->file)) return;
    fput(dmabuf->file);
}
EXPORT_SYMBOL(dma_buf_put);

static int dma_buf_file_release(struct inode *inode, struct file *file)
{
    struct dma_buf *dmabuf = file->private_data;
    bool own = dmabuf->resv == &container_of(dmabuf, struct dma_buf_own, buf)->resv;

    WARN_ON(dmabuf->vmapping_counter);
    WARN_ON(!list_empty(&dmabuf->attachments));
    dmabuf->ops->release(dmabuf);
    kfree(dmabuf->name);
    if (own) {
        dma_resv_fini(dmabuf->resv);
        kfree(container_of(dmabuf, struct dma_buf_own, buf));
    } else {
        kfree(dmabuf);
    }
    return 0;
}

/* ---- attachments ------------------------------------------------------------------------ */
static struct sg_table *map_locked(struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
    struct sg_table *sgt = attach->dmabuf->ops->map_dma_buf(attach, dir);
    if (!sgt) sgt = ERR_PTR(-ENOMEM);
    return sgt;
}

struct dma_buf_attachment *dma_buf_dynamic_attach(struct dma_buf *dmabuf, struct device *dev,
                                                  const struct dma_buf_attach_ops *importer_ops,
                                                  void *importer_priv)
{
    struct dma_buf_attachment *attach;
    int ret;

    if (WARN_ON(!dmabuf || !dev)) return ERR_PTR(-EINVAL);
    if (WARN_ON(importer_ops && !importer_ops->move_notify)) return ERR_PTR(-EINVAL);
    attach = kzalloc(sizeof(*attach), GFP_KERNEL);
    if (!attach) return ERR_PTR(-ENOMEM);
    attach->dev = dev;
    attach->dmabuf = dmabuf;
    if (importer_ops) attach->peer2peer = importer_ops->allow_peer2peer;
    attach->importer_ops = importer_ops;
    attach->importer_priv = importer_priv;

    if (dmabuf->ops->attach) {
        ret = dmabuf->ops->attach(dmabuf, attach);
        if (ret) goto fail_free;
    }
    dma_resv_lock(dmabuf->resv, NULL);
    list_add(&attach->node, &dmabuf->attachments);
    dma_resv_unlock(dmabuf->resv);

    /* an importer that cannot follow moves of a buffer that can move: pin
     * the buffer and map it now, for good */
    if (dma_buf_attachment_is_dynamic(attach) != dma_buf_is_dynamic(dmabuf)) {
        struct sg_table *sgt;

        dma_resv_lock(dmabuf->resv, NULL);
        if (dma_buf_is_dynamic(dmabuf)) {
            ret = dmabuf->ops->pin(attach);
            if (ret) goto fail_unlock;
        }
        sgt = map_locked(attach, DMA_BIDIRECTIONAL);
        if (IS_ERR(sgt)) {
            ret = (int)PTR_ERR(sgt);
            if (dma_buf_is_dynamic(dmabuf)) dmabuf->ops->unpin(attach);
            goto fail_unlock;
        }
        dma_resv_unlock(dmabuf->resv);
        attach->sgt = sgt;
        attach->dir = DMA_BIDIRECTIONAL;
    }
    return attach;

fail_unlock:
    dma_resv_unlock(dmabuf->resv);
    dma_buf_detach(dmabuf, attach);
    return ERR_PTR(ret);
fail_free:
    kfree(attach);
    return ERR_PTR(ret);
}
EXPORT_SYMBOL(dma_buf_dynamic_attach);

struct dma_buf_attachment *dma_buf_attach(struct dma_buf *dmabuf, struct device *dev)
{
    return dma_buf_dynamic_attach(dmabuf, dev, NULL, NULL);
}
EXPORT_SYMBOL(dma_buf_attach);

void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach)
{
    if (WARN_ON(!dmabuf || !attach || dmabuf != attach->dmabuf)) return;
    dma_resv_lock(dmabuf->resv, NULL);
    if (attach->sgt) {
        dmabuf->ops->unmap_dma_buf(attach, attach->sgt, attach->dir);
        if (dma_buf_is_dynamic(dmabuf)) dmabuf->ops->unpin(attach);
    }
    list_del(&attach->node);
    dma_resv_unlock(dmabuf->resv);
    if (dmabuf->ops->detach) dmabuf->ops->detach(dmabuf, attach);
    kfree(attach);
}
EXPORT_SYMBOL(dma_buf_detach);

int dma_buf_pin(struct dma_buf_attachment *attach)
{
    struct dma_buf *dmabuf = attach->dmabuf;
    WARN_ON(!dma_buf_attachment_is_dynamic(attach));
    dma_resv_assert_held(dmabuf->resv);
    return dmabuf->ops->pin ? dmabuf->ops->pin(attach) : 0;
}
EXPORT_SYMBOL(dma_buf_pin);

void dma_buf_unpin(struct dma_buf_attachment *attach)
{
    struct dma_buf *dmabuf = attach->dmabuf;
    WARN_ON(!dma_buf_attachment_is_dynamic(attach));
    dma_resv_assert_held(dmabuf->resv);
    if (dmabuf->ops->unpin) dmabuf->ops->unpin(attach);
}
EXPORT_SYMBOL(dma_buf_unpin);

/* with the reservation locked */
struct sg_table *dma_buf_map_attachment(struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
    struct dma_buf *dmabuf;
    struct sg_table *sgt;
    int ret;

    if (WARN_ON(!attach || !attach->dmabuf)) return ERR_PTR(-EINVAL);
    dmabuf = attach->dmabuf;
    dma_resv_assert_held(dmabuf->resv);
    if (attach->sgt) {                  /* the one mapping kept since attach */
        if (attach->dir != dir && attach->dir != DMA_BIDIRECTIONAL) return ERR_PTR(-EBUSY);
        return attach->sgt;
    }
    if (dma_buf_is_dynamic(dmabuf) && !dma_buf_attachment_is_dynamic(attach)) {
        ret = dmabuf->ops->pin(attach);
        if (ret) return ERR_PTR(ret);
    }
    sgt = map_locked(attach, dir);
    if (IS_ERR(sgt)) {
        if (dma_buf_is_dynamic(dmabuf) && !dma_buf_attachment_is_dynamic(attach)) dmabuf->ops->unpin(attach);
        return sgt;
    }
    if (dmabuf->ops->cache_sgt_mapping) {
        attach->sgt = sgt;
        attach->dir = dir;
    }
    return sgt;
}
EXPORT_SYMBOL(dma_buf_map_attachment);

struct sg_table *dma_buf_map_attachment_unlocked(struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
    struct sg_table *sgt;

    if (WARN_ON(!attach || !attach->dmabuf)) return ERR_PTR(-EINVAL);
    dma_resv_lock(attach->dmabuf->resv, NULL);
    sgt = dma_buf_map_attachment(attach, dir);
    dma_resv_unlock(attach->dmabuf->resv);
    return sgt;
}
EXPORT_SYMBOL(dma_buf_map_attachment_unlocked);

void dma_buf_unmap_attachment(struct dma_buf_attachment *attach, struct sg_table *sgt,
                              enum dma_data_direction dir)
{
    struct dma_buf *dmabuf;

    if (WARN_ON(!attach || !attach->dmabuf || !sgt)) return;
    dmabuf = attach->dmabuf;
    dma_resv_assert_held(dmabuf->resv);
    if (attach->sgt == sgt) return;     /* kept until detach */
    dmabuf->ops->unmap_dma_buf(attach, sgt, dir);
    if (dma_buf_is_dynamic(dmabuf) && !dma_buf_attachment_is_dynamic(attach)) dmabuf->ops->unpin(attach);
}
EXPORT_SYMBOL(dma_buf_unmap_attachment);

void dma_buf_unmap_attachment_unlocked(struct dma_buf_attachment *attach, struct sg_table *sgt,
                                       enum dma_data_direction dir)
{
    if (WARN_ON(!attach || !attach->dmabuf || !sgt)) return;
    dma_resv_lock(attach->dmabuf->resv, NULL);
    dma_buf_unmap_attachment(attach, sgt, dir);
    dma_resv_unlock(attach->dmabuf->resv);
}
EXPORT_SYMBOL(dma_buf_unmap_attachment_unlocked);

/* the buffer moved: importers that follow moves map it again */
void dma_buf_move_notify(struct dma_buf *dmabuf)
{
    struct dma_buf_attachment *attach;

    dma_resv_assert_held(dmabuf->resv);
    list_for_each_entry(attach, &dmabuf->attachments, node)
        if (attach->importer_ops) attach->importer_ops->move_notify(attach);
}
EXPORT_SYMBOL(dma_buf_move_notify);

/* ---- the CPU's access ----------------------------------------------------------------------- */
/* First wait for the work that would change what the CPU reads (writers),
 * or, if the CPU writes, also for the work reading it. */
int dma_buf_begin_cpu_access(struct dma_buf *dmabuf, enum dma_data_direction dir)
{
    bool write = dir == DMA_BIDIRECTIONAL || dir == DMA_TO_DEVICE;
    long r;

    if (WARN_ON(!dmabuf)) return -EINVAL;
    r = dma_resv_wait_timeout(dmabuf->resv, dma_resv_usage_rw(write), true, MAX_SCHEDULE_TIMEOUT);
    if (r < 0) return (int)r;
    return dmabuf->ops->begin_cpu_access ? dmabuf->ops->begin_cpu_access(dmabuf, dir) : 0;
}
EXPORT_SYMBOL(dma_buf_begin_cpu_access);

int dma_buf_end_cpu_access(struct dma_buf *dmabuf, enum dma_data_direction dir)
{
    if (WARN_ON(!dmabuf)) return -EINVAL;
    return dmabuf->ops->end_cpu_access ? dmabuf->ops->end_cpu_access(dmabuf, dir) : 0;
}
EXPORT_SYMBOL(dma_buf_end_cpu_access);

/* map the buffer into vma from page pgoff: the region maps the buffer's file from now on */
int dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma, unsigned long pgoff)
{
    if (WARN_ON(!dmabuf || !vma)) return -EINVAL;
    if (!dmabuf->ops->mmap) return -EINVAL;
    if (pgoff + vma_pages(vma) < pgoff || pgoff + vma_pages(vma) > dmabuf->size >> PAGE_SHIFT) return -EINVAL;
    vma_set_file(vma, dmabuf->file);
    vma->vm_pgoff = pgoff;
    return dmabuf->ops->mmap(dmabuf, vma);
}
EXPORT_SYMBOL(dma_buf_mmap);

/* with the reservation locked; counted, so the exporter maps it once */
int dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
    struct iosys_map ptr;
    int ret;

    iosys_map_clear(map);
    if (WARN_ON(!dmabuf)) return -EINVAL;
    dma_resv_assert_held(dmabuf->resv);
    if (!dmabuf->ops->vmap) return -EINVAL;
    if (dmabuf->vmapping_counter) {
        dmabuf->vmapping_counter++;
        *map = dmabuf->vmap_ptr;
        return 0;
    }
    ret = dmabuf->ops->vmap(dmabuf, &ptr);
    if (WARN_ON_ONCE(ret)) return ret;
    dmabuf->vmap_ptr = ptr;
    dmabuf->vmapping_counter = 1;
    *map = ptr;
    return 0;
}
EXPORT_SYMBOL(dma_buf_vmap);

int dma_buf_vmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map)
{
    int ret;

    iosys_map_clear(map);
    if (WARN_ON(!dmabuf)) return -EINVAL;
    dma_resv_lock(dmabuf->resv, NULL);
    ret = dma_buf_vmap(dmabuf, map);
    dma_resv_unlock(dmabuf->resv);
    return ret;
}
EXPORT_SYMBOL(dma_buf_vmap_unlocked);

void dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
    if (WARN_ON(!dmabuf)) return;
    dma_resv_assert_held(dmabuf->resv);
    if (WARN_ON(!dmabuf->vmapping_counter)) return;
    if (--dmabuf->vmapping_counter == 0) {
        if (dmabuf->ops->vunmap) dmabuf->ops->vunmap(dmabuf, map);
        iosys_map_clear(&dmabuf->vmap_ptr);
    }
}
EXPORT_SYMBOL(dma_buf_vunmap);

void dma_buf_vunmap_unlocked(struct dma_buf *dmabuf, struct iosys_map *map)
{
    if (WARN_ON(!dmabuf)) return;
    dma_resv_lock(dmabuf->resv, NULL);
    dma_buf_vunmap(dmabuf, map);
    dma_resv_unlock(dmabuf->resv);
}
EXPORT_SYMBOL(dma_buf_vunmap_unlocked);

/* ---- the file -------------------------------------------------------------------------------- */
static int dma_buf_file_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct dma_buf *dmabuf = file->private_data;

    if (!dmabuf->ops->mmap) return -EINVAL;
    if (vma->vm_pgoff + vma_pages(vma) > dmabuf->size >> PAGE_SHIFT) return -EINVAL;
    return dmabuf->ops->mmap(dmabuf, vma);
}

/* the size, as libdrm and Mesa find it: lseek(fd, 0, SEEK_END) */
static loff_t dma_buf_llseek(struct file *file, loff_t offset, int whence)
{
    struct dma_buf *dmabuf = file->private_data;

    if (offset) return -EINVAL;
    if (whence == SEEK_END) return (loff_t)dmabuf->size;
    if (whence == SEEK_SET) return 0;
    return -EINVAL;
}

/* readable when no work writes it, writable when no work uses it */
static __poll_t dma_buf_poll(struct file *file, poll_table *poll)
{
    struct dma_buf *dmabuf = file->private_data;
    __poll_t events = poll_requested_events(poll) & (EPOLLIN | EPOLLOUT), ready = 0;

    if (events & EPOLLOUT && dma_resv_test_signaled(dmabuf->resv, DMA_RESV_USAGE_READ))
        ready |= EPOLLOUT;
    if (events & EPOLLIN && dma_resv_test_signaled(dmabuf->resv, DMA_RESV_USAGE_WRITE))
        ready |= EPOLLIN;
    return ready;
}

static long ioctl_set_name(struct dma_buf *dmabuf, const char __user *uname)
{
    char *name = kzalloc(DMA_BUF_NAME_LEN, GFP_KERNEL);
    long n;

    if (!name) return -ENOMEM;
    n = strncpy_from_user(name, uname, DMA_BUF_NAME_LEN - 1);
    if (n < 0) { kfree(name); return n; }
    spin_lock(&dmabuf->name_lock);
    kfree(dmabuf->name);
    dmabuf->name = name;
    spin_unlock(&dmabuf->name_lock);
    return 0;
}

/* the buffer's fences as a sync file: those of writers (for a reader), or of every user */
static long ioctl_export_sync_file(struct dma_buf *dmabuf, void __user *arg)
{
    struct dma_buf_export_sync_file arg_;
    struct dma_fence *fence = NULL;
    struct sync_file *sf;
    int fd, ret;

    if (copy_from_user(&arg_, arg, sizeof(arg_))) return -EFAULT;
    if (arg_.flags & ~DMA_BUF_SYNC_RW || !(arg_.flags & DMA_BUF_SYNC_RW)) return -EINVAL;
    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) return fd;
    ret = dma_resv_get_singleton(dmabuf->resv, dma_resv_usage_rw(arg_.flags & DMA_BUF_SYNC_WRITE), &fence);
    if (ret) goto fail_fd;
    if (!fence) fence = dma_fence_get_stub();
    sf = sync_file_create(fence);
    dma_fence_put(fence);
    if (!sf) { ret = -ENOMEM; goto fail_fd; }
    arg_.fd = fd;
    if (copy_to_user(arg, &arg_, sizeof(arg_))) {
        fput(sf->file);
        ret = -EFAULT;
        goto fail_fd;
    }
    fd_install((unsigned int)fd, sf->file);
    return 0;
fail_fd:
    put_unused_fd((unsigned int)fd);
    return ret;
}

/* a sync file's fence joins the buffer's: as a writer's, or a reader's */
static long ioctl_import_sync_file(struct dma_buf *dmabuf, const void __user *arg)
{
    struct dma_buf_import_sync_file arg_;
    struct dma_fence *fence;
    int ret;

    if (copy_from_user(&arg_, arg, sizeof(arg_))) return -EFAULT;
    if (arg_.flags & ~DMA_BUF_SYNC_RW || !(arg_.flags & DMA_BUF_SYNC_RW)) return -EINVAL;
    fence = sync_file_get_fence(arg_.fd);
    if (!fence) return -EINVAL;
    dma_resv_lock(dmabuf->resv, NULL);
    ret = dma_resv_reserve_fences(dmabuf->resv, 1);
    if (!ret)
        dma_resv_add_fence(dmabuf->resv, fence,
                           arg_.flags & DMA_BUF_SYNC_WRITE ? DMA_RESV_USAGE_WRITE : DMA_RESV_USAGE_READ);
    dma_resv_unlock(dmabuf->resv);
    dma_fence_put(fence);
    return ret;
}

static long dma_buf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct dma_buf *dmabuf = file->private_data;
    struct dma_buf_sync sync;
    enum dma_data_direction dir;

    switch (cmd) {
    case DMA_BUF_IOCTL_SYNC:
        if (copy_from_user(&sync, (void __user *)arg, sizeof(sync))) return -EFAULT;
        if (sync.flags & ~DMA_BUF_SYNC_VALID_FLAGS_MASK) return -EINVAL;
        switch (sync.flags & DMA_BUF_SYNC_RW) {
        case DMA_BUF_SYNC_READ:  dir = DMA_FROM_DEVICE; break;
        case DMA_BUF_SYNC_WRITE: dir = DMA_TO_DEVICE; break;
        case DMA_BUF_SYNC_RW:    dir = DMA_BIDIRECTIONAL; break;
        default:                 return -EINVAL;
        }
        return sync.flags & DMA_BUF_SYNC_END ? dma_buf_end_cpu_access(dmabuf, dir)
                                             : dma_buf_begin_cpu_access(dmabuf, dir);
    case DMA_BUF_SET_NAME_A:
    case DMA_BUF_SET_NAME_B:
        return ioctl_set_name(dmabuf, (const char __user *)arg);
    case DMA_BUF_IOCTL_EXPORT_SYNC_FILE:
        return ioctl_export_sync_file(dmabuf, (void __user *)arg);
    case DMA_BUF_IOCTL_IMPORT_SYNC_FILE:
        return ioctl_import_sync_file(dmabuf, (const void __user *)arg);
    default:
        return -ENOTTY;
    }
}

static const struct file_operations dma_buf_fops = {
    .release = dma_buf_file_release,
    .mmap = dma_buf_file_mmap,
    .llseek = dma_buf_llseek,
    .poll = dma_buf_poll,
    .unlocked_ioctl = dma_buf_ioctl,
    .compat_ioctl = dma_buf_ioctl,
};
