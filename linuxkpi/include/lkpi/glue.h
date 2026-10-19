/* linuxkpi/include/lkpi/glue.h -- what the Linux KPI asks of Kestrel
 *
 * The Linux side (linuxkpi/src, third_party/linux) is compiled against the
 * KPI's own <linux/...> headers and never sees a Kestrel header: the two
 * kernels use the same names for different things (kmalloc, schedule,
 * copy_to_user, panic ...). This file is the only bridge. It is included by
 * both sides, so it uses plain C types only, and every function in it is
 * implemented in linuxkpi/glue/ with Kestrel's own interfaces. */
#ifndef LKPI_GLUE_H
#define LKPI_GLUE_H

/* ---- memory -------------------------------------------------------------- */
void         *lkpi_heap_alloc(unsigned long n);            /* 16-byte aligned, NULL on OOM   */
void          lkpi_heap_free(void *p);
unsigned long lkpi_frames_alloc(unsigned long n, int dma32);    /* zeroed, contiguous; 0 on OOM */
/* 2^order frames aligned to their size (alloc_pages: struct pages of one
 * allocation then lie in one chunk of the KPI's table, so page + i works) */
unsigned long lkpi_frames_alloc_order(unsigned int order, int dma32);
void          lkpi_frames_free(unsigned long phys, unsigned long n);
unsigned long lkpi_ram_total(void);
unsigned long lkpi_ram_free(void);
/* Make [phys, phys+size) usable as MMIO: uncached, or write-combining. */
int           lkpi_map_mmio(unsigned long phys, unsigned long size, int wc);
/* `n` frames at consecutive kernel addresses (Linux's vmap()); `cache` is
 * 0 (write-back), 0x08 (write-combining) or 0x18 (uncached). NULL on
 * failure. lkpi_vunmap() takes exactly the address returned. */
void         *lkpi_vmap(const unsigned long *frames, unsigned long n, unsigned long cache);
void          lkpi_vunmap(const void *addr);
unsigned long lkpi_vmap_phys(const void *addr);              /* 0 if not a vmap address */

/* ---- time ---------------------------------------------------------------- */
unsigned long long lkpi_time_ns(void);                     /* monotonic, since boot          */
unsigned long long lkpi_time_ms(void);
void          lkpi_udelay(unsigned int us);                 /* busy-wait                      */
long long     lkpi_realtime_ns(void);                       /* wall clock                     */

/* ---- CPUs and the scheduler ---------------------------------------------- */
int           lkpi_cpu_id(void);                            /* interrupts off                 */
int           lkpi_cpu_count(void);
void         *lkpi_task_self(void);                         /* the running thread (opaque)    */
int           lkpi_task_slot(void *task);                   /* 0 .. lkpi_task_slots()-1, or -1 */
int           lkpi_task_slots(void);
int           lkpi_task_pid(void *task);
const char   *lkpi_task_name(void *task);
int           lkpi_task_is_user(void *task);
/* A kernel thread running fn(arg); NULL if the task table is full. */
void         *lkpi_task_create(const char *name, int (*fn)(void *), void *arg);
void          lkpi_task_exit(int code) __attribute__((noreturn));
/* The sleep protocol, all with interrupts off: mark the current thread
 * asleep (until lkpi_task_wake or deadline_ms of lkpi_time_ms, 0: none),
 * then lkpi_task_switch() gives up the CPU. A wake between the two keeps
 * the thread runnable, so no wakeup is lost. */
void          lkpi_task_block(void *chan, unsigned long long deadline_ms);
void          lkpi_task_switch(void);
void          lkpi_task_wake(void *task);
void          lkpi_task_yield(void);
int           lkpi_task_signal_pending(void);
/* Boot thread only: run other threads until *flag becomes non-zero. */
void          lkpi_boot_wait(volatile int *flag);

/* ---- logging and boot parameters ----------------------------------------- */
void          lkpi_log(const char *s);                      /* one formatted string          */
const char   *lkpi_cmdline(void);
/* Called with the message at panic(), once, before the machine halts;
 * and at power off, reboot or halt (what: 0 off, 1 reboot, 2 halt). */
void          lkpi_set_hooks(void (*panic_fn)(const char *msg), void (*power_fn)(int what));
/* Kestrel's panic, power off (0), reboot (1) and halt (2) */
void          lkpi_die(const char *msg) __attribute__((noreturn));
void          lkpi_power(int what) __attribute__((noreturn));

/* ---- user memory --------------------------------------------------------- */
/* 0 or -EFAULT (Kestrel semantics; the KPI turns them into byte counts). */
int           lkpi_copy_from_user_glue(void *dst, const void *usrc, unsigned long n);
int           lkpi_copy_to_user_glue(void *udst, const void *src, unsigned long n);
int           lkpi_user_range_ok(unsigned long addr, unsigned long n);

/* ---- PCI ----------------------------------------------------------------- */
struct lkpi_pci_info {
    void          *handle;                  /* Kestrel's struct pci_device   */
    unsigned char  bus, dev, fn;
    unsigned short vendor, device, subvendor, subdevice;
    unsigned char  class_code, subclass, prog_if, revision;
    unsigned char  irq_line, irq_pin, header_type;
    struct {
        unsigned long long base, size;
        int            kind;                /* 0 unused, 1 I/O, 2 mem32, 3 mem64, 4 mem16, 5 upper half */
        int            prefetch;
    } bar[6];
    void          *parent;                  /* upstream bridge's handle, or NULL */
    const char    *native;                  /* the Kestrel driver using it (not offered
                                               to Linux drivers), or NULL           */
};
int           lkpi_pci_count(void);
int           lkpi_pci_get(int index, struct lkpi_pci_info *out);
unsigned int  lkpi_pci_read(void *handle, int off, int size);      /* size 1, 2 or 4 */
void          lkpi_pci_write(void *handle, int off, int size, unsigned int v);
/* Route the device's MSI-X entry 0 or MSI message to fn(arg): the Kestrel
 * vector, or -1. lkpi_pci_msi_disable gives the vector back. */
int           lkpi_pci_msi_enable(void *handle, void (*fn)(void *), void *arg, const char *name);
void          lkpi_pci_msi_disable(void *handle, int vector);
int           lkpi_pci_msi_is_msix(int vector);

/* ---- displays ----------------------------------------------------------- */
/* A monitor lit by a Linux DRM driver, as a Kestrel display head with its
 * own terminal. Either `vram` is the CPU-mapped scan-out buffer (Kestrel
 * draws into a RAM copy and writes changed lines through), or `vram` is
 * NULL and write_span(ctx, byte offset, ...) receives the changed bytes.
 * Pixels are 32-bit x8r8g8b8. Returns the monitor number, or -1. */
#define LKPI_DISPLAY_MAX_MODES 24
struct lkpi_display_mode {
    unsigned short width, height;
    unsigned int   refresh_mhz;             /* 0 = unknown                       */
    int            preferred;
};
struct lkpi_display {
    unsigned long long phys;                /* scan-out buffer, or 0             */
    void          *vram;
    int            width, height, pitch;
    void         (*write_span)(void *ctx, unsigned int offset, const void *src, unsigned int len);
    void          *ctx;
    const char    *name;                    /* "bochs-drm 0000:00:03.0"          */
    const char    *monitor;                 /* EDID monitor name, or ""          */
    unsigned int   refresh_mhz;             /* current mode, 0 = unknown          */
    int            width_mm, height_mm;     /* 0 = unknown                       */
    /* Modes the monitor can be switched to (KFB_SET_MODE): set_mode(ctx,
     * monitor, mode) switches and hands the new frame buffer to
     * lkpi_display_update(). NULL / 0: a fixed mode. */
    const struct lkpi_display_mode *modes;
    int            nmodes;
    int          (*set_mode)(void *ctx, int monitor, const struct lkpi_display_mode *mode);
};
int           lkpi_display_add(const struct lkpi_display *d);
/* After set_mode(): monitor `monitor` now shows `d`'s frame buffer (its
 * name, monitor and mode list are kept). The terminal has moved to it when
 * this returns, so the old buffer may be freed. 0 or -errno. */
int           lkpi_display_update(int monitor, const struct lkpi_display *d);
/* A Linux driver claims the adapter `handle` (sysfb_disable()): if Kestrel's
 * boot frame buffer is on it, Kestrel stops drawing there, and the driver's
 * first lkpi_display_add() takes that monitor's place. Returns the monitor
 * given up, or -1. */
int           lkpi_display_handover(void *handle);
/* Whether DRM drivers may drive displays: on unless linuxkpi.drm=0 or Safe Mode */
bool          lkpi_display_drm(void);
/* ... and its driver failed to probe: Kestrel draws on the boot frame buffer again. */
void          lkpi_display_reclaim(void *handle);

/* ---- files and device nodes ---------------------------------------------- */
/* The whole file at `path` in a buffer from lkpi_heap_alloc: its size, or
 * a negative errno. */
long          lkpi_file_read_all(const char *path, void **data, unsigned long max);

/* A character device in /dev. The callbacks get the open's private
 * pointer; read/write buffers are kernel memory, ioctl arguments are
 * user addresses (or values). */
struct lkpi_mmap;
struct lkpi_cdev_ops {
    int  (*open)(void *dev, int flags, void **file);
    void (*release)(void *file);
    long (*read)(void *file, char *buf, unsigned long n, long long *pos);
    long (*write)(void *file, const char *buf, unsigned long n, long long *pos);
    long (*ioctl)(void *file, unsigned int cmd, unsigned long arg);
    int  (*poll)(void *file, int events);
    int  (*mmap)(void *file, struct lkpi_mmap *m);          /* optional: MAP_SHARED */
    /* files without a node (lkpi_fd_fill): */
    unsigned long long (*size)(void *file);                 /* for lseek(SEEK_END) and fstat */
    void (*hold)(void *file);                               /* one more reference, for lkpi_fd_file */
};
void         *lkpi_cdev_add(const char *name, unsigned int mode, unsigned int major, unsigned int minor,
                            const struct lkpi_cdev_ops *ops, void *dev);
void          lkpi_cdev_remove(void *node);
/* /sys: the device node's entries (class/<cls>/<name>, dev/char/<major>:<minor>)
 * under the PCI function `pci` (a handle, or NULL); a driver bound to `pci`. */
void          lkpi_sysfs_class_dev(void *pci, const char *cls, const char *name, const char *devname,
                                   unsigned int major, unsigned int minor);
void          lkpi_sysfs_pci_driver(void *pci, const char *driver);

/* Descriptors of the calling process for files without a node in /dev
 * (anon_inode_getfile: dma-bufs, sync files, DRM sync objects), from inside
 * a system call. lkpi_fd_reserve() holds a free descriptor (or returns
 * -EMFILE), lkpi_fd_fill() puts the file there, served by `ops` (named
 * `name`, as /proc/self/fd shows it); the descriptor's last close calls
 * ops->release(file). lkpi_fd_unreserve() gives an unused one back.
 * lkpi_fd_file() is the file behind fd if `ops` serve it (an open of a
 * device node with them, or a file filled in with them), with
 * ops->hold() called on it, or NULL. */
int           lkpi_fd_reserve(int cloexec);
int           lkpi_fd_fill(int fd, const char *name, int flags, const struct lkpi_cdev_ops *ops, void *file);
void          lkpi_fd_unreserve(int fd);
void         *lkpi_fd_file(int fd, const struct lkpi_cdev_ops *ops);

/* ---- device memory mapped into a process (mmap of a device node) -------- */
/* The region [start, end) at file offset `off` of the process `mm` (opaque)
 * is set up by the cdev's mmap(), which fills in ops and ctx. Each region
 * has its own ctx: dup() copies it for a fork (into `mm`) or for the upper
 * part of a split region (mm NULL: the same process, and resize() shrinks
 * the original); close() ends it. fault() maps at least `va` with
 * lkpi_vm_insert() and returns 0, -EIO (SIGBUS), -ENOMEM or -EFAULT. All
 * of them may sleep; they run without Kestrel's big kernel lock. */
struct lkpi_mmap_ops {
    int   (*fault)(void *ctx, unsigned long va, int write);
    void *(*dup)(void *ctx, void *mm, unsigned long start, unsigned long end, unsigned long off);
    void  (*resize)(void *ctx, unsigned long start, unsigned long end, unsigned long off);
    void  (*close)(void *ctx);
};
struct lkpi_mmap {
    unsigned long start, end, off;
    int           prot, flags;              /* PROT_*, MAP_* (Linux values)  */
    void         *mm;
    const struct lkpi_mmap_ops *ops;
    void         *ctx;
};
/* From fault(): map the frame at `phys` at `va` of `mm`; `cache` is 0
 * (write-back), 0x08 (write-combining) or 0x18 (uncached). 0 or -errno. */
int           lkpi_vm_insert(void *mm, unsigned long va, unsigned long phys, unsigned long cache);
/* Unmap [start, end) of a region in `mm`: the memory behind it moved. */
void          lkpi_vm_zap(void *mm, unsigned long start, unsigned long end);

#endif
