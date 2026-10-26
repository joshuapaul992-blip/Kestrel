/* kernel/sysvshm.c -- System V shared memory: shmget, shmat, shmdt, shmctl
 *
 * X clients share images with the X server this way (MIT-SHM): Mesa's
 * Vulkan presentation for software rendering, cairo and GTK. A segment is
 * a vm_object, like a memfd's, that the table holds one reference to;
 * every attachment is a shared mapping of it holding another, so the
 * object's reference count, less the table's, is the number of
 * attachments (a fork copies them, exec and exit drop them).
 *
 * IPC_RMID takes the key away at once; the segment itself goes when its
 * last attachment does (the object's last_user hook). As on Linux, a
 * removed segment that is still attached can be attached again by its id:
 * a client creates a segment, attaches it, removes it so that it cannot
 * leak, and only then tells the X server its id.
 *
 * Ids are slot + SHM_SLOTS * sequence, so a reused slot gets a new id.
 * Everything is under one spinlock; mapping and unmapping happen outside
 * it, on a reference taken under it. */
#include <kernel/sysvshm.h>
#include <kernel/spinlock.h>
#include <kernel/task.h>
#include <kernel/vm.h>
#include <kernel/vmobj.h>
#include <kernel/uaccess.h>
#include <kernel/posix.h>
#include <kernel/time.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/uvm.h>

#define IPC_PRIVATE     0
#define IPC_CREAT       01000
#define IPC_EXCL        02000
#define IPC_RMID        0
#define IPC_SET         1
#define IPC_STAT        2
#define IPC_INFO        3
#define SHM_LOCK        11
#define SHM_UNLOCK      12
#define SHM_STAT        13
#define SHM_INFO        14
#define SHM_STAT_ANY    15
#define IPC_64          0x100           /* in the command: always the 64-bit layout here */

#define SHM_RDONLY      010000
#define SHM_RND         020000
#define SHM_REMAP       040000
#define SHM_EXEC        0100000
#define SHMLBA          UVM_PAGE

#define SHM_SLOTS       4096            /* SHMMNI */
#define SHM_MAX         VMOBJ_MAX_SIZE  /* SHMMAX */

struct shmseg {
    int32_t  key;
    uint32_t uid, gid, cuid, cgid, mode;
    uint16_t seq;
    bool     removed;
    uint64_t size;
    struct vm_object *obj;
    int64_t  atime, dtime, ctime;
    int      cpid, lpid;
};

/* struct ipc64_perm and struct shmid64_ds, x86_64 */
struct ipc64_perm {
    int32_t  key;
    uint32_t uid, gid, cuid, cgid, mode;
    uint16_t seq, pad2;
    uint64_t unused1, unused2;
};
struct shmid64_ds {
    struct ipc64_perm perm;
    uint64_t segsz;
    int64_t  atime, dtime, ctime;
    int32_t  cpid, lpid;
    uint64_t nattch, unused4, unused5;
};
struct shminfo64 { uint64_t shmmax, shmmin, shmmni, shmseg, shmall, unused[4]; };
struct shm_info { int32_t used_ids; uint64_t shm_tot, shm_rss, shm_swp, swap_attempts, swap_successes; };

static struct shmseg *segs[SHM_SLOTS];
static uint16_t next_seq[SHM_SLOTS];
static struct spinlock shm_lock;

static int seg_id(int slot) { return (int)segs[slot]->seq * SHM_SLOTS + slot; }

/* The segment with this id (lock held), or NULL */
static struct shmseg *by_id(int id, int *slot)
{
    if (id < 0) return NULL;
    int s = id % SHM_SLOTS;
    if (!segs[s] || segs[s]->seq != (uint16_t)(id / SHM_SLOTS)) return NULL;
    if (slot) *slot = s;
    return segs[s];
}

static uint64_t attached(const struct shmseg *g)
{
    int refs = __atomic_load_n(&g->obj->refs, __ATOMIC_ACQUIRE);
    return refs > 1 ? (uint64_t)(refs - 1) : 0;
}

/* May the caller use the segment (read: 4, write: 2)? root may. */
static bool allowed(const struct shmseg *g, unsigned want)
{
    struct tcb *t = current_task();
    if (t->uid == 0) return true;
    unsigned bits = g->mode;
    if (t->uid == g->uid || t->uid == g->cuid) bits >>= 6;
    else if (t->gid == g->gid || t->gid == g->cgid) bits >>= 3;
    return (bits & want) == want;
}

static bool owner(const struct shmseg *g)
{
    struct tcb *t = current_task();
    return t->uid == 0 || t->uid == g->uid || t->uid == g->cuid;
}

/* Free slot s (lock held); its object goes with the caller's put. */
static struct vm_object *release(int s)
{
    struct vm_object *o = segs[s]->obj;
    o->last_user = NULL;
    kfree(segs[s]);
    segs[s] = NULL;
    return o;
}

/* The last attachment of a segment went: a removed one goes with it. */
static void shm_last_user(struct vm_object *o)
{
    struct vm_object *dead = NULL;
    uint64_t f = spin_lock_irqsave(&shm_lock);
    for (int s = 0; s < SHM_SLOTS; s++) {
        if (!segs[s] || segs[s]->obj != o) continue;
        if (segs[s]->removed && __atomic_load_n(&o->refs, __ATOMIC_ACQUIRE) == 1) dead = release(s);
        break;
    }
    spin_unlock_irqrestore(&shm_lock, f);
    vmobj_put(dead);
}

int64_t sysv_shmget(int32_t key, uint64_t size, int flags)
{
    struct tcb *t = current_task();
    uint64_t f = spin_lock_irqsave(&shm_lock);
    if (key != IPC_PRIVATE) {
        for (int s = 0; s < SHM_SLOTS; s++) {
            struct shmseg *g = segs[s];
            if (!g || g->removed || g->key != key) continue;
            int64_t r;
            if ((flags & IPC_CREAT) && (flags & IPC_EXCL)) r = -EEXIST;
            else if (size > g->size) r = -EINVAL;
            else if (!allowed(g, (flags & 0400 ? 4 : 0) | (flags & 0200 ? 2 : 0))) r = -EACCES;
            else r = seg_id(s);
            spin_unlock_irqrestore(&shm_lock, f);
            return r;
        }
        if (!(flags & IPC_CREAT)) { spin_unlock_irqrestore(&shm_lock, f); return -ENOENT; }
    }
    spin_unlock_irqrestore(&shm_lock, f);
    if (size < 1 || size > SHM_MAX) return -EINVAL;

    struct shmseg *g = kzalloc(sizeof *g);
    struct vm_object *o = g ? vmobj_new((size + UVM_PAGE - 1) & ~(UVM_PAGE - 1)) : NULL;
    if (!o) { kfree(g); return -ENOMEM; }
    o->last_user = shm_last_user;
    g->key = key;
    g->uid = g->cuid = t->uid;
    g->gid = g->cgid = t->gid;
    g->mode = (uint32_t)flags & 0777;
    g->size = size;
    g->obj = o;
    g->ctime = time_realtime_sec();
    g->cpid = t->tgid;

    f = spin_lock_irqsave(&shm_lock);
    int slot = -1;
    for (int s = 0; s < SHM_SLOTS && slot < 0; s++) if (!segs[s]) slot = s;
    /* a racing shmget may have made the key meanwhile */
    for (int s = 0; key != IPC_PRIVATE && s < SHM_SLOTS; s++)
        if (segs[s] && !segs[s]->removed && segs[s]->key == key) { slot = -2; break; }
    if (slot < 0) {
        spin_unlock_irqrestore(&shm_lock, f);
        o->last_user = NULL;
        vmobj_put(o);
        kfree(g);
        return slot == -2 ? sysv_shmget(key, size, flags) : -ENOSPC;
    }
    g->seq = next_seq[slot]++;          /* a reused slot: another id */
    segs[slot] = g;
    int id = seg_id(slot);
    spin_unlock_irqrestore(&shm_lock, f);
    return id;
}

int64_t sysv_shmat(int id, uint64_t addr, int flags)
{
    struct tcb *t = current_task();
    if (!t->user || !t->mm) return -EINVAL;
    if (addr & (SHMLBA - 1)) {
        if (!(flags & SHM_RND)) return -EINVAL;
        addr &= ~(SHMLBA - 1);
    }
    int prot = PROT_READ | (flags & SHM_RDONLY ? 0 : PROT_WRITE) | (flags & SHM_EXEC ? PROT_EXEC : 0);
    int mflags = MAP_SHARED | (addr ? (flags & SHM_REMAP ? MAP_FIXED : MAP_FIXED_NOREPLACE) : 0);
    if ((flags & SHM_REMAP) && !addr) return -EINVAL;

    uint64_t f = spin_lock_irqsave(&shm_lock);
    struct shmseg *g = by_id(id, NULL);
    if (!g) { spin_unlock_irqrestore(&shm_lock, f); return -EINVAL; }
    if (!allowed(g, (prot & PROT_WRITE ? 2 : 0) | 4 | (prot & PROT_EXEC ? 1 : 0))) {
        spin_unlock_irqrestore(&shm_lock, f);
        return -EACCES;
    }
    struct vm_object *o = g->obj;
    uint64_t size = g->size;
    vmobj_get(o);                       /* across the mapping, which takes its own */
    spin_unlock_irqrestore(&shm_lock, f);

    int64_t r = vm_mmap(t->mm, addr, size, prot, mflags, NULL, o, 0, 0);
    if (r == -EEXIST) r = -EINVAL;
    if (r >= 0) {
        f = spin_lock_irqsave(&shm_lock);
        if ((g = by_id(id, NULL))) { g->atime = time_realtime_sec(); g->lpid = t->tgid; }
        spin_unlock_irqrestore(&shm_lock, f);
    }
    vmobj_put(o);
    return r;
}

int64_t sysv_shmdt(uint64_t addr)
{
    struct tcb *t = current_task();
    if (!t->user || !t->mm || (addr & (UVM_PAGE - 1))) return -EINVAL;
    struct vm_object *o = NULL;
    uint64_t off = 0;
    if (vm_shared_key(t->mm, addr, &o, &off) < 0 || !o || off) return -EINVAL;

    uint64_t size = 0;
    uint64_t f = spin_lock_irqsave(&shm_lock);
    for (int s = 0; s < SHM_SLOTS; s++)
        if (segs[s] && segs[s]->obj == o) {
            size = segs[s]->size;
            segs[s]->dtime = time_realtime_sec();
            segs[s]->lpid = t->tgid;
            break;
        }
    spin_unlock_irqrestore(&shm_lock, f);
    if (!size) return -EINVAL;                  /* a shared mapping, but not of a segment */
    return vm_munmap(t->mm, addr, (size + UVM_PAGE - 1) & ~(UVM_PAGE - 1));
}

static void fill_ds(const struct shmseg *g, int id, struct shmid64_ds *ds)
{
    memset(ds, 0, sizeof *ds);
    ds->perm.key = g->removed ? IPC_PRIVATE : g->key;
    ds->perm.uid = g->uid;  ds->perm.gid = g->gid;
    ds->perm.cuid = g->cuid; ds->perm.cgid = g->cgid;
    ds->perm.mode = g->mode | (g->removed ? 01000 : 0);    /* SHM_DEST */
    ds->perm.seq = (uint16_t)(id / SHM_SLOTS);
    ds->segsz = g->size;
    ds->atime = g->atime; ds->dtime = g->dtime; ds->ctime = g->ctime;
    ds->cpid = g->cpid; ds->lpid = g->lpid;
    ds->nattch = attached(g);
}

int64_t sysv_shmctl(int id, int cmd, uint64_t ubuf)
{
    cmd &= ~IPC_64;
    struct shmid64_ds ds;
    uint64_t f;
    int slot;

    switch (cmd) {
    case IPC_INFO: {
        struct shminfo64 si = { .shmmax = SHM_MAX, .shmmin = 1, .shmmni = SHM_SLOTS, .shmseg = SHM_SLOTS,
                                .shmall = SHM_MAX / UVM_PAGE };
        int high = 0;
        f = spin_lock_irqsave(&shm_lock);
        for (int s = 0; s < SHM_SLOTS; s++) if (segs[s]) high = s;
        spin_unlock_irqrestore(&shm_lock, f);
        return copy_to_user((void *)ubuf, &si, sizeof si) ? -EFAULT : high;
    }
    case SHM_INFO: {
        struct shm_info si = { 0 };
        int high = 0;
        f = spin_lock_irqsave(&shm_lock);
        for (int s = 0; s < SHM_SLOTS; s++) {
            if (!segs[s]) continue;
            high = s;
            si.used_ids++;
            si.shm_tot += (segs[s]->size + UVM_PAGE - 1) / UVM_PAGE;
            si.shm_rss += vmobj_resident(segs[s]->obj);
        }
        spin_unlock_irqrestore(&shm_lock, f);
        return copy_to_user((void *)ubuf, &si, sizeof si) ? -EFAULT : high;
    }
    case SHM_STAT: case SHM_STAT_ANY: case IPC_STAT: {
        f = spin_lock_irqsave(&shm_lock);
        struct shmseg *g;
        if (cmd == IPC_STAT) g = by_id(id, &slot);
        else { slot = id; g = id >= 0 && id < SHM_SLOTS ? segs[id] : NULL; }
        if (!g) { spin_unlock_irqrestore(&shm_lock, f); return -EINVAL; }
        if (cmd != SHM_STAT_ANY && !allowed(g, 4)) { spin_unlock_irqrestore(&shm_lock, f); return -EACCES; }
        int rid = seg_id(slot);
        fill_ds(g, rid, &ds);
        spin_unlock_irqrestore(&shm_lock, f);
        if (copy_to_user((void *)ubuf, &ds, sizeof ds)) return -EFAULT;
        return cmd == IPC_STAT ? 0 : rid;
    }
    case IPC_SET: {
        if (copy_from_user(&ds, (const void *)ubuf, sizeof ds)) return -EFAULT;
        f = spin_lock_irqsave(&shm_lock);
        struct shmseg *g = by_id(id, NULL);
        int64_t r = 0;
        if (!g) r = -EINVAL;
        else if (!owner(g)) r = -EPERM;
        else {
            g->uid = ds.perm.uid;
            g->gid = ds.perm.gid;
            g->mode = ds.perm.mode & 0777;
            g->ctime = time_realtime_sec();
        }
        spin_unlock_irqrestore(&shm_lock, f);
        return r;
    }
    case IPC_RMID: {
        struct vm_object *dead = NULL;
        f = spin_lock_irqsave(&shm_lock);
        struct shmseg *g = by_id(id, &slot);
        int64_t r = 0;
        if (!g) r = -EINVAL;
        else if (!owner(g)) r = -EPERM;
        else {
            g->removed = true;
            g->ctime = time_realtime_sec();
            if (__atomic_load_n(&g->obj->refs, __ATOMIC_ACQUIRE) == 1) dead = release(slot);
        }
        spin_unlock_irqrestore(&shm_lock, f);
        vmobj_put(dead);
        return r;
    }
    case SHM_LOCK: case SHM_UNLOCK: {
        f = spin_lock_irqsave(&shm_lock);
        struct shmseg *g = by_id(id, NULL);
        int64_t r = !g ? -EINVAL : !owner(g) ? -EPERM : 0;     /* nothing is swapped: already locked */
        spin_unlock_irqrestore(&shm_lock, f);
        return r;
    }
    default:
        return -EINVAL;
    }
}

size_t sysv_shm_proc(char *buf, size_t cap)
{
    size_t n = 0;
#define P(...) (n += (size_t)snprintf(buf + n, n < cap ? cap - n : 0, __VA_ARGS__))
    P("       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime                   rss                  swap\n");
    uint64_t f = spin_lock_irqsave(&shm_lock);
    for (int s = 0; s < SHM_SLOTS && n < cap; s++) {
        struct shmseg *g = segs[s];
        if (!g) continue;
        P("%10d %10d  %4o %21lu %5d %5d  %5lu %5u %5u %5u %5u %10ld %10ld %10ld %21lu %21u\n",
          g->removed ? 0 : g->key, seg_id(s), g->mode | (g->removed ? 01000 : 0), g->size, g->cpid, g->lpid,
          attached(g), g->uid, g->gid, g->cuid, g->cgid, g->atime, g->dtime, g->ctime,
          (unsigned long)(vmobj_resident(g->obj) * UVM_PAGE), 0u);
    }
    spin_unlock_irqrestore(&shm_lock, f);
#undef P
    return n < cap ? n : cap;
}
