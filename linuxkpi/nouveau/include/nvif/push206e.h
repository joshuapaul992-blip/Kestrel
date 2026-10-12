/* include/nvif/push206e.h -- Kestrel's Linux KPI: pushbuffer CALL for class 206e channels; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_PUSH206E_H
#define _LKPI_NV_NVIF_PUSH206E_H

#include <nvif/push006c.h>

#include <nvhw/class/cl206e.h>

#define PUSH_CALL(p,o) do {							\
	PUSH_ASSERT(!((o) & ~0xffffffffcULL), "offset");			\
	PUSH_DATA__((p),							\
		    NVDEF(NV206E, DMA, OPCODE2, CALL) |				\
		    NVVAL(NV206E, DMA, CALL_OFFSET, (o) >> 2),			\
		    " call 0x%08x - %s", (u32)(o), __func__);			\
} while (0)

#endif /* _LKPI_NV_NVIF_PUSH206E_H */
