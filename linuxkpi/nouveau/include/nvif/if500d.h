/* include/nvif/if500d.h -- Kestrel's Linux KPI: NV50 VMM object arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_IF500D_H
#define _LKPI_NV_NVIF_IF500D_H

#include "if000c.h"

struct nv50_vmm_vn {
};

struct nv50_vmm_map_vn {
};

struct nv50_vmm_map_v0 {
	__u8  version;
	__u8  ro;
	__u8  priv;
	__u8  kind;
	__u8  comp;
};

#endif /* _LKPI_NV_NVIF_IF500D_H */
