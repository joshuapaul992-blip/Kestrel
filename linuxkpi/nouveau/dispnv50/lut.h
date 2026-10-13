/* dispnv50/lut.h -- Kestrel's Linux KPI: nv50 KMS colour lookup-table buffers; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_LUT_H
#define _LKPI_NV_DISPNV50_LUT_H

#include <nvif/mem.h>

struct drm_color_lut;
struct drm_property_blob;
struct nv50_disp;

/* Two buffers, so one can be filled while the other is scanned out. */
struct nv50_lut {
	struct nvif_mem mem[2];
};

int nv50_lut_init(struct nv50_disp *, struct nvif_mmu *, struct nv50_lut *);
void nv50_lut_fini(struct nv50_lut *);
u32 nv50_lut_load(struct nv50_lut *, int buffer, struct drm_property_blob *,
		  void (*)(struct drm_color_lut *, int size, void __iomem *));

#endif /* _LKPI_NV_DISPNV50_LUT_H */
