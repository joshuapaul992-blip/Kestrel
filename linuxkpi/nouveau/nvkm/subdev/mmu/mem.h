/* nvkm/subdev/mmu/mem.h -- Kestrel's Linux KPI: MMU memory-object allocation and mapping entry points; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVKM_SUBDEV_MMU_MEM_H
#define _LKPI_NV_NVKM_SUBDEV_MMU_MEM_H
#include <subdev/mmu/priv.h>             /* vendored next to the original: name it from nvkm/ */

int nvkm_mem_new_type(struct nvkm_mmu *, int type, u8 page, u64 size,
		      void *argv, u32 argc, struct nvkm_memory **);
int nvkm_mem_map_host(struct nvkm_memory *, void **pmap);

/* per-generation backends: new(mmu, type, page, size, argv, argc, pmemory)
 * and map(mmu, memory, argv, argc, paddr, psize, pvma) */
int nv04_mem_new(struct nvkm_mmu *, int, u8, u64, void *, u32, struct nvkm_memory **);
int nv04_mem_map(struct nvkm_mmu *, struct nvkm_memory *, void *, u32,
		 u64 *, u64 *, struct nvkm_vma **);

int nv50_mem_new(struct nvkm_mmu *, int, u8, u64, void *, u32, struct nvkm_memory **);
int nv50_mem_map(struct nvkm_mmu *, struct nvkm_memory *, void *, u32,
		 u64 *, u64 *, struct nvkm_vma **);

int gf100_mem_new(struct nvkm_mmu *, int, u8, u64, void *, u32, struct nvkm_memory **);
int gf100_mem_map(struct nvkm_mmu *, struct nvkm_memory *, void *, u32,
		  u64 *, u64 *, struct nvkm_vma **);

#endif /* _LKPI_NV_NVKM_SUBDEV_MMU_MEM_H */
