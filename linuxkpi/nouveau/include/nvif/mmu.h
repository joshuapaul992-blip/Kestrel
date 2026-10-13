/* include/nvif/mmu.h -- Kestrel's Linux KPI: the NVIF MMU object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_MMU_H
#define _LKPI_NV_NVIF_MMU_H

#include <nvif/object.h>

/* Flag bits in nvif_mmu.type[].type */
#define NVIF_MEM_VRAM     0x01
#define NVIF_MEM_HOST     0x02
#define NVIF_MEM_COMP     0x04
#define NVIF_MEM_DISP     0x08
#define NVIF_MEM_KIND     0x10
#define NVIF_MEM_MAPPABLE 0x20
#define NVIF_MEM_COHERENT 0x40
#define NVIF_MEM_UNCACHED 0x80

struct nvif_mmu {
	struct nvif_object object;
	u8  dmabits;
	u8  heap_nr;
	u8  type_nr;
	u8  kind_inv;
	u16 kind_nr;
	s32 mem;

	struct {
		u64 size;
	} *heap;	/* heap_nr entries */

	struct {
		u8 type;
		u8 heap;
	} *type;	/* type_nr entries */

	u8 *kind;	/* kind_nr entries */
};

int  nvif_mmu_ctor(struct nvif_object *, const char *name, s32 oclass,
		   struct nvif_mmu *);
void nvif_mmu_dtor(struct nvif_mmu *);

/* Kind 0 is always accepted; others must be in range and not mapped to kind_inv. */
static inline bool
nvif_mmu_kind_valid(struct nvif_mmu *mmu, u8 kind)
{
	if (!kind)
		return true;
	return kind < mmu->kind_nr && mmu->kind[kind] != mmu->kind_inv;
}

/* Index of the first memory type having every flag in @mask, or -EINVAL. */
static inline int
nvif_mmu_type(struct nvif_mmu *mmu, u8 mask)
{
	int idx;

	for (idx = 0; idx < mmu->type_nr; idx++)
		if ((mmu->type[idx].type & mask) == mask)
			return idx;
	return -EINVAL;
}

#endif /* _LKPI_NV_NVIF_MMU_H */
