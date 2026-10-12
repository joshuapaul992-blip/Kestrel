/* dispnv50/oimm.h -- Kestrel's Linux KPI: nv50 KMS overlay-immediate channel; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_OIMM_H
#define _LKPI_NV_DISPNV50_OIMM_H

#include "wndw.h"

int nv50_oimm_init(struct nouveau_drm *, struct nv50_wndw *);
int oimm507b_init(struct nouveau_drm *, s32, struct nv50_wndw *);

#endif /* _LKPI_NV_DISPNV50_OIMM_H */
