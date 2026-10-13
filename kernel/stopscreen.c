/* kernel/stopscreen.c -- the kernel panic screen: white text on red, in the
 * style of the Windows XP to 7 stop screen, laid out for 800x600.
 *
 * The text is set in the Kestrel 10 font (11 x 15 cells) into an 800x600
 * canvas, word wrapped to 71 columns, with the panic message on the
 * "*** STOP:" line, which always stays one line: a longer message is cut
 * short with "...". Then the screen itself goes to 800x600 where that can
 * be done safely with every other CPU stopped:
 *
 *   - the boot frame buffer of a BIOS boot, through the video BIOS (VBE
 *     4F02), run in an x86 emulator so that the CPU never leaves long mode
 *     (vbe_emu.c), as long as no driver has taken the card over since;
 *   - anything else (UEFI GOP, a Linux display driver), where a mode switch
 *     would need firmware that is gone or locks a panic cannot take, gets
 *     the 800x600 canvas scaled to its whole screen, as a monitor's scaler
 *     shows an 800x600 picture.
 *
 * kestrel.panicmode=0 on the command line skips the BIOS call. */
#include <kernel/stopscreen.h>
#include <kernel/fb.h>
#include <kernel/kestrel_font.h>
#include <kernel/bootinfo.h>
#include <kernel/string.h>
#include <kernel/klog.h>
#include <kernel/serial.h>
#include <kernel/display.h>
#include <kernel/vbe_emu.h>

#define STOP_W      800
#define STOP_H      600
#define STOP_BG     RGB(0xAA, 0x00, 0x00)
#define STOP_FG     RGB(0xFF, 0xFF, 0xFF)
#define MARGIN_X    8
#define MARGIN_Y    8

static const char *const stop_text[] = {
    "A critical error has occurred.",
    "Kestrel has been shut down to prevent further damage to your computer.",
    "",
    "If this is the first time you have seen this kernel panic screen, restart your computer.",
    "If this screen appears again, follow these steps:",
    "",
    "Check to make sure any new hardware or software is properly installed. If this is a new "
    "installation, use a working computer and go to https://github.com/joshuapaul992-blip/Kestrel "
    "and open an issue for any problems you have encountered.",
    "",
    "If problems continue, disable or remove any newly installed hardware or software. Disable BIOS "
    "memory options such as caching or shadowing. If you need to use Safe Mode to remove or disable "
    "components, restart your computer, press F8 to select Advanced Startup Options, and then select "
    "Recovery Mode.",
    "",
    "Technical information:",
    "",
    NULL,                                       /* "*** STOP: " + the panic message */
    "",
    "The system has been halted. Please restart your computer.",
};
#define N_LINES ((int)(sizeof stop_text / sizeof *stop_text))

static uint8_t canvas[STOP_W * STOP_H];         /* 1 = text */

/* ---- layout ---------------------------------------------------------------- */
static void put_char(int col, int row, unsigned char c)
{
    const struct kfont *f = &kfont_10;
    const uint16_t *g = kfont_glyph(f, c);
    int x0 = MARGIN_X + col * f->width, y0 = MARGIN_Y + row * f->height;
    for (int y = 0; y < f->height; y++)
        for (int x = 0; x < f->width; x++)
            if (g[y] & (0x8000u >> x)) canvas[(y0 + y) * STOP_W + x0 + x] = 1;
}

/* One paragraph from `row`, broken at spaces into lines of `cols` (a word
 * longer than a line is cut). Returns the row after it. */
static int paragraph(int row, int rows, int cols, const char *s)
{
    if (!*s) return row + 1;
    while (*s && row < rows) {
        int n = (int)strlen(s);
        if (n > cols) {
            n = cols;
            while (n > 0 && s[n] != ' ') n--;
            if (n == 0) n = cols;
        }
        for (int i = 0; i < n; i++) put_char(i, row, (unsigned char)s[i]);
        row++;
        s += n;
        while (*s == ' ') s++;
    }
    return row;
}

static void render(const char *msg)
{
    const int cols = (STOP_W - 2 * MARGIN_X) / kfont_10.width;
    const int rows = (STOP_H - 2 * MARGIN_Y) / kfont_10.height;
    char stop[128];
    snprintf(stop, sizeof stop, "*** STOP: %s", msg);
    for (char *p = stop; *p; p++) if (*p == '\n' || *p == '\t') *p = ' ';
    if ((int)strlen(stop) > cols) memcpy(stop + cols - 3, "...", 4);  /* one line */
    memset(canvas, 0, sizeof canvas);
    int row = 0;
    for (int i = 0; i < N_LINES && row < rows; i++)
        row = paragraph(row, rows, cols, stop_text[i] ? stop_text[i] : stop);
}

/* ---- 800x600 through the video BIOS ------------------------------------------ */
static bool bios_800x600(void)
{
    /* only the card as the BIOS left it: not once a driver has taken it
     * over, nor while one is doing so (it may have reprogrammed it already) */
    if (g_boot.uefi || g_fb.driver[0] || display_handover_pending() ||
        strstr(g_boot.cmdline, "kestrel.panicmode=0")) return false;
    serial_write("panic: switching the boot frame buffer to 800x600 through the video BIOS (emulated)\n");
    struct vbe_mode m;
    if (!vbe_emu_set_mode(STOP_W, STOP_H, &m)) return false;
    g_fb.double_buffered = false;
    g_fb.phys = m.phys;
    g_fb.front = g_fb.back = (uint8_t *)(uintptr_t)m.phys;     /* below 4 GiB: identity mapped */
    g_fb.width = m.width; g_fb.height = m.height; g_fb.pitch = m.pitch;
    g_fb.bpp = m.bpp; g_fb.bytes_pp = m.bpp / 8;
    g_fb.r_pos = m.r_pos; g_fb.r_size = m.r_size;
    g_fb.g_pos = m.g_pos; g_fb.g_size = m.g_size;
    g_fb.b_pos = m.b_pos; g_fb.b_size = m.b_size;
    return true;
}

/* ---- the screen ------------------------------------------------------------------ */
void stop_screen(const char *msg)
{
    render(msg);
    if (!g_fb.ready) return;
    bios_800x600();
    fb_reset_clip();

    /* nearest pixel of the canvas for each screen pixel: 1:1 at 800x600 */
    int w = (int)g_fb.width, h = (int)g_fb.height;
    for (int y = 0; y < h; y++) {
        const uint8_t *src = &canvas[(y * STOP_H / h) * STOP_W];
        for (int x = 0; x < w; x++) put_pixel(x, y, src[x * STOP_W / w] ? STOP_FG : STOP_BG);
    }
    fb_flush();
}
