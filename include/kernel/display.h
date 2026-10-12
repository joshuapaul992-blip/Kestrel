/* include/kernel/display.h -- registry of physical display heads (monitors)
 *
 * Every display driver registers one head per connected monitor: the boot
 * frame buffer (VBE/GOP, set up by GRUB) is head 0, and drivers that can set
 * modes themselves (drivers/video/bochs_dispi.c today, a native GPU driver
 * later) add further heads at their monitors' native resolutions. The TTY
 * layer (tty.h) gives each head its own terminal; nothing above this layer
 * assumes a global screen size.
 *
 * Heads use 8-bit colour channels in 32-bpp pixels (every GOP, GPU and
 * DISPI scan-out format) or, on some VBE firmware, packed 24-bpp pixels. */
#ifndef KESTREL_DISPLAY_H
#define KESTREL_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_MONITORS    4

struct display_head {
    uint64_t  phys;                 /* frame buffer (VRAM aperture) physical address */
    uint32_t *vram;                 /* ... mapped                                    */
    uint32_t *shadow;               /* optional RAM copy with the same layout:
                                       drawing and scrolling read it instead of
                                       slow uncached VRAM; NULL = draw to VRAM    */
    int       width, height;        /* native resolution, pixels                     */
    int       pitch;                /* bytes per scan line (>= width * bytes_pp)     */
    int       bytes_pp;             /* 4, or 3 for packed 24-bpp VBE modes           */
    uint8_t   r_pos, g_pos, b_pos;  /* bit position of each 8-bit channel            */
    /* VRAM the CPU cannot map (vram == NULL, e.g. NVIDIA through the PRAMIN
     * window): the terminal draws into `shadow` and publishes changed spans
     * with write_span(write_ctx, byte offset into the frame buffer, ...). */
    void    (*write_span)(void *ctx, uint32_t offset, const void *src, uint32_t len);
    void     *write_ctx;
    char      name[32];             /* "VBE frame buffer", "bochs-display 00:04.0"   */
    char      monitor[16];          /* EDID monitor name, or "" if unknown           */
};

/* Register a head; returns its index (monitor number) or -1 when full or the
 * format is unsupported. The registry keeps its own copy. */
int  display_register(const struct display_head *head);
int  display_count(void);
const struct display_head *display_get(int index);

/* The name of the head an adapter drives (its frame buffer is in one of the
 * device's memory BARs), or NULL: the Linux KPI leaves such adapters alone. */
struct pci_device;
const char *display_pci_owner(const struct pci_device *dev);

/* linuxkpi.drm=1: a Linux driver takes over the adapter behind the boot
 * frame buffer. display_handover() stops all writes to its VRAM (returns
 * the monitor given up, or -1 if the boot frame buffer is not on `dev`);
 * the driver's first monitor then takes that monitor's place and terminal
 * through display_replace() (returns its number, or -1 if nothing waits). */
int display_handover(const struct pci_device *dev);
int display_replace(const struct display_head *head);
/* The driver failed after display_handover(): the boot frame buffer is
 * monitor `index` again. Returns it, or -1 if `dev` was not handed over. */
int display_reclaim(const struct pci_device *dev);
bool display_handover_pending(void);     /* a monitor waits for display_replace() */

/* Boot: head 0 from the boot frame buffer (g_fb), then every secondary
 * display adapter a driver knows how to program. Call after pci_init(). */
void display_probe(void);

#endif
