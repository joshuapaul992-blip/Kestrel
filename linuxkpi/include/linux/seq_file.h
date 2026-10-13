/* linux/seq_file.h -- Kestrel's Linux KPI: seq_file output (only debugfs uses it; nothing is shown) */
#ifndef _LKPI_LINUX_SEQ_FILE_H
#define _LKPI_LINUX_SEQ_FILE_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/fs.h>

struct seq_file {
    char *buf;
    size_t size;
    size_t count;
    void *private;
};
__printf(2, 3) void seq_printf(struct seq_file *m, const char *fmt, ...);
void seq_puts(struct seq_file *m, const char *s);
void seq_putc(struct seq_file *m, char c);
void seq_write(struct seq_file *m, const void *data, size_t len);
#define DEFINE_SHOW_ATTRIBUTE(name) \
    static const struct file_operations name##_fops __maybe_unused = { }

#endif
