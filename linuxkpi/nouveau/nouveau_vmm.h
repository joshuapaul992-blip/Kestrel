/* nouveau_vmm.h -- Kestrel's Linux KPI: nouveau per-client address spaces and buffer mappings; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NOUVEAU_VMM_H
#define _LKPI_NV_NOUVEAU_VMM_H

#include <nvif/vmm.h>

struct nouveau_bo;
struct nouveau_mem;

/* A refcounted mapping of a buffer object into one nouveau_vmm. */
struct nouveau_vma {
	struct nouveau_vmm *vmm;
	int refs;
	struct list_head head;	/* on nouveau_bo.vma_list */
	u64 addr;
	struct nouveau_mem *mem;
	struct nouveau_fence *fence;
};

struct nouveau_vma *nouveau_vma_find(struct nouveau_bo *,
				     struct nouveau_vmm *);
int nouveau_vma_new(struct nouveau_bo *, struct nouveau_vmm *,
		    struct nouveau_vma **);
void nouveau_vma_del(struct nouveau_vma **);
int nouveau_vma_map(struct nouveau_vma *, struct nouveau_mem *);
void nouveau_vma_unmap(struct nouveau_vma *);

struct nouveau_vmm {
	struct nouveau_cli *cli;
	struct nvif_vmm vmm;
	struct nouveau_svmm *svmm;
};

int nouveau_vmm_init(struct nouveau_cli *, s32 oclass, struct nouveau_vmm *);
void nouveau_vmm_fini(struct nouveau_vmm *);

#endif /* _LKPI_NV_NOUVEAU_VMM_H */
