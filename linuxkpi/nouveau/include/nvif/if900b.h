/* include/nvif/if900b.h -- Kestrel's Linux KPI: GF100 memory object arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_IF900B_H
#define _LKPI_NV_NVIF_IF900B_H

#include "if000a.h"

struct gf100_mem_vn {
};

struct gf100_mem_v0 {
	__u8  version;
	__u8  contig;
};

struct gf100_mem_map_vn {
};

struct gf100_mem_map_v0 {
	__u8  version;
	__u8  ro;
	__u8  kind;
};

#endif /* _LKPI_NV_NVIF_IF900B_H */
