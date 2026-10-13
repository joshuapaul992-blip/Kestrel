/* linux/pseudo_fs.h -- Kestrel's Linux KPI: in-kernel file systems with no files
 *
 * DRM makes an anonymous inode for each device from one; the KPI hands out
 * plain inodes instead (linuxkpi/src/drm.c). */
#ifndef _LKPI_LINUX_PSEUDO_FS_H
#define _LKPI_LINUX_PSEUDO_FS_H

#include <linux/fs.h>

struct fs_context;
struct file_system_type {
    const char *name;
    int (*init_fs_context)(struct fs_context *);
    void (*kill_sb)(struct super_block *);
    struct module *owner;
};
struct pseudo_fs_context { unsigned long magic; };
static inline struct pseudo_fs_context *init_pseudo(struct fs_context *fc, unsigned long magic) { return (void *)fc; }
static inline void kill_anon_super(struct super_block *sb) { }
struct inode *alloc_anon_inode(struct super_block *sb);
void iput(struct inode *inode);

#endif
