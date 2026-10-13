/* include/nvif/fifo.h -- Kestrel's Linux KPI: NVIF runlist queries; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_FIFO_H
#define _LKPI_NV_NVIF_FIFO_H

#include <nvif/device.h>

/* Bitmask of the runlists able to run engines of type @engine. */
u64 nvif_fifo_runlist(struct nvif_device *, u64 engine);

/*
 * Runlists usable for copy engines.  Prefer the ones not shared with the
 * graphics runlist; fall back to the shared ones if nothing else is left.
 */
static inline u64
nvif_fifo_runlist_ce(struct nvif_device *device)
{
	u64 gr = nvif_fifo_runlist(device, NV_DEVICE_HOST_RUNLIST_ENGINES_GR);
	u64 ce = nvif_fifo_runlist(device, NV_DEVICE_HOST_RUNLIST_ENGINES_CE);

	if (ce) {
		ce &= ~gr;
		if (!ce)
			ce = gr;
	}
	return ce;
}

#endif /* _LKPI_NV_NVIF_FIFO_H */
