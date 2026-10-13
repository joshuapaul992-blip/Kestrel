/* linuxkpi/src/drm_kestrel.c -- Kestrel's terminals on Linux DRM drivers
 *
 * Linux drivers call drm_fbdev_ttm_setup() (or its shmem/dma siblings) once
 * they are registered, to get a console on the fbdev layer. Kestrel has no
 * fbdev: this drm_fbdev_ttm_setup() is an in-kernel DRM client that lights
 * every monitor the driver finds at its preferred mode and hands each one to
 * Kestrel as a display head, so it gets a terminal like any other monitor.
 *
 *   - drm_client_modeset_probe() picks a mode for each CRTC with a connected
 *     monitor (the EDID's preferred mode, as Linux's fbdev console would);
 *   - each CRTC gets its own XRGB8888 frame buffer, from the driver's dumb
 *     buffer code, and drm_client_modeset_commit() puts it on screen;
 *   - when the frame buffer is CPU-mapped I/O memory that scans out as it
 *     is written (VRAM, no dirty callback), the head points straight at it;
 *     otherwise Kestrel's writes land in the buffer through write_span() and
 *     a worker reports the damage with drm_client_framebuffer_flush().
 *
 * Monitors are not removed again: Kestrel's display registry has no hot
 * unplug, so after the driver unbinds (only at shutdown here) the head's
 * writes are dropped. */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/iosys-map.h>
#include <linux/dma-resv.h>
#include <drm/drm_client.h>
#include <drm/drm_crtc.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fbdev_ttm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>
#include <drm/drm_property.h>
#include <drm/drm_rect.h>
#include <lkpi/glue.h>
#include "../../third_party/linux/drivers/gpu/drm/drm_internal.h"   /* drm_gem_vmap() */

#define KDRM_MAX_HEADS  4

struct kdrm_head {
    struct kdrm_client       *kc;
    struct drm_client_buffer *buffer;
    struct drm_crtc          *crtc;
    struct iosys_map          map;
    int                       width, height, pitch;
    int                       monitor;          /* Kestrel's monitor number */
    spinlock_t                lock;             /* damage                    */
    int                       damage_y0, damage_y1;     /* rows, y1 exclusive; empty if y0 >= y1 */
    struct work_struct        flush_work;
};

struct kdrm_client {
    struct drm_client_dev client;
    struct kdrm_head      heads[KDRM_MAX_HEADS];
    int                   nheads;
    bool                  lit;                  /* frame buffers are on screen */
    bool                  gone;                 /* the driver unbound          */
};

static void kdrm_flush_work(struct work_struct *work)
{
    struct kdrm_head *h = container_of(work, struct kdrm_head, flush_work);
    unsigned long flags;

    spin_lock_irqsave(&h->lock, flags);
    int y0 = h->damage_y0, y1 = h->damage_y1;
    h->damage_y0 = h->height;
    h->damage_y1 = 0;
    spin_unlock_irqrestore(&h->lock, flags);
    if (y0 >= y1 || READ_ONCE(h->kc->gone)) return;

    struct drm_rect clip = DRM_RECT_INIT(0, y0, h->width, y1 - y0);
    int ret = drm_client_framebuffer_flush(h->buffer, &clip);
    if (ret) drm_dbg_kms(h->kc->client.dev, "kestrel: flush of monitor %d failed: %d\n", h->monitor, ret);
}

/* Kestrel's terminal publishes changed bytes of its RAM copy: copy them into
 * the frame buffer and report the rows as damaged. */
static void kdrm_write_span(void *ctx, unsigned int offset, const void *src, unsigned int len)
{
    struct kdrm_head *h = ctx;
    unsigned long flags;

    if (!len || READ_ONCE(h->kc->gone)) return;
    if ((unsigned long)offset + len > (unsigned long)h->pitch * h->height) return;
    iosys_map_memcpy_to(&h->map, offset, src, len);
    if (!h->buffer->fb->funcs->dirty) return;           /* scans out what was written */

    spin_lock_irqsave(&h->lock, flags);
    int y0 = offset / h->pitch, y1 = (offset + len - 1) / h->pitch + 1;
    if (y0 < h->damage_y0) h->damage_y0 = y0;
    if (y1 > h->damage_y1) h->damage_y1 = y1;
    spin_unlock_irqrestore(&h->lock, flags);
    schedule_work(&h->flush_work);
}

static void kdrm_monitor_name(struct drm_mode_set *ms, char *name, int size)
{
    struct drm_connector *connector = ms->num_connectors ? ms->connectors[0] : NULL;
    struct drm_property_blob *blob;

    name[0] = 0;
    if (!connector) return;
    mutex_lock(&connector->dev->mode_config.mutex);
    blob = connector->edid_blob_ptr;                    /* the EDID the last probe read */
    if (blob && blob->length >= EDID_LENGTH) drm_edid_get_monitor_name(blob->data, name, size);
    mutex_unlock(&connector->dev->mode_config.mutex);
}

/* Map a frame buffer for good, where it is. drm_client_buffer_vmap() pins
 * the buffer first through the driver's PRIME pin, which nouveau places in
 * GART: a buffer the mode set already pinned in VRAM for scan-out then
 * fails with "pinned elsewhere". Being on screen keeps it pinned, so map it
 * in place. */
static int kdrm_vmap(struct drm_client_buffer *buffer, struct iosys_map *map)
{
    struct drm_gem_object *gem = buffer->gem;
    int ret;

    dma_resv_lock(gem->resv, NULL);
    ret = drm_gem_vmap(gem, &buffer->map);
    dma_resv_unlock(gem->resv);
    if (!ret) *map = buffer->map;
    return ret;
}

/* The driver's own account of a frame buffer (nouveau: linuxkpi/nouveau/kestrel.c) */
void __weak lkpi_drm_fb_where(struct drm_crtc *crtc, struct drm_framebuffer *fb, size_t last) { }

/* A frame buffer drawn in place must take what is written: write a pattern
 * to its first and last words and read it back, so a mapping that drops
 * writes shows in the log rather than as a blank monitor. */
static void kdrm_probe_vram(struct kdrm_head *h)
{
    struct drm_device *dev = h->kc->client.dev;
    size_t last = (size_t)h->pitch * h->height - 4;
    u32 a, b;

    iosys_map_wr(&h->map, 0, u32, 0x00a5c35a);
    iosys_map_wr(&h->map, last, u32, 0x005ac3a5);
    a = iosys_map_rd(&h->map, 0, u32);
    b = iosys_map_rd(&h->map, last, u32);
    lkpi_drm_fb_where(h->crtc, h->buffer->fb, last);
    iosys_map_wr(&h->map, 0, u32, 0);
    iosys_map_wr(&h->map, last, u32, 0);
    drm_info(dev, "kestrel: %dx%d frame buffer at %lx (GEM %zu KiB): read back %08x %08x%s\n", h->width,
             h->height, (unsigned long)h->map.vaddr_iomem, h->buffer->gem->size >> 10, a, b,
             a == 0x00a5c35a && b == 0x005ac3a5 ? "" : ", WRITES ARE LOST");
}

/* Light every CRTC the probe gave a mode, then hand each one to Kestrel. */
static int kdrm_setup(struct kdrm_client *kc)
{
    struct drm_client_dev *client = &kc->client;
    struct drm_device *dev = client->dev;
    char monitors[KDRM_MAX_HEADS][16];
    struct drm_mode_set *ms;
    int ret, n = 0;

    ret = drm_client_modeset_probe(client, 0, 0);
    if (ret) return ret;

    mutex_lock(&client->modeset_mutex);
    drm_client_for_each_modeset(ms, client) {
        if (!ms->mode || !ms->num_connectors) continue;
        if (n == KDRM_MAX_HEADS) break;
        struct drm_client_buffer *buffer =
            drm_client_framebuffer_create(client, ms->mode->hdisplay, ms->mode->vdisplay, DRM_FORMAT_XRGB8888);
        if (IS_ERR(buffer)) {
            drm_err(dev, "kestrel: no %dx%d frame buffer: %pe\n", ms->mode->hdisplay, ms->mode->vdisplay, buffer);
            continue;
        }
        ms->fb = buffer->fb;
        struct kdrm_head *h = &kc->heads[n];
        h->kc = kc;
        h->buffer = buffer;
        h->crtc = ms->crtc;
        h->width = ms->mode->hdisplay;
        h->height = ms->mode->vdisplay;
        h->pitch = buffer->fb->pitches[0];
        h->monitor = -1;
        spin_lock_init(&h->lock);
        h->damage_y0 = h->height;
        INIT_WORK(&h->flush_work, kdrm_flush_work);
        kdrm_monitor_name(ms, monitors[n], sizeof monitors[n]);
        drm_info(dev, "kestrel: %s on %s: %dx%d@%d\n", ms->connectors[0]->name, ms->crtc->name,
                 h->width, h->height, drm_mode_vrefresh(ms->mode));
        n++;
    }
    mutex_unlock(&client->modeset_mutex);
    if (!n) {
        drm_info(dev, "kestrel: no connected monitor\n");
        return 0;
    }

    ret = drm_client_modeset_commit(client);           /* pins the frame buffers for scan-out */
    if (ret) {
        drm_err(dev, "kestrel: mode set failed: %d\n", ret);
        goto err_delete;
    }
    kc->lit = true;

    for (int i = 0; i < n; i++) {
        struct kdrm_head *h = &kc->heads[i];
        ret = kdrm_vmap(h->buffer, &h->map);
        if (ret) {
            drm_err(dev, "kestrel: cannot map the frame buffer of %dx%d: %d\n", h->width, h->height, ret);
            continue;
        }
        kc->nheads = i + 1;

        char name[32];
        snprintf(name, sizeof name, "%s %s", dev->driver->name, dev_name(dev->dev));
        /* VRAM that scans out as it is written: Kestrel draws to it directly
         * (KPI I/O mappings are identity mappings, so the address is physical) */
        bool direct = h->map.is_iomem && !h->buffer->fb->funcs->dirty;
        struct lkpi_display d = {
            .phys = direct ? (unsigned long)h->map.vaddr_iomem : 0,
            .vram = direct ? (void __force *)h->map.vaddr_iomem : NULL,
            .width = h->width, .height = h->height, .pitch = h->pitch,
            .write_span = direct ? NULL : kdrm_write_span, .ctx = h,
            .name = name, .monitor = monitors[i],
        };
        if (!direct) iosys_map_memset(&h->map, 0, 0, (size_t)h->pitch * h->height);
        else kdrm_probe_vram(h);
        h->monitor = lkpi_display_add(&d);
        if (h->monitor < 0) drm_err(dev, "kestrel: Kestrel took no monitor for %s\n", name);
        else drm_info(dev, "kestrel: monitor %d (%s)\n", h->monitor, direct ? "drawn in VRAM" : "damage flushed");
    }
    return 0;

err_delete:
    mutex_lock(&client->modeset_mutex);
    drm_client_for_each_modeset(ms, client) ms->fb = NULL;
    mutex_unlock(&client->modeset_mutex);
    for (int i = 0; i < n; i++) drm_client_framebuffer_delete(kc->heads[i].buffer);
    return ret;
}

static int kdrm_hotplug(struct drm_client_dev *client)
{
    struct kdrm_client *kc = container_of(client, struct kdrm_client, client);

    if (kc->lit) return drm_client_modeset_commit(client);       /* keep what Kestrel draws on */
    return kdrm_setup(kc);
}

static int kdrm_restore(struct drm_client_dev *client)
{
    struct kdrm_client *kc = container_of(client, struct kdrm_client, client);
    return kc->lit ? drm_client_modeset_commit(client) : 0;
}

static void kdrm_unregister(struct drm_client_dev *client)
{
    struct kdrm_client *kc = container_of(client, struct kdrm_client, client);

    WRITE_ONCE(kc->gone, true);
    for (int i = 0; i < kc->nheads; i++) cancel_work_sync(&kc->heads[i].flush_work);
    /* Kestrel keeps the heads: those drawn in VRAM keep pointing at the BAR,
     * and write_span() still gets `kc`, so it is never freed. */
    drm_client_release(client);
}

static const struct drm_client_funcs kdrm_client_funcs = {
    .owner      = THIS_MODULE,
    .unregister = kdrm_unregister,
    .restore    = kdrm_restore,
    .hotplug    = kdrm_hotplug,
};

/* Called by drivers after drm_dev_register(). */
void drm_fbdev_ttm_setup(struct drm_device *dev, unsigned int preferred_bpp)
{
    struct kdrm_client *kc;
    int ret;

    drm_WARN(dev, !dev->registered, "Device has not been registered.\n");
    kc = kzalloc(sizeof *kc, GFP_KERNEL);
    if (!kc) return;
    ret = drm_client_init(dev, &kc->client, "kestrel", &kdrm_client_funcs);
    if (ret) {
        drm_err(dev, "kestrel: cannot start the display client: %d\n", ret);
        kfree(kc);
        return;
    }
    drm_client_register(&kc->client);               /* runs the first hotplug */
}
EXPORT_SYMBOL(drm_fbdev_ttm_setup);

/* drm_modeset_helper.c tells fbdev about suspend; there is no fbdev. */
void drm_fb_helper_set_suspend_unlocked(struct drm_fb_helper *fb_helper, bool suspend) { }
EXPORT_SYMBOL(drm_fb_helper_set_suspend_unlocked);
