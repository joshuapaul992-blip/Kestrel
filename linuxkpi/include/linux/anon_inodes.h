/* linux/anon_inodes.h -- Kestrel's Linux KPI: anonymous files (DRM sync objects, sync files, dma-bufs) */
#ifndef _LKPI_LINUX_ANON_INODES_H
#define _LKPI_LINUX_ANON_INODES_H

#include <linux/types.h>

struct file;
struct file_operations;
struct inode;
struct file *anon_inode_getfile(const char *name, const struct file_operations *fops, void *priv, int flags);
int anon_inode_getfd(const char *name, const struct file_operations *fops, void *priv, int flags);

#endif
