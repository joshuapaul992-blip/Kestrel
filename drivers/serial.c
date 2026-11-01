/* drivers/serial.c -- COM1 (0x3F8) 115200 8N1, polled output.
 * Used for the kernel log so you can watch boot with `-serial stdio`. */
#include <kernel/serial.h>
#include <kernel/cpu.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

#define COM1 0x3F8
static bool present;
static int  fifo_bytes = 1;         /* written at once when the transmitter is empty */

bool serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* disable UART interrupts      */
    outb(COM1 + 3, 0x80);   /* DLAB on                      */
    outb(COM1 + 0, 0x01);   /* divisor 1 -> 115200 baud     */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8 bits, no parity, 1 stop    */
    outb(COM1 + 2, 0xC7);   /* FIFO on, clear, 14-byte trig */
    if ((inb(COM1 + 2) & 0xC0) == 0xC0) fifo_bytes = 16;    /* IIR: a 16550A's FIFO is on */
    outb(COM1 + 4, 0x1E);   /* loopback mode for self-test  */
    outb(COM1 + 0, 0xAE);
    if (inb(COM1 + 0) != 0xAE) { present = false; return false; }
    outb(COM1 + 4, 0x0F);   /* normal mode, OUT1/OUT2/RTS/DTR */
    present = true;
    return true;
}

bool serial_present(void) { return present; }

/* Non-blocking receive: -1 when nothing is waiting (or no UART). */
int serial_getc(void)
{
    if (!present || !(inb(COM1 + 5) & 0x01)) return -1;     /* LSR.DR */
    return inb(COM1);
}

void serial_putc(char c)
{
    if (!present) return;
    if (c == '\n') serial_putc('\r');
    for (int spin = 0; spin < 100000 && !(inb(COM1 + 5) & 0x20); spin++)
        cpu_relax();
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) { while (*s) serial_putc(*s++); }

/* ---- the kernel log's mirror (serial.h) ----------------------------------- */
#define LOG_BUF (64 * 1024)
static char log_buf[LOG_BUF];
static uint64_t log_head, log_tail;     /* bytes ever queued, ever sent */
static uint64_t log_lost;               /* dropped since the last note of it */
static bool async;
static struct spinlock log_lock;

static void queue(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (log_head - log_tail >= LOG_BUF) { log_lost += n - i; return; }
        log_buf[log_head++ % LOG_BUF] = s[i];
    }
}

/* Feed the UART what it takes without waiting: a FIFO's worth once the
 * transmitter is empty, '\n' as "\r\n". Caller holds log_lock. */
static bool cr_sent;                    /* the '\r' of the '\n' at log_tail went out */

static void drain(void)
{
    /* a real UART is busy again after one FIFO's worth; an emulated one
     * that sends at once takes more, up to a bound */
    for (int sent = 0; log_tail != log_head && sent < 4096 && (inb(COM1 + 5) & 0x20); )   /* LSR.THRE */
        for (int i = 0; i < fifo_bytes && log_tail != log_head; i++, sent++) {
            char c = log_buf[log_tail % LOG_BUF];
            if (c == '\n' && !cr_sent) { outb(COM1, '\r'); cr_sent = true; continue; }
            cr_sent = false;
            log_tail++;
            outb(COM1, (uint8_t)c);
        }
}

void serial_log(const char *s)
{
    if (!present) return;
    if (!async) { serial_write(s); return; }
    uint64_t f = spin_lock_irqsave(&log_lock);
    if (log_lost && LOG_BUF - (log_head - log_tail) > 64) {
        char note[64];
        snprintf(note, sizeof note, "\n[serial: %lu bytes of the log dropped]\n", log_lost);
        log_lost = 0;
        queue(note, strlen(note));
    }
    queue(s, strlen(s));
    drain();
    spin_unlock_irqrestore(&log_lock, f);
}

void serial_log_async(void) { async = true; }

void serial_poll(void)
{
    if (!present || !async || __atomic_load_n(&log_lock.locked, __ATOMIC_RELAXED)) return;
    uint64_t f = spin_lock_irqsave(&log_lock);
    drain();
    spin_unlock_irqrestore(&log_lock, f);
}

void serial_flush(void)
{
    if (!present || !async) return;
    async = false;                                  /* from here on: the slow way */
    for (int spin = 0; log_tail != log_head && spin < 100000; spin++) {
        uint64_t before = log_tail;
        drain();
        if (log_tail != before || cr_sent) spin = 0;        /* a UART that stopped taking bytes: give up */
        else cpu_relax();
    }
}
