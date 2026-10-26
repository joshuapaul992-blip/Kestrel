/*
 * hw/kestrel/kestrel.c -- XLibre display server backend (DDX) for Kestrel
 *
 * One X screen for all of Kestrel's monitors (/dev/fb0, /dev/fb1, ...), with
 * RandR 1.2+: each monitor is an output driven by its own CRTC, which shows
 * a rectangle of the screen. xrandr places, mirrors, rotates, reflects,
 * switches off and (where the kernel driver can) changes the mode of each
 * monitor, and grows or shrinks the screen to match. The screen starts with
 * the monitors side by side, left to right in /dev/fbN order, tops aligned,
 * and the first as the primary one.
 *
 * The screen is rendered in software by fb/ into an ordinary memory buffer;
 * damage tracking collects what changed, and before the server goes idle
 * (block handler) each changed box is handed, per monitor showing it, to the
 * kernel with KFB_BLIT, which draws it whatever the monitor's driver (boot
 * frame buffer, Bochs DISPI, NVIDIA). Rotated and reflected monitors get the
 * box turned in a scratch buffer first. KDSETMODE(KD_GRAPHICS) stops the
 * kernel terminal on each monitor while the server runs; it comes back by
 * itself when the server exits.
 *
 * Input comes from /dev/kinput: Linux-style key and relative-pointer events
 * (struct kinput_event), keycodes in the evdev numbering, so the default
 * "evdev" XKB rules apply unchanged.
 *
 * Based on hw/vfb (Copyright 1993, 1998 The Open Group; MIT/X Consortium
 * licence, see COPYING). Kestrel parts: MIT licence.
 */
#include <dix-config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/Xos.h>

#include "dix/colormap_priv.h"
#include "dix/dix_priv.h"
#include "dix/input_priv.h"
#include "dix/inpututils_priv.h"
#include "dix/screenint_priv.h"
#include "include/extinit.h"
#include "mi/mi_priv.h"
#include "mi/mipointer_priv.h"
#include "miext/extinit_priv.h"
#include "os/cmdline.h"
#include "os/ddx_priv.h"
#include "os/osdep.h"

#include "scrnintstr.h"
#include "servermd.h"
#define PSZ 8
#include "fb.h"
#include "gcstruct.h"
#include "input.h"
#include "inputstr.h"
#include "mipointer.h"
#include "micmap.h"
#include "damage.h"
#include "dixstruct.h"
#include "randrstr.h"
#include "xkbsrv.h"
#include "xserver-properties.h"
#include "exevents.h"
#include "globals.h"
#ifdef DRI3
#include "kdri3.h"
#endif

/* ---- kernel interfaces (Kestrel include/kernel/kfb.h, kinput.h) ---------- */
#define KFB_GET_INFO    0x4b00
#define KFB_BLIT        0x4b01
#define KFB_GET_MODES   0x4b02
#define KFB_SET_MODE    0x4b03
#define KDSETMODE       0x4b3a
#define KD_TEXT         0x00
#define KD_GRAPHICS     0x01

struct kfb_info {
    uint32_t width, height, pitch, bpp;
    uint8_t  r_pos, g_pos, b_pos, index;
    char     name[32];
    char     monitor[16];
};

struct kfb_blit {
    int32_t  x, y, w, h;
    uint64_t src;
    uint32_t src_pitch, reserved;
};

#define KFB_MAX_MODES       24
#define KFB_MODE_PREFERRED  1
#define KFB_MODE_CURRENT    2
struct kfb_mode {
    uint32_t width, height;
    uint32_t refresh_mhz;       /* 0 = unknown */
    uint32_t flags;
};
struct kfb_modes {
    uint32_t count;
    uint32_t width_mm, height_mm;   /* 0 = unknown */
    uint32_t reserved;
    struct kfb_mode mode[KFB_MAX_MODES];
};

struct kinput_event {
    uint64_t time_us;
    uint16_t type, code;
    int32_t  value;
};
#define KINPUT_GRAB     0x4b10
#define KI_EV_KEY       1
#define KI_EV_REL       2
#define KI_REL_X        0
#define KI_REL_Y        1
#define KI_REL_WHEEL    8
#define KI_BTN_LEFT     0x110
#define KI_BTN_RIGHT    0x111
#define KI_BTN_MIDDLE   0x112

/* ---- monitors ------------------------------------------------------------- */
#define KMAX_MONITORS   4
#define KMAX_SIZE       8192    /* largest screen xrandr may ask for */
#define KROTATIONS      (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270 | \
                         RR_Reflect_X | RR_Reflect_Y)

typedef struct {
    int         fd;
    struct kfb_info info;
    struct kfb_modes modes;     /* what the kernel can switch this monitor to */
    char        name[24];       /* RandR output name: the connector, else "fbN" */
    RRCrtcPtr   crtc;
    RROutputPtr output;
    Bool        on;             /* showing part of the screen */
    int         mw, mh;         /* its current mode */
    struct kfb_mode start;      /* the mode it had before the server, restored at exit */
    int         x, y, w, h;     /* the screen rectangle it shows (w, h swapped when turned) */
    Rotation    rotation;
    uint64_t    blits, pixels;
} KMonitor;

static KMonitor kmon[KMAX_MONITORS];
static int knum;
static int kwant = KMAX_MONITORS;       /* -screens N */
static Bool knodri3;                    /* -nodri3 */

static struct {
    uint8_t    *bits;           /* the screen pixmap, 32 bpp */
    int         stride;         /* bytes */
    DamagePtr   damage;
    CloseScreenProcPtr closeScreen;
    CreateScreenResourcesProcPtr createScreenResources;
    ScreenBlockHandlerProcPtr blockHandler;
    uint32_t   *turn;           /* scratch for rotated and reflected monitors */
    size_t      turnsize;       /* pixels */
} kscr;

/* "GP106 head 1 DP-2" -> "DP-2": the connector, when the driver names one. */
static void
kOutputName(KMonitor *m, int i)
{
    const char *sp = strrchr(m->info.name, ' ');
    const char *c = sp ? sp + 1 : m->info.name;
    const char *dash = strchr(c, '-');

    if (dash && dash > c && isupper((unsigned char)c[0]) && isdigit((unsigned char)dash[1]))
        snprintf(m->name, sizeof(m->name), "%s", c);
    else
        snprintf(m->name, sizeof(m->name), "fb%d", i);
    for (int j = 0; j < knum; j++)          /* names must be unique */
        if (&kmon[j] != m && strcmp(kmon[j].name, m->name) == 0)
            snprintf(m->name, sizeof(m->name), "fb%d", i);
}

static void
kOpenMonitors(void)
{
    if (knum)
        return;
    for (int i = 0; i < KMAX_MONITORS && knum < kwant; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/fb%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0)
            break;
        KMonitor *m = &kmon[knum];
        if (ioctl(fd, KFB_GET_INFO, &m->info) < 0 || m->info.bpp != 32) {
            ErrorF("kestrel: %s: unusable (%s)\n", path, strerror(errno));
            close(fd);
            continue;
        }
        m->fd = fd;
        /* a kernel without mode lists: the current mode is the only one */
        if (ioctl(fd, KFB_GET_MODES, &m->modes) < 0 || !m->modes.count ||
            m->modes.count > KFB_MAX_MODES) {
            memset(&m->modes, 0, sizeof(m->modes));
            m->modes.count = 1;
            m->modes.mode[0] = (struct kfb_mode) { m->info.width, m->info.height, 0,
                                                   KFB_MODE_PREFERRED | KFB_MODE_CURRENT };
        }
        m->mw = (int)m->info.width;
        m->mh = (int)m->info.height;
        m->start = (struct kfb_mode) { m->info.width, m->info.height, 0, 0 };
        for (uint32_t j = 0; j < m->modes.count; j++)
            if (m->modes.mode[j].flags & KFB_MODE_CURRENT)
                m->start = m->modes.mode[j];
        kOutputName(m, i);
        LogMessage(X_INFO, "kestrel: %s (%s): %ux%u, %u mode(s), R/G/B at bits %u/%u/%u, %s%s%s\n",
                   path, m->name, m->info.width, m->info.height, m->modes.count,
                   m->info.r_pos, m->info.g_pos, m->info.b_pos,
                   m->info.name, m->info.monitor[0] ? ", monitor " : "", m->info.monitor);
        knum++;
    }
    if (!knum)
        FatalError("kestrel: no monitor (/dev/fb0) available\n");
}

/* Where a monitor pixel comes from: screen pixel (u, v) relative to the
 * monitor's rectangle. RandR's rotation first, then its reflection, as in
 * RRTransformCompute(). */
static void
kToScreen(const KMonitor *m, int mx, int my, int *u, int *v)
{
    switch (m->rotation & 0xf) {
    default:            *u = mx;             *v = my;             break;
    case RR_Rotate_90:  *u = m->mh - 1 - my; *v = mx;             break;
    case RR_Rotate_180: *u = m->mw - 1 - mx; *v = m->mh - 1 - my; break;
    case RR_Rotate_270: *u = my;             *v = m->mw - 1 - mx; break;
    }
    if (m->rotation & RR_Reflect_X) *u = m->w - 1 - *u;
    if (m->rotation & RR_Reflect_Y) *v = m->h - 1 - *v;
}

static void
kToMonitor(const KMonitor *m, int u, int v, int *mx, int *my)
{
    if (m->rotation & RR_Reflect_X) u = m->w - 1 - u;
    if (m->rotation & RR_Reflect_Y) v = m->h - 1 - v;
    switch (m->rotation & 0xf) {
    default:            *mx = u;             *my = v;             break;
    case RR_Rotate_90:  *mx = v;             *my = m->mh - 1 - u; break;
    case RR_Rotate_180: *mx = m->mw - 1 - u; *my = m->mh - 1 - v; break;
    case RR_Rotate_270: *mx = m->mw - 1 - v; *my = u;             break;
    }
}

static void
kBlit(KMonitor *m, int x, int y, int w, int h, const void *src, int pitch)
{
    struct kfb_blit bl = {
        .x = x, .y = y, .w = w, .h = h,
        .src = (uint64_t)(uintptr_t)src, .src_pitch = (uint32_t)pitch,
    };
    if (ioctl(m->fd, KFB_BLIT, &bl) < 0)
        ErrorF("kestrel: %s: blit %dx%d+%d+%d failed: %s\n", m->name, w, h, x, y, strerror(errno));
    m->blits++;
    m->pixels += (uint64_t)w * h;
}

/* Show screen box b (inside the monitor's rectangle) on the monitor. */
static void
kShow(KMonitor *m, const BoxRec *b)
{
    if (m->rotation == RR_Rotate_0) {
        kBlit(m, b->x1 - m->x, b->y1 - m->y, b->x2 - b->x1, b->y2 - b->y1,
              kscr.bits + (size_t)b->y1 * kscr.stride + (size_t)b->x1 * 4, kscr.stride);
        return;
    }
    int ax, ay, bx, by;
    kToMonitor(m, b->x1 - m->x, b->y1 - m->y, &ax, &ay);
    kToMonitor(m, b->x2 - 1 - m->x, b->y2 - 1 - m->y, &bx, &by);
    int mx0 = min(ax, bx), my0 = min(ay, by);
    int w = max(ax, bx) - mx0 + 1, h = max(ay, by) - my0 + 1;
    if ((size_t)w * h > kscr.turnsize) {
        uint32_t *t = realloc(kscr.turn, (size_t)w * h * 4);
        if (!t)
            return;
        kscr.turn = t;
        kscr.turnsize = (size_t)w * h;
    }
    /* the source address moves by a fixed step per monitor pixel */
    for (int y = 0; y < h; y++) {
        int u0, v0, u1, v1;
        kToScreen(m, mx0, my0 + y, &u0, &v0);
        kToScreen(m, mx0 + 1, my0 + y, &u1, &v1);
        const uint32_t *s = (const uint32_t *)(kscr.bits + (size_t)(m->y + v0) * kscr.stride) + m->x + u0;
        ptrdiff_t step = (ptrdiff_t)(v1 - v0) * (kscr.stride / 4) + (u1 - u0);
        uint32_t *d = kscr.turn + (size_t)y * w;
        for (int x = 0; x < w; x++, s += step)
            d[x] = *s;
    }
    kBlit(m, mx0, my0, w, h, kscr.turn, w * 4);
}

/* A monitor no CRTC drives any more goes black. */
static void
kBlank(KMonitor *m)
{
    void *zero = calloc((size_t)m->mw * 4, m->mh);
    if (zero) {
        kBlit(m, 0, 0, m->mw, m->mh, zero, m->mw * 4);
        free(zero);
    }
}

/* Hand every changed box to each monitor that shows it, then forget the damage. */
static void
kFlush(ScreenPtr pScreen)
{
    RegionPtr r = DamageRegion(kscr.damage);
    int n = RegionNumRects(r);
    BoxPtr b = RegionRects(r);

    for (int i = 0; i < n; i++, b++) {
        for (int j = 0; j < knum; j++) {
            KMonitor *m = &kmon[j];
            if (!m->on)
                continue;
            BoxRec c = {
                max(b->x1, max(m->x, 0)), max(b->y1, max(m->y, 0)),
                min(b->x2, min(m->x + m->w, pScreen->width)),
                min(b->y2, min(m->y + m->h, pScreen->height)),
            };
            if (c.x1 < c.x2 && c.y1 < c.y2)
                kShow(m, &c);
        }
    }
    DamageEmpty(kscr.damage);
}

/* Everything again, on every monitor: after the layout changed. */
static void
kDamageAll(ScreenPtr pScreen)
{
    if (!kscr.damage)
        return;
    RegionRec all;
    BoxRec box = { 0, 0, (short)pScreen->width, (short)pScreen->height };
    RegionInit(&all, &box, 1);
    DamageReportDamage(kscr.damage, &all);
    RegionUninit(&all);
}

/* Push damage to the display just before the server sleeps. This wraps the
 * screen's BlockHandler rather than using RegisterBlockAndWakeupHandlers():
 * dix runs those first, while the software cursor (misprite) puts the pointer
 * back on screen in its own screen BlockHandler, which then calls down to
 * this one -- so flushing here includes the cursor. */
static void
kBlockHandler(ScreenPtr pScreen, void *timeout)
{
    pScreen->BlockHandler = kscr.blockHandler;
    if (pScreen->BlockHandler)
        pScreen->BlockHandler(pScreen, timeout);
    kscr.blockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = kBlockHandler;

    if (kscr.damage)
        kFlush(pScreen);
}

static Bool
kCreateScreenResources(ScreenPtr pScreen)
{
    pScreen->CreateScreenResources = kscr.createScreenResources;
    if (!pScreen->CreateScreenResources(pScreen))
        return FALSE;
    PixmapPtr root = pScreen->GetScreenPixmap(pScreen);
    kscr.damage = DamageCreate(NULL, NULL, DamageReportNone, TRUE, pScreen, root);
    if (!kscr.damage)
        return FALSE;
    DamageRegister(&root->drawable, kscr.damage);
    kDamageAll(pScreen);        /* the whole screen once: the server starts with its root weave */
    return TRUE;
}

static Bool
kCloseScreen(ScreenPtr pScreen)
{
    pScreen->CloseScreen = kscr.closeScreen;
    pScreen->BlockHandler = kscr.blockHandler;
    if (kscr.damage) {
        DamageUnregister(kscr.damage);
        DamageDestroy(kscr.damage);
        kscr.damage = NULL;
    }
    dixDestroyPixmap(pScreen->devPrivate, 0);
    pScreen->devPrivate = NULL;
    for (int i = 0; i < knum; i++) {
        LogMessage(X_INFO, "kestrel: %s: %llu blits, %llu pixels sent\n", kmon[i].name,
                   (unsigned long long)kmon[i].blits, (unsigned long long)kmon[i].pixels);
        kmon[i].crtc = NULL;
        kmon[i].output = NULL;
    }
    Bool ret = pScreen->CloseScreen(pScreen);
    free(kscr.bits);
    kscr.bits = NULL;
    free(kscr.turn);
    kscr.turn = NULL;
    kscr.turnsize = 0;
    return ret;
}

/* One screen: the pointer never leaves it (RandR keeps it on the monitors). */
static Bool
kCursorOffScreen(ScreenPtr *ppScreen, int *x, int *y)
{
    return FALSE;
}

static void
kCrossScreen(ScreenPtr pScreen, Bool entering)
{
}

static miPointerScreenFuncRec kPointerCursorFuncs = {
    kCursorOffScreen,
    kCrossScreen,
    miPointerWarpCursor
};

/* ---- RandR ------------------------------------------------------------------ */
static int
kMmFromPixels(int px, uint32_t mm)
{
    return mm ? (int)mm : (int)(px * 25.4 / monitorResolution + 0.5);
}

static RRModePtr
kModeGet(const struct kfb_mode *km)
{
    xRRModeInfo info = { 0 };
    char name[32];
    uint32_t hz = km->refresh_mhz ? km->refresh_mhz : 60000;

    snprintf(name, sizeof(name), "%ux%u", km->width, km->height);
    info.width = km->width;
    info.height = km->height;
    /* no real timings: totals equal to the visible size, a clock that
     * gives the refresh rate (what xrandr prints) */
    info.hSyncStart = info.hSyncEnd = info.hTotal = km->width;
    info.vSyncStart = info.vSyncEnd = info.vTotal = km->height;
    info.dotClock = (CARD32)((uint64_t)km->width * km->height * hz / 1000);
    info.nameLength = strlen(name);
    return RRModeGet(&info, name);
}

static Bool
kRRGetInfo(ScreenPtr pScreen, Rotation *rotations)
{
    *rotations = KROTATIONS;
    return TRUE;
}

static const struct kfb_mode *
kFindMode(KMonitor *m, RRModePtr mode)
{
    const struct kfb_mode *best = NULL;
    for (uint32_t i = 0; i < m->modes.count; i++) {
        const struct kfb_mode *km = &m->modes.mode[i];
        if (km->width != mode->mode.width || km->height != mode->mode.height)
            continue;
        if (!best)
            best = km;
        /* the refresh rate xrandr asked for, when there is a choice */
        if (mode->mode.hTotal && mode->mode.vTotal) {
            uint64_t want = (uint64_t)mode->mode.dotClock * 1000 / ((uint64_t)mode->mode.hTotal * mode->mode.vTotal);
            uint32_t hz = km->refresh_mhz ? km->refresh_mhz : 60000;
            uint32_t bhz = best->refresh_mhz ? best->refresh_mhz : 60000;
            if (llabs((long long)hz - (long long)want) < llabs((long long)bhz - (long long)want))
                best = km;
        }
    }
    return best;
}

static Bool
kRROutputValidateMode(ScreenPtr pScreen, RROutputPtr output, RRModePtr mode)
{
    return kFindMode(output->devPrivate, mode) != NULL;
}

static Bool
kRRCrtcSet(ScreenPtr pScreen, RRCrtcPtr crtc, RRModePtr mode, int x, int y,
           Rotation rotation, int numOutputs, RROutputPtr *outputs)
{
    KMonitor *m = crtc->devPrivate;

    if (mode && numOutputs) {
        const struct kfb_mode *km = kFindMode(m, mode);
        if (!km)
            return FALSE;
        if ((int)km->width != m->mw || (int)km->height != m->mh ||
            !(km->flags & KFB_MODE_CURRENT)) {
            struct kfb_mode want = *km;
            if (ioctl(m->fd, KFB_SET_MODE, &want) < 0) {
                ErrorF("kestrel: %s: cannot switch to %ux%u: %s\n", m->name, km->width, km->height,
                       strerror(errno));
                return FALSE;
            }
            for (uint32_t i = 0; i < m->modes.count; i++)
                m->modes.mode[i].flags &= ~KFB_MODE_CURRENT;
            ((struct kfb_mode *)km)->flags |= KFB_MODE_CURRENT;
            m->mw = (int)km->width;
            m->mh = (int)km->height;
            LogMessage(X_INFO, "kestrel: %s: now %dx%d\n", m->name, m->mw, m->mh);
        }
        m->on = TRUE;
        m->x = x;
        m->y = y;
        m->rotation = rotation;
        Bool turned = (rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0;
        m->w = turned ? m->mh : m->mw;
        m->h = turned ? m->mw : m->mh;
    }
    else if (m->on) {
        m->on = FALSE;
        kBlank(m);
    }
    kDamageAll(pScreen);
    return RRCrtcNotify(crtc, m->on ? mode : NULL, x, y, rotation, NULL, numOutputs, outputs);
}

/* A new screen size: a new buffer, the old picture copied into its corner. */
static Bool
kRRScreenSetSize(ScreenPtr pScreen, CARD16 width, CARD16 height, CARD32 mmWidth, CARD32 mmHeight)
{
    int stride = (int)PixmapBytePad(width, 24);
    uint8_t *bits = calloc((size_t)stride, height);
    PixmapPtr pix = pScreen->GetScreenPixmap(pScreen);

    if (!bits)
        return FALSE;
    for (int y = 0; y < min((int)height, pScreen->height); y++)
        memcpy(bits + (size_t)y * stride, kscr.bits + (size_t)y * kscr.stride,
               (size_t)min((int)width, pScreen->width) * 4);

    SetRootClip(pScreen, ROOT_CLIP_NONE);       /* no drawing while things move */
    pScreen->ModifyPixmapHeader(pix, width, height, -1, -1, stride, bits);
    free(kscr.bits);
    kscr.bits = bits;
    kscr.stride = stride;
    pScreen->width = width;
    pScreen->height = height;
    pScreen->mmWidth = (int)mmWidth;
    pScreen->mmHeight = (int)mmHeight;
    SetRootClip(pScreen, ROOT_CLIP_FULL);
    kDamageAll(pScreen);

    RRScreenSizeNotify(pScreen);
    RRTellChanged(pScreen);
    LogMessage(X_INFO, "kestrel: screen now %ux%u\n", width, height);
    return TRUE;
}

static Bool
kRandRInit(ScreenPtr pScreen)
{
    if (!RRScreenInit(pScreen))
        return FALSE;
    rrScrPrivPtr pScrPriv = rrGetScrPriv(pScreen);
    pScrPriv->rrGetInfo = kRRGetInfo;
    pScrPriv->rrCrtcSet = kRRCrtcSet;
    pScrPriv->rrScreenSetSize = kRRScreenSetSize;
    pScrPriv->rrOutputSetProperty = NULL;
#if RANDR_13_INTERFACE
    pScrPriv->rrOutputGetProperty = NULL;
#endif
    pScrPriv->rrOutputValidateMode = kRROutputValidateMode;
    pScrPriv->rrModeDestroy = NULL;
    RRScreenSetSizeRange(pScreen, 320, 200, KMAX_SIZE, KMAX_SIZE);

    for (int i = 0; i < knum; i++) {
        KMonitor *m = &kmon[i];
        RRModePtr modes[KFB_MAX_MODES], current = NULL;
        int n = 0, npreferred = 0;

        m->crtc = RRCrtcCreate(pScreen, m);
        if (!m->crtc)
            return FALSE;
        RRCrtcSetRotations(m->crtc, KROTATIONS);
        RRCrtcGammaSetSize(m->crtc, 256);       /* xrandr complains without one */
        m->output = RROutputCreate(pScreen, m->name, strlen(m->name), m);
        if (!m->output)
            return FALSE;
        if (!RROutputSetClones(m->output, NULL, 0) ||
            !RROutputSetCrtcs(m->output, &m->crtc, 1) ||
            !RROutputSetConnection(m->output, RR_Connected) ||
            !RROutputSetPhysicalSize(m->output, kMmFromPixels(m->mw, m->modes.width_mm),
                                     kMmFromPixels(m->mh, m->modes.height_mm)))
            return FALSE;
        RROutputSetSubpixelOrder(m->output, SubPixelUnknown);

        /* preferred modes first, as RandR expects */
        for (int pass = 0; pass < 2; pass++)
            for (uint32_t j = 0; j < m->modes.count; j++) {
                const struct kfb_mode *km = &m->modes.mode[j];
                if ((km->flags & KFB_MODE_PREFERRED) ? pass : !pass)
                    continue;
                RRModePtr mode = kModeGet(km);
                if (!mode)
                    return FALSE;
                modes[n++] = mode;
                npreferred += !pass;
                if ((km->flags & KFB_MODE_CURRENT) && !current)
                    current = mode;
            }
        if (!RROutputSetModes(m->output, modes, n, npreferred))
            return FALSE;
        if (!current)
            current = modes[0];
        if (!RRCrtcNotify(m->crtc, current, m->x, m->y, RR_Rotate_0, NULL, 1, &m->output))
            return FALSE;
    }
    pScrPriv->primaryOutput = kmon[0].output;
    return TRUE;
}

static bool
kScreenInit(ScreenPtr pScreen, int argc, char **argv, void *closure)
{
    int dpi = monitorResolution ? monitorResolution : 96;
    int width = 0, height = 0;

    /* monitors side by side, left to right in /dev/fbN order, tops aligned */
    for (int i = 0; i < knum; i++) {
        KMonitor *m = &kmon[i];
        m->on = TRUE;
        m->rotation = RR_Rotate_0;
        m->x = width;
        m->y = 0;
        m->w = m->mw;
        m->h = m->mh;
        width += m->mw;
        height = max(height, m->mh);
    }
    kscr.stride = (int)PixmapBytePad(width, 24);
    kscr.bits = calloc((size_t)kscr.stride, height);
    if (!kscr.bits)
        return FALSE;

    const struct kfb_info *fmt = &kmon[0].info;
    if (!miSetVisualTypesAndMasks(24, (1 << TrueColor) | (1 << DirectColor), 8, TrueColor,
                                  0xffu << fmt->r_pos, 0xffu << fmt->g_pos, 0xffu << fmt->b_pos))
        return FALSE;
    miSetPixmapDepths();

    if (!fbScreenInit(pScreen, kscr.bits, width, height, dpi, dpi, kscr.stride / 4, 32))
        return FALSE;
    if (!noRenderExtension)
        fbPictureInit(pScreen, 0, 0);

    miDCInitialize(pScreen, &kPointerCursorFuncs);
    pScreen->blackPixel = 0;
    pScreen->whitePixel = (0xffu << fmt->r_pos) | (0xffu << fmt->g_pos) | (0xffu << fmt->b_pos);

    kscr.createScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = kCreateScreenResources;
    kscr.closeScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = kCloseScreen;
    kscr.blockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = kBlockHandler;

    if (!fbCreateDefColormap(pScreen))
        return FALSE;
    if (!kRandRInit(pScreen))
        return FALSE;
#ifdef DRI3
    if (!knodri3 && !kDri3Init(pScreen))
        return FALSE;
#endif

    for (int i = 0; i < knum; i++)
        if (ioctl(kmon[i].fd, KDSETMODE, KD_GRAPHICS) < 0)
            ErrorF("kestrel: KD_GRAPHICS on %s: %s\n", kmon[i].name, strerror(errno));
    return TRUE;
}

/* ---- DDX entry points ---------------------------------------------------------- */
void
InitOutput(int argc, char **argv)
{
    static const int depths[] = { 1, 4, 8, 15, 16, 24, 32 };
    int n = 0;

    kOpenMonitors();
    if (!monitorResolution)
        monitorResolution = 96;

    for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
        screenInfo.formats[n].depth = depths[i];
        screenInfo.formats[n].bitsPerPixel = depths[i] == 1 ? 1 : depths[i] <= 8 ? 8 :
                                             depths[i] <= 16 ? 16 : 32;
        screenInfo.formats[n].scanlinePad = BITMAP_SCANLINE_PAD;
        n++;
    }
    screenInfo.imageByteOrder = IMAGE_BYTE_ORDER;
    screenInfo.bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    screenInfo.bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    screenInfo.bitmapBitOrder = BITMAP_BIT_ORDER;
    screenInfo.numPixmapFormats = n;

    if (AddScreen(kScreenInit, argc, argv, NULL) == -1)
        FatalError("kestrel: couldn't add the screen\n");
}

void
ddxGiveUp(enum ExitCode error)
{
    for (int i = 0; i < knum; i++) {
        if (kmon[i].fd > 0) {
            KMonitor *m = &kmon[i];
            /* the terminal gets the mode back that it had */
            for (uint32_t j = 0; j < m->modes.count; j++) {
                const struct kfb_mode *now = &m->modes.mode[j];
                if ((now->flags & KFB_MODE_CURRENT) &&
                    (now->width != m->start.width || now->height != m->start.height ||
                     now->refresh_mhz != m->start.refresh_mhz))
                    ioctl(m->fd, KFB_SET_MODE, &m->start);
            }
            ioctl(kmon[i].fd, KDSETMODE, KD_TEXT);
            close(kmon[i].fd);
            kmon[i].fd = -1;
        }
    }
}

void
ddxInit(void)
{
}

void
ddxFatalError(const char *f, va_list args)
{
}

void
ddxUseMsg(void)
{
    ErrorF("-screens N             use at most N monitors (default: all /dev/fbN)\n");
    ErrorF("-nodri3                no DRI3, even with a render node\n");
}

int
ddxProcessArgument(int argc, char *argv[], int i)
{
    if (strcmp(argv[i], "-screens") == 0) {
        CHECK_FOR_REQUIRED_ARGUMENTS(1);
        kwant = atoi(argv[i + 1]);
        if (kwant < 1 || kwant > KMAX_MONITORS)
            UseMsg();
        return 2;
    }
    if (strcmp(argv[i], "-nodri3") == 0) {
        knodri3 = TRUE;
        return 1;
    }
    return 0;
}

void
ddxBeforeReset(void)
{
}

#if INPUTTHREAD
void
ddxInputThreadInit(void)
{
}
#endif

/* ---- input --------------------------------------------------------------------- */
static DeviceIntPtr kKeyboard, kPointer;
static int kInputFd = -1;

void
ProcessInputEvents(void)
{
    mieqProcessInputEvents();
}

void
DDXRingBell(int volume, int pitch, int duration)
{
}

static void
kReadInput(int fd, int ready, void *data)
{
    struct kinput_event ev[64];
    ssize_t n;

    while ((n = read(fd, ev, sizeof(ev))) > 0) {
        int dx = 0, dy = 0;
        for (size_t i = 0; i < (size_t)n / sizeof(ev[0]); i++) {
            const struct kinput_event *e = &ev[i];
            if (e->type == KI_EV_KEY && e->code < 0x100 && kKeyboard) {
                if (e->value == 2)          /* autorepeat: X repeats by itself */
                    continue;
                QueueKeyboardEvents(kKeyboard, e->value ? KeyPress : KeyRelease, e->code + 8);
            }
            else if (e->type == KI_EV_KEY && kPointer) {
                int b = e->code == KI_BTN_LEFT ? 1 : e->code == KI_BTN_MIDDLE ? 2 :
                        e->code == KI_BTN_RIGHT ? 3 : 0;
                if (b) {
                    ValuatorMask mask;
                    valuator_mask_zero(&mask);
                    QueuePointerEvents(kPointer, e->value ? ButtonPress : ButtonRelease, b, 0, &mask);
                }
            }
            else if (e->type == KI_EV_REL && kPointer) {
                if (e->code == KI_REL_X) dx += e->value;
                else if (e->code == KI_REL_Y) dy += e->value;
                else if (e->code == KI_REL_WHEEL && e->value) {
                    ValuatorMask mask;
                    int b = e->value > 0 ? 4 : 5;
                    valuator_mask_zero(&mask);
                    QueuePointerEvents(kPointer, ButtonPress, b, 0, &mask);
                    QueuePointerEvents(kPointer, ButtonRelease, b, 0, &mask);
                }
            }
        }
        if (dx || dy) {
            ValuatorMask mask;
            valuator_mask_zero(&mask);
            valuator_mask_set(&mask, 0, dx);
            valuator_mask_set(&mask, 1, dy);
            QueuePointerEvents(kPointer, MotionNotify, 0, POINTER_RELATIVE | POINTER_ACCELERATE, &mask);
        }
    }
}

static int
kKeybdProc(DeviceIntPtr pDevice, int onoff)
{
    DevicePtr pDev = (DevicePtr) pDevice;

    switch (onoff) {
    case DEVICE_INIT:
        InitKeyboardDeviceStruct(pDevice, NULL, NULL, NULL);
        break;
    case DEVICE_ON:
        pDev->on = TRUE;
        break;
    case DEVICE_OFF:
        pDev->on = FALSE;
        break;
    case DEVICE_CLOSE:
        break;
    }
    return Success;
}

static int
kMouseProc(DeviceIntPtr pDevice, int onoff)
{
#define NBUTTONS 7
#define NAXES 2
    BYTE map[NBUTTONS + 1];
    DevicePtr pDev = (DevicePtr) pDevice;
    Atom btn_labels[NBUTTONS] = { 0 };
    Atom axes_labels[NAXES] = { 0 };

    switch (onoff) {
    case DEVICE_INIT:
        for (int i = 1; i <= NBUTTONS; i++)
            map[i] = i;
        btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
        btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
        btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
        btn_labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
        btn_labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
        btn_labels[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
        btn_labels[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
        axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
        axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);
        InitPointerDeviceStruct(pDev, map, NBUTTONS, btn_labels, (PtrCtrlProcPtr) NoopDDA,
                                GetMotionHistorySize(), NAXES, axes_labels);
        break;
    case DEVICE_ON:
        pDev->on = TRUE;
        break;
    case DEVICE_OFF:
        pDev->on = FALSE;
        break;
    case DEVICE_CLOSE:
        break;
    }
    return Success;
#undef NBUTTONS
#undef NAXES
}

void
InitInput(int argc, char *argv[])
{
    kPointer = AddInputDevice(serverClient, kMouseProc, TRUE);
    kKeyboard = AddInputDevice(serverClient, kKeybdProc, TRUE);
    AssignTypeAndName(kPointer, dixAddAtom(XI_MOUSE), "Kestrel pointer");
    AssignTypeAndName(kKeyboard, dixAddAtom(XI_KEYBOARD), "Kestrel keyboard");
    (void) mieqInit();

    kInputFd = open("/dev/kinput", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (kInputFd < 0) {
        LogMessage(X_WARNING, "kestrel: /dev/kinput: %s (no keyboard or mouse)\n", strerror(errno));
        return;
    }
    /* Take keyboard and mouse away from the text consoles while we run. */
    int on = 1;
    if (ioctl(kInputFd, KINPUT_GRAB, &on) < 0)
        LogMessage(X_WARNING, "kestrel: cannot grab /dev/kinput: %s\n", strerror(errno));
    SetNotifyFd(kInputFd, kReadInput, X_NOTIFY_READ, NULL);
}

void
CloseInput(void)
{
    if (kInputFd >= 0) {
        RemoveNotifyFd(kInputFd);
        close(kInputFd);
        kInputFd = -1;
    }
    mieqFini();
}
