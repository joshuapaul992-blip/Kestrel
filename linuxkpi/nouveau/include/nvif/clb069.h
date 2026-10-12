/* include/nvif/clb069.h -- Kestrel's Linux KPI: NVIF class 0xb069 (fault buffer) arguments; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_CLB069_H
#define _LKPI_NV_NVIF_CLB069_H

struct nvif_clb069_v0 {
	__u8  version;
	__u8  pad01[3];
	__u32 entries;
	__u32 get;
	__u32 put;
};

/* No event arguments are carried; the empty member is a GNU extension. */
union nvif_clb069_event_args {
	struct nvif_clb069_event_vn {
	} vn;
};

#endif /* _LKPI_NV_NVIF_CLB069_H */
