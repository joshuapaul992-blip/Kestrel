/* nvkm/subdev/mmu/uvmm.h -- Kestrel's Linux KPI: user-visible VMM object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVKM_SUBDEV_MMU_UVMM_H
#define _LKPI_NV_NVKM_SUBDEV_MMU_UVMM_H
#define nvkm_uvmm(p) container_of((p), struct nvkm_uvmm, object)
#include <core/object.h>
#include "vmm.h"

struct nvkm_uvmm {
	struct nvkm_object object;
	struct nvkm_vmm *vmm;
};

int nvkm_uvmm_new(const struct nvkm_oclass *, void *argv, u32 argc,
		  struct nvkm_object **);

#endif /* _LKPI_NV_NVKM_SUBDEV_MMU_UVMM_H */
