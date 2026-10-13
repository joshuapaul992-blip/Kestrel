/* dispnv50/wimm.h -- Kestrel's Linux KPI: nv50 KMS window-immediate channel; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_WIMM_H
#define _LKPI_NV_DISPNV50_WIMM_H

#include "wndw.h"

int nv50_wimm_init(struct nouveau_drm *, struct nv50_wndw *);
int wimmc37b_init(struct nouveau_drm *, s32, struct nv50_wndw *);

#endif /* _LKPI_NV_DISPNV50_WIMM_H */
