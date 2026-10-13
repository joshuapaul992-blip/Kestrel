/* nvkm/subdev/mmu/ummu.h -- Kestrel's Linux KPI: user-visible MMU object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVKM_SUBDEV_MMU_UMMU_H
#define _LKPI_NV_NVKM_SUBDEV_MMU_UMMU_H
#define nvkm_ummu(p) container_of((p), struct nvkm_ummu, object)
#include <core/object.h>
#include <subdev/mmu/priv.h>             /* vendored next to the original: name it from nvkm/ */

struct nvkm_ummu {
	struct nvkm_object object;
	struct nvkm_mmu *mmu;
};

int nvkm_ummu_new(struct nvkm_device *, const struct nvkm_oclass *,
		  void *argv, u32 argc, struct nvkm_object **);

#endif /* _LKPI_NV_NVKM_SUBDEV_MMU_UMMU_H */
