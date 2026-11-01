/* kernel/installer.c -- /dev/install: copy the boot stick onto a disk (alpha)
 *
 * The stick's layout already boots a PC by BIOS and by UEFI: GRUB in the
 * MBR gap, the Kestrel boot volume, the ext2 root, the EFI System
 * Partition and the report volume. Installing copies the stick, block for
 * block, from sector 0 to the end of its last partition, onto a SATA or
 * NVMe disk; everything on that disk is lost. The copy boots the same way
 * once the stick is out: kestrel.root=usb finds no stick and falls back to
 * the other disks (bootvol.c). Its root file system keeps the stick's size.
 *
 * Reading /dev/install gives the state, the disk it would copy from, and
 * the disks it can copy to:
 *   state idle|copying|done|failed
 *   source usb0 2544
 *   progress 512 2544          (MiB copied, MiB to copy)
 *   error <text>
 *   disk sata0 476940 <model>  (MiB)
 * Writing "install <disk>" starts the copy in a kernel thread; the
 * installer program (/bin/kestrel-install) asks first and shows progress.
 *
 * Only MBR sticks with 512-byte sectors, onto disks with 512-byte sectors,
 * and never the stick itself or the disk holding the mounted root. */
#include <kernel/block.h>
#include <kernel/bootvol.h>
#include <kernel/rootfs.h>
#include <kernel/task.h>
#include <kernel/vfs.h>
#include <kernel/mm.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <kernel/posix.h>
#include <kernel/installer.h>
#include <stdarg.h>

#define CHUNK_BLOCKS 512                                /* 256 KiB a request */

enum { IDLE, COPYING, DONE, FAILED };
static const char *const state_names[] = { "idle", "copying", "done", "failed" };
static volatile int state = IDLE;
static struct blkdev *src, *dst;
static volatile uint64_t copied, total;                 /* bytes */
static char error[96];

/* The disk the boot volume came from. */
static struct blkdev *source_disk(void)
{
    const struct bootvol_status *bv = bootvol_status();
    struct blkdev *b = bv->device[0] ? blk_find(bv->device) : NULL;
    return b ? blk_disk_of(b) : NULL;
}

/* Bytes from sector 0 to the end of the disk's last partition; 0 if it
 * cannot be copied (no partitions, GPT, not 512-byte sectors). */
static uint64_t source_bytes(struct blkdev *disk)
{
    if (!disk || disk->block_size != 512) return 0;
    uint64_t end = 0;
    for (int i = 0; i < blk_count(); i++) {
        struct blkdev *p = blk_get(i);
        if (!p || p->parent != disk || p->removed) continue;
        if (p->gpt) return 0;
        if (p->start + p->blocks > end) end = p->start + p->blocks;
    }
    return end * 512;
}

/* Why disk cannot be written to, or NULL if it can. */
static const char *refuse(struct blkdev *disk, struct blkdev *from)
{
    if (!disk || disk->parent || disk->removed) return "no such disk";
    if (disk->kind != BLK_DISK_SATA && disk->kind != BLK_DISK_NVME && disk->kind != BLK_DISK_VIRTIO)
        return "only SATA and NVMe disks";
    if (disk == from) return "that is the stick Kestrel is running from";
    if (disk->block_size != 512) return "only disks with 512-byte sectors";
    const char *root = rootfs_device();
    struct blkdev *r = root ? blk_find(root) : NULL;
    if (r && blk_disk_of(r) == disk) return "the running system's root file system is on it";
    if (blk_size_bytes(disk) < total) return "too small for the stick's contents";
    return NULL;
}

static int fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, sizeof error, fmt, ap);
    va_end(ap);
    kprintf("install: %s\n", error);
    state = FAILED;
    return 0;
}

static int copy_main(void *arg)
{
    (void)arg;
    vfs_sync_all(false);                                /* the root's cached blocks onto the stick first */
    blk_flush(src);
    const size_t frames = CHUNK_BLOCKS * 512 / PAGE_SIZE;
    uint64_t pa = pmm_alloc_contig(frames);             /* identity mapped: DMA safe */
    if (!pa) return fail("out of memory");
    uint8_t *buf = (uint8_t *)(uintptr_t)pa;
    kprintf("install: copying %lu MiB from %s to %s (%s)\n", total >> 20, src->name, dst->name, dst->model);
    /* A GPT the disk had keeps a backup header in its last sectors, which
     * firmware may "repair" the new MBR from: zero them first. */
    memset(buf, 0, 33 * 512);
    int rc = dst->blocks > 33 + total / 512 ? blk_write(dst, dst->blocks - 33, 33, buf) : 0;
    if (rc < 0) { pmm_free_contig(pa, frames); return fail("writing %s: %s", dst->name, blk_strerror(rc)); }
    uint64_t blocks = total / 512;
    for (uint64_t lba = 0; lba < blocks; lba += CHUNK_BLOCKS) {
        uint32_t n = blocks - lba < CHUNK_BLOCKS ? (uint32_t)(blocks - lba) : CHUNK_BLOCKS;
        rc = blk_read(src, lba, n, buf);
        if (rc < 0) { pmm_free_contig(pa, frames); return fail("reading %s at sector %lu: %s", src->name, lba, blk_strerror(rc)); }
        rc = blk_write(dst, lba, n, buf);
        if (rc < 0) { pmm_free_contig(pa, frames); return fail("writing %s at sector %lu: %s", dst->name, lba, blk_strerror(rc)); }
        copied = (lba + n) * 512;
    }
    pmm_free_contig(pa, frames);
    rc = blk_flush(dst);
    if (rc < 0) return fail("flushing %s: %s", dst->name, blk_strerror(rc));
    kprintf("install: %s done: %lu MiB written\n", dst->name, total >> 20);
    state = DONE;
    return 0;
}

#define P(...) (n += (size_t)snprintf(text + n, n < sizeof text ? sizeof text - n : 0, __VA_ARGS__))

static ssize_t install_read(struct vnode *vn, void *buf, size_t len, uint64_t off)
{
    (void)vn;
    static char text[2048];
    size_t n = 0;
    struct blkdev *from = state == IDLE ? source_disk() : src;
    uint64_t bytes = state == IDLE ? source_bytes(from) : total;
    P("state %s\n", state_names[state]);
    if (from && bytes) P("source %s %lu\n", from->name, bytes >> 20);
    else if (from) P("error the stick %s cannot be copied (GPT or not 512-byte sectors)\n", from->name);
    else P("error not started from a Kestrel stick\n");
    if (state != IDLE) P("target %s\nprogress %lu %lu\n", dst->name, copied >> 20, total >> 20);
    if (state == FAILED) P("error %s\n", error);
    if (state == IDLE && bytes) {
        uint64_t keep = total;
        total = bytes;                                  /* refuse() compares sizes against it */
        for (int i = 0; i < blk_count(); i++) {
            struct blkdev *d = blk_get(i);
            if (d && !refuse(d, from)) P("disk %s %lu %s\n", d->name, blk_size_bytes(d) >> 20, d->model);
        }
        total = keep;
    }
    if (off >= n) return 0;
    if (len > n - off) len = n - off;
    memcpy(buf, text + off, len);
    return (ssize_t)len;
}

static ssize_t install_write(struct vnode *vn, const void *buf, size_t len, uint64_t off)
{
    (void)vn; (void)off;
    char cmd[48];
    size_t n = len < sizeof cmd - 1 ? len : sizeof cmd - 1;
    memcpy(cmd, buf, n);
    while (n && (cmd[n - 1] == '\n' || cmd[n - 1] == ' ')) n--;
    cmd[n] = 0;
    if (strncmp(cmd, "install ", 8)) return -EINVAL;
    if (state == COPYING) return -EBUSY;
    struct blkdev *from = source_disk();
    uint64_t bytes = source_bytes(from);
    if (!bytes) return -ENODEV;
    struct blkdev *to = blk_find(cmd + 8);
    total = bytes;
    const char *why = refuse(to, from);
    if (why) {
        kprintf("install: %s: %s\n", cmd + 8, why);
        return to ? -EPERM : -ENOENT;
    }
    src = from;
    dst = to;
    copied = 0;
    error[0] = 0;
    state = COPYING;
    if (!task_create("install", copy_main, NULL)) { fail("cannot start the copy"); return -ENOMEM; }
    return (ssize_t)len;
}

static const struct vnode_ops install_ops = { .read = install_read, .write = install_write };

void installer_init(void)
{
    devfs_register("install", 0600, (10u << 8) | 243, &install_ops, NULL);
}
