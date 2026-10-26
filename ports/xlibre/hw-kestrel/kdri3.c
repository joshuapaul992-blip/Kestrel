/*
 * hw/kestrel/kdri3.c -- DRI3 for Xkestrel: windows shown from GPU buffers
 *
 * A client that draws with a GPU (Mesa's Vulkan drivers, NVK among them)
 * hands the server each finished image as a dma-buf descriptor
 * (DRI3PixmapFromBuffers) and asks for it to be shown with
 * PresentPixmap; Mesa's Vulkan drivers for a GPU present no other way
 * on X11. The screen here is drawn by fb/ in ordinary memory, so an
 * imported buffer becomes a pixmap whose pixels are the buffer itself,
 * mapped into the server: only plain linear 32-bit images, the one layout
 * this offers (DRI3GetSupportedModifiers answers DRM_FORMAT_MOD_LINEAR).
 * Before fb reads such a pixmap (SourceValidate, ahead of every copy and
 * composite from it) the server waits for the GPU work that writes it:
 * DMA_BUF_IOCTL_SYNC waits for the fences attached to the buffer, which
 * is where Mesa leaves its rendering's fence at present time.
 *
 * DRI3Open gives the client the first render node (/dev/dri/renderD128
 * and on). With none, the screen has no DRI3, as before. The idle fences
 * Present signals are shared-memory futexes (miSyncShm, libxshmfence).
 *
 * Kestrel's own code: MIT licence.
 */
#include <dix-config.h>

#ifdef DRI3
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "scrnintstr.h"
#include "pixmapstr.h"
#include "privates.h"
#include "dixstruct.h"
#include "dri3.h"
#include "misyncshm.h"
#include "dix/screen_hooks_priv.h"
#include "kdri3.h"

#define K_FOURCC(a, b, c, d)    ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define K_FORMAT_XRGB8888       K_FOURCC('X', 'R', '2', '4')
#define K_FORMAT_ARGB8888       K_FOURCC('A', 'R', '2', '4')
#define K_MOD_LINEAR            0ull
#define K_MOD_INVALID           0x00ffffffffffffffull

struct k_dma_buf_sync { uint64_t flags; };
#define K_DMA_BUF_SYNC_READ     (1 << 0)
#define K_DMA_BUF_SYNC_START    (0 << 2)
#define K_DMA_BUF_SYNC_END      (1 << 2)
#define K_DMA_BUF_IOCTL_SYNC    _IOW('b', 0, struct k_dma_buf_sync)

/* an imported buffer: the descriptor, and the mapping the pixmap shows */
typedef struct {
    int      fd;
    void    *map;
    size_t   len;
    uint32_t stride, offset;
} KDri3Buffer;

static DevPrivateKeyRec kdri3_pixmap_key;
static SourceValidateProcPtr kdri3_source_validate;
static char kdri3_node[32];

static KDri3Buffer *
kdri3_buffer(PixmapPtr pixmap)
{
    return dixLookupPrivate(&pixmap->devPrivates, &kdri3_pixmap_key);
}

static int
kdri3_open_client(ClientPtr client, ScreenPtr screen, RRProviderPtr provider, int *fd)
{
    int f = open(kdri3_node, O_RDWR | O_CLOEXEC);
    if (f < 0)
        return BadAlloc;
    *fd = f;
    return Success;
}

static PixmapPtr
kdri3_pixmap_from_fds(ScreenPtr screen, CARD8 num_fds, const int *fds,
                      CARD16 width, CARD16 height, const CARD32 *strides,
                      const CARD32 *offsets, CARD8 depth, CARD8 bpp, CARD64 modifier)
{
    if (num_fds != 1 || bpp != 32 || (depth != 24 && depth != 32))
        return NULL;
    if (modifier != K_MOD_LINEAR && modifier != K_MOD_INVALID)
        return NULL;
    if (!width || !height || strides[0] % 4 || strides[0] < (CARD32)width * 4)
        return NULL;

    off_t size = lseek(fds[0], 0, SEEK_END);
    uint64_t need = (uint64_t)offsets[0] + (uint64_t)strides[0] * height;
    if (size <= 0 || need > (uint64_t)size)
        return NULL;
    void *map = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fds[0], 0);
    if (map == MAP_FAILED) {
        ErrorF("kestrel: DRI3: cannot map a %dx%d buffer: %s\n", width, height, strerror(errno));
        return NULL;
    }
    KDri3Buffer *b = calloc(1, sizeof(*b));
    int fd = b ? dup(fds[0]) : -1;       /* the request closes its own descriptors */
    PixmapPtr pixmap = fd >= 0 ? screen->CreatePixmap(screen, 0, 0, depth, 0) : NULL;
    if (!pixmap) {
        if (fd >= 0)
            close(fd);
        free(b);
        munmap(map, (size_t)size);
        return NULL;
    }
    screen->ModifyPixmapHeader(pixmap, width, height, depth, bpp, strides[0],
                               (char *)map + offsets[0]);
    b->fd = fd;
    b->map = map;
    b->len = (size_t)size;
    b->stride = strides[0];
    b->offset = offsets[0];
    dixSetPrivate(&pixmap->devPrivates, &kdri3_pixmap_key, b);
    return pixmap;
}

/* Only the buffers clients gave us go back out: the server's own pixmaps
 * live in plain memory, with no descriptor to give. */
static int
kdri3_fds_from_pixmap(ScreenPtr screen, PixmapPtr pixmap, int *fds,
                      uint32_t *strides, uint32_t *offsets, uint64_t *modifier)
{
    KDri3Buffer *b = kdri3_buffer(pixmap);
    if (!b || (fds[0] = dup(b->fd)) < 0)
        return 0;
    strides[0] = b->stride;
    offsets[0] = b->offset;
    *modifier = K_MOD_LINEAR;
    return 1;
}

static int
kdri3_fd_from_pixmap(ScreenPtr screen, PixmapPtr pixmap, CARD16 *stride, CARD32 *size)
{
    KDri3Buffer *b = kdri3_buffer(pixmap);
    if (!b || b->offset || b->stride > 0xffff)
        return -1;
    *stride = (CARD16)b->stride;
    *size = (CARD32)b->len;
    return dup(b->fd);
}

static int
kdri3_get_formats(ScreenPtr screen, CARD32 *num_formats, CARD32 **formats)
{
    CARD32 *f = calloc(2, sizeof(*f));
    if (!f)
        return FALSE;
    f[0] = K_FORMAT_XRGB8888;
    f[1] = K_FORMAT_ARGB8888;
    *num_formats = 2;
    *formats = f;
    return TRUE;
}

static int
kdri3_get_modifiers(ScreenPtr screen, uint32_t format, uint32_t *num_modifiers, uint64_t **modifiers)
{
    uint64_t *m = calloc(1, sizeof(*m));
    if (!m)
        return FALSE;
    m[0] = K_MOD_LINEAR;
    *num_modifiers = 1;
    *modifiers = m;
    return TRUE;
}

static int
kdri3_get_drawable_modifiers(DrawablePtr draw, uint32_t format, uint32_t *num_modifiers,
                             uint64_t **modifiers)
{
    *num_modifiers = 0;
    *modifiers = NULL;
    return TRUE;
}

static const dri3_screen_info_rec kdri3_info = {
    .version = 2,
    .open_client = kdri3_open_client,
    .pixmap_from_fds = kdri3_pixmap_from_fds,
    .fds_from_pixmap = kdri3_fds_from_pixmap,
    .fd_from_pixmap = kdri3_fd_from_pixmap,
    .get_formats = kdri3_get_formats,
    .get_modifiers = kdri3_get_modifiers,
    .get_drawable_modifiers = kdri3_get_drawable_modifiers,
};

/* fb is about to read the drawable: let the GPU finish writing it first */
static void
kdri3_source_validate_wrap(DrawablePtr draw, int x, int y, int w, int h, unsigned int mode)
{
    if (draw->type == DRAWABLE_PIXMAP) {
        KDri3Buffer *b = kdri3_buffer((PixmapPtr)draw);
        if (b) {
            struct k_dma_buf_sync s = { K_DMA_BUF_SYNC_START | K_DMA_BUF_SYNC_READ };
            while (ioctl(b->fd, K_DMA_BUF_IOCTL_SYNC, &s) < 0 && errno == EINTR)
                ;
            s.flags = K_DMA_BUF_SYNC_END | K_DMA_BUF_SYNC_READ;
            ioctl(b->fd, K_DMA_BUF_IOCTL_SYNC, &s);
        }
    }
    if (kdri3_source_validate)
        kdri3_source_validate(draw, x, y, w, h, mode);
}

static void
kdri3_pixmap_destroy(CallbackListPtr *pcbl, ScreenPtr screen, PixmapPtr pixmap)
{
    KDri3Buffer *b = kdri3_buffer(pixmap);
    if (!b)
        return;
    dixSetPrivate(&pixmap->devPrivates, &kdri3_pixmap_key, NULL);
    munmap(b->map, b->len);
    close(b->fd);
    free(b);
}

Bool
kDri3Init(ScreenPtr screen)
{
    for (int i = 128; i < 136; i++) {
        snprintf(kdri3_node, sizeof(kdri3_node), "/dev/dri/renderD%d", i);
        int fd = open(kdri3_node, O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
            goto found;
        }
    }
    LogMessage(X_INFO, "kestrel: no render node, no DRI3\n");
    return TRUE;

found:
    if (!dixRegisterPrivateKey(&kdri3_pixmap_key, PRIVATE_PIXMAP, 0))
        return FALSE;
    if (!miSyncShmScreenInit(screen))
        return FALSE;
    if (!dri3_screen_init(screen, &kdri3_info))
        return FALSE;
    dixScreenHookPixmapDestroy(screen, kdri3_pixmap_destroy);
    kdri3_source_validate = screen->SourceValidate;
    screen->SourceValidate = kdri3_source_validate_wrap;
    LogMessage(X_INFO, "kestrel: DRI3 on %s (linear buffers)\n", kdri3_node);
    return TRUE;
}
#endif
