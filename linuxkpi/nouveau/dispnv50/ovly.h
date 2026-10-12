/* dispnv50/ovly.h -- Kestrel's Linux KPI: nv50 KMS overlay plane channel classes; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_OVLY_H
#define _LKPI_NV_DISPNV50_OVLY_H

#include "wndw.h"

int nv50_ovly_new(struct nouveau_drm *, int head, struct nv50_wndw **);

/* NV50 (507e) */
int ovly507e_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
int ovly507e_new_(const struct nv50_wndw_func *, const u32 *format,
		  struct nouveau_drm *, int head, s32 oclass,
		  u32 interlock_data, struct nv50_wndw **);
int ovly507e_acquire(struct nv50_wndw *, struct nv50_wndw_atom *,
		     struct nv50_head_atom *);
void ovly507e_release(struct nv50_wndw *, struct nv50_wndw_atom *,
		      struct nv50_head_atom *);
int ovly507e_scale_set(struct nv50_wndw *, struct nv50_wndw_atom *);

/* G82 (827e) */
int ovly827e_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
extern const u32 ovly827e_format[];
void ovly827e_ntfy_reset(struct nouveau_bo *, u32);
int ovly827e_ntfy_wait_begun(struct nouveau_bo *, u32, struct nvif_device *);

/* GF110 (907e) */
int ovly907e_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
extern const struct nv50_wndw_func ovly907e;

/* GK104 (917e) */
int ovly917e_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);

#endif /* _LKPI_NV_DISPNV50_OVLY_H */
