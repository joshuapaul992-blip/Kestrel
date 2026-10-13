/* include/nvif/if500b.h -- Kestrel's Linux KPI: NV50 memory object arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_IF500B_H
#define _LKPI_NV_NVIF_IF500B_H

#include "if000a.h"

struct nv50_mem_vn {
};

struct nv50_mem_v0 {
	__u8  version;
	__u8  bankswz;
	__u8  contig;
};

struct nv50_mem_map_vn {
};

struct nv50_mem_map_v0 {
	__u8  version;
	__u8  ro;
	__u8  kind;
	__u8  comp;
};

#endif /* _LKPI_NV_NVIF_IF500B_H */
