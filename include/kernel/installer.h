/* include/kernel/installer.h -- /dev/install: copy the boot stick onto a disk (alpha) */
#ifndef KESTREL_INSTALLER_H
#define KESTREL_INSTALLER_H

void installer_init(void);              /* after rootfs_mount(): registers /dev/install */

#endif
