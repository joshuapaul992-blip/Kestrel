/* kernel/klog.c -- kernel message log, serial mirror and panic() */
#include <kernel/smp.h>
#include <kernel/klog.h>
#include <kernel/serial.h>
#include <kernel/console.h>
#include <kernel/fb.h>
#include <kernel/string.h>
#include <kernel/cpu.h>
#include <kernel/spinlock.h>
#include <kernel/tty.h>
#include <kernel/stopscreen.h>

static char   log_buf[KLOG_SIZE];
static size_t log_len;              /* total bytes ever written (wraps buffer) */
static bool   to_console;
static struct spinlock log_lock;

void klog_set_console(bool on) { to_console = on; }

void klog_putc(char c)
{
    static char line[192];
    static size_t len;
    if (c == '\n' || len == sizeof line - 1) {
        line[len] = 0;
        kprintf("%s\n", line);
        len = 0;
        if (c == '\n') return;
    }
    line[len++] = c;
}
bool klog_console_enabled(void) { return to_console; }

static void log_append(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        log_buf[(log_len + i) % KLOG_SIZE] = s[i];
    log_len += n;
}

size_t klog_size(void) { return log_len < KLOG_SIZE ? log_len : KLOG_SIZE; }

size_t klog_read(size_t off, char *buf, size_t len)
{
    size_t avail = klog_size();
    size_t start = log_len - avail;           /* oldest byte still present */
    if (off >= avail) return 0;
    if (len > avail - off) len = avail - off;
    for (size_t i = 0; i < len; i++) buf[i] = log_buf[(start + off + i) % KLOG_SIZE];
    return len;
}

void kprintf(const char *fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n >= (int)sizeof buf) n = sizeof buf - 1;

    /* CPUs log at once (calls without the big kernel lock, exit among
     * them); a panic does not wait for a CPU it has stopped. */
    bool lock = !smp_panicking;
    uint64_t f = lock ? spin_lock_irqsave(&log_lock) : irq_save();
    log_append(buf, (size_t)n);
    serial_write(buf);
    if (lock) spin_unlock_irqrestore(&log_lock, f);
    else irq_restore(f);
    if (!to_console || !g_fb.ready) return;
    if (tty_ready()) tty_write(tty_get(0), buf, (size_t)n);     /* system console: monitor 0 */
    else con_write(&g_con, buf, (size_t)n);
}

static void (*panic_hook)(const char *msg);

void panic_set_hook(void (*fn)(const char *msg)) { panic_hook = fn; }

void panic(const char *fmt, ...)
{
    cli();
    smp_stop_others();          /* NMI to the other CPUs: they halt where they are */
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    serial_write("\n*** KERNEL PANIC: ");
    serial_write(buf);
    serial_write("\n");

    void (*hook)(const char *) = panic_hook;
    panic_hook = NULL;          /* once: a panic inside the hook goes straight on */
    if (hook) hook(buf);

    stop_screen(buf);           /* red, 800x600 (kernel/stopscreen.c) */
    cpu_halt_forever();
}
