/* nvkm/subdev/mmu/vmm.h -- Kestrel's Linux KPI: VMM internals (page-table tracking, descriptors, per-chip backends, PTE write helpers); written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVKM_SUBDEV_MMU_VMM_H
#define _LKPI_NV_NVKM_SUBDEV_MMU_VMM_H
#include <subdev/mmu/priv.h>             /* vendored next to the original: name it from nvkm/ */
#include <core/memory.h>
enum nvkm_memory_target;

/* Values stored in nvkm_vmm_pt::pde[]: a child table pointer, NULL, or the
 * "sparse" marker. */
#define NVKM_VMM_PDE_INVALID(pde)	IS_ERR_OR_NULL(pde)
#define NVKM_VMM_PDE_SPARSED(pde)	IS_ERR(pde)
#define NVKM_VMM_PDE_SPARSE		ERR_PTR(-EBUSY)

/* Bits of nvkm_vmm_pt::pte[] (one byte per large PTE): sparse, valid, and a
 * count of small PTEs in use beneath it. */
#define NVKM_VMM_PTE_SPARSE	0x80
#define NVKM_VMM_PTE_VALID	0x40
#define NVKM_VMM_PTE_SPTES	0x3f

/* Software state for one page table / directory.  pt[]/refs[] have two
 * slots for levels where a large- and small-page table share a range. */
struct nvkm_vmm_pt {
	struct nvkm_mmu_pt *pt[2];
	u32 refs[2];
	u8 page;		/* page size index (needed by nv50 PDE writes) */
	bool sparse:1;		/* whole table is sparse */
	struct nvkm_vmm_pt **pde;	/* directories: child per PDE */
	u8 pte[];		/* dual tables: NVKM_VMM_PTE_* per LPTE */
};

typedef void (*nvkm_vmm_pxe_func)(struct nvkm_vmm *,
				  struct nvkm_mmu_pt *, u32 ptei, u32 ptes);
typedef void (*nvkm_vmm_pde_func)(struct nvkm_vmm *,
				  struct nvkm_vmm_pt *, u32 pdei);
typedef void (*nvkm_vmm_pte_func)(struct nvkm_vmm *, struct nvkm_mmu_pt *,
				  u32 ptei, u32 ptes, struct nvkm_vmm_map *);

/* how to write entries at one page-table level */
struct nvkm_vmm_desc_func {
	nvkm_vmm_pxe_func invalid;
	nvkm_vmm_pxe_func unmap;
	nvkm_vmm_pxe_func sparse;

	nvkm_vmm_pde_func pde;

	nvkm_vmm_pte_func mem;
	nvkm_vmm_pte_func dma;
	nvkm_vmm_pte_func sgl;

	nvkm_vmm_pte_func pfn;
	bool (*pfn_clear)(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32 ptei, u32 ptes);
	nvkm_vmm_pxe_func pfn_unmap;
};

extern const struct nvkm_vmm_desc_func gf100_vmm_pgd;
void gf100_vmm_pgd_pde(struct nvkm_vmm *, struct nvkm_vmm_pt *, u32);
extern const struct nvkm_vmm_desc_func gf100_vmm_pgt;
void gf100_vmm_pgt_unmap(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32, u32);
void gf100_vmm_pgt_mem(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32, u32,
		       struct nvkm_vmm_map *);
void gf100_vmm_pgt_dma(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32, u32,
		       struct nvkm_vmm_map *);
void gf100_vmm_pgt_sgl(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32, u32,
		       struct nvkm_vmm_map *);

void gk104_vmm_lpt_invalid(struct nvkm_vmm *, struct nvkm_mmu_pt *, u32, u32);

/* one level of a page-table layout; arrays end with a zero entry */
struct nvkm_vmm_desc {
	enum {
		PGD,
		PGT,
		SPT,
		LPT,
	} type;
	u8 bits;	/* address bits this level translates */
	u8 size;	/* bytes per entry */
	u32 align;	/* table alignment */
	const struct nvkm_vmm_desc_func *func;
};

extern const struct nvkm_vmm_desc nv50_vmm_desc_12[];
extern const struct nvkm_vmm_desc nv50_vmm_desc_16[];

extern const struct nvkm_vmm_desc gk104_vmm_desc_16_12[];
extern const struct nvkm_vmm_desc gk104_vmm_desc_16_16[];
extern const struct nvkm_vmm_desc gk104_vmm_desc_17_12[];
extern const struct nvkm_vmm_desc gk104_vmm_desc_17_17[];

extern const struct nvkm_vmm_desc gm200_vmm_desc_16_12[];
extern const struct nvkm_vmm_desc gm200_vmm_desc_16_16[];
extern const struct nvkm_vmm_desc gm200_vmm_desc_17_12[];
extern const struct nvkm_vmm_desc gm200_vmm_desc_17_17[];

extern const struct nvkm_vmm_desc gp100_vmm_desc_12[];
extern const struct nvkm_vmm_desc gp100_vmm_desc_16[];

/* nvkm_vmm_page::type flags: Sparse, Vram, Host, Compression */
#define NVKM_VMM_PAGE_SPARSE	0x01
#define NVKM_VMM_PAGE_VRAM	0x02
#define NVKM_VMM_PAGE_HOST	0x04
#define NVKM_VMM_PAGE_COMP	0x08
#define NVKM_VMM_PAGE_Sxxx	(NVKM_VMM_PAGE_SPARSE)
#define NVKM_VMM_PAGE_xVxx	(NVKM_VMM_PAGE_VRAM)
#define NVKM_VMM_PAGE_SVxx	(NVKM_VMM_PAGE_Sxxx | NVKM_VMM_PAGE_VRAM)
#define NVKM_VMM_PAGE_xxHx	(NVKM_VMM_PAGE_HOST)
#define NVKM_VMM_PAGE_SxHx	(NVKM_VMM_PAGE_Sxxx | NVKM_VMM_PAGE_HOST)
#define NVKM_VMM_PAGE_xVHx	(NVKM_VMM_PAGE_xVxx | NVKM_VMM_PAGE_HOST)
#define NVKM_VMM_PAGE_SVHx	(NVKM_VMM_PAGE_SVxx | NVKM_VMM_PAGE_HOST)
#define NVKM_VMM_PAGE_xVxC	(NVKM_VMM_PAGE_xVxx | NVKM_VMM_PAGE_COMP)
#define NVKM_VMM_PAGE_SVxC	(NVKM_VMM_PAGE_SVxx | NVKM_VMM_PAGE_COMP)
#define NVKM_VMM_PAGE_xxHC	(NVKM_VMM_PAGE_xxHx | NVKM_VMM_PAGE_COMP)
#define NVKM_VMM_PAGE_SxHC	(NVKM_VMM_PAGE_SxHx | NVKM_VMM_PAGE_COMP)

/* a supported page size; arrays end with shift == 0 */
struct nvkm_vmm_page {
	u8 shift;
	const struct nvkm_vmm_desc *desc;
	u8 type;
};

struct nvkm_vmm_func {
	int (*join)(struct nvkm_vmm *, struct nvkm_memory *inst);
	void (*part)(struct nvkm_vmm *, struct nvkm_memory *inst);

	int (*aper)(enum nvkm_memory_target);
	int (*valid)(struct nvkm_vmm *, void *argv, u32 argc, struct nvkm_vmm_map *);
	void (*flush)(struct nvkm_vmm *, int depth);

	int (*mthd)(struct nvkm_vmm *, struct nvkm_client *, u32 mthd, void *argv, u32 argc);

	void (*invalidate_pdb)(struct nvkm_vmm *, u64 addr);

	u64 page_block;
	const struct nvkm_vmm_page page[];
};

/* an instance block bound to the VMM */
struct nvkm_vmm_join {
	struct nvkm_memory *inst;
	struct list_head head;
};

int nvkm_vmm_new_(const struct nvkm_vmm_func *, struct nvkm_mmu *, u32 pd_header,
		  bool managed, u64 addr, u64 size, struct lock_class_key *,
		  const char *name, struct nvkm_vmm **);
struct nvkm_vma *nvkm_vma_new(u64 addr, u64 size);
struct nvkm_vma *nvkm_vmm_node_search(struct nvkm_vmm *, u64 addr);
struct nvkm_vma *nvkm_vmm_node_split(struct nvkm_vmm *, struct nvkm_vma *,
				     u64 addr, u64 size);
int nvkm_vmm_get_locked(struct nvkm_vmm *, bool getref, bool mapref, bool sparse,
			u8 page, u8 align, u64 size, struct nvkm_vma **pvma);
void nvkm_vmm_put_locked(struct nvkm_vmm *, struct nvkm_vma *);
void nvkm_vmm_unmap_locked(struct nvkm_vmm *, struct nvkm_vma *, bool pfn);
void nvkm_vmm_unmap_region(struct nvkm_vmm *, struct nvkm_vma *);

/* raw page-table access, bypassing VMA tracking */
int nvkm_vmm_raw_get(struct nvkm_vmm *vmm, u64 addr, u64 size, u8 refd);
void nvkm_vmm_raw_put(struct nvkm_vmm *vmm, u64 addr, u64 size, u8 refd);
void nvkm_vmm_raw_unmap(struct nvkm_vmm *vmm, u64 addr, u64 size, bool sparse, u8 refd);
int nvkm_vmm_raw_sparse(struct nvkm_vmm *, u64 addr, u64 size, bool ref);

/* true when [start, start + size) lies wholly inside one of the two
 * kernel-managed windows (managed.p or managed.n) */
static inline bool
nvkm_vmm_in_managed_range(struct nvkm_vmm *vmm, u64 start, u64 size)
{
	u64 end = start + size;

	if (start >= vmm->managed.p.addr &&
	    end <= vmm->managed.p.addr + vmm->managed.p.size)
		return true;
	return start >= vmm->managed.n.addr &&
	       end <= vmm->managed.n.addr + vmm->managed.n.size;
}

/* PFN array entry format for nvkm_vmm_pfn_map() */
#define NVKM_VMM_PFN_ADDR	0xfffffffffffff000ULL
#define NVKM_VMM_PFN_ADDR_SHIFT	12
#define NVKM_VMM_PFN_APER	0x00000000000000f0ULL
#define NVKM_VMM_PFN_HOST	0x0000000000000000ULL
#define NVKM_VMM_PFN_VRAM	0x0000000000000010ULL
#define NVKM_VMM_PFN_A		0x0000000000000004ULL
#define NVKM_VMM_PFN_W		0x0000000000000002ULL
#define NVKM_VMM_PFN_V		0x0000000000000001ULL
#define NVKM_VMM_PFN_NONE	0x0000000000000000ULL

int nvkm_vmm_pfn_map(struct nvkm_vmm *, u8 page, u64 addr, u64 size, u64 *pfn);
int nvkm_vmm_pfn_unmap(struct nvkm_vmm *, u64 addr, u64 size);

struct nvkm_vma *nvkm_vma_tail(struct nvkm_vma *, u64 tail);

/* shared per-generation pieces */
int nv04_vmm_new_(const struct nvkm_vmm_func *, struct nvkm_mmu *, u32,
		  bool, u64, u64, void *, u32, struct lock_class_key *,
		  const char *, struct nvkm_vmm **);
int nv04_vmm_valid(struct nvkm_vmm *, void *, u32, struct nvkm_vmm_map *);

int nv50_vmm_join(struct nvkm_vmm *, struct nvkm_memory *);
void nv50_vmm_part(struct nvkm_vmm *, struct nvkm_memory *);
int nv50_vmm_valid(struct nvkm_vmm *, void *, u32, struct nvkm_vmm_map *);
void nv50_vmm_flush(struct nvkm_vmm *, int);

int gf100_vmm_new_(const struct nvkm_vmm_func *, const struct nvkm_vmm_func *,
		   struct nvkm_mmu *, bool, u64, u64, void *, u32,
		   struct lock_class_key *, const char *, struct nvkm_vmm **);
int gf100_vmm_join_(struct nvkm_vmm *, struct nvkm_memory *, u64 base);
int gf100_vmm_join(struct nvkm_vmm *, struct nvkm_memory *);
void gf100_vmm_part(struct nvkm_vmm *, struct nvkm_memory *);
int gf100_vmm_aper(enum nvkm_memory_target);
int gf100_vmm_valid(struct nvkm_vmm *, void *, u32, struct nvkm_vmm_map *);
void gf100_vmm_flush(struct nvkm_vmm *, int);
void gf100_vmm_invalidate(struct nvkm_vmm *, u32 type);
void gf100_vmm_invalidate_pdb(struct nvkm_vmm *, u64 addr);

int gk20a_vmm_aper(enum nvkm_memory_target);

int gm200_vmm_new_(const struct nvkm_vmm_func *, const struct nvkm_vmm_func *,
		   struct nvkm_mmu *, bool, u64, u64, void *, u32,
		   struct lock_class_key *, const char *, struct nvkm_vmm **);
int gm200_vmm_join_(struct nvkm_vmm *, struct nvkm_memory *, u64 base);
int gm200_vmm_join(struct nvkm_vmm *, struct nvkm_memory *);

int gp100_vmm_new_(const struct nvkm_vmm_func *, struct nvkm_mmu *, bool, u64, u64,
		   void *, u32, struct lock_class_key *, const char *,
		   struct nvkm_vmm **);
int gp100_vmm_join(struct nvkm_vmm *, struct nvkm_memory *);
int gp100_vmm_valid(struct nvkm_vmm *, void *, u32, struct nvkm_vmm_map *);
void gp100_vmm_flush(struct nvkm_vmm *, int);
int gp100_vmm_mthd(struct nvkm_vmm *, struct nvkm_client *, u32, void *, u32);
void gp100_vmm_invalidate_pdb(struct nvkm_vmm *, u64 addr);

int gv100_vmm_join(struct nvkm_vmm *, struct nvkm_memory *);

/* chip constructors:
 * (mmu, managed, addr, size, argv, argc, lock key, name, pvmm) */
#define _LKPI_NVKM_VMM_NEW_ARGS \
	struct nvkm_mmu *, bool, u64, u64, void *, u32, \
	struct lock_class_key *, const char *, struct nvkm_vmm **
int nv04_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int nv41_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int nv44_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int nv50_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int mcp77_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int g84_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gf100_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gk104_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gk20a_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gm200_vmm_new_fixed(_LKPI_NVKM_VMM_NEW_ARGS);
int gm200_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gm20b_vmm_new_fixed(_LKPI_NVKM_VMM_NEW_ARGS);
int gm20b_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gp100_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gp10b_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int gv100_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
int tu102_vmm_new(_LKPI_NVKM_VMM_NEW_ARGS);
#undef _LKPI_NVKM_VMM_NEW_ARGS

/* Logging: compiled out above CONFIG_NOUVEAU_DEBUG, filtered at run time by
 * the VMM's own debug level. */
#define VMM_PRINT(l,v,p,f,a...) do {						\
	struct nvkm_vmm *_vmm = (v);						\
	if (CONFIG_NOUVEAU_DEBUG >= (l) && _vmm->debug >= (l))			\
		nvkm_printk_(&_vmm->mmu->subdev, 0, p, "%s: "f"\n",		\
			     _vmm->name, ##a);					\
} while (0)
#define VMM_DEBUG(v,f,a...) VMM_PRINT(NV_DBG_DEBUG, (v), info, f, ##a)
#define VMM_TRACE(v,f,a...) VMM_PRINT(NV_DBG_TRACE, (v), info, f, ##a)
#define VMM_SPAM(v,f,a...)  VMM_PRINT(NV_DBG_SPAM, (v), dbg, f, ##a)

/*
 * Walk PTEN entries starting at PTEI of page table PT, filling them from the
 * backing described by MAP.  Each step covers what is left of the current
 * chunk [BASE, BASE + SIZE) from MAP->off on, or fewer if PTEN runs out
 * first (MAP->off then records where to resume).  When a chunk is used up,
 * NEXT advances to the following one.  FILL(VMM, PT, ptei, ptes, MAP, addr)
 * writes the entries.  PTEI and PTEN are updated in place.
 */
#define VMM_MAP_ITER(VMM,PT,PTEI,PTEN,MAP,FILL,BASE,SIZE,NEXT) do {		\
	nvkm_kmap((PT)->memory);						\
	while (PTEN) {								\
		u64 _ptes = ((SIZE) - MAP->off) >> MAP->page->shift;		\
		u64 _addr = ((BASE) + MAP->off);				\
										\
		if (_ptes <= PTEN) {						\
			MAP->off = 0;						\
			NEXT;							\
		} else {							\
			MAP->off += PTEN << MAP->page->shift;			\
			_ptes = PTEN;						\
		}								\
										\
		VMM_SPAM(VMM, "ITER %08x %08x PTE(s)", PTEI, (u32)_ptes);	\
		FILL(VMM, PT, PTEI, _ptes, MAP, _addr);				\
		PTEI += _ptes;							\
		PTEN -= _ptes;							\
	}									\
	nvkm_done((PT)->memory);						\
} while (0)

/* VRAM node list */
#define VMM_MAP_ITER_MEM(VMM,PT,PTEI,PTEN,MAP,FILL)				\
	VMM_MAP_ITER(VMM, PT, PTEI, PTEN, MAP, FILL,				\
		     ((u64)MAP->mem->offset << NVKM_RAM_MM_SHIFT),		\
		     ((u64)MAP->mem->length << NVKM_RAM_MM_SHIFT),		\
		     (MAP->mem = MAP->mem->next))
/* array of page-sized DMA addresses */
#define VMM_MAP_ITER_DMA(VMM,PT,PTEI,PTEN,MAP,FILL)				\
	VMM_MAP_ITER(VMM, PT, PTEI, PTEN, MAP, FILL,				\
		     *MAP->dma, PAGE_SIZE, MAP->dma++)
/* scatterlist */
#define VMM_MAP_ITER_SGL(VMM,PT,PTEI,PTEN,MAP,FILL)				\
	VMM_MAP_ITER(VMM, PT, PTEI, PTEN, MAP, FILL,				\
		     sg_dma_address(MAP->sgl), sg_dma_len(MAP->sgl),		\
		     (MAP->sgl = sg_next(MAP->sgl)))

/*
 * PTE writes into an nvkm_mmu_pt (m) of VMM v at byte offset o: WO writes a
 * single value, FO fills c consecutive entries with it.  Each logs at SPAM.
 */
#define VMM_FO(m,o,d,c,b) nvkm_fo##b((m)->memory, (o), (d), (c))
#define VMM_WO(m,o,d,c,b) nvkm_wo##b((m)->memory, (o), (d))
#define VMM_XO(m,v,o,d,c,b,fn,f,a...) do {					\
	const u32 _pteo = (o);							\
	u##b _data = (d);							\
	VMM_SPAM((v), "   %010llx "f, (m)->addr + _pteo, _data, ##a);		\
	VMM_##fn((m), (m)->base + _pteo, _data, (c), b);			\
} while (0)

#define VMM_WO032(m,v,o,d) VMM_XO((m), (v), (o), (d), 1, 32, WO, "%08x")
#define VMM_FO032(m,v,o,d,c) \
	VMM_XO((m), (v), (o), (d), (c), 32, FO, "%08x %08x", (c))

#define VMM_WO064(m,v,o,d) VMM_XO((m), (v), (o), (d), 1, 64, WO, "%016llx")
#define VMM_FO064(m,v,o,d,c) \
	VMM_XO((m), (v), (o), (d), (c), 64, FO, "%016llx %08x", (c))

/* 128-bit entries, written as two 64-bit halves, c times */
#define VMM_XO128(m,v,o,lo,hi,c,f,a...) do {					\
	u32 _pteo = (o), _ptes = (c);						\
	const u64 _addr = (m)->addr + _pteo;					\
	VMM_SPAM((v), "   %010llx %016llx%016llx"f, _addr, (hi), (lo), ##a);	\
	for (; _ptes; _ptes--, _pteo += 0x10) {					\
		nvkm_wo64((m)->memory, (m)->base + _pteo + 0, (lo));		\
		nvkm_wo64((m)->memory, (m)->base + _pteo + 8, (hi));		\
	}									\
} while (0)

#define VMM_WO128(m,v,o,lo,hi) VMM_XO128((m), (v), (o), (lo), (hi), 1, "")
#define VMM_FO128(m,v,o,lo,hi,c) do {						\
	nvkm_kmap((m)->memory);							\
	VMM_XO128((m), (v), (o), (lo), (hi), (c), " %08x", (c));		\
	nvkm_done((m)->memory);							\
} while (0)

#endif /* _LKPI_NV_NVKM_SUBDEV_MMU_VMM_H */
