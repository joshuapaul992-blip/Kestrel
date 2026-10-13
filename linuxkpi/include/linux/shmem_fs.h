/* linux/shmem_fs.h -- Kestrel's Linux KPI: memory-backed files (GEM objects backed by them: not available yet) */
#ifndef _LKPI_LINUX_SHMEM_FS_H
#define _LKPI_LINUX_SHMEM_FS_H

#include <linux/fs.h>
#include <linux/mm.h>

struct file *shmem_file_setup(const char *name, loff_t size, unsigned long flags);
struct page *shmem_read_mapping_page_gfp(struct address_space *mapping, pgoff_t index, gfp_t gfp);
struct folio *shmem_read_folio_gfp(struct address_space *mapping, pgoff_t index, gfp_t gfp);
#define shmem_file(f)                   false

#endif
