/* fs/fuse.c -- FUSE: file systems served by a program through /dev/fuse
 *
 * A program (squashfuse, an AppImage's runtime, sshfs ...) opens /dev/fuse
 * and mounts it with mount(2) ("fuse.NAME", data "fd=N,rootmode=40000,
 * user_id=0,group_id=0"). From then on the kernel turns lookups, listings
 * and reads of that mount into requests the program reads from its
 * descriptor, and the program writes the answers back. The messages are
 * those of the FUSE protocol, version 7 (Linux's include/uapi/linux/
 * fuse.h describes them; the layouts below are written from that
 * description).
 *
 * This is the read-only half, which is what AppImages need:
 *   - INIT (sent at mount, answered asynchronously: the program only starts
 *     reading after mount() returns), then LOOKUP, READDIR or READDIRPLUS,
 *     OPENDIR/RELEASEDIR, READLINK, OPEN and READ, STATFS.
 *   - A directory's entries are fetched all at once, the first time a path
 *     reaches it (vnode_ops.load, without the tree's lock), with each one's
 *     attributes (READDIRPLUS, or a LOOKUP per name) and a symbolic link's
 *     target. They are not asked for again: the file system is taken to
 *     stay as it is while mounted, as an AppImage's does.
 *   - File data goes through the page cache (pagecache.h), filled by READs
 *     of up to FUSE_READ_MAX bytes, so programs and libraries on the mount
 *     map shared pages like on a disk.
 *   - The mount is read-only (EROFS); nothing is ever written, so there is
 *     no WRITE, CREATE, SETATTR and so on. FORGET is never sent: the
 *     program keeps every node it was asked about until unmounted.
 *
 * Waiting: a request sleeps until its answer, or until the connection
 * ends (the program closes /dev/fuse: ENOTCONN), or the waiter gets
 * SIGKILL. Unmounting hangs up: the program's next read gets ENODEV,
 * which is how libfuse knows to exit. */
#include <kernel/vfs.h>
#include <kernel/uaccess.h>
#include <kernel/fuse.h>
#include <kernel/task.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>
#include <kernel/kmutex.h>
#include <kernel/pagecache.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/klog.h>
#include <kernel/time.h>
#include <kernel/posix.h>
#include <kernel/cpu.h>
#include <kernel/lockcheck.h>

#define FUSE_KERNEL_VERSION 7
#define FUSE_KERNEL_MINOR   31
#define FUSE_ROOT_ID        1
#define FUSE_READ_MAX       (32 * 1024)     /* a READ's answer fits one bounce buffer */
#define FUSE_DIRBUF         8192            /* bytes of entries asked for per READDIR */

enum {
    FUSE_LOOKUP = 1, FUSE_FORGET = 2, FUSE_GETATTR = 3, FUSE_READLINK = 5, FUSE_OPEN = 14,
    FUSE_READ = 15, FUSE_STATFS = 17, FUSE_RELEASE = 18, FUSE_INIT = 26, FUSE_OPENDIR = 27,
    FUSE_READDIR = 28, FUSE_RELEASEDIR = 29, FUSE_READDIRPLUS = 44,
};
#define FUSE_ASYNC_READ      (1u << 0)
#define FUSE_BIG_WRITES      (1u << 5)
#define FUSE_DO_READDIRPLUS  (1u << 13)
#define FUSE_PARALLEL_DIROPS (1u << 18)

struct fuse_in_header  { uint32_t len, opcode; uint64_t unique, nodeid; uint32_t uid, gid, pid; uint16_t total_extlen, padding; };
struct fuse_out_header { uint32_t len; int32_t error; uint64_t unique; };
struct fuse_attr {
    uint64_t ino, size, blocks, atime, mtime, ctime;
    uint32_t atimensec, mtimensec, ctimensec, mode, nlink, uid, gid, rdev, blksize, flags;
};
struct fuse_entry_out { uint64_t nodeid, generation, entry_valid, attr_valid;
                        uint32_t entry_valid_nsec, attr_valid_nsec; struct fuse_attr attr; };
struct fuse_init_in   { uint32_t major, minor, max_readahead, flags; };
struct fuse_init_out  { uint32_t major, minor, max_readahead, flags; uint16_t max_background, congestion_threshold;
                        uint32_t max_write, time_gran; uint16_t max_pages, map_alignment; uint32_t flags2, unused[7]; };
struct fuse_open_in   { uint32_t flags, open_flags; };
struct fuse_open_out  { uint64_t fh; uint32_t open_flags; int32_t backing_id; };
struct fuse_read_in   { uint64_t fh, offset; uint32_t size, read_flags; uint64_t lock_owner; uint32_t flags, padding; };
struct fuse_release_in { uint64_t fh; uint32_t flags, release_flags; uint64_t lock_owner; };
struct fuse_dirent    { uint64_t ino, off; uint32_t namelen, type; char name[]; };
struct fuse_kstatfs   { uint64_t blocks, bfree, bavail, files, ffree; uint32_t bsize, namelen, frsize, padding, spare[6]; };
_Static_assert(sizeof(struct fuse_in_header) == 40, "fuse_in_header");
_Static_assert(sizeof(struct fuse_attr) == 88, "fuse_attr");
_Static_assert(sizeof(struct fuse_entry_out) == 128, "fuse_entry_out");
_Static_assert(sizeof(struct fuse_init_out) == 64, "fuse_init_out");
_Static_assert(sizeof(struct fuse_read_in) == 40, "fuse_read_in");
_Static_assert(sizeof(struct fuse_kstatfs) == 80, "fuse_kstatfs");

struct fuse_req {
    struct fuse_req *next;
    uint64_t unique;
    uint32_t opcode;
    uint32_t msglen;            /* the whole request, header first, in msg */
    uint8_t *msg;
    void    *out;               /* the answer's payload goes here (NULL: dropped) */
    uint32_t outcap, outlen;
    int      error;             /* the answer's (negative errno) */
    bool     sent, done;
    bool     orphan;            /* nobody waits any more: the answer frees it */
};

struct fuse_conn {
    struct spinlock lock;
    int      refs;              /* the /dev/fuse file, the mount, each node */
    int      devs;              /* /dev/fuse files on it (FUSE_DEV_IOC_CLONE) */
    bool     connected;         /* until the program closes /dev/fuse or the mount goes */
    bool     mounted;
    bool     initialized;       /* INIT answered */
    bool     readdirplus;
    uint32_t minor;
    uint64_t next_unique;
    struct fuse_req *pending, *sent;    /* to be read; read and awaiting their answers */
    struct fuse_init_out init_out;
    struct vnode dev_vn;        /* the open /dev/fuse */
    char     name[64];          /* the type, for messages */
};

/* Every FUSE node's ctx */
struct fuse_node {
    struct fuse_conn *conn;
    uint64_t nodeid;
    struct kmutex lock;         /* opening the file and filling its pages */
    bool     open;
    uint64_t fh;
    struct pcache *pc;
};

static const struct vnode_ops fuse_dir_ops, fuse_file_ops, fuse_link_ops, fuse_dev_ops;

static void conn_put(struct fuse_conn *c)
{
    uint64_t fl = spin_lock_irqsave(&c->lock);
    bool last = --c->refs == 0;
    spin_unlock_irqrestore(&c->lock, fl);
    if (last) kfree(c);
}

static bool fatal_signal(void) { return (signal_pending_mask() & (1ull << (SIGKILL - 1))) != 0; }

/* Hang up (lock held): every request waiting or in progress fails with
 * ENOTCONN, and the program's reads get ENODEV. */
static void abort_locked(struct fuse_conn *c)
{
    c->connected = false;
    struct fuse_req *lists[2] = { c->pending, c->sent };
    c->pending = c->sent = NULL;
    for (int i = 0; i < 2; i++)
        for (struct fuse_req *r = lists[i], *n; r; r = n) {
            n = r->next;
            if (r->orphan) { kfree(r); continue; }
            r->error = -ENOTCONN;
            r->done = true;
            wakeup_waiters(r);
        }
    wakeup_waiters(&c->initialized);
    wakeup(c);                                          /* the program's read and poll */
}

static struct fuse_req *req_new(uint32_t opcode, uint64_t nodeid, const void *a1, size_t l1, const void *a2, size_t l2)
{
    size_t len = sizeof(struct fuse_in_header) + l1 + l2;
    struct fuse_req *r = kzalloc(sizeof *r + len);
    if (!r) return NULL;
    r->msg = (uint8_t *)(r + 1);
    r->msglen = (uint32_t)len;
    r->opcode = opcode;
    struct tcb *t = current_task();
    struct fuse_in_header h = { .len = (uint32_t)len, .opcode = opcode, .nodeid = nodeid,
                                .uid = t ? t->uid : 0, .gid = t ? t->gid : 0, .pid = t ? (uint32_t)t->tgid : 0 };
    memcpy(r->msg, &h, sizeof h);
    if (l1) memcpy(r->msg + sizeof h, a1, l1);
    if (l2) memcpy(r->msg + sizeof h + l1, a2, l2);
    return r;
}

/* Queue r (lock held) for the program to read. */
static void queue_locked(struct fuse_conn *c, struct fuse_req *r)
{
    r->unique = ++c->next_unique;
    memcpy(r->msg + offsetof(struct fuse_in_header, unique), &r->unique, sizeof r->unique);
    struct fuse_req **pp = &c->pending;
    while (*pp) pp = &(*pp)->next;
    r->next = NULL;
    *pp = r;
    wakeup(c);
}

/* Send a request and wait for the answer: its payload into out (up to
 * outcap bytes; *outlen gets the size). 0 or a negative errno. */
static int fuse_call(struct fuse_conn *c, uint32_t opcode, uint64_t nodeid, const void *a1, size_t l1,
                     const void *a2, size_t l2, void *out, uint32_t outcap, uint32_t *outlen)
{
    struct fuse_req *r = req_new(opcode, nodeid, a1, l1, a2, l2);
    if (!r) return -ENOMEM;
    r->out = out;
    r->outcap = outcap;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    while (c->connected && !c->initialized) {
        if (fatal_signal()) { spin_unlock_irqrestore(&c->lock, fl); kfree(r); return -EINTR; }
        sleep_on_locked(&c->initialized, &c->lock);
    }
    if (!c->connected) { spin_unlock_irqrestore(&c->lock, fl); kfree(r); return -ENOTCONN; }
    queue_locked(c, r);
    while (!r->done) {
        if (fatal_signal()) {                           /* give up on it */
            if (!r->sent) {
                for (struct fuse_req **pp = &c->pending; *pp; pp = &(*pp)->next)
                    if (*pp == r) { *pp = r->next; break; }
                spin_unlock_irqrestore(&c->lock, fl);
                kfree(r);
            } else {
                r->orphan = true;                       /* the answer frees it */
                r->out = NULL;
                spin_unlock_irqrestore(&c->lock, fl);
            }
            return -EINTR;
        }
        sleep_on_locked(r, &c->lock);
    }
    spin_unlock_irqrestore(&c->lock, fl);
    int err = r->error;
    if (outlen) *outlen = r->outlen;
    kfree(r);
    return err;
}

/* ---- /dev/fuse ------------------------------------------------------------ */

/* The next request, whole; a buffer too small for it gets EINVAL. */
static ssize_t dev_read(struct file *f, void *buf, size_t len)
{
    struct fuse_conn *c = f->vn->data;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    for (;;) {
        if (!c->connected) { spin_unlock_irqrestore(&c->lock, fl); return -ENODEV; }
        if (c->pending) break;
        if (f->flags & O_NONBLOCK) { spin_unlock_irqrestore(&c->lock, fl); return -EAGAIN; }
        if (signal_pending()) { spin_unlock_irqrestore(&c->lock, fl); return -EINTR; }
        sleep_on_locked(c, &c->lock);
    }
    struct fuse_req *r = c->pending;
    if (len < r->msglen) { spin_unlock_irqrestore(&c->lock, fl); return -EINVAL; }
    c->pending = r->next;
    r->sent = true;
    r->next = c->sent;
    c->sent = r;
    size_t n = r->msglen;
    memcpy(buf, r->msg, n);                             /* under the lock: a hang-up may free r */
    spin_unlock_irqrestore(&c->lock, fl);
    return (ssize_t)n;
}

static void init_answered(struct fuse_conn *c, struct fuse_req *r)
{
    const struct fuse_init_out *o = &c->init_out;
    if (r->error || r->outlen < 8 || o->major != FUSE_KERNEL_VERSION || o->minor < 12) {
        kprintf("fuse: %s: INIT refused (error %d, version %u.%u)\n", c->name, r->error, o->major, o->minor);
        abort_locked(c);
        return;
    }
    c->minor = o->minor < FUSE_KERNEL_MINOR ? o->minor : FUSE_KERNEL_MINOR;
    c->readdirplus = (o->flags & FUSE_DO_READDIRPLUS) != 0;
    c->initialized = true;
    wakeup_waiters(&c->initialized);
}

/* An answer: the out header, then the payload, in one write (the device
 * asks for whole writes: vnode_ops.whole_writes). */
static ssize_t dev_write(struct file *f, const void *buf, size_t len)
{
    struct fuse_conn *c = f->vn->data;
    struct fuse_out_header h;
    if (len < sizeof h) return -EINVAL;
    memcpy(&h, buf, sizeof h);
    if (h.len != len || h.error > 0 || h.error < -4095) return -EINVAL;
    if (!h.unique) return (ssize_t)len;                 /* a notification: none is acted on */
    uint64_t fl = spin_lock_irqsave(&c->lock);
    if (!c->connected) { spin_unlock_irqrestore(&c->lock, fl); return -ENODEV; }
    struct fuse_req *r = NULL;
    for (struct fuse_req **pp = &c->sent; *pp; pp = &(*pp)->next)
        if ((*pp)->unique == h.unique) { r = *pp; *pp = r->next; break; }
    if (!r) { spin_unlock_irqrestore(&c->lock, fl); return -ENOENT; }
    size_t n = len - sizeof h;
    if (r->out) {
        if (n > r->outcap) n = r->outcap;
        memcpy(r->out, (const uint8_t *)buf + sizeof h, n);
    }
    r->outlen = (uint32_t)n;
    r->error = h.error;
    r->done = true;
    if (r->opcode == FUSE_INIT) init_answered(c, r);
    if (r->orphan) kfree(r);
    else wakeup_waiters(r);
    spin_unlock_irqrestore(&c->lock, fl);
    return (ssize_t)len;
}

static int dev_poll(struct file *f, int events)
{
    (void)events;
    struct fuse_conn *c = f->vn->data;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    int r = !c->connected ? POLLERR : (POLLOUT | POLLWRNORM) | (c->pending ? POLLIN | POLLRDNORM : 0);
    spin_unlock_irqrestore(&c->lock, fl);
    return r;
}

/* The program closed its last descriptor: hang up (the mount answers
 * ENOTCONN). */
static void dev_release(struct file *f)
{
    struct fuse_conn *c = f->vn->data;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    if (--c->devs == 0) abort_locked(c);
    spin_unlock_irqrestore(&c->lock, fl);
    conn_put(c);
}

/* FUSE_DEV_IOC_CLONE: this new descriptor serves the connection of another
 * (libfuse's clone_fd: one per worker thread). */
#define FUSE_DEV_IOC_CLONE 0x8004e500ul
static int dev_fioctl(struct file *f, unsigned long req, void *arg)
{
    if (req != FUSE_DEV_IOC_CLONE) return -ENOTTY;
    uint32_t ofd;
    if (get_user_u32(&ofd, arg)) return -EFAULT;
    struct file *of = fd_file((int)ofd);
    if (!of) return -EBADF;
    struct fuse_conn *n = f->vn->data, *o = of->vn->ops == &fuse_dev_ops ? of->vn->data : NULL;
    int r = -EINVAL;
    if (o && o != n) {
        uint64_t fl = spin_lock_irqsave(&n->lock);
        bool fresh = !n->mounted && n->devs == 1 && n->refs == 1;
        spin_unlock_irqrestore(&n->lock, fl);
        if (fresh) {
            fl = spin_lock_irqsave(&o->lock);
            if (o->connected) { o->refs++; o->devs++; r = 0; }
            spin_unlock_irqrestore(&o->lock, fl);
        }
    }
    vfs_close(of);
    if (r < 0) return r;
    vfs_vnode_get(&o->dev_vn);
    f->vn = &o->dev_vn;
    kfree(n);                                           /* nothing else knew it */
    return 0;
}

static const struct vnode_ops fuse_dev_ops = {
    .fread = dev_read, .fwrite = dev_write, .poll = dev_poll, .release = dev_release, .fioctl = dev_fioctl,
    .nolock = true, .whole_writes = true,
};

/* Each open of /dev/fuse is a new connection. */
static int fuse_dev_open(struct vnode *vn, int flags, struct file **out)
{
    struct fuse_conn *c = kzalloc(sizeof *c);
    if (!c) return -ENOMEM;
    lockcheck_name(&c->lock, "fuse");
    c->refs = 1;
    c->devs = 1;
    c->connected = true;
    strlcpy(c->name, "fuse", sizeof c->name);
    struct vnode *d = &c->dev_vn;
    strlcpy(d->name, "fuse", sizeof d->name);
    d->type = VCHR;
    d->mode = S_IFCHR | 0666;
    d->rdev = (10u << 8) | 229;
    d->ops = &fuse_dev_ops;
    d->data = c;
    d->fs = vn->fs;
    /* refs from 0, never VN_GONE: like eventfd's, freed with the connection,
     * not by the last vfs_vnode_put */
    struct file *f = vfs_file_new(d, flags);
    if (!f) { kfree(c); return -ENOMEM; }
    *out = f;
    return 0;
}

const struct vnode_ops fuse_ctl_ops = { .open = fuse_dev_open, .nolock = true };

/* ---- nodes ------------------------------------------------------------------ */

static struct fuse_node *fnode(struct vnode *vn) { return vn->ctx; }

static void set_attr(struct vnode *vn, const struct fuse_attr *a)
{
    vn->ino = a->ino ? a->ino : vn->ino;
    vn->mode = (vn->mode & S_IFMT) | (a->mode & 07777);
    vn->size = vn->type == VLNK ? vn->size : a->size;
    vn->uid = a->uid;
    vn->gid = a->gid;
    vn->mtime = a->mtime;
    vn->atime = a->atime ? a->atime : a->mtime;
    vn->ctime = a->ctime ? a->ctime : a->mtime;
    vn->nlink = a->nlink;
    vn->blocks = a->blocks;
    vn->rdev = a->rdev;
}

/* A node for an entry the program described; NULL on OOM. */
static struct vnode *make_node(struct fuse_conn *c, struct mount *m, const char *name,
                               const struct fuse_entry_out *e, const char *link)
{
    uint32_t fmt = e->attr.mode & S_IFMT;
    enum vtype t = fmt == S_IFDIR ? VDIR : fmt == S_IFLNK ? VLNK : fmt == S_IFCHR ? VCHR : fmt == S_IFSOCK ? VSOCK : VREG;
    const struct vnode_ops *ops = t == VDIR ? &fuse_dir_ops : t == VLNK ? &fuse_link_ops : &fuse_file_ops;
    struct vnode *vn = vfs_node_new(m, name, t, e->attr.mode & 07777, ops);
    struct fuse_node *fn = kzalloc(sizeof *fn);
    if (!vn || !fn) { kfree(vn); kfree(fn); return NULL; }
    fn->conn = c;
    fn->nodeid = e->nodeid;
    vn->ctx = fn;
    if (link) {
        if (!(vn->data = kstrdup(link))) { kfree(vn); kfree(fn); return NULL; }
        vn->size = strlen(link);
    }
    set_attr(vn, &e->attr);
    uint64_t fl = spin_lock_irqsave(&c->lock);
    c->refs++;
    spin_unlock_irqrestore(&c->lock, fl);
    return vn;
}

static void fuse_evict(struct vnode *vn)
{
    struct fuse_node *fn = fnode(vn);
    if (!fn) return;
    if (fn->pc) pcache_free(fn->pc);
    if (vn->type == VLNK) kfree(vn->data);
    struct fuse_conn *c = fn->conn;
    kfree(fn);
    vn->ctx = NULL;
    conn_put(c);
}

static int lookup(struct fuse_conn *c, uint64_t dir, const char *name, struct fuse_entry_out *e)
{
    memset(e, 0, sizeof *e);
    uint32_t got = 0;
    int r = fuse_call(c, FUSE_LOOKUP, dir, name, strlen(name) + 1, NULL, 0, e, sizeof *e, &got);
    if (r == 0 && (got < sizeof *e || !e->nodeid)) r = -ENOENT;
    return r;
}

/* An entry into the list being built: with a symbolic link's target. */
static int add_entry(struct fuse_conn *c, struct mount *m, const char *name, const struct fuse_entry_out *e,
                     struct vnode **list)
{
    char *link = NULL;
    if ((e->attr.mode & S_IFMT) == S_IFLNK) {
        link = kmalloc(VFS_PATH_MAX);
        if (!link) return -ENOMEM;
        uint32_t got = 0;
        int r = fuse_call(c, FUSE_READLINK, e->nodeid, NULL, 0, NULL, 0, link, VFS_PATH_MAX - 1, &got);
        if (r < 0) { kfree(link); return r; }
        link[got] = 0;
    }
    struct vnode *vn = make_node(c, m, name, e, link);
    kfree(link);
    if (!vn) return -ENOMEM;
    vn->sibling = *list;
    *list = vn;
    return 0;
}

static void free_list(struct vnode *list)
{
    while (list) {
        struct vnode *n = list->sibling;
        fuse_evict(list);
        kfree(list);
        list = n;
    }
}

/* vnode_ops.load: every entry of the directory, with its attributes. */
static int fuse_load(struct vnode *dir)
{
    struct fuse_node *fn = fnode(dir);
    struct fuse_conn *c = fn->conn;
    if (__atomic_exchange_n(&dir->loading, true, __ATOMIC_ACQ_REL)) {     /* someone else is at it */
        while (!__atomic_load_n(&dir->loaded, __ATOMIC_ACQUIRE) && __atomic_load_n(&dir->loading, __ATOMIC_ACQUIRE)) {
            if (fatal_signal()) return -EINTR;
            uint64_t fl = irq_save();
            sleep_on_until(&dir->loading, time_ms() + 10);
            irq_restore(fl);
        }
        return __atomic_load_n(&dir->loaded, __ATOMIC_ACQUIRE) ? 0 : -EIO;
    }
    struct fuse_open_in oi = { .flags = O_RDONLY | O_DIRECTORY };
    struct fuse_open_out oo = { 0 };
    uint8_t *buf = kmalloc(FUSE_DIRBUF);
    struct vnode *list = NULL;
    int r = buf ? fuse_call(c, FUSE_OPENDIR, fn->nodeid, &oi, sizeof oi, NULL, 0, &oo, sizeof oo, NULL) : -ENOMEM;
    bool opened = r == 0;
    bool plus = c->readdirplus;
    uint64_t off = 0;
    while (r == 0) {
        struct fuse_read_in ri = { .fh = oo.fh, .offset = off, .size = FUSE_DIRBUF };
        uint32_t got = 0;
        r = fuse_call(c, plus ? FUSE_READDIRPLUS : FUSE_READDIR, fn->nodeid, &ri, sizeof ri, NULL, 0, buf, FUSE_DIRBUF, &got);
        if (r < 0 || !got) break;
        size_t pos = 0;
        bool any = false;
        while (r == 0) {
            const struct fuse_entry_out *e = NULL;
            if (plus) {
                if (pos + sizeof *e + sizeof(struct fuse_dirent) > got) break;
                e = (const struct fuse_entry_out *)(buf + pos);
                pos += sizeof *e;
            }
            if (pos + sizeof(struct fuse_dirent) > got) break;
            const struct fuse_dirent *d = (const struct fuse_dirent *)(buf + pos);
            if (!d->namelen || d->namelen >= VFS_NAME_MAX || pos + sizeof *d + d->namelen > got) { r = -EIO; break; }
            char name[VFS_NAME_MAX];
            memcpy(name, d->name, d->namelen);
            name[d->namelen] = 0;
            pos += (sizeof *d + d->namelen + 7) & ~(size_t)7;
            off = d->off;
            any = true;
            if (!strcmp(name, ".") || !strcmp(name, "..") || strchr(name, '/')) continue;
            struct fuse_entry_out le;
            if (!e || !e->nodeid) {                     /* no attributes with it: look it up */
                int lr = lookup(c, fn->nodeid, name, &le);
                if (lr == -ENOENT) continue;            /* gone meanwhile */
                if (lr < 0) { r = lr; break; }
                e = &le;
            }
            r = add_entry(c, dir->fs, name, e, &list);
        }
        if (!any) break;
    }
    if (opened) {
        struct fuse_release_in rl = { .fh = oo.fh, .flags = O_RDONLY | O_DIRECTORY };
        fuse_call(c, FUSE_RELEASEDIR, fn->nodeid, &rl, sizeof rl, NULL, 0, NULL, 0, NULL);
    }
    kfree(buf);
    if (r < 0) {
        free_list(list);
        __atomic_store_n(&dir->loading, false, __ATOMIC_RELEASE);
        return r;
    }
    vfs_load_done(dir, list);
    return 0;
}

static int fuse_readlink(struct vnode *vn, char *buf, size_t size)
{
    size_t n = vn->data ? strlen(vn->data) : 0;
    if (n > size) n = size;
    memcpy(buf, vn->data, n);
    return (int)n;
}

/* The file's handle, opened at first use and kept (fn->lock held). */
static int open_fh(struct fuse_node *fn)
{
    if (fn->open) return 0;
    struct fuse_open_in oi = { .flags = O_RDONLY };
    struct fuse_open_out oo = { 0 };
    int r = fuse_call(fn->conn, FUSE_OPEN, fn->nodeid, &oi, sizeof oi, NULL, 0, &oo, sizeof oo, NULL);
    if (r < 0) return r;
    fn->fh = oo.fh;
    fn->open = true;
    return 0;
}

/* Page idx of the file, filled with a READ of the pages around it. */
static int fuse_getpage(struct vnode *vn, uint64_t idx, bool io, uint64_t *phys)
{
    struct fuse_node *fn = fnode(vn);
    struct pcache *pc = __atomic_load_n(&fn->pc, __ATOMIC_ACQUIRE);
    *phys = pc ? pcache_lookup(pc, idx, io) : 0;
    if (*phys || !io) return 0;
    kmutex_lock(&fn->lock);
    int r = 0;
    if (!fn->pc && !(fn->pc = pcache_new())) { r = -ENOMEM; goto out; }
    if ((*phys = pcache_lookup(fn->pc, idx, false))) goto out;          /* filled meanwhile */
    if ((r = open_fh(fn)) < 0) goto out;
    const uint64_t per = FUSE_READ_MAX / PAGE_SIZE;
    uint64_t first = idx - idx % per, pages = (vn->size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t n = first >= pages ? 1 : (pages - first < per ? pages - first : per);
    if (idx >= first + n) { first = idx; n = 1; }       /* past the end: a page of zeros */
    uint8_t *buf = kmalloc(n * PAGE_SIZE);
    if (!buf) { r = -ENOMEM; goto out; }
    struct fuse_read_in ri = { .fh = fn->fh, .offset = first * PAGE_SIZE, .size = (uint32_t)(n * PAGE_SIZE), .flags = O_RDONLY };
    uint32_t got = 0;
    r = first < pages ? fuse_call(fn->conn, FUSE_READ, fn->nodeid, &ri, sizeof ri, NULL, 0, buf, (uint32_t)(n * PAGE_SIZE), &got) : 0;
    if (r < 0) { kfree(buf); goto out; }
    memset(buf + got, 0, n * PAGE_SIZE - got);          /* a short read: the rest is past the end */
    for (uint64_t i = 0; i < n; i++) {
        uint64_t p = page_alloc();
        if (!p) { if (!*phys) r = -ENOMEM; break; }
        memcpy((void *)(uintptr_t)p, buf + i * PAGE_SIZE, PAGE_SIZE);
        p = pcache_insert(fn->pc, first + i, p, first + i != idx);
        if (first + i == idx) *phys = p;
        else page_unref(p);
    }
    kfree(buf);
out:
    kmutex_unlock(&fn->lock);
    return r;
}

static ssize_t fuse_read(struct vnode *vn, void *buf, size_t len, uint64_t off)
{
    if (off >= vn->size) return 0;
    if (len > vn->size - off) len = (size_t)(vn->size - off);
    size_t done = 0;
    while (done < len) {
        uint64_t pos = off + done, in = pos % PAGE_SIZE, p;
        size_t n = PAGE_SIZE - in < len - done ? (size_t)(PAGE_SIZE - in) : len - done;
        int r = fuse_getpage(vn, pos / PAGE_SIZE, true, &p);
        if (r < 0) return done ? (ssize_t)done : r;
        memcpy((uint8_t *)buf + done, (const uint8_t *)(uintptr_t)p + in, n);
        page_unref(p);
        done += n;
    }
    return (ssize_t)done;
}

static ssize_t fuse_write(struct vnode *vn, const void *buf, size_t len, uint64_t off)
{ (void)vn; (void)buf; (void)len; (void)off; return -EROFS; }

static const struct vnode_ops fuse_dir_ops  = { .load = fuse_load, .evict = fuse_evict, .nolock = true };
static const struct vnode_ops fuse_link_ops = { .readlink = fuse_readlink, .evict = fuse_evict, .nolock = true };
static const struct vnode_ops fuse_file_ops = {
    .read = fuse_read, .write = fuse_write, .getpage = fuse_getpage, .evict = fuse_evict, .nolock = true,
};

/* ---- the mount -------------------------------------------------------------- */

static int fuse_statfs(struct mount *m, struct kstatfs *st)
{
    struct fuse_conn *c = m->priv;
    struct fuse_kstatfs k = { 0 };
    uint32_t got = 0;
    int r = fuse_call(c, FUSE_STATFS, FUSE_ROOT_ID, NULL, 0, NULL, 0, &k, sizeof k, &got);
    if (r == -ENOSYS) { r = 0; memset(&k, 0, sizeof k); k.bsize = 512; k.namelen = 255; }
    if (r < 0) return r;
    st->f_bsize = k.bsize ? k.bsize : 512;
    st->f_frsize = k.frsize ? k.frsize : st->f_bsize;
    st->f_blocks = k.blocks;
    st->f_bfree = k.bfree;
    st->f_bavail = k.bavail;
    st->f_files = k.files;
    st->f_ffree = k.ffree;
    st->f_namelen = k.namelen ? k.namelen : 255;
    return 0;
}

/* Unmounted (the tree's lock held exclusively): hang up, so the program's
 * next read ends its loop. */
static void fuse_umount(struct mount *m)
{
    struct fuse_conn *c = m->priv;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    abort_locked(c);
    spin_unlock_irqrestore(&c->lock, fl);
    conn_put(c);
}

/* "fd=N,rootmode=40000,user_id=0,group_id=0[,allow_other,...]": the options
 * this mount needs; others (default_permissions, allow_other, blksize ...)
 * are accepted and change nothing here. */
static bool opt_num(const char *opts, const char *key, uint64_t *v, int base)
{
    size_t kl = strlen(key);
    for (const char *p = opts; p && *p;) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            uint64_t n = 0;
            const char *q = p + kl + 1;
            if (*q < '0' || *q > '9') return false;
            for (; *q >= '0' && *q <= '9'; q++) n = n * (uint64_t)base + (uint64_t)(*q - '0');
            *v = n;
            return true;
        }
        p = strchr(p, ',');
        if (p) p++;
    }
    return false;
}

int fuse_mount(const char *target, const char *source, const char *fstype, uint32_t mflags, const char *data)
{
    uint64_t fd, rootmode, uid = 0, gid = 0;
    if (!data || !opt_num(data, "fd", &fd, 10) || !opt_num(data, "rootmode", &rootmode, 8)) return -EINVAL;
    opt_num(data, "user_id", &uid, 10);
    opt_num(data, "group_id", &gid, 10);
    if ((rootmode & S_IFMT) != S_IFDIR) return -EINVAL;     /* fuseblk and file mounts: not here */
    struct file *f = fd_file((int)fd);
    if (!f) return -EBADF;
    if (f->vn->ops != &fuse_dev_ops) { vfs_close(f); return -EINVAL; }
    struct fuse_conn *c = f->vn->data;
    uint64_t fl = spin_lock_irqsave(&c->lock);
    bool ok = c->connected && !c->mounted;
    if (ok) { c->mounted = true; c->refs += 2; }       /* the mount, and the root node */
    spin_unlock_irqrestore(&c->lock, fl);
    vfs_close(f);
    if (!ok) return -EINVAL;

    struct mount *m;
    struct vnode *root = vfs_mkfs("fuse", MNT_RDONLY | mflags, &m);
    strlcpy(m->fstype, fstype, sizeof m->fstype);
    strlcpy(c->name, fstype, sizeof c->name);
    if (source) strlcpy(m->source, source, sizeof m->source);
    struct fuse_node *fn = kzalloc(sizeof *fn);
    if (!fn) { conn_put(c); conn_put(c); return -ENOMEM; }
    fn->conn = c;
    fn->nodeid = FUSE_ROOT_ID;
    root->ops = &fuse_dir_ops;
    root->ctx = fn;
    root->mode = S_IFDIR | (rootmode & 07777);
    root->uid = (uint32_t)uid;
    root->gid = (uint32_t)gid;
    m->priv = c;
    m->statfs = fuse_statfs;
    m->umount = fuse_umount;

    struct fuse_init_in in = { FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR, FUSE_READ_MAX,
                               FUSE_ASYNC_READ | FUSE_BIG_WRITES | FUSE_DO_READDIRPLUS | FUSE_PARALLEL_DIROPS };
    struct fuse_req *r = req_new(FUSE_INIT, 0, &in, sizeof in, NULL, 0);
    if (!r) return -ENOMEM;
    r->orphan = true;                                   /* nobody waits: the program reads it after mount() */
    r->out = &c->init_out;
    r->outcap = sizeof c->init_out;
    fl = spin_lock_irqsave(&c->lock);
    queue_locked(c, r);
    spin_unlock_irqrestore(&c->lock, fl);
    int rc = vfs_mount(m, target);
    if (rc < 0) {
        fl = spin_lock_irqsave(&c->lock);
        abort_locked(c);
        spin_unlock_irqrestore(&c->lock, fl);
        conn_put(c);                                    /* the mount's; the root keeps its own */
    }
    return rc;
}
