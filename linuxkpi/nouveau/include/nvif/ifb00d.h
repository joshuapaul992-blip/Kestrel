/* include/nvif/ifb00d.h -- Kestrel's Linux KPI: GM200 VMM object arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_IFB00D_H
#define _LKPI_NV_NVIF_IFB00D_H

#include "if000c.h"

struct gm200_vmm_vn {
};

struct gm200_vmm_v0 {
	__u8  version;
	__u8  bigpage;
};

struct gm200_vmm_map_vn {
};

struct gm200_vmm_map_v0 {
	__u8  version;
	__u8  vol;
	__u8  ro;
	__u8  priv;
	__u8  kind;
};

#endif /* _LKPI_NV_NVIF_IFB00D_H */
