/* linux/file.h -- Kestrel's Linux KPI: file descriptors of the calling process (dma-bufs, sync files, DRM sync objects) */
#ifndef _LKPI_LINUX_FILE_H
#define _LKPI_LINUX_FILE_H

#include <linux/types.h>
#include <linux/fs.h>

struct file;
struct fd { struct file *file; unsigned int flags; };
void fput(struct file *file);
struct file *fget(unsigned int fd);
struct file *get_file(struct file *f);              /* one more reference */
int  get_unused_fd_flags(unsigned flags);
void put_unused_fd(unsigned int fd);
void fd_install(unsigned int fd, struct file *file);
static inline struct fd fdget(unsigned int fd) { return (struct fd){ fget(fd), 0 }; }
static inline void fdput(struct fd fd) { if (fd.file) fput(fd.file); }
#define fd_file(f)                      ((f).file)

#endif
