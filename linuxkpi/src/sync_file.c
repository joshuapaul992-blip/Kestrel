/* linuxkpi/src/sync_file.c -- sync files: a fence as a file descriptor
 * (include/linux/sync_file.h)
 *
 * After NetBSD's linux_sync_file.c (sys/external/bsd/drm2/linux), whose
 * notice follows; written here for the KPI's files, with the ioctls Linux
 * gives user space (SYNC_IOC_MERGE, SYNC_IOC_FILE_INFO,
 * SYNC_IOC_SET_DEADLINE). Kestrel's poll() asks again at every wakeup and
 * every 100 ms, so polling only looks at the fence: the first poll turns
 * signaling on, so a fence that signals from an interrupt does.
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
#include <linux/sync_file.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/ktime.h>

/* user space's view (Linux's include/uapi/linux/sync_file.h) */
struct sync_merge_data {
    char    name[32];
    __s32   fd2;
    __s32   fence;
    __u32   flags;
    __u32   pad;
};
struct sync_fence_info {
    char    obj_name[32];
    char    driver_name[32];
    __s32   status;
    __u32   flags;
    __u64   timestamp_ns;
};
struct sync_file_info {
    char    name[32];
    __s32   status;
    __u32   flags;
    __u32   num_fences;
    __u32   pad;
    __u64   sync_fence_info;
};
struct sync_set_deadline {
    __u64   deadline_ns;
    __u64   pad;
};
#define SYNC_IOC_MAGIC          '>'
#define SYNC_IOC_MERGE          _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#define SYNC_IOC_FILE_INFO      _IOWR(SYNC_IOC_MAGIC, 4, struct sync_file_info)
#define SYNC_IOC_SET_DEADLINE   _IOW(SYNC_IOC_MAGIC, 5, struct sync_set_deadline)

static const struct file_operations sync_file_fops;

static struct sync_file *sync_file_alloc(void)
{
    struct sync_file *sf = kzalloc(sizeof(*sf), GFP_KERNEL);
    if (!sf) return NULL;
    sf->file = anon_inode_getfile("sync_file", &sync_file_fops, sf, 0);
    if (IS_ERR(sf->file)) { kfree(sf); return NULL; }
    return sf;
}

struct sync_file *sync_file_create(struct dma_fence *fence)
{
    struct sync_file *sf = sync_file_alloc();
    if (!sf) return NULL;
    sf->fence = dma_fence_get(fence);
    return sf;
}
EXPORT_SYMBOL(sync_file_create);

static struct sync_file *sync_file_fdget(int fd)
{
    struct file *file = fget(fd);
    if (!file) return NULL;
    if (file->f_op != &sync_file_fops) { fput(file); return NULL; }
    return file->private_data;
}

struct dma_fence *sync_file_get_fence(int fd)
{
    struct sync_file *sf = sync_file_fdget(fd);
    struct dma_fence *fence;
    if (!sf) return NULL;
    fence = dma_fence_get(sf->fence);
    fput(sf->file);
    return fence;
}
EXPORT_SYMBOL(sync_file_get_fence);

char *sync_file_get_name(struct sync_file *sf, char *buf, int len)
{
    if (sf->user_name[0])
        strscpy(buf, sf->user_name, len);
    else
        snprintf(buf, len, "%s-%s%llu-%llu", sf->fence->ops->get_driver_name(sf->fence),
                 sf->fence->ops->get_timeline_name(sf->fence),
                 (unsigned long long)sf->fence->context, (unsigned long long)sf->fence->seqno);
    return buf;
}

static int sync_file_release(struct inode *inode, struct file *file)
{
    struct sync_file *sf = file->private_data;
    dma_fence_put(sf->fence);
    kfree(sf);
    return 0;
}

static __poll_t sync_file_poll(struct file *file, poll_table *wait)
{
    struct sync_file *sf = file->private_data;
    dma_fence_enable_sw_signaling(sf->fence);
    return dma_fence_is_signaled(sf->fence) ? EPOLLIN : 0;
}

/* ---- merging: every fence of both, at most one per context (the later) ---------------- */
static int add_fence(struct dma_fence **fences, int n, struct dma_fence *f)
{
    if (dma_fence_is_signaled(f)) return n;
    for (int i = 0; i < n; i++) {
        if (fences[i]->context == f->context) {
            if (__dma_fence_is_later(f->seqno, fences[i]->seqno, f->ops)) {
                dma_fence_put(fences[i]);
                fences[i] = dma_fence_get(f);
            }
            return n;
        }
    }
    fences[n] = dma_fence_get(f);
    return n + 1;
}

static int count_fences(struct dma_fence *f)
{
    struct dma_fence_array *a = to_dma_fence_array(f);
    return a ? (int)a->num_fences : 1;
}

static int add_all(struct dma_fence **fences, int n, struct dma_fence *f)
{
    struct dma_fence_array *a = to_dma_fence_array(f);
    if (!a) return add_fence(fences, n, f);
    for (unsigned i = 0; i < a->num_fences; i++) n = add_fence(fences, n, a->fences[i]);
    return n;
}

static struct sync_file *sync_file_merge(const char *name, struct sync_file *a, struct sync_file *b)
{
    int max = count_fences(a->fence) + count_fences(b->fence), n = 0;
    struct dma_fence **fences = kcalloc(max, sizeof(*fences), GFP_KERNEL);
    struct dma_fence *fence;
    struct sync_file *sf;

    if (!fences) return NULL;
    n = add_all(fences, n, a->fence);
    n = add_all(fences, n, b->fence);
    if (n == 0) {
        fence = dma_fence_get_stub();
        kfree(fences);
    } else if (n == 1) {
        fence = fences[0];
        kfree(fences);
    } else {
        struct dma_fence_array *array = dma_fence_array_create(n, fences, dma_fence_context_alloc(1), 1, false);
        if (!array) {
            while (n--) dma_fence_put(fences[n]);
            kfree(fences);
            return NULL;
        }
        fence = &array->base;
    }
    sf = sync_file_alloc();
    if (!sf) { dma_fence_put(fence); return NULL; }
    sf->fence = fence;
    strscpy(sf->user_name, name, sizeof(sf->user_name));
    return sf;
}

static long ioctl_merge(struct sync_file *sf, unsigned long arg)
{
    struct sync_merge_data data;
    struct sync_file *other, *merged;
    int fd, err;

    if (copy_from_user(&data, (void __user *)arg, sizeof(data))) return -EFAULT;
    if (data.flags || data.pad) return -EINVAL;
    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) return fd;
    other = sync_file_fdget(data.fd2);
    if (!other) { err = -ENOENT; goto out_fd; }
    data.name[sizeof(data.name) - 1] = 0;
    merged = sync_file_merge(data.name, sf, other);
    fput(other->file);
    if (!merged) { err = -ENOMEM; goto out_fd; }
    data.fence = fd;
    if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
        fput(merged->file);
        err = -EFAULT;
        goto out_fd;
    }
    fd_install(fd, merged->file);
    return 0;
out_fd:
    put_unused_fd(fd);
    return err;
}

static void fill_info(struct dma_fence *f, struct sync_fence_info *info)
{
    strscpy(info->obj_name, f->ops->get_timeline_name(f), sizeof(info->obj_name));
    strscpy(info->driver_name, f->ops->get_driver_name(f), sizeof(info->driver_name));
    info->status = dma_fence_get_status(f);
    info->timestamp_ns = info->status ? ktime_to_ns(f->timestamp) : 0;
}

static long ioctl_file_info(struct sync_file *sf, unsigned long arg)
{
    struct sync_file_info info;
    struct dma_fence_array *a = to_dma_fence_array(sf->fence);
    unsigned n = a ? a->num_fences : 1;

    if (copy_from_user(&info, (void __user *)arg, sizeof(info))) return -EFAULT;
    if (info.flags || info.pad) return -EINVAL;
    /* num_fences 0: only how many there are */
    if (info.num_fences) {
        struct sync_fence_info *fi;
        if (info.num_fences < n) return -EINVAL;
        fi = kcalloc(n, sizeof(*fi), GFP_KERNEL);
        if (!fi) return -ENOMEM;
        for (unsigned i = 0; i < n; i++) fill_info(a ? a->fences[i] : sf->fence, &fi[i]);
        if (copy_to_user(u64_to_user_ptr(info.sync_fence_info), fi, n * sizeof(*fi))) {
            kfree(fi);
            return -EFAULT;
        }
        kfree(fi);
    }
    sync_file_get_name(sf, info.name, sizeof(info.name));
    info.status = dma_fence_get_status(sf->fence);
    info.num_fences = n;
    return copy_to_user((void __user *)arg, &info, sizeof(info)) ? -EFAULT : 0;
}

static long sync_file_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct sync_file *sf = file->private_data;
    struct sync_set_deadline d;

    switch (cmd) {
    case SYNC_IOC_MERGE:
        return ioctl_merge(sf, arg);
    case SYNC_IOC_FILE_INFO:
        return ioctl_file_info(sf, arg);
    case SYNC_IOC_SET_DEADLINE:
        if (copy_from_user(&d, (void __user *)arg, sizeof(d))) return -EFAULT;
        if (d.pad) return -EINVAL;
        dma_fence_set_deadline(sf->fence, ns_to_ktime(d.deadline_ns));
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations sync_file_fops = {
    .release = sync_file_release,
    .poll = sync_file_poll,
    .unlocked_ioctl = sync_file_ioctl,
    .compat_ioctl = sync_file_ioctl,
};
