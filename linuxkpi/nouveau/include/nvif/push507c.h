/* include/nvif/push507c.h -- Kestrel's Linux KPI: pushbuffer method headers for NV50 display (class 507c) channels; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_PUSH507C_H
#define _LKPI_NV_NVIF_PUSH507C_H

#include <nvif/push.h>

#include <nvhw/class/cl507c.h>

/* Display channels have no subchannels: method m, count c. */
#define PUSH_HDR(p,m,c) do {							\
	PUSH_ASSERT(!((m) & ~DRF_SMASK(NV507C_DMA_METHOD_OFFSET)), "mthd");	\
	PUSH_ASSERT(!((c) & ~DRF_MASK(NV507C_DMA_METHOD_COUNT)), "size");	\
	PUSH_DATA__((p),							\
		    NVDEF(NV507C, DMA, OPCODE, METHOD) |			\
		    NVVAL(NV507C, DMA, METHOD_COUNT, (c)) |			\
		    NVVAL(NV507C, DMA, METHOD_OFFSET, (m) >> 2),		\
		    " mthd 0x%04x size %d - %s", (u32)(m), (u32)(c), __func__);	\
} while (0)

#define PUSH_MTHD_INC 4:4
#define PUSH_MTHD_HDR(p,s,m,c) PUSH_HDR(p,m,c)

#define PUSH_JUMP(p,o) do {							\
	PUSH_ASSERT(!((o) & ~DRF_SMASK(NV507C_DMA_JUMP_OFFSET)), "offset");	\
	PUSH_DATA__((p),							\
		    NVDEF(NV507C, DMA, OPCODE, JUMP) |				\
		    NVVAL(NV507C, DMA, JUMP_OFFSET, (o) >> 2),			\
		    " jump 0x%08x - %s", (u32)(o), __func__);			\
} while (0)

#endif /* _LKPI_NV_NVIF_PUSH507C_H */
