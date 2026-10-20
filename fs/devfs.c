/* fs/devfs.c -- /dev character devices (major/minor numbers follow Linux) */
#include <kernel/kfb.h>
#include <kernel/task.h>
#include <kernel/display.h>
#include <kernel/uvm.h>
#include <kernel/vfs.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <kernel/serial.h>
#include <kernel/report.h>
#include <kernel/fb.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/cpu.h>
#include <kernel/mm.h>
#include <kernel/tty.h>
#include <kernel/kinput.h>
#include <kernel/uaccess.h>
#include <kernel/fuse.h>

#define MKDEV(ma, mi) (((ma) << 8) | (mi))

/* ---- null / zero / random --------------------------------------------- */
static ssize_t null_read(struct vnode *v, void *b, size_t n, uint64_t o)        { (void)v; (void)b; (void)n; (void)o; return 0; }
static ssize_t sink_write(struct vnode *v, const void *b, size_t n, uint64_t o) { (void)v; (void)b; (void)o; return (ssize_t)n; }
static ssize_t zero_read(struct vnode *v, void *b, size_t n, uint64_t o)        { (void)v; (void)o; memset(b, 0, n); return (ssize_t)n; }

/* Each read its own xorshift stream, seeded from a shared counter and the
 * clock: readers on several CPUs never share a state. */
static uint64_t rng_seed;
static ssize_t random_read(struct vnode *v, void *b, size_t n, uint64_t o)
{
    (void)v; (void)o;
    uint64_t s = __atomic_add_fetch(&rng_seed, 0x9e3779b97f4a7c15ull, __ATOMIC_RELAXED) ^ rdtsc();
    if (!s) s = 1;
    uint8_t *p = b;
    for (size_t i = 0; i < n; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        p[i] = (uint8_t)(s >> 24);
    }
    return (ssize_t)n;
}

/* these, the serial port and kmsg need no big kernel lock (nolock) */
static const struct vnode_ops null_ops   = { .read = null_read,   .write = sink_write, .nolock = true };
static const struct vnode_ops zero_ops   = { .read = zero_read,   .write = sink_write, .nolock = true };
static const struct vnode_ops random_ops = { .read = random_read, .write = sink_write, .nolock = true };

/* ---- tty: one terminal per monitor --------------------------------------
 *
 *   /dev/tty         the calling task's TTY (shell, its utilities)
 *   /dev/console     TTY 0, the system console
 *   /dev/tty1 ...    the TTY of monitor 0, 1, ... (Linux numbering: Ctrl+Alt+F1 = tty1)
 *   /dev/tty0        the TTY with the keyboard focus (Linux: the foreground VT)
 *
 * The line discipline lives in tty.c and keeps its line in that TTY's own
 * input_buffer. Without any TTY (no usable frame buffer) the text console
 * g_con and the global keyboard queue are used instead. */

static struct kestrel_tty *vnode_tty(struct vnode *v) { return v->ctx ? v->ctx : tty_current(); }

static char   con_line[256];
static size_t con_len, con_pos;     /* bytes in completed line / already consumed */
static bool   con_ready;

static void tty_echo(const char *s) { con_puts(&g_con, s); }

static ssize_t con_read(void *buf, size_t n)
{
    if (!con_ready) {                       /* gather one edited line */
        con_len = con_pos = 0;
        con_show_cursor(&g_con, true);
        for (;;) {
            struct key_event ev;
            keyboard_wait(&ev);
            char c = ev.ascii;
            if (c == '\n') { con_line[con_len++] = '\n'; tty_echo("\n"); break; }
            if (c == '\b') { if (con_len) { con_len--; tty_echo("\b \b"); } continue; }
            if (c == 3)    { tty_echo("^C\n"); con_len = 0; con_line[con_len++] = '\n'; break; }   /* Ctrl+C */
            if (c == 4)    { if (con_len == 0) { con_show_cursor(&g_con, false); return 0; } continue; } /* Ctrl+D = EOF */
            if (c == 12)   { con_puts(&g_con, "\033[2J"); continue; }                                     /* Ctrl+L */
            if (c == 21)   { while (con_len) { con_len--; tty_echo("\b \b"); } continue; }               /* Ctrl+U */
            if (c >= ' ' && c < 127 && con_len < sizeof con_line - 1) {
                con_line[con_len++] = c;
                char e[2] = { c, 0 };
                tty_echo(e);
            }
        }
        con_show_cursor(&g_con, false);
        con_ready = true;
    }
    size_t avail = con_len - con_pos;
    if (n > avail) n = avail;
    memcpy(buf, con_line + con_pos, n);
    con_pos += n;
    if (con_pos >= con_len) con_ready = false;
    return (ssize_t)n;
}

static ssize_t devtty_read(struct vnode *v, void *buf, size_t n, uint64_t o)
{
    (void)o;
    struct kestrel_tty *t = vnode_tty(v);
    return t ? (ssize_t)tty_read_line(t, buf, n) : con_read(buf, n);
}

static ssize_t devtty_write(struct vnode *v, const void *buf, size_t n, uint64_t o)
{
    (void)o;
    struct kestrel_tty *t = vnode_tty(v);
    if (t) tty_write(t, buf, n);
    else con_write(&g_con, buf, n);
    return (ssize_t)n;
}

static int devtty_ioctl(struct vnode *v, unsigned long req, void *arg)
{
    if (req == TIOCGWINSZ) {                /* the size of this TTY's own monitor */
        struct winsize *ws = arg;
        struct kestrel_tty *t = vnode_tty(v);
        if (t) {
            ws->ws_row = (uint16_t)t->max_rows; ws->ws_col = (uint16_t)t->max_cols;
            ws->ws_xpixel = (uint16_t)t->native_width; ws->ws_ypixel = (uint16_t)t->native_height;
        } else {
            ws->ws_row = (uint16_t)g_con.rows; ws->ws_col = (uint16_t)g_con.cols;
            ws->ws_xpixel = (uint16_t)(g_con.cols * g_con.font->width);
            ws->ws_ypixel = (uint16_t)(g_con.rows * g_con.font->height);
        }
        return 0;
    }
    return -ENOTTY;
}

/* ---- Linux's VT and KD ioctls, for X servers that take a terminal over ------
 * Kestrel's VTs are its monitors' terminals, all on screen at once, and the
 * "active" one is the one with the keyboard focus. What an X server asks:
 *   KDSETMODE KD_GRAPHICS   the terminal stops drawing on its monitor
 *   KDSKBMODE K_OFF/K_RAW   the terminals stop taking keys (X reads them
 *                           from /dev/input), until K_XLATE/K_UNICODE
 *   VT_GETSTATE, VT_OPENQRY the focused terminal; VT_ACTIVATE moves the focus
 *   VT_SETMODE              kept and handed back; Kestrel never switches VTs
 *                           under a program, so it sends no signals
 * The file that changed the modes puts them back when it closes, so a
 * crashed X server leaves a working console. Arguments are the caller's
 * own pointers (none of these is in sys_ioctl's table). */
#define KDGKBTYPE   0x4B33
#define KDGETMODE   0x4B3B
#define KDGKBMODE   0x4B44
#define KDSKBMODE   0x4B45
#define KDGETLED    0x4B31
#define KDSETLED    0x4B32
#define KDGKBLED    0x4B64
#define KDSKBLED    0x4B65
#define VT_OPENQRY  0x5600
#define VT_GETMODE  0x5601
#define VT_SETMODE  0x5602
#define VT_GETSTATE 0x5603
#define VT_RELDISP  0x5605
#define VT_ACTIVATE 0x5606
#define VT_WAITACTIVE 0x5607
#define K_RAW       0
#define K_MEDIUMRAW 2
#define K_UNICODE   3
#define K_OFF       4

struct vt_mode { char mode, waitv; int16_t relsig, acqsig, frsig; };
struct vt_stat { uint16_t v_active, v_signal, v_state; };

static struct kmutex vt_lock;
static struct file *vt_owner;           /* changed the KD modes: puts them back at close */
static int  vt_graphics = -1;           /* the TTY in KD_GRAPHICS, or -1 */
static int  vt_kbmode = K_UNICODE;
static struct vt_mode vt_mode;

static int put_int(void *uarg, int v) { return copy_to_user(uarg, &v, sizeof v) ? -EFAULT : 0; }

static int vt_ioctl(struct file *f, int idx, unsigned long req, void *arg)
{
    int r = 0;
    switch (req) {
    case KDGKBTYPE: { char kb = 0x02; return copy_to_user(arg, &kb, 1) ? -EFAULT : 0; }    /* KB_101 */
    case KDGETLED: case KDGKBLED: { char l = 0; return copy_to_user(arg, &l, 1) ? -EFAULT : 0; }
    case KDSETLED: case KDSKBLED: return 0;
    case KDGETMODE: return put_int(arg, vt_graphics == idx);
    case KDGKBMODE: return put_int(arg, vt_kbmode);
    case VT_OPENQRY: return put_int(arg, active_keyboard_tty + 1);
    case VT_GETSTATE: {
        struct vt_stat st = { (uint16_t)(active_keyboard_tty + 1), 0, 0 };
        for (int i = 0; i < tty_count && i < 15; i++) st.v_state |= (uint16_t)(1u << (i + 1));
        return copy_to_user(arg, &st, sizeof st) ? -EFAULT : 0;
    }
    case VT_GETMODE: return copy_to_user(arg, &vt_mode, sizeof vt_mode) ? -EFAULT : 0;
    case VT_SETMODE: return copy_from_user(&vt_mode, arg, sizeof vt_mode) ? -EFAULT : 0;
    case VT_RELDISP: case VT_WAITACTIVE: return 0;
    case VT_ACTIVATE:
        if ((uintptr_t)arg < 1 || (uintptr_t)arg > (uintptr_t)tty_count) return -ENXIO;
        tty_set_focus((int)(uintptr_t)arg - 1);
        return 0;
    case KDSETMODE: case KDSKBMODE:
        kmutex_lock(&vt_lock);
        if (req == KDSETMODE) {
            bool on = (uintptr_t)arg == 1;
            if (on && vt_graphics < 0 && tty_set_graphics(idx, true) == 0) vt_graphics = idx;
            else if (!on && vt_graphics == idx) { tty_set_graphics(idx, false); vt_graphics = -1; }
        } else {
            int m = (int)(uintptr_t)arg;
            if (m < K_RAW || m > K_OFF) r = -EINVAL;
            else { vt_kbmode = m; kinput_console_keys(m != K_OFF && m != K_RAW && m != K_MEDIUMRAW); }
        }
        if (!r) vt_owner = vt_graphics >= 0 || vt_kbmode != K_UNICODE ? f : NULL;
        kmutex_unlock(&vt_lock);
        return r;
    }
    return -ENOTTY;                     /* not one of these */
}

/* The same on a text terminal's console pty (fs/pty.c), whose slave /dev/ttyN is */
int vt_console_ioctl(struct file *f, int idx, unsigned long req, void *arg) { return vt_ioctl(f, idx, req, arg); }

static int vt_fioctl(struct file *f, unsigned long req, void *arg)
{
    struct kestrel_tty *t = vnode_tty(f->vn);
    int r = vt_ioctl(f, t ? t->index : 0, req, arg);
    return r == -ENOTTY ? devtty_ioctl(f->vn, req, arg) : r;
}

void vt_release(struct file *f)
{
    kmutex_lock(&vt_lock);
    if (vt_owner == f) {
        if (vt_graphics >= 0) tty_set_graphics(vt_graphics, false);
        vt_graphics = -1;
        vt_kbmode = K_UNICODE;
        kinput_console_keys(true);
        vt_mode = (struct vt_mode){ 0 };
        vt_owner = NULL;
    }
    kmutex_unlock(&vt_lock);
}

static const struct vnode_ops tty_ops     = { .read = devtty_read, .write = devtty_write, .ioctl = devtty_ioctl,
                                              .fioctl = vt_fioctl, .release = vt_release };

/* /dev/tty0: the focused TTY */
static ssize_t tty0_write(struct vnode *v, const void *buf, size_t n, uint64_t o)
{
    (void)v; (void)o;
    struct kestrel_tty *t = tty_get(active_keyboard_tty);
    if (t) tty_write(t, buf, n);
    return (ssize_t)n;
}
static int tty0_fioctl(struct file *f, unsigned long req, void *arg)
{
    int r = vt_ioctl(f, active_keyboard_tty, req, arg);
    return r == -ENOTTY ? devtty_ioctl(f->vn, req, arg) : r;
}
static const struct vnode_ops tty0_ops    = { .read = null_read, .write = tty0_write, .fioctl = tty0_fioctl,
                                              .release = vt_release };

/* /dev/tty: the controlling terminal. A process in a pty session gets its
 * pty slave; everything else keeps Kestrel's own terminal (t->tty). */
static int devtty_open(struct vnode *vn, int flags, struct file **out)
{
    if (current_task()->ctty) return pty_open_ctty(flags, out);
    struct file *f = vfs_file_new(vn, flags);
    if (!f) return -ENOMEM;
    *out = f;
    return 0;
}
static const struct vnode_ops ctty_ops    = { .open = devtty_open, .read = devtty_read, .write = devtty_write,
                                              .ioctl = devtty_ioctl };
static const struct vnode_ops console_ops = { .read = null_read, .write = devtty_write, .ioctl = devtty_ioctl };

/* ---- serial ------------------------------------------------------------ */
static ssize_t serial_dev_write(struct vnode *v, const void *buf, size_t n, uint64_t o)
{
    (void)v; (void)o;
    const char *s = buf;
    for (size_t i = 0; i < n; i++) serial_putc(s[i]);
    return (ssize_t)n;
}
static const struct vnode_ops serial_ops = { .read = null_read, .write = serial_dev_write, .nolock = true };

/* ---- report: text for REPORT.TXT on the boot stick (report.h) ----------- */
static ssize_t report_dev_write(struct vnode *v, const void *buf, size_t n, uint64_t o)
{
    (void)v; (void)o;
    return report_user_write(buf, n);
}
static const struct vnode_ops report_ops = { .read = null_read, .write = report_dev_write };

/* ---- results: RESULTS.TAR on the boot stick (report.h) ------------------- */
static ssize_t results_dev_write(struct vnode *v, const void *buf, size_t n, uint64_t off)
{
    (void)v;
    return report_results_write(buf, n, off);
}
static void results_release(struct file *f) { (void)f; report_results_close(); }
static const struct vnode_ops results_ops = { .read = null_read, .write = results_dev_write, .release = results_release };

/* ---- kmsg -------------------------------------------------------------- */
static ssize_t kmsg_read(struct vnode *v, void *buf, size_t n, uint64_t off)
{
    (void)v;
    return (ssize_t)klog_read(off, buf, n);
}
static ssize_t kmsg_write(struct vnode *v, const void *buf, size_t n, uint64_t o)
{
    (void)v; (void)o;
    char tmp[256];
    for (size_t done = 0; done < n; ) {                 /* all of it, 255 bytes at a time */
        size_t k = n - done < sizeof tmp - 1 ? n - done : sizeof tmp - 1;
        memcpy(tmp, (const char *)buf + done, k); tmp[k] = 0;
        kprintf("%s", tmp);
        done += k;
    }
    return (ssize_t)n;
}
static uint64_t kmsg_size(struct vnode *v) { (void)v; return klog_size(); }
static const struct vnode_ops kmsg_ops = { .read = kmsg_read, .write = kmsg_write, .size = kmsg_size, .nolock = true };   /* log_lock */

/* ---- fbN: one per monitor, for graphics clients (see kfb.h) ------------- */
static int fb_owner[MAX_MONITORS];          /* pid holding KD_GRAPHICS, 0 = none */

static int fb_ioctl(struct vnode *v, unsigned long req, void *arg)
{
    struct kestrel_tty *t = v->ctx;
    int idx = t->index;
    const struct display_head *h = display_get(idx);
    switch (req) {
    case KFB_GET_INFO: {
        if (!arg) return -EFAULT;
        struct kfb_info *i = arg;                       /* a kernel copy (sys_ioctl) */
        memset(i, 0, sizeof *i);
        i->width = (uint32_t)t->native_width; i->height = (uint32_t)t->native_height;
        i->pitch = (uint32_t)t->native_width * 4; i->bpp = (uint32_t)t->bytes_pp * 8;
        i->r_pos = t->r_pos; i->g_pos = t->g_pos; i->b_pos = t->b_pos; i->index = (uint8_t)idx;
        if (h) { strlcpy(i->name, h->name, sizeof i->name); strlcpy(i->monitor, h->monitor, sizeof i->monitor); }
        return 0;
    }
    case FBIOGET_VSCREENINFO: {
        if (!arg) return -EFAULT;
        struct fb_var_screeninfo_lite *si = arg;
        si->xres = (uint32_t)t->native_width; si->yres = (uint32_t)t->native_height;
        si->bits_per_pixel = (uint32_t)t->bytes_pp * 8; si->line_length = (uint32_t)t->native_width * 4;
        si->smem_start = h ? h->phys : 0;
        return 0;
    }
    case KDSETMODE: {
        uint64_t mode = (uint64_t)arg;
        int pid = current_task()->pid;
        if (mode == KD_GRAPHICS) {
            if (fb_owner[idx] && fb_owner[idx] != pid) return -EBUSY;
            int rc = tty_set_graphics(idx, true);
            if (rc == 0) fb_owner[idx] = pid;
            return rc;
        }
        if (mode == KD_TEXT) {
            if (fb_owner[idx] && fb_owner[idx] != pid) return -EBUSY;
            fb_owner[idx] = 0;
            return tty_set_graphics(idx, false);
        }
        return -EINVAL;
    }
    case KFB_GET_MODES: {
        if (!arg || !h) return -EFAULT;
        struct kfb_modes *m = arg;                      /* a kernel copy (sys_ioctl) */
        memset(m, 0, sizeof *m);
        m->width_mm = h->width_mm > 0 ? (uint32_t)h->width_mm : 0;
        m->height_mm = h->height_mm > 0 ? (uint32_t)h->height_mm : 0;
        if (!h->nmodes) {                               /* a fixed mode */
            m->count = 1;
            m->mode[0] = (struct kfb_mode){ (uint32_t)t->native_width, (uint32_t)t->native_height, h->refresh_mhz,
                                            KFB_MODE_PREFERRED | KFB_MODE_CURRENT };
            return 0;
        }
        bool found = false;
        for (int i = 0; i < h->nmodes && i < KFB_MAX_MODES; i++) {
            const struct display_mode *d = &h->modes[i];
            struct kfb_mode *k = &m->mode[m->count++];
            *k = (struct kfb_mode){ d->width, d->height, d->refresh_mhz, d->preferred ? KFB_MODE_PREFERRED : 0 };
            if (!found && d->width == h->width && d->height == h->height &&
                (!h->refresh_mhz || !d->refresh_mhz || d->refresh_mhz == h->refresh_mhz)) {
                k->flags |= KFB_MODE_CURRENT;
                found = true;
            }
        }
        return 0;
    }
    case KFB_SET_MODE: {
        if (!arg) return -EFAULT;
        const struct kfb_mode *k = arg;
        if (fb_owner[idx] && fb_owner[idx] != current_task()->pid) return -EBUSY;
        if (!k->width || !k->height || k->width > 0xffff || k->height > 0xffff) return -EINVAL;
        struct display_mode m = { (uint16_t)k->width, (uint16_t)k->height, k->refresh_mhz, false };
        return display_set_mode(idx, &m);
    }
    case KFB_BLIT: {
        if (!arg) return -EFAULT;
        const struct kfb_blit *b = arg;                 /* kernel copy; b->src is a user pointer */
        if (b->w <= 0 || b->h <= 0) return 0;
        if (b->w > 16384 || b->h > 16384 || b->src_pitch < (uint32_t)b->w * 4) return -EINVAL;
        return tty_blit_user(idx, b->x, b->y, b->w, b->h, b->src, b->src_pitch);
    }
    default:
        return -ENOTTY;
    }
}

/* The owner closing its last descriptor (or exiting) gives the text back. */
static void fb_release(struct file *f)
{
    struct kestrel_tty *t = f->vn->ctx;
    if (fb_owner[t->index] == current_task()->pid) {
        fb_owner[t->index] = 0;
        tty_set_graphics(t->index, false);
    }
}

static const struct vnode_ops fb_ops = { .ioctl = fb_ioctl, .release = fb_release };

/* ------------------------------------------------------------------------- */
static void add_dev(struct vnode *dir, const char *name, uint32_t mode, uint32_t rdev,
                    const struct vnode_ops *ops)
{
    struct vnode *d = vfs_node_new(dir->fs, name, VCHR, mode, ops);
    d->rdev = rdev;
    vfs_node_add(dir, d);
}

/* /dev/fd, /dev/stdin, ...: fixed symbolic links into /proc/self/fd */
static int devlink_read(struct vnode *vn, char *buf, size_t size)
{
    const char *t = vn->ctx;
    size_t n = strlen(t);
    if (n > size) n = size;
    memcpy(buf, t, n);
    return (int)n;
}
static const struct vnode_ops devlink_ops = { .readlink = devlink_read, .nolock = true };

static void add_link(struct vnode *dir, const char *name, const char *target)
{
    struct vnode *l = vfs_node_new(dir->fs, name, VLNK, 0777, &devlink_ops);
    if (!l) return;
    l->ctx = (void *)target;
    vfs_node_add(dir, l);
}

static void add_tty(struct vnode *dir, const char *name, uint32_t mode, uint32_t rdev,
                    const struct vnode_ops *ops, struct kestrel_tty *t)
{
    struct vnode *d = vfs_node_new(dir->fs, name, VCHR, mode, ops);
    d->rdev = rdev;
    d->ctx = t;
    vfs_node_add(dir, d);
}

static struct vnode *devfs_root;

/* /dev/ttyN and /dev/fbN of a monitor added after boot (tty_add_monitor) */
void devfs_add_monitor(int i)
{
    char name[8];
    snprintf(name, sizeof name, "tty%d", i + 1);
    if (!devfs_register(name, 0620, MKDEV(4, (uint32_t)(i + 1)), &tty_ops, tty_get(i)))
        return;
    snprintf(name, sizeof name, "fb%d", i);
    devfs_register(name, 0660, MKDEV(29, (uint32_t)i), &fb_ops, tty_get(i));
}

/* `name` may have one directory in front ("dri/card0"), made on first use. */
struct vnode *devfs_register(const char *name, uint32_t mode, uint32_t rdev,
                             const struct vnode_ops *ops, void *ctx)
{
    if (!devfs_root) return NULL;
    const char *slash = strchr(name, '/');
    struct vnode *d = vfs_node_new(devfs_root->fs, slash ? slash + 1 : name, VCHR, mode, ops);
    if (!d) return NULL;
    d->rdev = rdev;
    d->ctx = ctx;
    vfs_tree_lock(true);                                /* devices come and go after boot (USB) */
    struct vnode *dir = devfs_root;
    if (slash) {
        char dname[32];
        size_t n = (size_t)(slash - name);
        if (n >= sizeof dname) n = sizeof dname - 1;
        memcpy(dname, name, n);
        dname[n] = 0;
        for (dir = devfs_root->children; dir; dir = dir->sibling)
            if (dir->type == VDIR && !strcmp(dir->name, dname)) break;
        if (!dir) {
            dir = vfs_node_new(devfs_root->fs, dname, VDIR, 0755, NULL);
            if (dir) vfs_node_add(devfs_root, dir);
        }
    }
    if (dir) vfs_node_add(dir, d);
    vfs_tree_unlock();
    return dir ? d : NULL;
}

void devfs_init(const char *mountpoint)
{
    struct mount *m;
    struct vnode *r = vfs_mkfs("devtmpfs", MNT_NOCREATE, &m);
    devfs_root = r;
    add_dev(r, "null",    0666, MKDEV(1, 3),  &null_ops);
    add_dev(r, "zero",    0666, MKDEV(1, 5),  &zero_ops);
    add_dev(r, "random",  0666, MKDEV(1, 8),  &random_ops);
    add_dev(r, "urandom", 0666, MKDEV(1, 9),  &random_ops);
    add_dev(r, "kmsg",    0644, MKDEV(1, 11), &kmsg_ops);
    add_dev(r, "tty",     0666, MKDEV(5, 0),  &ctty_ops);
    add_tty(r, "console", 0600, MKDEV(5, 1),  &console_ops, tty_get(0));
    add_tty(r, "tty0",    0620, MKDEV(4, 0),  &tty0_ops, NULL);
    for (int i = 0; i < tty_count; i++) {
        char name[8];
        snprintf(name, sizeof name, "tty%d", i + 1);
        add_tty(r, name, 0620, MKDEV(4, (uint32_t)(i + 1)), &tty_ops, tty_get(i));
    }
    add_dev(r, "ttyS0",   0660, MKDEV(4, 64), &serial_ops);
    add_dev(r, "report",  0600, MKDEV(10, 240), &report_ops);
    add_dev(r, "results", 0600, MKDEV(10, 241), &results_ops);
    add_dev(r, "fuse",    0666, MKDEV(10, 229), &fuse_ctl_ops);
    add_link(r, "fd", "/proc/self/fd");
    add_link(r, "stdin", "/proc/self/fd/0");
    add_link(r, "stdout", "/proc/self/fd/1");
    add_link(r, "stderr", "/proc/self/fd/2");
    for (int i = 0; i < tty_count; i++) {              /* one per monitor */
        char name[8];
        snprintf(name, sizeof name, "fb%d", i);
        add_tty(r, name, 0660, MKDEV(29, (uint32_t)i), &fb_ops, tty_get(i));
    }
    pty_init(r);
    pseudo_dir(r, "shm");                               /* tmpfs mounted here: shm_open() */
    vfs_mount(m, mountpoint);
}
