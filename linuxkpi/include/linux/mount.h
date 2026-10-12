/* linux/mount.h -- Kestrel's Linux KPI: mounts (DRM's inode file system: see linux/pseudo_fs.h) */
#ifndef _LKPI_LINUX_MOUNT_H
#define _LKPI_LINUX_MOUNT_H

struct super_block;
struct vfsmount { struct super_block *mnt_sb; };
struct file_system_type;
void kern_unmount(struct vfsmount *mnt);
int simple_pin_fs(struct file_system_type *type, struct vfsmount **mount, int *count);
void simple_release_fs(struct vfsmount **mount, int *count);

#endif
