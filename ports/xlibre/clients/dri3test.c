/* =============================================================================
 *  dri3test.c -- checks Xkestrel's DRI3 the way a GPU client uses it
 *
 *  Makes a linear 32-bit buffer the way a GPU driver would hand one over: a
 *  dumb buffer on /dev/dri/card0 (or $DRI3TEST_CARD), exported as a dma-buf
 *  (PRIME). Then, against $DISPLAY:
 *    1. DRI3QueryVersion is 1.2 or later and DRI3Open gives a render node
 *    2. DRI3GetSupportedModifiers offers DRM_FORMAT_MOD_LINEAR
 *    3. DRI3PixmapFromBuffers makes a pixmap of the buffer; a copy to a
 *       window shows what the CPU wrote into the buffer
 *    4. PresentPixmap with an idle fence from DRI3FenceFromFD (a shared
 *       memory fence, as Mesa's Vulkan drivers use) shows a second picture,
 *       sends CompleteNotify and IdleNotify, and triggers the fence
 *    5. N more presents in a row (-n N, default 120) all complete
 *  Prints "ok" or "FAIL" per check and "dri3test: all checks passed" at the
 *  end; exits 1 if any check failed.
 *
 *  Uses libxcb (dri3, present) and libxshmfence, linked statically against
 *  the musl sysroot built by ports/xlibre/build-sysroot.sh.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <X11/xshmfence.h>

#define W 64
#define H 48

/* DRM uapi: dumb buffers and PRIME */
struct drm_mode_create_dumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
struct drm_mode_map_dumb    { uint32_t handle, pad; uint64_t offset; };
struct drm_prime_handle     { uint32_t handle, flags; int32_t fd; };
#define DRM_IOCTL_MODE_CREATE_DUMB  _IOWR('d', 0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB     _IOWR('d', 0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD _IOWR('d', 0x2d, struct drm_prime_handle)
#define DRM_CLOEXEC                 O_CLOEXEC
#define DRM_RDWR                    O_RDWR
#define MOD_LINEAR                  0ull

static int failures;

static void check(int ok, const char *what)
{
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void fill(uint32_t *p, uint32_t pitch, uint32_t seed)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            p[y * (pitch / 4) + x] = ((x * 4 + seed) & 0xff) << 16 | ((y * 5) & 0xff) << 8 | (seed & 0xff);
}

/* the window's pixels equal what fill() wrote with this seed? */
static int shows(xcb_connection_t *c, xcb_window_t win, uint32_t seed)
{
    xcb_get_image_reply_t *r = xcb_get_image_reply(c,
        xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, win, 0, 0, W, H, ~0u), NULL);
    if (!r) return 0;
    const uint32_t *px = (const uint32_t *)xcb_get_image_data(r);
    int ok = xcb_get_image_data_length(r) >= W * H * 4;
    for (int y = 0; ok && y < H; y++)
        for (int x = 0; ok && x < W; x++) {
            uint32_t want = ((x * 4 + seed) & 0xff) << 16 | ((y * 5) & 0xff) << 8 | (seed & 0xff);
            if ((px[y * W + x] & 0xffffff) != want) {
                printf("    pixel %d,%d is %06x, want %06x\n", x, y, px[y * W + x] & 0xffffff, want);
                ok = 0;
            }
        }
    free(r);
    return ok;
}

/* wait for this serial's CompleteNotify and IdleNotify */
static int present_events(xcb_connection_t *c, xcb_special_event_t *ev, uint32_t serial)
{
    int complete = 0, idle = 0;
    while (!(complete && idle)) {
        xcb_generic_event_t *e = xcb_wait_for_special_event(c, ev);
        if (!e) return 0;
        xcb_present_generic_event_t *g = (xcb_present_generic_event_t *)e;
        if (g->evtype == XCB_PRESENT_EVENT_COMPLETE_NOTIFY &&
            ((xcb_present_complete_notify_event_t *)e)->serial == serial) complete = 1;
        if (g->evtype == XCB_PRESENT_EVENT_IDLE_NOTIFY &&
            ((xcb_present_idle_notify_event_t *)e)->serial == serial) idle = 1;
        free(e);
    }
    return 1;
}

int main(int argc, char **argv)
{
    int rounds = 120;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-n") && i + 1 < argc) rounds = atoi(argv[++i]);
    const char *card = getenv("DRI3TEST_CARD") ? getenv("DRI3TEST_CARD") : "/dev/dri/card0";

    /* the buffer */
    int dfd = open(card, O_RDWR | O_CLOEXEC);
    if (dfd < 0) { printf("dri3test: %s: %s\n", card, strerror(errno)); return 1; }
    struct drm_mode_create_dumb cd = { .height = H, .width = W, .bpp = 32 };
    if (ioctl(dfd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) < 0) { printf("dri3test: CREATE_DUMB: %s\n", strerror(errno)); return 1; }
    struct drm_prime_handle ph = { .handle = cd.handle, .flags = DRM_CLOEXEC | DRM_RDWR, .fd = -1 };
    if (ioctl(dfd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph) < 0) { printf("dri3test: PRIME_HANDLE_TO_FD: %s\n", strerror(errno)); return 1; }
    uint32_t *pix = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, ph.fd, 0);
    if (pix == MAP_FAILED) { printf("dri3test: mmap of the dma-buf: %s\n", strerror(errno)); return 1; }
    printf("dri3test: %dx%d buffer on %s, pitch %u, %llu bytes\n", W, H, card, cd.pitch, (unsigned long long)cd.size);

    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c)) { printf("dri3test: cannot connect to the X server\n"); return 1; }
    xcb_screen_t *s = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    /* 1. version and DRI3Open */
    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(c, &xcb_dri3_id);
    if (!ext || !ext->present) { check(0, "the server has DRI3"); return 1; }
    xcb_dri3_query_version_reply_t *v = xcb_dri3_query_version_reply(c, xcb_dri3_query_version(c, 1, 2), NULL);
    check(v && (v->major_version > 1 || v->minor_version >= 2), "DRI3 1.2 or later");
    if (v) printf("    DRI3 %u.%u\n", v->major_version, v->minor_version);
    free(v);
    xcb_dri3_open_reply_t *o = xcb_dri3_open_reply(c, xcb_dri3_open(c, s->root, 0), NULL);
    int rfd = o && o->nfd == 1 ? xcb_dri3_open_reply_fds(c, o)[0] : -1;
    struct stat st;
    check(rfd >= 0 && fstat(rfd, &st) == 0 && S_ISCHR(st.st_mode) && minor(st.st_rdev) >= 128,
          "DRI3Open gives a render node");
    free(o);
    if (rfd >= 0) close(rfd);

    /* 2. modifiers */
    xcb_dri3_get_supported_modifiers_reply_t *m = xcb_dri3_get_supported_modifiers_reply(c,
        xcb_dri3_get_supported_modifiers(c, s->root, 24, 32), NULL);
    int linear = 0;
    if (m) {
        uint64_t *mods = xcb_dri3_get_supported_modifiers_screen_modifiers(m);
        for (int i = 0; i < xcb_dri3_get_supported_modifiers_screen_modifiers_length(m); i++)
            if (mods[i] == MOD_LINEAR) linear = 1;
    }
    check(linear, "DRM_FORMAT_MOD_LINEAR is supported");
    free(m);

    /* 3. pixmap from the buffer, copied to a window */
    xcb_window_t win = xcb_generate_id(c);
    uint32_t wv[] = { s->black_pixel, 1 };
    xcb_create_window(c, s->root_depth, win, s->root, 0, 0, W, H, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      s->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT, wv);
    xcb_map_window(c, win);
    xcb_gcontext_t gc = xcb_generate_id(c);
    xcb_create_gc(c, gc, win, 0, NULL);
    fill(pix, cd.pitch, 0x11);
    xcb_pixmap_t p = xcb_generate_id(c);
    int fds[4] = { dup(ph.fd), -1, -1, -1 };
    xcb_generic_error_t *err = xcb_request_check(c, xcb_dri3_pixmap_from_buffers_checked(c, p, win, 1, W, H,
        cd.pitch, 0, 0, 0, 0, 0, 0, 0, 24, 32, MOD_LINEAR, fds));
    check(!err, "DRI3PixmapFromBuffers takes the buffer");
    if (err) { printf("    X error %d\n", err->error_code); free(err); return 1; }
    xcb_copy_area(c, p, win, gc, 0, 0, 0, 0, W, H);
    check(shows(c, win, 0x11), "a copy from the pixmap shows the buffer's pixels");

    /* 4. PresentPixmap with an idle fence */
    int ffd = xshmfence_alloc_shm();
    struct xshmfence *f = ffd >= 0 ? xshmfence_map_shm(ffd) : NULL;
    check(f != NULL, "a shared memory fence");
    if (!f) return 1;
    uint32_t fence = xcb_generate_id(c);
    xcb_dri3_fence_from_fd(c, p, fence, 0, ffd);
    xcb_present_event_t eid = xcb_generate_id(c);
    xcb_special_event_t *ev = xcb_register_for_special_xge(c, &xcb_present_id, eid, NULL);
    xcb_present_select_input(c, eid, win, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY | XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);
    fill(pix, cd.pitch, 0x5a);
    xshmfence_reset(f);
    xcb_present_pixmap(c, win, p, 1, 0, 0, 0, 0, 0, 0, fence, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, NULL);
    xcb_flush(c);
    check(present_events(c, ev, 1), "PresentPixmap completes and the pixmap goes idle");
    check(xshmfence_query(f), "the idle fence is triggered");
    check(shows(c, win, 0x5a), "the window shows the presented picture");

    /* 5. a run of presents, each waiting for the fence as Mesa does */
    int done = 0;
    for (uint32_t i = 0; i < (uint32_t)rounds; i++) {
        xshmfence_await(f);
        fill(pix, cd.pitch, i);
        xshmfence_reset(f);
        xcb_present_pixmap(c, win, p, 2 + i, 0, 0, 0, 0, 0, 0, fence, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, NULL);
        xcb_flush(c);
        if (!present_events(c, ev, 2 + i)) break;
        done++;
    }
    char what[64];
    snprintf(what, sizeof what, "%d presents in a row complete", rounds);
    check(done == rounds, what);
    check(shows(c, win, (uint32_t)(rounds - 1)), "the window shows the last of them");

    xcb_unregister_for_special_event(c, ev);
    xcb_disconnect(c);
    if (failures) { printf("dri3test: %d check(s) failed\n", failures); return 1; }
    printf("dri3test: all checks passed\n");
    return 0;
}
