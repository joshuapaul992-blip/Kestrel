/* user/drmtest.c -- the DRM interface programs use, on /dev/dri
 *
 *   drmtest          checks on the first DRM device (bochs in QEMU): the
 *                    version and capabilities, the KMS objects, a dumb
 *                    buffer mapped and drawn, set on the first connected
 *                    monitor, a page flip with its event, the mapping
 *                    after fork and a partial munmap, the render node,
 *                    the /sys entries libdrm finds the device by, and
 *                    PRIME: a buffer as a dma-buf descriptor (its size,
 *                    mapping, CPU-access ioctl and fences as a sync file),
 *                    imported again here and through a second open, and
 *                    sync objects (binary and timeline, as descriptors and
 *                    sync files, and an eventfd on a timeline point)
 *   drmtest -h N     ... and keep the picture on screen for N seconds
 *                    (colour bars), so the host can take a screenshot
 * Exit status 0 = every check passed; 77 = no DRM device (skipped). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

/* the DRM interface (<drm/drm.h>, <drm/drm_mode.h>), defined here:
 * musl-gcc has no kernel headers */
struct drm_version {
    int major, minor, patch;
    size_t name_len; char *name;
    size_t date_len; char *date;
    size_t desc_len; char *desc;
};
struct drm_get_cap { uint64_t capability, value; };
struct drm_set_client_cap { uint64_t capability, value; };
struct drm_mode_card_res {
    uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
    uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
    uint32_t min_width, max_width, min_height, max_height;
};
struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh, flags, type;
    char name[32];
};
struct drm_mode_get_connector {
    uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
    uint32_t count_modes, count_props, count_encoders;
    uint32_t encoder_id, connector_id, connector_type, connector_type_id;
    uint32_t connection, mm_width, mm_height, subpixel, pad;
};
struct drm_mode_get_encoder { uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones; };
struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
    struct drm_mode_modeinfo mode;
};
struct drm_mode_create_dumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
struct drm_mode_map_dumb { uint32_t handle, pad; uint64_t offset; };
struct drm_mode_destroy_dumb { uint32_t handle; };
struct drm_mode_fb_cmd2 {
    uint32_t fb_id, width, height, pixel_format, flags;
    uint32_t handles[4], pitches[4], offsets[4];
    uint64_t modifier[4];
};
struct drm_mode_crtc_page_flip { uint32_t crtc_id, fb_id, flags, reserved; uint64_t user_data; };
struct drm_event { uint32_t type, length; };
struct drm_event_vblank { struct drm_event base; uint64_t user_data; uint32_t tv_sec, tv_usec, sequence, crtc_id; };

struct drm_prime_handle { uint32_t handle, flags; int32_t fd; };
struct drm_gem_close { uint32_t handle, pad; };
struct dma_buf_sync { uint64_t flags; };
struct dma_buf_export_sync_file { uint32_t flags; int32_t fd; };
struct sync_file_info { char name[32]; int32_t status; uint32_t flags, num_fences, pad; uint64_t sync_fence_info; };

#define DRM_IOCTL_VERSION            _IOWR('d', 0x00, struct drm_version)
#define DRM_IOCTL_GEM_CLOSE          _IOW('d', 0x09, struct drm_gem_close)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD _IOWR('d', 0x2d, struct drm_prime_handle)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE _IOWR('d', 0x2e, struct drm_prime_handle)
#define DMA_BUF_IOCTL_SYNC           _IOW('b', 0, struct dma_buf_sync)
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE _IOWR('b', 2, struct dma_buf_export_sync_file)
#define SYNC_IOC_FILE_INFO           _IOWR('>', 4, struct sync_file_info)
#define DRM_CAP_PRIME                0x5
#define DMA_BUF_SYNC_RW              3
#define DMA_BUF_SYNC_END             4
#define DRM_IOCTL_GET_CAP            _IOWR('d', 0x0c, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP     _IOW('d', 0x0d, struct drm_set_client_cap)
#define DRM_IOCTL_MODE_GETRESOURCES  _IOWR('d', 0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC       _IOWR('d', 0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC       _IOWR('d', 0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER    _IOWR('d', 0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR  _IOWR('d', 0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_RMFB          _IOWR('d', 0xAF, unsigned int)
#define DRM_IOCTL_MODE_PAGE_FLIP     _IOWR('d', 0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_CREATE_DUMB   _IOWR('d', 0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB      _IOWR('d', 0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB  _IOWR('d', 0xB4, struct drm_mode_destroy_dumb)
#define DRM_IOCTL_MODE_ADDFB2        _IOWR('d', 0xB8, struct drm_mode_fb_cmd2)
#define DRM_CAP_DUMB_BUFFER          0x1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_MODE_CONNECTED           1
#define DRM_MODE_PAGE_FLIP_EVENT     0x01
#define DRM_EVENT_FLIP_COMPLETE      0x02
#define DRM_FORMAT_XRGB8888          0x34325258      /* 'XR24' */

static int failures;
#define CHECK(c, what) do { int ok_ = (c); printf("drmtest:  %s  %s\n", ok_ ? "ok  " : "FAIL", what); \
                            if (!ok_) { failures++; printf("drmtest:        (errno %d: %s)\n", errno, strerror(errno)); } } while (0)

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do r = ioctl(fd, req, arg); while (r < 0 && (errno == EINTR || errno == EAGAIN));
    return r;
}

struct buf {
    uint32_t handle, pitch, fb;
    uint64_t size;
    uint32_t *map;
};

static int make_buf(int fd, uint32_t w, uint32_t h, struct buf *b)
{
    struct drm_mode_create_dumb c = { .height = h, .width = w, .bpp = 32 };
    if (xioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c)) return -1;
    b->handle = c.handle;
    b->pitch = c.pitch;
    b->size = c.size;
    struct drm_mode_map_dumb m = { .handle = c.handle };
    if (xioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &m)) return -1;
    b->map = mmap(NULL, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)m.offset);
    if (b->map == MAP_FAILED) { b->map = NULL; return -1; }
    struct drm_mode_fb_cmd2 f = { .width = w, .height = h, .pixel_format = DRM_FORMAT_XRGB8888 };
    f.handles[0] = c.handle;
    f.pitches[0] = c.pitch;
    if (xioctl(fd, DRM_IOCTL_MODE_ADDFB2, &f)) return -1;
    b->fb = f.fb_id;
    return 0;
}

static const uint32_t col[8] = { 0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x000000 };

/* eight vertical colour bars, or a solid colour */
static void paint(struct buf *b, uint32_t w, uint32_t h, int bars, uint32_t solid)
{
    for (uint32_t y = 0; y < h; y++) {
        uint32_t *row = b->map + (size_t)y * (b->pitch / 4);
        for (uint32_t x = 0; x < w; x++) row[x] = bars ? col[x * 8 / w] : solid;
    }
}

/* A file under /sys, its first line in buf (without the newline). */
static int sys_read(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char *r = fgets(buf, (int)cap, f);
    fclose(f);
    if (!r) return -1;
    buf[strcspn(buf, "\n")] = 0;
    return 0;
}

/* What libdrm's drmGetDevice2() reads (xf86drm.c), for the node open as fd:
 * /sys/dev/char/M:m/device/drm exists, device/subsystem names "pci", the
 * device's uevent has PCI_SLOT_NAME, and vendor/device agree with config. */
static void check_sysfs(int fd)
{
    struct stat st;
    char path[128], link[256], line[128];
    if (fstat(fd, &st) < 0) return;
    unsigned maj = major(st.st_rdev), min = minor(st.st_rdev);
    printf("drmtest:        /dev/dri/card0 is %u:%u\n", maj, min);
    snprintf(path, sizeof path, "/sys/dev/char/%u:%u/device/drm", maj, min);
    CHECK(maj == 226 && stat(path, &st) == 0 && S_ISDIR(st.st_mode), "/sys/dev/char/226:N/device/drm is there");
    snprintf(path, sizeof path, "/sys/dev/char/%u:%u/device/subsystem", maj, min);
    ssize_t n = readlink(path, link, sizeof link - 1);
    if (n > 0) link[n] = 0;
    CHECK(n > 0 && strrchr(link, '/') && !strcmp(strrchr(link, '/'), "/pci"), "its device is on the PCI bus");

    snprintf(path, sizeof path, "/sys/dev/char/%u:%u/device/uevent", maj, min);
    FILE *f = fopen(path, "r");
    unsigned dom = 99, bus = 0, dev = 0, fn = 0;
    while (f && fgets(line, sizeof line, f))
        if (!strncmp(line, "PCI_SLOT_NAME=", 14)) sscanf(line + 14, "%x:%x:%x.%u", &dom, &bus, &dev, &fn);
    if (f) fclose(f);
    CHECK(dom == 0, "uevent has PCI_SLOT_NAME");
    printf("drmtest:        PCI %04x:%02x:%02x.%u\n", dom, bus, dev, fn);

    unsigned vendor = 0, device = 0;
    uint16_t cfg[2] = { 0 };
    snprintf(path, sizeof path, "/sys/bus/pci/devices/%04x:%02x:%02x.%u/vendor", dom, bus, dev, fn);
    if (sys_read(path, line, sizeof line) == 0) vendor = (unsigned)strtoul(line, NULL, 16);
    snprintf(path, sizeof path, "/sys/bus/pci/devices/%04x:%02x:%02x.%u/device", dom, bus, dev, fn);
    if (sys_read(path, line, sizeof line) == 0) device = (unsigned)strtoul(line, NULL, 16);
    snprintf(path, sizeof path, "/sys/bus/pci/devices/%04x:%02x:%02x.%u/config", dom, bus, dev, fn);
    int cfd = open(path, O_RDONLY);
    if (cfd >= 0) { if (read(cfd, cfg, 4) != 4) cfg[0] = 0; close(cfd); }
    CHECK(vendor && vendor == cfg[0] && device == cfg[1], "vendor and device match the configuration space");
    printf("drmtest:        %04x:%04x\n", vendor, device);

    snprintf(path, sizeof path, "/sys/class/drm/card%u/dev", min);
    snprintf(link, sizeof link, "%u:%u", maj, min);
    CHECK(sys_read(path, line, sizeof line) == 0 && !strcmp(line, link), "/sys/class/drm/cardN/dev names the node");
}

/* PRIME: a dumb buffer exported as a dma-buf and imported back, here and
 * through a second open of the device (as Mesa hands buffers to X) */
static void check_prime(int fd)
{
    struct drm_get_cap cap = { .capability = DRM_CAP_PRIME };
    CHECK(xioctl(fd, DRM_IOCTL_GET_CAP, &cap) == 0 && cap.value == 3, "PRIME import and export are offered");
    struct drm_mode_create_dumb c = { .height = 64, .width = 64, .bpp = 32 };
    struct drm_mode_map_dumb m = { 0 };
    uint32_t *map = MAP_FAILED, *pmap = MAP_FAILED;
    if (xioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) == 0) {
        m.handle = c.handle;
        if (xioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &m) == 0)
            map = mmap(NULL, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)m.offset);
    }
    CHECK(map != MAP_FAILED, "a dumb buffer to share");
    if (map == MAP_FAILED) return;

    struct drm_prime_handle ph = { .handle = c.handle, .flags = O_CLOEXEC | O_RDWR, .fd = -1 };
    CHECK(xioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph) == 0 && ph.fd >= 0, "PRIME_HANDLE_TO_FD gives a dma-buf");
    int pfd = ph.fd;
    struct stat st;
    CHECK(pfd >= 0 && lseek(pfd, 0, SEEK_END) == (off_t)c.size && fstat(pfd, &st) == 0 &&
          st.st_size == (off_t)c.size, "its size, by lseek(SEEK_END) and fstat");
    char link[64] = "";
    char path[40];
    snprintf(path, sizeof path, "/proc/self/fd/%d", pfd);
    ssize_t ln = readlink(path, link, sizeof link - 1);
    if (ln > 0) link[ln] = 0;
    printf("drmtest:        %s -> %s\n", path, link);
    struct drm_prime_handle back = { .fd = pfd };
    CHECK(xioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &back) == 0 && back.handle == c.handle,
          "PRIME_FD_TO_HANDLE gives the same handle back");
    struct drm_prime_handle again = { .handle = c.handle, .flags = O_CLOEXEC };
    struct drm_prime_handle back2 = { 0 };
    CHECK(xioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &again) == 0 &&
          (back2.fd = again.fd, xioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &back2)) == 0 && back2.handle == c.handle,
          "exporting it again gives the same buffer");
    if (again.fd >= 0) close(again.fd);
    struct drm_prime_handle bad = { .fd = fd };
    CHECK(ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &bad) < 0 && errno == EINVAL, "a descriptor that is no dma-buf: EINVAL");

    pmap = mmap(NULL, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, pfd, 0);
    CHECK(pmap != MAP_FAILED, "the dma-buf maps");
    struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_RW };
    CHECK(xioctl(pfd, DMA_BUF_IOCTL_SYNC, &sync) == 0, "DMA_BUF_IOCTL_SYNC: start of CPU access");
    if (pmap != MAP_FAILED) for (unsigned i = 0; i < c.size / 4; i++) pmap[i] = 0x5a000000u | i;
    sync.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END;
    CHECK(xioctl(pfd, DMA_BUF_IOCTL_SYNC, &sync) == 0, "DMA_BUF_IOCTL_SYNC: end");
    CHECK(pmap != MAP_FAILED && map[100] == (0x5a000000u | 100) && map[c.size / 4 - 1] == (0x5a000000u | (c.size / 4 - 1)),
          "what was written through the dma-buf reads back through the buffer's own mapping");

    struct dma_buf_export_sync_file es = { .flags = DMA_BUF_SYNC_RW, .fd = -1 };
    CHECK(xioctl(pfd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &es) == 0 && es.fd >= 0, "its fences as a sync file");
    struct pollfd p = { es.fd, POLLIN, 0 };
    struct sync_file_info si = { 0 };
    CHECK(es.fd >= 0 && poll(&p, 1, 1000) == 1 && (p.revents & POLLIN) &&
          xioctl(es.fd, SYNC_IOC_FILE_INFO, &si) == 0 && si.status == 1,
          "the sync file polls ready, and SYNC_IOC_FILE_INFO says signaled (no work is pending)");
    if (es.fd >= 0) close(es.fd);

    /* a second open of the device imports it: its own handle, the same memory */
    int fd2 = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    struct drm_prime_handle imp = { .fd = pfd };
    struct drm_mode_map_dumb m2 = { 0 };
    uint32_t *map2 = MAP_FAILED;
    if (fd2 >= 0 && xioctl(fd2, DRM_IOCTL_PRIME_FD_TO_HANDLE, &imp) == 0) {
        m2.handle = imp.handle;
        if (xioctl(fd2, DRM_IOCTL_MODE_MAP_DUMB, &m2) == 0)
            map2 = mmap(NULL, c.size, PROT_READ, MAP_SHARED, fd2, (off_t)m2.offset);
    }
    CHECK(map2 != MAP_FAILED && map2[100] == (0x5a000000u | 100), "another open of the device imports it and sees the same pixels");
    if (map2 != MAP_FAILED) munmap(map2, c.size);
    if (fd2 >= 0) close(fd2);

    /* the buffer outlives its handle while the dma-buf is open */
    struct drm_gem_close gc = { .handle = c.handle };
    CHECK(xioctl(fd, DRM_IOCTL_GEM_CLOSE, &gc) == 0, "the handle is closed");
    struct drm_prime_handle re = { .fd = pfd };
    CHECK(xioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &re) == 0 && pmap != MAP_FAILED && pmap[7] == (0x5a000000u | 7),
          "the dma-buf keeps the buffer: it imports again and still maps");
    gc.handle = re.handle;
    xioctl(fd, DRM_IOCTL_GEM_CLOSE, &gc);
    if (pmap != MAP_FAILED) munmap(pmap, c.size);
    munmap(map, c.size);
    CHECK(close(pfd) == 0, "the dma-buf closes");
}

/* sync objects (DRM_CAP_SYNCOBJ): binary and timeline ones, their
 * descriptors and sync files, and an eventfd that a timeline point signals */
struct drm_syncobj_create { uint32_t handle, flags; };
struct drm_syncobj_destroy { uint32_t handle, pad; };
struct drm_syncobj_handle { uint32_t handle, flags; int32_t fd; uint32_t pad; };
struct drm_syncobj_wait {
    uint64_t handles; int64_t timeout_nsec;
    uint32_t count_handles, flags, first_signaled, pad; uint64_t deadline_nsec;
};
struct drm_syncobj_timeline_wait {
    uint64_t handles, points; int64_t timeout_nsec;
    uint32_t count_handles, flags, first_signaled, pad; uint64_t deadline_nsec;
};
struct drm_syncobj_eventfd { uint32_t handle, flags; uint64_t point; int32_t fd; uint32_t pad; };
struct drm_syncobj_array { uint64_t handles; uint32_t count_handles, pad; };
struct drm_syncobj_timeline_array { uint64_t handles, points; uint32_t count_handles, flags; };
#define DRM_CAP_SYNCOBJ                      0x13
#define DRM_CAP_SYNCOBJ_TIMELINE             0x14
#define DRM_SYNCOBJ_CREATE_SIGNALED          1
#define DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE 1
#define DRM_IOCTL_SYNCOBJ_CREATE             _IOWR('d', 0xBF, struct drm_syncobj_create)
#define DRM_IOCTL_SYNCOBJ_DESTROY            _IOWR('d', 0xC0, struct drm_syncobj_destroy)
#define DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD       _IOWR('d', 0xC1, struct drm_syncobj_handle)
#define DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE       _IOWR('d', 0xC2, struct drm_syncobj_handle)
#define DRM_IOCTL_SYNCOBJ_WAIT               _IOWR('d', 0xC3, struct drm_syncobj_wait)
#define DRM_IOCTL_SYNCOBJ_RESET              _IOWR('d', 0xC4, struct drm_syncobj_array)
#define DRM_IOCTL_SYNCOBJ_SIGNAL             _IOWR('d', 0xC5, struct drm_syncobj_array)
#define DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT      _IOWR('d', 0xCA, struct drm_syncobj_timeline_wait)
#define DRM_IOCTL_SYNCOBJ_QUERY              _IOWR('d', 0xCB, struct drm_syncobj_timeline_array)
#define DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL    _IOWR('d', 0xCD, struct drm_syncobj_timeline_array)
#define DRM_IOCTL_SYNCOBJ_EVENTFD            _IOWR('d', 0xCF, struct drm_syncobj_eventfd)

static void check_syncobj(int fd)
{
    struct drm_get_cap cap = { .capability = DRM_CAP_SYNCOBJ }, tcap = { .capability = DRM_CAP_SYNCOBJ_TIMELINE };
    if (xioctl(fd, DRM_IOCTL_GET_CAP, &cap) != 0 || cap.value != 1) {
        printf("drmtest:        no sync objects (the driver does not offer them)\n");
        return;
    }
    /* binary: created signalled, reset, signalled again */
    struct drm_syncobj_create c = { .flags = DRM_SYNCOBJ_CREATE_SIGNALED };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &c) == 0 && c.handle, "SYNCOBJ_CREATE (signalled)");
    struct drm_syncobj_wait w = { .handles = (uintptr_t)&c.handle, .count_handles = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &w) == 0, "SYNCOBJ_WAIT returns at once on a signalled one");
    struct drm_syncobj_array a = { .handles = (uintptr_t)&c.handle, .count_handles = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_RESET, &a) == 0 && ioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &w) < 0 && errno == EINVAL,
          "after SYNCOBJ_RESET there is nothing to wait for (EINVAL)");
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_SIGNAL, &a) == 0 && xioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &w) == 0,
          "SYNCOBJ_SIGNAL signals it again");

    /* as a descriptor, and as a sync file */
    struct drm_syncobj_handle h = { .handle = c.handle, .fd = -1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &h) == 0 && h.fd >= 0, "SYNCOBJ_HANDLE_TO_FD gives a descriptor");
    struct drm_syncobj_handle back = { .fd = h.fd };
    CHECK(h.fd >= 0 && xioctl(fd, DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, &back) == 0 && back.handle && back.handle != c.handle,
          "SYNCOBJ_FD_TO_HANDLE makes a new handle for it");
    if (h.fd >= 0) close(h.fd);
    struct drm_syncobj_handle sf = { .handle = c.handle, .flags = DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE, .fd = -1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &sf) == 0 && sf.fd >= 0, "its fence as a sync file");
    struct pollfd p = { .fd = sf.fd, .events = POLLIN };
    CHECK(sf.fd >= 0 && poll(&p, 1, 0) == 1 && (p.revents & POLLIN), "the sync file polls ready");
    if (sf.fd >= 0) close(sf.fd);
    struct drm_syncobj_destroy d = { .handle = back.handle };
    if (back.handle) xioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d);
    d.handle = c.handle;
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d) == 0 && ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d) < 0 && errno == EINVAL,
          "SYNCOBJ_DESTROY, then the handle is gone");

    if (xioctl(fd, DRM_IOCTL_GET_CAP, &tcap) != 0 || tcap.value != 1) return;
    /* timeline: a point signalled, queried, waited for; an eventfd on a later point */
    struct drm_syncobj_create t = { 0 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &t) == 0, "a timeline sync object");
    uint64_t point = 5, got = 0;
    struct drm_syncobj_timeline_array ta = { .handles = (uintptr_t)&t.handle, .points = (uintptr_t)&point, .count_handles = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL, &ta) == 0, "SYNCOBJ_TIMELINE_SIGNAL point 5");
    struct drm_syncobj_timeline_array q = { .handles = (uintptr_t)&t.handle, .points = (uintptr_t)&got, .count_handles = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_QUERY, &q) == 0 && got == 5, "SYNCOBJ_QUERY says 5");
    uint64_t three = 3;
    struct drm_syncobj_timeline_wait tw = { .handles = (uintptr_t)&t.handle, .points = (uintptr_t)&three, .count_handles = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &tw) == 0, "SYNCOBJ_TIMELINE_WAIT for point 3 returns at once");
    int efd = eventfd(0, EFD_CLOEXEC);
    struct drm_syncobj_eventfd ev = { .handle = t.handle, .point = 7, .fd = efd };
    CHECK(efd >= 0 && xioctl(fd, DRM_IOCTL_SYNCOBJ_EVENTFD, &ev) == 0, "SYNCOBJ_EVENTFD on point 7");
    struct pollfd ep = { .fd = efd, .events = POLLIN };
    CHECK(efd >= 0 && poll(&ep, 1, 0) == 0, "the eventfd waits while point 7 is not signalled");
    point = 7;
    uint64_t n = 0;
    CHECK(xioctl(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL, &ta) == 0 && efd >= 0 && poll(&ep, 1, 2000) == 1 &&
          read(efd, &n, sizeof n) == (ssize_t)sizeof n && n >= 1, "signalling point 7 wakes the eventfd");
    if (efd >= 0) close(efd);
    d.handle = t.handle;
    xioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d);
}

int main(int argc, char **argv)
{
    int hold = argc > 2 && !strcmp(argv[1], "-h") ? atoi(argv[2]) : 0;
    setvbuf(stdout, NULL, _IOLBF, 0);

    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) { printf("drmtest: no /dev/dri/card0: skipped\n"); return 77; }
    CHECK(fd >= 0, "/dev/dri/card0 opens");
    if (fd < 0) return 1;

    char name[64] = "", date[64] = "", desc[128] = "";
    struct drm_version v = { .name_len = sizeof name - 1, .name = name, .date_len = sizeof date - 1, .date = date,
                             .desc_len = sizeof desc - 1, .desc = desc };
    CHECK(xioctl(fd, DRM_IOCTL_VERSION, &v) == 0 && name[0], "DRM_IOCTL_VERSION names the driver");
    printf("drmtest:        driver %s %d.%d.%d (%s)\n", name, v.major, v.minor, v.patch, desc);

    check_sysfs(fd);

    struct drm_get_cap cap = { .capability = DRM_CAP_DUMB_BUFFER };
    CHECK(xioctl(fd, DRM_IOCTL_GET_CAP, &cap) == 0 && cap.value == 1, "dumb buffers are offered");
    struct drm_set_client_cap cc = { .capability = DRM_CLIENT_CAP_UNIVERSAL_PLANES, .value = 1 };
    CHECK(xioctl(fd, DRM_IOCTL_SET_CLIENT_CAP, &cc) == 0, "universal planes can be asked for");

    /* the KMS objects: ask for the counts, then the ids */
    struct drm_mode_card_res res = { 0 };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0 && res.count_crtcs && res.count_connectors,
          "GETRESOURCES lists CRTCs and connectors");
    uint32_t crtcs[16], conns[16], encs[16];
    if (res.count_crtcs > 16 || res.count_connectors > 16 || res.count_encoders > 16) return 1;
    res.count_fbs = 0;
    res.crtc_id_ptr = (uintptr_t)crtcs;
    res.connector_id_ptr = (uintptr_t)conns;
    res.encoder_id_ptr = (uintptr_t)encs;
    CHECK(xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == 0, "GETRESOURCES fills in the ids");
    printf("drmtest:        %u CRTC(s), %u connector(s), %u encoder(s), up to %ux%u\n", res.count_crtcs,
           res.count_connectors, res.count_encoders, res.max_width, res.max_height);

    /* the first connected connector, its preferred (first) mode and a CRTC */
    struct drm_mode_modeinfo modes[64], mode;
    uint32_t conn_id = 0, crtc_id = 0;
    for (uint32_t i = 0; i < res.count_connectors && !conn_id; i++) {
        struct drm_mode_get_connector c = { .connector_id = conns[i] };
        if (xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c) || c.connection != DRM_MODE_CONNECTED || !c.count_modes)
            continue;
        uint32_t n = c.count_modes < 64 ? c.count_modes : 64, enc[8];
        struct drm_mode_get_connector c2 = { .connector_id = conns[i], .count_modes = n,
                                             .modes_ptr = (uintptr_t)modes, .count_encoders = 8,
                                             .encoders_ptr = (uintptr_t)enc };
        if (xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c2)) continue;
        mode = modes[0];
        conn_id = conns[i];
        struct drm_mode_get_encoder e = { .encoder_id = c2.encoder_id ? c2.encoder_id : enc[0] };
        if (xioctl(fd, DRM_IOCTL_MODE_GETENCODER, &e) == 0) {
            crtc_id = e.crtc_id;
            for (uint32_t k = 0; !crtc_id && k < res.count_crtcs; k++)
                if (e.possible_crtcs & (1u << k)) crtc_id = crtcs[k];
        }
    }
    CHECK(conn_id && crtc_id, "a connected monitor with modes and a CRTC");
    if (!conn_id || !crtc_id) return 1;
    uint32_t w = mode.hdisplay, h = mode.vdisplay;
    printf("drmtest:        connector %u on CRTC %u: %s (%ux%u@%u)\n", conn_id, crtc_id, mode.name, w, h, mode.vrefresh);

    /* what the CRTC shows now (Kestrel's terminal), to compare after */
    struct drm_mode_crtc before = { .crtc_id = crtc_id };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_GETCRTC, &before) == 0 && before.fb_id, "GETCRTC: the terminal's frame buffer is up");

    /* two dumb buffers, mapped and drawn */
    struct buf a = { 0 }, b = { 0 };
    CHECK(make_buf(fd, w, h, &a) == 0 && make_buf(fd, w, h, &b) == 0, "two dumb buffers: created, mapped, added as frame buffers");
    if (!a.map || !b.map) return 1;
    paint(&a, w, h, 1, 0);
    paint(&b, w, h, 0, 0x203080);
    int same = 1;
    for (uint32_t y = 0; y < h && same; y += 37)
        for (uint32_t x = 0; x < w; x += 13)
            if ((a.map[(size_t)y * (a.pitch / 4) + x] & 0xffffff) != col[x * 8 / w]) {
                same = 0;
                break;
            }
    CHECK(same, "what was written through the mapping reads back");

    /* light the monitor with buffer a */
    struct drm_mode_crtc set = { .crtc_id = crtc_id, .fb_id = a.fb, .set_connectors_ptr = (uintptr_t)&conn_id,
                                 .count_connectors = 1, .mode_valid = 1, .mode = mode };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_SETCRTC, &set) == 0, "SETCRTC puts the colour bars on the monitor");
    struct drm_mode_crtc now = { .crtc_id = crtc_id };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_GETCRTC, &now) == 0 && now.fb_id == a.fb, "GETCRTC shows our frame buffer");

    /* flip to b and wait for the event */
    struct drm_mode_crtc_page_flip flip = { .crtc_id = crtc_id, .fb_id = b.fb, .flags = DRM_MODE_PAGE_FLIP_EVENT,
                                            .user_data = 0x1234 };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) == 0, "PAGE_FLIP to the second buffer");
    struct pollfd p = { .fd = fd, .events = POLLIN };
    int pr = poll(&p, 1, 2000);
    struct drm_event_vblank ev = { 0 };
    ssize_t n = pr > 0 ? read(fd, &ev, sizeof ev) : -1;
    CHECK(pr > 0 && n == (ssize_t)sizeof ev && ev.base.type == DRM_EVENT_FLIP_COMPLETE && ev.user_data == 0x1234 &&
          ev.crtc_id == crtc_id, "the flip's completion event arrives (poll, read)");
    /* and back to the bars */
    flip.fb_id = a.fb;
    if (xioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) == 0 && poll(&p, 1, 2000) > 0) (void)!read(fd, &ev, sizeof ev);

    /* a child sees the same buffer: its mapping faults in again */
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        uint32_t px = b.map[(size_t)(h / 2) * (b.pitch / 4) + w / 2] & 0xffffff;
        a.map[1] = 0x00c0ffee;
        _exit(px == 0x203080 ? 0 : 1);
    }
    int st = -1;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0 && (a.map[1] & 0xffffff) == 0xc0ffee,
          "after fork, parent and child share the buffer");
    a.map[1] = 0xffffff;

    /* unmap the first half of b: the second half stays */
    size_t half = (b.size / 2) & ~(size_t)4095;
    CHECK(munmap(b.map, half) == 0, "munmap of half a buffer");
    uint32_t *rest = (uint32_t *)((char *)b.map + half);
    CHECK((rest[1024] & 0xffffff) == 0x203080, "the other half is still mapped");

    /* the render node, if the driver renders (bochs only scans out) */
    int rfd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (rfd < 0 && errno == ENOENT) {
        printf("drmtest:        no render node (%s has none)\n", name);
    } else {
        char rname[64] = "";
        struct drm_version rv = { .name_len = sizeof rname - 1, .name = rname };
        CHECK(rfd >= 0 && xioctl(rfd, DRM_IOCTL_VERSION, &rv) == 0 && !strcmp(rname, name),
              "/dev/dri/renderD128 opens and answers");
        struct drm_mode_card_res rres = { 0 };
        CHECK(rfd >= 0 && ioctl(rfd, DRM_IOCTL_MODE_GETRESOURCES, &rres) < 0, "the render node refuses modesetting");
        if (rfd >= 0) close(rfd);
    }

    if (hold > 0) {
        printf("drmtest: holding the colour bars for %d s\n", hold);
        sleep((unsigned)hold);
    }

    check_prime(fd);
    check_syncobj(fd);

    /* tidy up: the terminal comes back when the file closes */
    munmap(rest, b.size - half);
    munmap(a.map, a.size);
    unsigned int fbid = a.fb;
    xioctl(fd, DRM_IOCTL_MODE_RMFB, &fbid);
    fbid = b.fb;
    xioctl(fd, DRM_IOCTL_MODE_RMFB, &fbid);
    struct drm_mode_destroy_dumb d = { .handle = a.handle };
    CHECK(xioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d) == 0, "DESTROY_DUMB");
    d.handle = b.handle;
    xioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
    close(fd);

    int fd2 = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    struct drm_mode_crtc after = { .crtc_id = crtc_id };
    CHECK(fd2 >= 0 && xioctl(fd2, DRM_IOCTL_MODE_GETCRTC, &after) == 0 && after.fb_id == before.fb_id,
          "after the last close the terminal's frame buffer is back");
    if (fd2 >= 0) close(fd2);

    printf("drmtest: %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
