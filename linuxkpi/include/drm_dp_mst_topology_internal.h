/* drm_dp_mst_topology_internal.h -- Kestrel's Linux KPI: the sideband message
 * coders drm_dp_mst_topology.c exports for DRM's self-tests (Linux's copy of
 * this header is GPL-2.0-only, so it is written here). */
#ifndef _LKPI_DRM_DP_MST_TOPOLOGY_INTERNAL_H
#define _LKPI_DRM_DP_MST_TOPOLOGY_INTERNAL_H

struct drm_dp_sideband_msg_req_body;
struct drm_dp_sideband_msg_tx;
struct drm_printer;

void drm_dp_encode_sideband_req(const struct drm_dp_sideband_msg_req_body *req, struct drm_dp_sideband_msg_tx *raw);
int  drm_dp_decode_sideband_req(const struct drm_dp_sideband_msg_tx *raw, struct drm_dp_sideband_msg_req_body *req);
void drm_dp_dump_sideband_msg_req_body(const struct drm_dp_sideband_msg_req_body *req, int indent,
                                       struct drm_printer *printer);

#endif
