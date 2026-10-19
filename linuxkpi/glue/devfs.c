/* linuxkpi/glue/devfs.c -- character devices of Linux drivers in /dev
 *
 * A node registered here looks like any other device in /dev. Each open
 * gets a vnode of its own, outside the tree (as /dev/ptmx does), so the
 * Linux side can keep per-open state (its struct file) in it; the open
 * file's last close calls release and frees it.
 *
 * The Linux side locks its own state, so its files and mappings run
 * without the big kernel lock (vnode_ops.nolock, vm_devmap_ops.nolock):
 * opens, reads, writes, ioctls, polls, mmap, page faults and the last
 * close. */
#include <kernel/vfs.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/posix.h>
#include <kernel/vm.h>
#include "../include/lkpi/glue.h"

struct cdev_node {
    const struct lkpi_cdev_ops *ops;
    void         *dev;
    struct vnode *vn;               /* the node in /dev */
};

struct cdev_open {
    struct vnode      vn;           /* first: the open file's node */
    struct cdev_node *node;
    void             *file;         /* the Linux side's per-open state */
};

static struct cdev_open *of(struct file *f) { return (struct cdev_open *)f->vn; }

static ssize_t cdev_fread(struct file *f, void *buf, size_t len)
{
    struct cdev_open *o = of(f);
    if (!o->node->ops->read) return -EINVAL;
    long long pos = (long long)f->off;
    long r = o->node->ops->read(o->file, buf, len, &pos);
    if (r >= 0) f->off = (uint64_t)pos;
    return r;
}

static ssize_t cdev_fwrite(struct file *f, const void *buf, size_t len)
{
    struct cdev_open *o = of(f);
    if (!o->node->ops->write) return -EINVAL;
    long long pos = (long long)f->off;
    long r = o->node->ops->write(o->file, buf, len, &pos);
    if (r >= 0) f->off = (uint64_t)pos;
    return r;
}

static int cdev_fioctl(struct file *f, unsigned long req, void *arg)
{
    struct cdev_open *o = of(f);
    if (!o->node->ops->ioctl) return -ENOTTY;
    return (int)o->node->ops->ioctl(o->file, (unsigned int)req, (unsigned long)arg);
}

static int cdev_poll(struct file *f, int events)
{
    struct cdev_open *o = of(f);
    if (!o->node->ops->poll) return events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM);
    return o->node->ops->poll(o->file, events);
}

/* mmap: Kestrel's device-mapping callbacks forward to the Linux side's,
 * through a small record per region. */
struct dmap {
    const struct lkpi_mmap_ops *ops;
    void *ctx;
};

static int dmap_fault(void *c, uint64_t va, bool write)
{
    struct dmap *d = c;
    return d->ops->fault(d->ctx, va, write);
}

static void *dmap_dup(void *c, struct mm *mm, uint64_t start, uint64_t end, uint64_t off)
{
    struct dmap *d = c, *n = kmalloc(sizeof *n);
    if (!n) return NULL;
    n->ops = d->ops;
    n->ctx = d->ops->dup(d->ctx, mm, start, end, off);
    if (!n->ctx) { kfree(n); return NULL; }
    return n;
}

static void dmap_resize(void *c, uint64_t start, uint64_t end, uint64_t off)
{
    struct dmap *d = c;
    d->ops->resize(d->ctx, start, end, off);
}

static void dmap_close(void *c)
{
    struct dmap *d = c;
    d->ops->close(d->ctx);
    kfree(d);
}

static const struct vm_devmap_ops dmap_ops = {
    .fault = dmap_fault, .dup = dmap_dup, .resize = dmap_resize, .close = dmap_close,
    .nolock = true,
};

static int cdev_mmap(struct file *f, struct vm_devmap *m)
{
    struct cdev_open *o = of(f);
    if (!o->node->ops->mmap) return -ENODEV;
    struct dmap *d = kmalloc(sizeof *d);
    if (!d) return -ENOMEM;
    struct lkpi_mmap lm = { .start = m->start, .end = m->end, .off = m->off, .prot = m->prot,
                            .flags = m->flags, .mm = m->mm };
    int r = o->node->ops->mmap(o->file, &lm);
    if (r) { kfree(d); return r; }
    d->ops = lm.ops;
    d->ctx = lm.ctx;
    m->ops = &dmap_ops;
    m->ctx = d;
    return 0;
}

int lkpi_vm_insert(void *mm, unsigned long va, unsigned long phys, unsigned long cache)
{
    return vm_dev_insert(mm, va, phys, cache);
}

void lkpi_vm_zap(void *mm, unsigned long start, unsigned long end)
{
    vm_dev_zap(mm, start, end);
}

static void cdev_release(struct file *f)
{
    struct cdev_open *o = of(f);
    if (o->node->ops->release) o->node->ops->release(o->file);
    kfree(o);
}

static const struct vnode_ops open_ops = {
    .fread = cdev_fread, .fwrite = cdev_fwrite, .fioctl = cdev_fioctl,
    .poll = cdev_poll, .release = cdev_release, .mmap = cdev_mmap, .nolock = true,
};

/* files without a node: seekable to their size (a dma-buf's size is found
 * with lseek(SEEK_END)) */
static uint64_t anon_size(struct vnode *vn)
{
    struct cdev_open *o = (struct cdev_open *)vn;
    return o->node->ops->size ? o->node->ops->size(o->file) : 0;
}

static const struct vnode_ops anon_ops = {
    .fread = cdev_fread, .fwrite = cdev_fwrite, .fioctl = cdev_fioctl,
    .poll = cdev_poll, .release = cdev_release, .mmap = cdev_mmap, .size = anon_size,
    .nolock = true,
};

static int cdev_open(struct vnode *vn, int flags, struct file **out)
{
    struct cdev_node *n = vn->ctx;
    struct cdev_open *o = kzalloc(sizeof *o);
    if (!o) return -ENOMEM;
    memcpy(&o->vn, vn, sizeof o->vn);           /* name, mode, rdev, owner, fs */
    o->vn.ops = &open_ops;
    o->vn.parent = o->vn.children = o->vn.sibling = o->vn.mounted = NULL;
    o->vn.refs = 0;
    o->node = n;
    if (n->ops->open) {
        int r = n->ops->open(n->dev, flags, &o->file);
        if (r < 0) { kfree(o); return r; }
    }
    struct file *f = vfs_file_new(&o->vn, flags);
    if (!f) {
        if (n->ops->release) n->ops->release(o->file);
        kfree(o);
        return -ENOMEM;
    }
    *out = f;
    return 0;
}

static const struct vnode_ops node_ops = { .open = cdev_open, .nolock = true };

int lkpi_fd_reserve(int cloexec) { return fd_reserve(cloexec != 0); }
void lkpi_fd_unreserve(int fd) { fd_unreserve(fd); }

int lkpi_fd_fill(int fd, const char *name, int flags, const struct lkpi_cdev_ops *ops, void *file)
{
    struct cdev_open *o = kzalloc(sizeof *o + sizeof(struct cdev_node));
    if (!o) return -ENOMEM;
    struct cdev_node *n = (struct cdev_node *)(o + 1);  /* its own: no device behind it */
    n->ops = ops;
    strlcpy(o->vn.name, name, sizeof o->vn.name);
    o->vn.type = VCHR;
    o->vn.mode = S_IFCHR | 0600;
    o->vn.ops = &anon_ops;
    o->node = n;
    o->file = file;
    struct file *f = vfs_file_new(&o->vn, flags & (O_ACCMODE | O_NONBLOCK));
    if (!f) { kfree(o); return -ENOMEM; }
    fd_fill(fd, f);
    return 0;
}

void *lkpi_fd_file(int fd, const struct lkpi_cdev_ops *ops)
{
    struct file *f = fd_file(fd);
    void *lf = NULL;
    if (f && (f->vn->ops == &open_ops || f->vn->ops == &anon_ops) && of(f)->node->ops == ops) {
        lf = of(f)->file;
        if (ops->hold) ops->hold(lf);
    }
    vfs_close(f);
    return lf;
}

void *lkpi_cdev_add(const char *name, unsigned int mode, unsigned int major, unsigned int minor,
                    const struct lkpi_cdev_ops *ops, void *dev)
{
    struct cdev_node *n = kzalloc(sizeof *n);
    if (!n) return NULL;
    n->ops = ops;
    n->dev = dev;
    n->vn = devfs_register(name, S_IFCHR | (mode & 07777), (major << 8) | (minor & 0xFF), &node_ops, n);
    if (!n->vn) { kfree(n); return NULL; }
    return n;
}

/* The node stays in /dev (devfs has no removal yet) but opens fail; files
 * already open keep their callbacks, which the Linux side keeps valid. */
static int gone_open(struct vnode *vn, int flags, struct file **out) { return -ENODEV; }
static const struct vnode_ops gone_ops = { .open = gone_open, .nolock = true };

void lkpi_cdev_remove(void *node)
{
    struct cdev_node *n = node;
    if (n && n->vn) n->vn->ops = &gone_ops;
}

void lkpi_sysfs_class_dev(void *pci, const char *cls, const char *name, const char *devname,
                          unsigned int major, unsigned int minor)
{
    sysfs_class_dev(pci, cls, name, devname, major, minor);
}

void lkpi_sysfs_pci_driver(void *pci, const char *driver) { sysfs_pci_driver(pci, driver); }
