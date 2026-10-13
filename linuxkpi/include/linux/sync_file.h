/* linux/sync_file.h -- Kestrel's Linux KPI: sync files (fence file descriptors for user space; not available yet) */
#ifndef _LKPI_LINUX_SYNC_FILE_H
#define _LKPI_LINUX_SYNC_FILE_H

#include <linux/types.h>
#include <linux/fs.h>

struct dma_fence;
struct sync_file { struct file *file; struct dma_fence *fence; };
static inline struct sync_file *sync_file_create(struct dma_fence *fence) { return NULL; }
static inline struct dma_fence *sync_file_get_fence(int fd) { return NULL; }

#endif
