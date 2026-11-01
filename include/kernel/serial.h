/* include/kernel/serial.h -- 16550 UART (COM1) debug port */
#ifndef KESTREL_SERIAL_H
#define KESTREL_SERIAL_H

#include <stdbool.h>

bool serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
bool serial_present(void);
int  serial_getc(void);           /* non-blocking, -1 if no byte waiting */

/* The kernel log's mirror. Until serial_log_async(), each message waits
 * for the UART like serial_write(). After, a message goes into a buffer
 * and serial_poll() (the boot CPU's timer tick) feeds the UART's FIFO
 * from it: at 115200 baud a line takes ~7 ms, which must not be spent
 * with interrupts off in kprintf(). A full buffer drops text (the kernel
 * log keeps it all) and says so. serial_flush() empties the buffer the
 * slow way (panic). */
void serial_log(const char *s);
void serial_log_async(void);
void serial_poll(void);
void serial_flush(void);

#endif
