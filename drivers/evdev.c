/* =============================================================================
 *  evdev.c -- /dev/input/event0 (keyboard), event1 (mouse): Linux's evdev
 *
 *  The same events /dev/kinput carries, in Linux's own interface, for X's
 *  evdev driver and libevdev: each open file has its queue of struct
 *  input_event (a timestamp, type, code, value), and the EVIOC* ioctls
 *  describe the device (name, ids, which event types and codes it sends,
 *  which keys are down). EVIOCGRAB gives one file all of the device's
 *  events and takes the keyboard and mouse away from the terminals, as a
 *  /dev/kinput grab does.
 *
 *  The producers are kinput's (keyboard and USB interrupts), so queues are
 *  guarded by a spinlock taken with interrupts off. A key the PS/2
 *  keyboard repeats while it is held is sent as value 2, as Linux does.
 * ============================================================================= */
#include <kernel/evdev.h>
#include <kernel/vfs.h>
#include <kernel/posix.h>
#include <kernel/process.h>
#include <kernel/task.h>
#include <kernel/time.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/uaccess.h>
#include <kernel/spinlock.h>

#define EV_SYN  0x00
#define EV_KEY  0x01
#define EV_REL  0x02
#define EV_MSC  0x04
#define EV_LED  0x11
#define EV_REP  0x14
#define EV_CNT  0x20
#define KEY_CNT 0x300
#define REL_CNT 0x10
#define LED_CNT 0x10

#define QLEN    256                     /* events per open file */

struct input_event {                    /* x86-64: struct timeval, then the event */
    int64_t  sec, usec;
    uint16_t type, code;
    int32_t  value;
};

struct evdev_dev {
    const char *name, *phys;
    uint16_t bustype, vendor, product, version;
    uint8_t  evbit[EV_CNT / 8], keybit[KEY_CNT / 8], relbit[REL_CNT / 8];
    uint8_t  key[KEY_CNT / 8];          /* keys down now */
    struct client *grab;
};

struct client {
    struct file *f;
    struct evdev_dev *dev;
    struct client *next;
    struct input_event q[QLEN];
    uint32_t head, tail;
    bool     overflow;                  /* events were lost: report SYN_DROPPED */
    bool     revoked;
    int      clock;                     /* CLOCK_REALTIME (0) or CLOCK_MONOTONIC (1) */
};

static struct evdev_dev devs[EVDEV_COUNT] = {
    [EVDEV_KEYBOARD] = { .name = "Kestrel keyboard", .phys = "kestrel/input0",
                         .bustype = 0x11 /* BUS_I8042 */, .vendor = 1, .product = 1, .version = 0xab41 },
    [EVDEV_MOUSE]    = { .name = "Kestrel mouse", .phys = "kestrel/input1",
                         .bustype = 0x03 /* BUS_USB */, .vendor = 1, .product = 2, .version = 1 },
};
static struct client *clients;
static struct spinlock elock;           /* clients, their queues, devs[].key and .grab */
static volatile int grabs;

static void set_bit8(uint8_t *map, unsigned bit) { map[bit / 8] |= (uint8_t)(1u << (bit % 8)); }
static bool test_bit8(const uint8_t *map, unsigned bit) { return map[bit / 8] & (1u << (bit % 8)); }

bool evdev_grabbed(void) { return grabs > 0; }

static void put(struct client *c, uint16_t type, uint16_t code, int32_t value)
{
    uint32_t next = (c->head + 1) % QLEN;
    if (next == c->tail) {              /* full: drop the oldest, say so at the next read */
        c->overflow = true;
        c->tail = (c->tail + 1) % QLEN;
    }
    int64_t sec, nsec;
    if (c->clock == 0) time_realtime(&sec, &nsec);
    else { uint64_t us = time_us(); sec = (int64_t)(us / 1000000); nsec = (int64_t)(us % 1000000) * 1000; }
    c->q[c->head] = (struct input_event){ sec, nsec / 1000, type, code, value };
    c->head = next;
}

void evdev_event(int which, uint16_t type, uint16_t code, int32_t value)
{
    if (which < 0 || which >= EVDEV_COUNT) return;
    struct evdev_dev *d = &devs[which];
    uint64_t fl = spin_lock_irqsave(&elock);
    if (type == EV_KEY && code < KEY_CNT) {
        bool down = test_bit8(d->key, code);
        if (value == 1 && down) value = 2;                  /* the keyboard repeats a held key */
        if (value == 0) d->key[code / 8] &= (uint8_t)~(1u << (code % 8));
        else set_bit8(d->key, code);
        if (value == 0 && !down) { spin_unlock_irqrestore(&elock, fl); return; }
    }
    bool any = false;
    for (struct client *c = clients; c; c = c->next) {
        if (c->dev != d || c->revoked || (d->grab && d->grab != c)) continue;
        put(c, type, code, value);
        any = true;
    }
    spin_unlock_irqrestore(&elock, fl);
    if (any && type == EV_SYN) wakeup(d);
}

static struct client *client_of(struct file *f)
{
    for (struct client *c = clients; c; c = c->next)
        if (c->f == f) return c;
    return NULL;
}

/* ---- the device files --------------------------------------------------------- */
static int evdev_open(struct vnode *vn, int flags, struct file **out)
{
    struct client *c = kzalloc(sizeof *c);
    if (!c) return -ENOMEM;
    struct file *f = vfs_file_new(vn, flags);
    if (!f) { kfree(c); return -ENOMEM; }
    c->f = f;
    c->dev = vn->ctx;
    uint64_t fl = spin_lock_irqsave(&elock);
    c->next = clients;
    clients = c;
    spin_unlock_irqrestore(&elock, fl);
    *out = f;
    return 0;
}

static void ungrab_locked(struct client *c)
{
    if (c->dev->grab == c) { c->dev->grab = NULL; grabs--; }
}

static void evdev_release(struct file *f)
{
    uint64_t fl = spin_lock_irqsave(&elock);
    struct client *c = NULL;
    for (struct client **p = &clients; *p; p = &(*p)->next)
        if ((*p)->f == f) { c = *p; *p = c->next; break; }
    if (c) ungrab_locked(c);
    spin_unlock_irqrestore(&elock, fl);
    kfree(c);
}

static ssize_t evdev_fread(struct file *f, void *buf, size_t len)
{
    if (len < sizeof(struct input_event)) return -EINVAL;
    size_t max = len / sizeof(struct input_event), n = 0;
    struct input_event *out = buf;
    uint64_t fl = spin_lock_irqsave(&elock);
    struct client *c = client_of(f);
    if (!c || c->revoked) { spin_unlock_irqrestore(&elock, fl); return -ENODEV; }
    while (c->head == c->tail && !c->overflow) {
        if (f->flags & O_NONBLOCK) { spin_unlock_irqrestore(&elock, fl); return -EAGAIN; }
        if (signal_pending()) { spin_unlock_irqrestore(&elock, fl); return -ERESTARTSYS; }
        sleep_on_locked(c->dev, &elock);
        if (!(c = client_of(f)) || c->revoked) { spin_unlock_irqrestore(&elock, fl); return -ENODEV; }
    }
    if (c->overflow) {                  /* SYN_DROPPED: the reader resyncs with EVIOCGKEY */
        c->overflow = false;
        int64_t sec, nsec;
        time_realtime(&sec, &nsec);
        out[n++] = (struct input_event){ sec, nsec / 1000, EV_SYN, 3, 0 };
    }
    while (n < max && c->tail != c->head) {
        out[n++] = c->q[c->tail];
        c->tail = (c->tail + 1) % QLEN;
    }
    spin_unlock_irqrestore(&elock, fl);
    return (ssize_t)(n * sizeof(struct input_event));
}

/* LEDs and the like: accepted and ignored */
static ssize_t evdev_fwrite(struct file *f, const void *buf, size_t len)
{
    (void)f; (void)buf;
    return (ssize_t)(len - len % sizeof(struct input_event));
}

static int evdev_poll(struct file *f, int events)
{
    uint64_t fl = spin_lock_irqsave(&elock);
    struct client *c = client_of(f);
    if (c) poll_wait(c->dev);
    int r = !c || c->revoked ? POLLERR | POLLHUP : c->head != c->tail || c->overflow ? events & (POLLIN | POLLRDNORM) : 0;
    spin_unlock_irqrestore(&elock, fl);
    return r | (events & POLLOUT);
}

/* Copy up to `size` bytes of `src` (n long) to user memory; the count. */
static int give(void *uarg, unsigned size, const void *src, unsigned n)
{
    if (n > size) n = size;
    return copy_to_user(uarg, src, n) ? -EFAULT : (int)n;
}

#define IOC_NR(r)   ((r) & 0xff)
#define IOC_TYPE(r) (((r) >> 8) & 0xff)
#define IOC_SIZE(r) (((r) >> 16) & 0x3fff)
#define IOC_DIR(r)  (((r) >> 30) & 3)

/* The argument is the caller's own pointer (none of these is in sys_ioctl's table). */
static int evdev_ioctl(struct file *f, unsigned long req, void *arg)
{
    uint64_t fl = spin_lock_irqsave(&elock);
    struct client *c = client_of(f);
    bool revoked = !c || c->revoked;
    spin_unlock_irqrestore(&elock, fl);
    if (revoked) return -ENODEV;
    struct evdev_dev *d = c->dev;
    if (IOC_TYPE(req) != 'E') return -ENOTTY;
    unsigned nr = IOC_NR(req), size = IOC_SIZE(req);

    switch (req & 0xc000ffff) {         /* the fixed-size ones, by direction, type and number */
    case 0x80004501: {                  /* EVIOCGVERSION */
        int v = 0x010001;
        return copy_to_user(arg, &v, sizeof v) ? -EFAULT : 0;
    }
    case 0x80004502: {                  /* EVIOCGID */
        uint16_t id[4] = { d->bustype, d->vendor, d->product, d->version };
        return copy_to_user(arg, id, sizeof id) ? -EFAULT : 0;
    }
    case 0x80004503: {                  /* EVIOCGREP */
        if (d != &devs[EVDEV_KEYBOARD]) return -ENOSYS;
        unsigned rep[2] = { 250, 33 };
        return copy_to_user(arg, rep, sizeof rep) ? -EFAULT : 0;
    }
    case 0x40004503:                    /* EVIOCSREP */
        return d == &devs[EVDEV_KEYBOARD] ? 0 : -ENOSYS;
    case 0x80004584: {                  /* EVIOCGEFFECTS */
        int n = 0;
        return copy_to_user(arg, &n, sizeof n) ? -EFAULT : 0;
    }
    case 0x40004590: {                  /* EVIOCGRAB: the argument is a value */
        int r = 0;
        fl = spin_lock_irqsave(&elock);
        if (arg) {
            if (d->grab && d->grab != c) r = -EBUSY;
            else if (!d->grab) { d->grab = c; grabs++; }
        } else if (d->grab == c) ungrab_locked(c);
        else r = -EINVAL;
        spin_unlock_irqrestore(&elock, fl);
        return r;
    }
    case 0x40004591:                    /* EVIOCREVOKE */
        if (arg) return -EINVAL;
        fl = spin_lock_irqsave(&elock);
        c->revoked = true;
        ungrab_locked(c);
        spin_unlock_irqrestore(&elock, fl);
        wakeup(d);
        return 0;
    case 0x400045a0: {                  /* EVIOCSCLOCKID */
        int id;
        if (copy_from_user(&id, arg, sizeof id)) return -EFAULT;
        if (id != 0 && id != 1 && id != 7) return -EINVAL;
        c->clock = id == 0 ? 0 : 1;     /* CLOCK_BOOTTIME: the monotonic clock (no suspend) */
        return 0;
    }
    }

    if (IOC_DIR(req) != 2) return -EINVAL;              /* the rest read, with a length */
    uint8_t map[KEY_CNT / 8];
    switch (nr) {
    case 0x06: return give(arg, size, d->name, (unsigned)strlen(d->name) + 1);     /* EVIOCGNAME */
    case 0x07: return give(arg, size, d->phys, (unsigned)strlen(d->phys) + 1);     /* EVIOCGPHYS */
    case 0x08: return -ENOENT;                                                     /* EVIOCGUNIQ */
    case 0x09: memset(map, 0, sizeof map); return give(arg, size, map, 4);         /* EVIOCGPROP */
    case 0x18:                                                                     /* EVIOCGKEY */
        fl = spin_lock_irqsave(&elock);
        memcpy(map, d->key, sizeof map);
        spin_unlock_irqrestore(&elock, fl);
        return give(arg, size, map, sizeof map);
    case 0x19: case 0x1a: case 0x1b:                                               /* LED, SND, SW */
        memset(map, 0, sizeof map);
        return give(arg, size, map, nr == 0x19 ? LED_CNT / 8 : nr == 0x1a ? 1 : 2);
    }
    if (nr >= 0x20 && nr < 0x20 + EV_CNT) {                                        /* EVIOCGBIT */
        memset(map, 0, sizeof map);
        switch (nr - 0x20) {
        case 0:      return give(arg, size, d->evbit, sizeof d->evbit);
        case EV_KEY: return give(arg, size, d->keybit, sizeof d->keybit);
        case EV_REL: return give(arg, size, d->relbit, sizeof d->relbit);
        case EV_LED: return give(arg, size, map, LED_CNT / 8);
        default:     return give(arg, size, map, 8);
        }
    }
    return -EINVAL;                     /* no absolute axes, force feedback or multitouch */
}

static const struct vnode_ops evdev_ops = {
    .open = evdev_open, .fread = evdev_fread, .fwrite = evdev_fwrite, .poll = evdev_poll,
    .fioctl = evdev_ioctl, .release = evdev_release, .nolock = true,
};

void evdev_init(void)
{
    struct evdev_dev *k = &devs[EVDEV_KEYBOARD], *m = &devs[EVDEV_MOUSE];
    set_bit8(k->evbit, EV_SYN); set_bit8(k->evbit, EV_KEY); set_bit8(k->evbit, EV_REP);
    for (unsigned code = 1; code < 0x100; code++) set_bit8(k->keybit, code);
    set_bit8(m->evbit, EV_SYN); set_bit8(m->evbit, EV_KEY); set_bit8(m->evbit, EV_REL);
    set_bit8(m->keybit, 0x110); set_bit8(m->keybit, 0x111); set_bit8(m->keybit, 0x112);  /* BTN_LEFT, RIGHT, MIDDLE */
    set_bit8(m->relbit, 0); set_bit8(m->relbit, 1); set_bit8(m->relbit, 8);              /* REL_X, REL_Y, REL_WHEEL */
    for (int i = 0; i < EVDEV_COUNT; i++) {
        char path[24], name[8];
        snprintf(name, sizeof name, "event%d", i);
        snprintf(path, sizeof path, "input/%s", name);
        devfs_register(path, 0660, (13u << 8) | (64u + (unsigned)i), &evdev_ops, &devs[i]);
        sysfs_class_dev(NULL, "input", name, path, 13, 64u + (unsigned)i);
    }
}
