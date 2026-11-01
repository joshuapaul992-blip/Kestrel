/* linuxkpi/src/printk.c -- printk, dev_printk, BUG/WARN and hex dumps
 *
 * Linux messages carry their level as a "\001<digit>" prefix; it is
 * stripped here and the text goes to Kestrel's log. KERN_DEBUG messages
 * are dropped unless linuxkpi.debug=1. A message that does not end in a
 * newline is finished by the next one unless that is a KERN_CONT.
 *
 * Some messages come as fast as the hardware raises them (nouveau's GPU
 * traps: several lines for each, many a second), and logging them costs
 * the driver's interrupt handler its time. Each message (its format) may
 * log RL_BURST lines every RL_WINDOW_MS; the rest are counted, and the
 * count is logged when it may log again. Only errors and warnings are
 * limited: critical ones always go, and so does information (a driver
 * listing what it found). */
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/bug.h>
#include "internal.h"

const char hex_asc[] = "0123456789abcdef";
const char hex_asc_upper[] = "0123456789ABCDEF";

static bool line_open;              /* the last message did not end in '\n' */

#define RL_SITES     32
#define RL_BURST     10
#define RL_WINDOW_MS 5000
static struct { const void *site; unsigned long long t0; unsigned int n, dropped; } rl[RL_SITES];
static DEFINE_RAW_SPINLOCK(rl_lock);
static bool dropping;               /* the last line was dropped: so are its KERN_CONT parts */

/* May site log a line now? *dropped: lines it lost since it last could. */
static bool rl_allow(const void *site, unsigned int *dropped)
{
    unsigned long long now = lkpi_time_ms();
    unsigned long flags;
    bool ok = true;
    *dropped = 0;
    raw_spin_lock_irqsave(&rl_lock, flags);
    int i, oldest = 0;
    for (i = 0; i < RL_SITES && rl[i].site != site; i++)
        if (rl[i].t0 < rl[oldest].t0) oldest = i;
    if (i == RL_SITES) {                    /* new: take the slot used longest ago */
        i = oldest;
        rl[i].site = site;
        rl[i].t0 = now;
        rl[i].n = rl[i].dropped = 0;
    }
    if (now - rl[i].t0 >= RL_WINDOW_MS) {
        *dropped = rl[i].dropped;
        rl[i].t0 = now;
        rl[i].n = rl[i].dropped = 0;
    }
    if (rl[i].n >= RL_BURST) { rl[i].dropped++; ok = false; }
    else rl[i].n++;
    raw_spin_unlock_irqrestore(&rl_lock, flags);
    return ok;
}

/* site: what identifies the message for rate limiting (its format; NULL:
 * not limited, such as the lines of a hex dump) */
static int vprintk_site(const void *site, const char *fmt, va_list args)
{
    char buf[512];
    int level = 6;                  /* KERN_INFO when there is no prefix: Linux's default is 4, close enough */
    bool cont = false;

    while (fmt[0] == KERN_SOH_ASCII && fmt[1]) {
        if (fmt[1] == 'c') cont = true;
        else if (fmt[1] >= '0' && fmt[1] <= '7') level = fmt[1] - '0';
        fmt += 2;
    }
    if (level == 7 && !lkpi_debug_enabled()) return 0;
    if (cont && dropping) return 0;
    unsigned int dropped = 0;
    if (!cont) {
        dropping = site && (level == 3 || level == 4) && !rl_allow(site, &dropped);
        if (dropping) return 0;
    }
    if (dropped) {
        snprintf(buf, sizeof buf, "%slinuxkpi: %u more like the next line not logged\n", line_open ? "\n" : "", dropped);
        line_open = false;
        lkpi_log(buf);
    }

    int n = 0;
    if (line_open && !cont) buf[n++] = '\n';
    int len = vscnprintf(buf + n, sizeof buf - n, fmt, args);
    n += len;
    if (n == 0) return 0;
    line_open = buf[n - 1] != '\n';
    buf[n] = 0;
    lkpi_log(buf);
    return len;
}

int vprintk(const char *fmt, va_list args) { return vprintk_site(fmt, fmt, args); }

int printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintk(fmt, ap);
    va_end(ap);
    return r;
}

/* printk with a KERN_<LEVEL> given apart from the format (a string such as
 * dev_printk's level argument): vprintk reads the level from the format.
 * site: the caller's own format, which names the message. */
static __printf(3, 4) int level_printk(const void *site, const char *level, const char *fmt, ...)
{
    char f[64];
    size_t l = level ? strnlen(level, 4) : 0;
    va_list ap;
    if (l) memcpy(f, level, l);
    strscpy(f + l, fmt, sizeof f - l);
    va_start(ap, fmt);
    int r = vprintk_site(site, f, ap);
    va_end(ap);
    return r;
}

/* ---- messages about a device ------------------------------------------------ */
const char *dev_driver_string(const struct device *dev)
{
    struct device_driver *drv = READ_ONCE(dev->driver);
    if (drv && drv->name) return drv->name;
    if (dev->bus && dev->bus->name) return dev->bus->name;
    if (dev->class && dev->class->name) return dev->class->name;
    return "";
}

void _dev_printk(const char *level, const struct device *dev, const char *fmt, ...)
{
    struct va_format vaf;
    va_list ap;
    va_start(ap, fmt);
    vaf.fmt = fmt;
    vaf.va = &ap;
    if (dev) level_printk(fmt, level, "%s %s: %pV", dev_driver_string(dev), dev_name(dev), &vaf);
    else level_printk(fmt, level, "(NULL device *): %pV", &vaf);
    va_end(ap);
}

int dev_err_probe(const struct device *dev, int err, const char *fmt, ...)
{
    struct va_format vaf;
    va_list ap;
    va_start(ap, fmt);
    vaf.fmt = fmt;
    vaf.va = &ap;
    if (err != -EPROBE_DEFER) _dev_printk(KERN_ERR, dev, "error %pe: %pV", ERR_PTR(err), &vaf);
    else _dev_printk(KERN_DEBUG, dev, "deferred probe: %pV", &vaf);
    va_end(ap);
    return err;
}

/* ---- BUG, WARN, panic ---------------------------------------------------------- */
void lkpi_panic(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vscnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    lkpi_die(buf);
}

void lkpi_bug(const char *file, int line)
{
    lkpi_panic("kernel BUG at %s:%d", file, line);
}

void lkpi_warn(const char *file, int line)
{
    printk(KERN_WARNING "WARNING: at %s:%d (linuxkpi)\n", file, line);
}

void dump_stack(void)
{
    printk(KERN_INFO "dump_stack: no stack traces in the Linux KPI yet\n");
}

void __might_sleep(const char *file, int line)
{
    static int reported;
    if (!in_atomic() || reported >= 8) return;
    reported++;
    printk(KERN_ERR "BUG: sleeping function called from invalid context at %s:%d\n", file, line);
}

/* ---- hex ------------------------------------------------------------------------- */
int hex_to_bin(unsigned char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    ch = (unsigned char)tolower(ch);
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

int hex2bin(u8 *dst, const char *src, size_t count)
{
    while (count--) {
        int hi = hex_to_bin((unsigned char)*src++);
        int lo = hi < 0 ? -1 : hex_to_bin((unsigned char)*src++);
        if (hi < 0 || lo < 0) return -EINVAL;
        *dst++ = (u8)((hi << 4) | lo);
    }
    return 0;
}

char *bin2hex(char *dst, const void *src, size_t count)
{
    const unsigned char *s = src;
    while (count--) dst = hex_byte_pack(dst, *s++);
    return dst;
}

/* One row of a hex dump: groups of 1, 2, 4 or 8 bytes, then the ASCII. */
int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize, char *linebuf,
                       size_t linebuflen, bool ascii)
{
    const u8 *p = buf;
    size_t n = 0;
    if (rowsize != 16 && rowsize != 32) rowsize = 16;
    if (len > (size_t)rowsize) len = rowsize;
    if (groupsize != 1 && groupsize != 2 && groupsize != 4 && groupsize != 8) groupsize = 1;
    if (len % groupsize) groupsize = 1;
    if (!linebuflen) return 0;
    linebuf[0] = 0;
    for (size_t i = 0; i < len; i += groupsize) {
        u64 v = 0;
        for (int b = groupsize - 1; b >= 0; b--) v = (v << 8) | p[i + b];     /* little endian */
        n += scnprintf(linebuf + n, linebuflen - n, "%s%0*llx", i ? " " : "", groupsize * 2,
                       (unsigned long long)v);
    }
    if (ascii) {
        size_t groups = (size_t)rowsize / groupsize;
        size_t width = groups * (groupsize * 2 + 1);
        while (n < width && n + 1 < linebuflen) linebuf[n++] = ' ';
        for (size_t i = 0; i < len && n + 1 < linebuflen; i++)
            linebuf[n++] = isprint(p[i]) ? (char)p[i] : '.';
        if (n < linebuflen) linebuf[n] = 0;
    }
    return (int)n;
}

void print_hex_dump(const char *level, const char *prefix_str, int prefix_type, int rowsize, int groupsize,
                    const void *buf, size_t len, bool ascii)
{
    const u8 *p = buf;
    char line[200];
    if (rowsize != 16 && rowsize != 32) rowsize = 16;
    for (size_t i = 0; i < len; i += rowsize) {
        size_t n = len - i < (size_t)rowsize ? len - i : (size_t)rowsize;
        hex_dump_to_buffer(p + i, n, rowsize, groupsize, line, sizeof line, ascii);
        if (prefix_type == DUMP_PREFIX_ADDRESS) level_printk(NULL, level, "%s%p: %s\n", prefix_str, p + i, line);
        else if (prefix_type == DUMP_PREFIX_OFFSET) level_printk(NULL, level, "%s%.8zx: %s\n", prefix_str, i, line);
        else level_printk(NULL, level, "%s%s\n", prefix_str, line);
    }
}
