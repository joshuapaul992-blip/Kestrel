/* include/nvif/disp.h -- Kestrel's Linux KPI: the NVIF display object; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_DISP_H
#define _LKPI_NV_NVIF_DISP_H

#include <nvif/object.h>

struct nvif_device;

struct nvif_disp {
	struct nvif_object object;
	unsigned long conn_mask;
	unsigned long outp_mask;
	unsigned long head_mask;
};

int  nvif_disp_ctor(struct nvif_device *, const char *name, s32 oclass,
		    struct nvif_disp *);
void nvif_disp_dtor(struct nvif_disp *);

#endif /* _LKPI_NV_NVIF_DISP_H */
