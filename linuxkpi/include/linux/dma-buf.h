/* linux/dma-buf.h -- Kestrel's Linux KPI: buffers shared between drivers (PRIME; not available yet) */
#ifndef _LKPI_LINUX_DMA_BUF_H
#define _LKPI_LINUX_DMA_BUF_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/dma-direction.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dma-fence.h>
#include <linux/scatterlist.h>

struct dma_buf;
struct dma_buf_attachment;
struct sg_table;
struct iosys_map;
struct dma_resv;

struct dma_buf_ops {
    int  (*attach)(struct dma_buf *, struct dma_buf_attachment *);
    void (*detach)(struct dma_buf *, struct dma_buf_attachment *);
    void (*release)(struct dma_buf *);
    int  (*vmap)(struct dma_buf *dmabuf, struct iosys_map *map);
    void (*vunmap)(struct dma_buf *dmabuf, struct iosys_map *map);
};
struct dma_buf {
    size_t size;
    struct file *file;
    const struct dma_buf_ops *ops;
    void *priv;
    struct dma_resv *resv;
};
struct dma_buf_attachment {
    struct dma_buf *dmabuf;
    struct device *dev;
    void *priv;
    void *importer_priv;
};
struct dma_buf_export_info {
    const char *exp_name;
    struct module *owner;
    const struct dma_buf_ops *ops;
    size_t size;
    int flags;
    struct dma_resv *resv;
    void *priv;
};
#define DEFINE_DMA_BUF_EXPORT_INFO(name) struct dma_buf_export_info name = { .exp_name = KBUILD_MODNAME, .owner = THIS_MODULE }
static inline void dma_buf_put(struct dma_buf *dmabuf) { }
static inline int dma_buf_begin_cpu_access(struct dma_buf *dmabuf, enum dma_data_direction dir) { return 0; }
static inline int dma_buf_end_cpu_access(struct dma_buf *dmabuf, enum dma_data_direction dir) { return 0; }
static inline int dma_buf_vmap(struct dma_buf *dmabuf, struct iosys_map *map) { return -EOPNOTSUPP; }
static inline void dma_buf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map) { }

#endif
