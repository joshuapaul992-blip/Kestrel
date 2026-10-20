/* include/kernel/fuse.h -- FUSE: file systems served by a program (fs/fuse.c) */
#ifndef KESTREL_FUSE_H
#define KESTREL_FUSE_H

#include <stdint.h>

struct vnode_ops;

/* /dev/fuse (10, 229): each open is a new connection */
extern const struct vnode_ops fuse_ctl_ops;

/* mount(2) of type "fuse" or "fuse.NAME" (fstype) at target, with the
 * options in data ("fd=N,rootmode=40000,user_id=0,group_id=0"). The mount
 * is read-only; mflags adds MNT_NOSUID, MNT_NODEV and MNT_NOEXEC. */
int fuse_mount(const char *target, const char *source, const char *fstype, uint32_t mflags, const char *data);

#endif
