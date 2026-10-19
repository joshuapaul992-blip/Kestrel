/* drivers/virtio/virtio_blk.c -- virtio block device (virtio 1.x over PCI)
 *
 * QEMU's and most hypervisors' paravirtual disk: PCI 1af4:1001
 * (transitional, subsystem 2) or 1af4:1042 (modern). Like virtio-net, the
 * driver uses the modern interface only: the common, notify and device
 * configuration structures found through the vendor capabilities, and one
 * split virtqueue of up to 128 entries. A request is a chain: the 16-byte
 * header, the data in as many segments as the device allows (seg_max,
 * size_max), and the status byte. The block layer sends one request per
 * disk at a time, so one chain is in flight and the descriptors are laid
 * out the same way every time.
 *
 * Completion: with MSI-X (storage_enable_irqs), the queue's vector wakes
 * the waiting thread, which sleeps meanwhile; before that, or without
 * MSI-X, the driver polls the used ring. Plain MSI is not offered by
 * virtio devices, so it is not used.
 *
 * Disks register as "vdN" (block.h) and are found by the boot volume
 * search like any other: `-drive if=virtio` in QEMU boots Kestrel. */
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/klog.h>
#include <kernel/cpu.h>
#include <kernel/block.h>
#include <kernel/posix.h>
#include <kernel/task.h>
#include <kernel/time.h>
#include <kernel/msi.h>
#include "pci.h"
#include "virtio_blk.h"

#define VIRTIO_VENDOR       0x1AF4
#define VIRTIO_BLK_LEGACY   0x1001
#define VIRTIO_BLK_MODERN   0x1042

#define CAP_COMMON  1
#define CAP_NOTIFY  2
#define CAP_ISR     3
#define CAP_DEVICE  4

#define ST_ACK          1
#define ST_DRIVER       2
#define ST_DRIVER_OK    4
#define ST_FEATURES_OK  8
#define ST_FAILED       128

#define F_SIZE_MAX      (1ull << 1)
#define F_SEG_MAX       (1ull << 2)
#define F_RO            (1ull << 5)
#define F_FLUSH         (1ull << 9)
#define F_VERSION_1     (1ull << 32)

#define T_IN        0                   /* read  */
#define T_OUT       1                   /* write */
#define T_FLUSH     4
#define S_OK        0

#define DESC_NEXT   1
#define DESC_WRITE  2
#define AVAIL_NO_INTERRUPT 1

#define QMAX        128
#define SECTOR      512u
#define SEG_DEFAULT (1u << 20)          /* per segment when the device sets no size_max */
#define TIMEOUT_US  30000000ull

struct vq_desc { uint64_t addr; uint32_t len; uint16_t flags, next; } __attribute__((packed));
struct vq_avail { uint16_t flags, idx, ring[]; } __attribute__((packed));
struct vq_used_elem { uint32_t id, len; } __attribute__((packed));
struct vq_used { uint16_t flags, idx; struct vq_used_elem ring[]; } __attribute__((packed));

struct blk_req { uint32_t type, reserved; uint64_t sector; } __attribute__((packed));

struct vblk {
    struct pci_device *pci;
    volatile uint8_t *common, *devcfg, *notify_base;
    uint32_t notify_mult;
    uint64_t features;
    uint64_t sectors;
    uint32_t seg_bytes;                 /* largest data segment */
    uint16_t max_segs;                  /* data segments per request */
    uint16_t qsize;
    volatile struct vq_desc *desc;
    volatile struct vq_avail *avail;
    volatile struct vq_used *used;
    volatile uint16_t *notify;
    uint16_t avail_idx, last_used;
    volatile struct blk_req *req;       /* in the same DMA page as status */
    volatile uint8_t *status;
    int      vector;                    /* MSI-X, or -1: polled */
    uint64_t requests, errors;
};

#define VBLK_MAX 8
static struct vblk *disks[VBLK_MAX];
static int ndisks;

#define C8(v, o)  (*(volatile uint8_t  *)((v)->common + (o)))
#define C16(v, o) (*(volatile uint16_t *)((v)->common + (o)))
#define C32(v, o) (*(volatile uint32_t *)((v)->common + (o)))
/* 64-bit fields as two 32-bit stores, low half first, as Linux writes them:
 * not every device takes a single 64-bit access to its common config */
#define C64_SET(v, o, x) do { uint64_t x_ = (x); C32(v, o) = (uint32_t)x_; C32(v, (o) + 4) = (uint32_t)(x_ >> 32); } while (0)
enum { DFSEL = 0, DF = 4, GFSEL = 8, GF = 12, MSIX = 16, NUMQ = 18, STATUS = 20, QSEL = 22, QSIZE = 24,
       QMSIX = 26, QENABLE = 28, QNOFF = 30, QDESC = 32, QDRIVER = 40, QDEVICE = 48 };
#define NO_VECTOR 0xffff

static void barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

static volatile uint8_t *map_cap(struct pci_device *d, uint8_t bar, uint32_t off, uint32_t len)
{
    if (bar > 5) return NULL;
    struct pci_bar *b = &d->bars[bar];
    if (b->kind != PCI_BAR_MEM32 && b->kind != PCI_BAR_MEM64) return NULL;
    if (!b->base || (b->size && (uint64_t)off + len > b->size)) return NULL;
    uint64_t phys = b->base + off;
    if (!vmm_set_uncached(phys, len ? len : 4096)) return NULL;
    return (volatile uint8_t *)(uintptr_t)phys;
}

static bool find_caps(struct vblk *v)
{
    struct pci_device *d = v->pci;
    if (!(pci_read16(d->bus, d->device, d->function, PCI_REG_STATUS) & PCI_STATUS_CAP_LIST)) return false;
    uint8_t p = pci_read8(d->bus, d->device, d->function, PCI_REG_CAP_PTR) & 0xfc;
    for (int guard = 0; p && guard < 48; guard++) {
        uint8_t id = pci_read8(d->bus, d->device, d->function, p);
        uint8_t next = pci_read8(d->bus, d->device, d->function, (uint8_t)(p + 1)) & 0xfc;
        if (id == 0x09) {
            uint8_t type = pci_read8(d->bus, d->device, d->function, (uint8_t)(p + 3));
            uint8_t bar = pci_read8(d->bus, d->device, d->function, (uint8_t)(p + 4));
            uint32_t off = pci_read32(d->bus, d->device, d->function, (uint8_t)(p + 8));
            uint32_t len = pci_read32(d->bus, d->device, d->function, (uint8_t)(p + 12));
            switch (type) {
            case CAP_COMMON: if (!v->common) v->common = map_cap(d, bar, off, len); break;
            case CAP_DEVICE: if (!v->devcfg) v->devcfg = map_cap(d, bar, off, len); break;
            case CAP_NOTIFY:
                if (!v->notify_base) {
                    v->notify_base = map_cap(d, bar, off, len);
                    v->notify_mult = pci_read32(d->bus, d->device, d->function, (uint8_t)(p + 16));
                }
                break;
            }
        }
        p = next;
    }
    return v->common && v->notify_base && v->devcfg;
}

static void *dma_pages(size_t bytes)
{
    size_t frames = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t phys = frames == 1 ? pmm_alloc() : pmm_alloc_contig(frames);
    if (!phys) return NULL;
    memset((void *)(uintptr_t)phys, 0, frames * PAGE_SIZE);
    return (void *)(uintptr_t)phys;     /* RAM is identity-mapped */
}

static bool vq_setup(struct vblk *v)
{
    C16(v, QSEL) = 0;
    uint16_t max = C16(v, QSIZE);
    if (max < 3) return false;                          /* header, one segment, status */
    v->qsize = max < QMAX ? max : QMAX;
    v->desc = dma_pages(16u * v->qsize);
    v->avail = dma_pages(6u + 2u * v->qsize);
    v->used = dma_pages(6u + 8u * v->qsize);
    uint8_t *page = dma_pages(PAGE_SIZE);
    if (!v->desc || !v->avail || !v->used || !page) return false;
    v->req = (volatile struct blk_req *)page;
    v->status = page + 64;
    v->avail->flags = AVAIL_NO_INTERRUPT;               /* polled until MSI-X is set up */
    C16(v, QSIZE) = v->qsize;
    C16(v, QMSIX) = NO_VECTOR;
    C64_SET(v, QDESC, (uint64_t)(uintptr_t)v->desc);
    C64_SET(v, QDRIVER, (uint64_t)(uintptr_t)v->avail);
    C64_SET(v, QDEVICE, (uint64_t)(uintptr_t)v->used);
    v->notify = (volatile uint16_t *)(v->notify_base + (uint32_t)C16(v, QNOFF) * v->notify_mult);
    C16(v, QENABLE) = 1;
    return true;
}

/* Wait for the chain to come back: sleep when the vector will wake us
 * (each pause ends at the next tick anyway), else spin. */
static bool wait_used(struct vblk *v)
{
    uint64_t start = time_us();
    while (__atomic_load_n(&v->used->idx, __ATOMIC_ACQUIRE) == v->last_used) {
        if (time_us() - start > TIMEOUT_US) return false;
        if (v->vector >= 0) io_pause(v, 10); else cpu_relax();
    }
    barrier();
    v->last_used++;
    return true;
}

/* One request: header, `bytes` at buf in segments, status. */
static int submit(struct vblk *v, uint32_t type, uint64_t sector, void *buf, size_t bytes)
{
    v->req->type = type;
    v->req->reserved = 0;
    v->req->sector = sector;
    *v->status = 0xff;
    uint16_t i = 0;
    v->desc[0] = (struct vq_desc){ (uint64_t)(uintptr_t)v->req, sizeof(struct blk_req), DESC_NEXT, 1 };
    for (size_t off = 0; off < bytes; off += v->seg_bytes) {
        i++;
        uint32_t len = bytes - off < v->seg_bytes ? (uint32_t)(bytes - off) : v->seg_bytes;
        v->desc[i] = (struct vq_desc){ (uint64_t)(uintptr_t)buf + off, len,
                                       (uint16_t)(DESC_NEXT | (type == T_IN ? DESC_WRITE : 0)), (uint16_t)(i + 1) };
    }
    i++;
    v->desc[i] = (struct vq_desc){ (uint64_t)(uintptr_t)v->status, 1, DESC_WRITE, 0 };
    v->avail->ring[v->avail_idx % v->qsize] = 0;
    v->avail_idx++;
    barrier();
    v->avail->idx = v->avail_idx;
    barrier();
    *v->notify = 0;
    v->requests++;
    if (!wait_used(v)) {
        kprintf("virtio-blk: request (type %u, sector %lu, %lu bytes) timed out\n", type, sector, (uint64_t)bytes);
        v->errors++;
        return -ETIMEDOUT;
    }
    if (*v->status != S_OK) { v->errors++; return -EIO; }
    return 0;
}

static int transfer(struct vblk *v, uint32_t type, uint64_t lba, uint32_t count, void *buf)
{
    if (lba > v->sectors || count > v->sectors - lba) return -ERANGE;
    size_t per = (size_t)v->max_segs * v->seg_bytes / SECTOR;      /* sectors per request */
    while (count) {
        uint32_t n = count < per ? count : (uint32_t)per;
        int rc = submit(v, type, lba, buf, (size_t)n * SECTOR);
        if (rc) return rc;
        lba += n;
        count -= n;
        buf = (uint8_t *)buf + (size_t)n * SECTOR;
    }
    return 0;
}

/* ---- block layer ----------------------------------------------------------- */
static int vblk_read(struct blkdev *b, uint64_t lba, uint32_t count, void *buf)
{
    return transfer(b->ctx, T_IN, lba, count, buf);
}

static int vblk_write(struct blkdev *b, uint64_t lba, uint32_t count, const void *buf)
{
    struct vblk *v = b->ctx;
    if (v->features & F_RO) return -EROFS;
    return transfer(v, T_OUT, lba, count, (void *)buf);
}

static int vblk_flush(struct blkdev *b)
{
    struct vblk *v = b->ctx;
    return v->features & F_FLUSH ? submit(v, T_FLUSH, 0, NULL, 0) : 0;
}

/* One request per disk at a time (block.c: disk_lock), one queue per disk */
static const struct blkdev_ops vblk_ops = { .read = vblk_read, .write = vblk_write, .flush = vblk_flush,
                                            .nolock = true };

/* ---- bring-up -------------------------------------------------------------- */
static bool vblk_init(struct pci_device *d)
{
    struct vblk *v = kzalloc(sizeof *v);
    if (!v) return false;
    v->pci = d;
    v->vector = -1;
    pci_enable(d, false, true, true);
    if (!find_caps(v)) {
        kprintf("virtio-blk: %02x:%02x.%u has no modern (virtio 1.0) interface, skipped\n", d->bus, d->device, d->function);
        kfree(v);
        return false;
    }
    C8(v, STATUS) = 0;                                  /* reset */
    for (int i = 0; i < 100000 && C8(v, STATUS); i++) cpu_relax();
    C8(v, STATUS) = ST_ACK;
    C8(v, STATUS) = ST_ACK | ST_DRIVER;
    C32(v, DFSEL) = 0;
    uint64_t have = C32(v, DF);
    C32(v, DFSEL) = 1;
    have |= (uint64_t)C32(v, DF) << 32;
    if (!(have & F_VERSION_1)) { C8(v, STATUS) = ST_FAILED; kfree(v); return false; }
    v->features = have & (F_VERSION_1 | F_SIZE_MAX | F_SEG_MAX | F_RO | F_FLUSH);
    C32(v, GFSEL) = 0; C32(v, GF) = (uint32_t)v->features;
    C32(v, GFSEL) = 1; C32(v, GF) = (uint32_t)(v->features >> 32);
    C8(v, STATUS) = ST_ACK | ST_DRIVER | ST_FEATURES_OK;
    if (!(C8(v, STATUS) & ST_FEATURES_OK)) { C8(v, STATUS) = ST_FAILED; kfree(v); return false; }
    C16(v, MSIX) = NO_VECTOR;                           /* no configuration-change interrupts */
    if (!vq_setup(v)) {
        kprintf("virtio-blk: queue setup failed\n");
        C8(v, STATUS) = ST_FAILED;
        return false;
    }
    v->sectors = *(volatile uint64_t *)v->devcfg;
    uint32_t size_max = v->features & F_SIZE_MAX ? *(volatile uint32_t *)(v->devcfg + 8) : 0;
    uint32_t seg_max = v->features & F_SEG_MAX ? *(volatile uint32_t *)(v->devcfg + 12) : 0;
    v->seg_bytes = size_max >= PAGE_SIZE && size_max < SEG_DEFAULT ? size_max & ~(SECTOR - 1) : SEG_DEFAULT;
    v->max_segs = (uint16_t)(v->qsize - 2);
    if (seg_max && seg_max < v->max_segs) v->max_segs = (uint16_t)seg_max;
    C8(v, STATUS) = ST_ACK | ST_DRIVER | ST_FEATURES_OK | ST_DRIVER_OK;

    kprintf("virtio-blk: %02x:%02x.%u: %lu MiB%s, %u segment(s) of up to %u KiB per request\n",
            d->bus, d->device, d->function, v->sectors * SECTOR >> 20, v->features & F_RO ? ", read-only" : "",
            v->max_segs, v->seg_bytes >> 10);
    disks[ndisks++] = v;
    return true;
}

void virtio_blk_init(void)
{
    for (size_t i = 0; i < pci_device_count() && ndisks < VBLK_MAX; i++) {
        struct pci_device *d = pci_device_at(i);
        if (d->vendor_id != VIRTIO_VENDOR) continue;
        if (d->device_id != VIRTIO_BLK_LEGACY && d->device_id != VIRTIO_BLK_MODERN) continue;
        if (d->device_id == VIRTIO_BLK_LEGACY && d->subsystem_id != 2) continue;   /* transitional: subsystem 2 = block */
        vblk_init(d);
    }
}

void virtio_blk_register(void)
{
    for (int i = 0; i < ndisks; i++) {
        struct blkdev *b = blk_register_disk("vd", BLK_DISK_VIRTIO, "virtio disk", SECTOR, disks[i]->sectors,
                                             &vblk_ops, disks[i]);
        if (b) blk_scan_partitions(b);
    }
}

/* The queue's MSI-X vector: a finished request wakes its waiter. */
static void vblk_msi(void *arg) { io_wake(arg); }

void virtio_blk_enable_irqs(void)
{
    for (int i = 0; i < ndisks; i++) {
        struct vblk *v = disks[i];
        int vec = msi_enable(v->pci, vblk_msi, v, "virtio-blk");
        if (vec < 0) continue;
        if (!msi_is_msix(vec)) { msi_disable(v->pci, vec); continue; }
        C16(v, QSEL) = 0;
        C16(v, QMSIX) = 0;                              /* table entry 0 */
        if (C16(v, QMSIX) != 0) { msi_disable(v->pci, vec); continue; }    /* the device refused it */
        msi_set_lockless(vec);                          /* it only wakes the waiter */
        v->avail->flags = 0;                            /* interrupts on */
        barrier();
        v->vector = vec;
    }
}

int virtio_blk_count(void) { return ndisks; }
