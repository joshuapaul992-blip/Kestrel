/* nvkm/subdev/mmu/umem.h -- Kestrel's Linux KPI: user-visible MMU memory object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVKM_SUBDEV_MMU_UMEM_H
#define _LKPI_NV_NVKM_SUBDEV_MMU_UMEM_H
#define nvkm_umem(p) container_of((p), struct nvkm_umem, object)
#include <core/object.h>
#include "mem.h"

struct nvkm_umem {
	struct nvkm_object object;
	struct nvkm_mmu *mmu;
	u8 type:8;
	bool mappable:1;
	bool io:1;

	struct nvkm_memory *memory;
	struct list_head head;

	/* current mapping: a BAR VMA when io, otherwise a kernel pointer */
	union {
		struct nvkm_vma *bar;
		void *map;
	};
};

int nvkm_umem_new(const struct nvkm_oclass *, void *argv, u32 argc,
		  struct nvkm_object **);

#endif /* _LKPI_NV_NVKM_SUBDEV_MMU_UMEM_H */
