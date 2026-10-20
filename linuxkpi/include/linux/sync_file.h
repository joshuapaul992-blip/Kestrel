/* linux/sync_file.h -- Kestrel's Linux KPI: sync files, a fence as a file
 * descriptor for user space (linuxkpi/src/sync_file.c) */
#ifndef _LKPI_LINUX_SYNC_FILE_H
#define _LKPI_LINUX_SYNC_FILE_H

#include <linux/types.h>
#include <linux/fs.h>

struct dma_fence;

struct sync_file {
    struct file         *file;
    char                 user_name[32];     /* SYNC_IOC_MERGE's name */
    struct dma_fence    *fence;
};

/* A file holding a reference to the fence (f_count 1: fd_install it, or fput). */
struct sync_file *sync_file_create(struct dma_fence *fence);
/* The fence of the sync file behind fd, with a reference, or NULL. */
struct dma_fence *sync_file_get_fence(int fd);
char *sync_file_get_name(struct sync_file *sync_file, char *buf, int len);

#endif
