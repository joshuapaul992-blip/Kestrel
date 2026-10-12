/* include/nvif/if900d.h -- Kestrel's Linux KPI: GF100 VMM object arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_IF900D_H
#define _LKPI_NV_NVIF_IF900D_H

#include "if000c.h"

struct gf100_vmm_vn {
};

struct gf100_vmm_map_vn {
};

struct gf100_vmm_map_v0 {
	__u8  version;
	__u8  vol;
	__u8  ro;
	__u8  priv;
	__u8  kind;
};

#endif /* _LKPI_NV_NVIF_IF900D_H */
