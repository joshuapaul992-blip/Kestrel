/* include/nvif/mem.h -- Kestrel's Linux KPI: the NVIF memory object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_MEM_H
#define _LKPI_NV_NVIF_MEM_H

#include "mmu.h"

struct nvif_mem {
	struct nvif_object object;
	u8  type;
	u8  page;
	u64 addr;
	u64 size;
};

int  nvif_mem_ctor_type(struct nvif_mmu *mmu, const char *name, s32 oclass,
			int type, u8 page, u64 size, void *argv, u32 argc,
			struct nvif_mem *);
int  nvif_mem_ctor(struct nvif_mmu *mmu, const char *name, s32 oclass,
		   u8 type, u8 page, u64 size, void *argv, u32 argc,
		   struct nvif_mem *);
void nvif_mem_dtor(struct nvif_mem *);

int  nvif_mem_ctor_map(struct nvif_mmu *, const char *name, u8 type, u64 size,
		       struct nvif_mem *);

#endif /* _LKPI_NV_NVIF_MEM_H */
