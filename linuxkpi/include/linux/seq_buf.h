/* linux/seq_buf.h -- Kestrel's Linux KPI: a bounded string buffer */
#ifndef _LKPI_LINUX_SEQ_BUF_H
#define _LKPI_LINUX_SEQ_BUF_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/seq_file.h>

struct seq_buf {
    char *buffer;
    size_t size;
    size_t len;
};
#define DECLARE_SEQ_BUF(name, sz)                                               \
    struct seq_buf name = { .buffer = (char[sz]) { 0 }, .size = (sz), .len = 0 }
static inline void seq_buf_init(struct seq_buf *s, char *buf, unsigned int size)
{ s->buffer = buf; s->size = size; s->len = 0; if (size) buf[0] = 0; }
static inline bool seq_buf_has_overflowed(struct seq_buf *s) { return s->len > s->size; }
static inline void seq_buf_clear(struct seq_buf *s) { s->len = 0; if (s->size) s->buffer[0] = 0; }
static inline const char *seq_buf_str(struct seq_buf *s)
{
    if (s->size) s->buffer[s->len < s->size ? s->len : s->size - 1] = 0;
    return s->buffer;
}
__printf(2, 3) int seq_buf_printf(struct seq_buf *s, const char *fmt, ...);
int seq_buf_puts(struct seq_buf *s, const char *str);

#endif
