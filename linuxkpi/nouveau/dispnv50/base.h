/* dispnv50/base.h -- Kestrel's Linux KPI: nv50 KMS base (primary plane) channel classes; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_BASE_H
#define _LKPI_NV_DISPNV50_BASE_H

#include "wndw.h"

int nv50_base_new(struct nouveau_drm *, int head, struct nv50_wndw **);

/* NV50 (507c) */
int base507c_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
int base507c_new_(const struct nv50_wndw_func *, const u32 *format,
		  struct nouveau_drm *, int head, s32 oclass,
		  u32 interlock_data, struct nv50_wndw **);
extern const u32 base507c_format[];
int base507c_acquire(struct nv50_wndw *, struct nv50_wndw_atom *,
		     struct nv50_head_atom *);
void base507c_release(struct nv50_wndw *, struct nv50_wndw_atom *,
		      struct nv50_head_atom *);
int base507c_sema_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int base507c_sema_clr(struct nv50_wndw *);
int base507c_xlut_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int base507c_xlut_clr(struct nv50_wndw *);

/* G82 (827c) */
int base827c_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);

/* GF110 (907c) */
int base907c_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
extern const struct nv50_wndw_func base907c;

/* GK104 (917c) */
int base917c_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);

#endif /* _LKPI_NV_DISPNV50_BASE_H */
