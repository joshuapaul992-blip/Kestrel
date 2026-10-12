/* dispnv50/curs.h -- Kestrel's Linux KPI: nv50 KMS cursor channel classes; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_CURS_H
#define _LKPI_NV_DISPNV50_CURS_H

#include "wndw.h"

int nv50_curs_new(struct nouveau_drm *, int head, struct nv50_wndw **);

int curs507a_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
int curs507a_new_(const struct nv50_wimm_func *, struct nouveau_drm *,
		  int head, s32 oclass, u32 interlock_data,
		  struct nv50_wndw **);
int curs907a_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);
int cursc37a_new(struct nouveau_drm *, int, s32, struct nv50_wndw **);

#endif /* _LKPI_NV_DISPNV50_CURS_H */
