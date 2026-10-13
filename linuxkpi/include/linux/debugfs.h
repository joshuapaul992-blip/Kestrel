/* linux/debugfs.h -- Kestrel's Linux KPI: debugfs (none: every call does nothing) */
#ifndef _LKPI_LINUX_DEBUGFS_H
#define _LKPI_LINUX_DEBUGFS_H

#include <linux/err.h>
#include <linux/fs.h>

struct dentry;
struct file_operations;
struct inode;
#define debugfs_create_dir(name, parent)                        ERR_PTR(-ENODEV)
#define debugfs_create_file(name, mode, parent, data, fops)     ({ (void)(data); (void)(fops); ERR_PTR(-ENODEV); })
#define debugfs_create_file_unsafe(name, mode, parent, data, fops) ({ (void)(data); (void)(fops); ERR_PTR(-ENODEV); })
#define debugfs_create_symlink(name, parent, target)            ERR_PTR(-ENODEV)
#define debugfs_lookup(name, parent)                            ((struct dentry *)NULL)
#define debugfs_create_atomic_t(name, mode, parent, value)      do { (void)(value); } while (0)
#define debugfs_create_bool(name, mode, parent, value)          do { (void)(value); } while (0)
#define debugfs_create_u32(name, mode, parent, value)           do { (void)(value); } while (0)
#define debugfs_create_u64(name, mode, parent, value)           do { (void)(value); } while (0)
#define debugfs_create_ulong(name, mode, parent, value)         do { (void)(value); } while (0)
static inline void debugfs_remove(struct dentry *d) { }
static inline void debugfs_remove_recursive(struct dentry *d) { }
static inline void debugfs_lookup_and_remove(const char *name, struct dentry *parent) { }
static inline bool debugfs_initialized(void) { return false; }
#define DEFINE_DEBUGFS_ATTRIBUTE(fops, get, set, fmt) \
    static const struct file_operations fops __maybe_unused = { }

struct debugfs_reg32 { char *name; unsigned long offset; };
struct debugfs_regset32 {
    const struct debugfs_reg32 *regs;
    int nregs;
    void __iomem *base;
    struct device *dev;
};

#endif
