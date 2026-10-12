/* drm/drm_writeback.h -- Kestrel's Linux KPI: writeback connectors
 *
 * Connectors that write the composed picture back to memory. No driver the
 * KPI runs has one, so none is ever created; the types exist because the
 * atomic core checks for them (linuxkpi/src/drm.c). */
#ifndef _LKPI_DRM_DRM_WRITEBACK_H
#define _LKPI_DRM_DRM_WRITEBACK_H

#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <linux/workqueue.h>

struct drm_writeback_connector {
    struct drm_connector base;
    struct drm_encoder encoder;
    struct drm_property_blob *pixel_formats_blob_ptr;
    spinlock_t job_lock;
    struct list_head job_queue;
    unsigned int fence_context;
    spinlock_t fence_lock;
    unsigned long fence_seqno;
    char timeline_name[32];
};

struct drm_writeback_job {
    struct drm_writeback_connector *connector;
    bool prepared;
    struct work_struct cleanup_work;
    struct list_head list_entry;
    struct drm_framebuffer *fb;
    struct dma_fence *out_fence;
    void *priv;
};

static inline struct drm_writeback_connector *drm_connector_to_writeback(struct drm_connector *connector)
{
    return container_of(connector, struct drm_writeback_connector, base);
}

int  drm_writeback_set_fb(struct drm_connector_state *conn_state, struct drm_framebuffer *fb);
int  drm_writeback_prepare_job(struct drm_writeback_job *job);
void drm_writeback_queue_job(struct drm_writeback_connector *wb_connector, struct drm_connector_state *conn_state);
void drm_writeback_cleanup_job(struct drm_writeback_job *job);
void drm_writeback_signal_completion(struct drm_writeback_connector *wb_connector, int status);
struct dma_fence *drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector);

#endif
