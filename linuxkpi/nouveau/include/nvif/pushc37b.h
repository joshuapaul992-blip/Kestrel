/* include/nvif/pushc37b.h -- Kestrel's Linux KPI: pushbuffer method headers for Volta display (class c37b) channels; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_PUSHC37B_H
#define _LKPI_NV_NVIF_PUSHC37B_H

#include <nvif/push.h>

#include <nvhw/class/clc37b.h>

#define PUSH_HDR(p,m,c) do {							\
	PUSH_ASSERT(!((m) & ~DRF_SMASK(NVC37B_DMA_METHOD_OFFSET)), "mthd");	\
	PUSH_ASSERT(!((c) & ~DRF_MASK(NVC37B_DMA_METHOD_COUNT)), "size");	\
	PUSH_DATA__((p),							\
		    NVDEF(NVC37B, DMA, OPCODE, METHOD) |			\
		    NVVAL(NVC37B, DMA, METHOD_COUNT, (c)) |			\
		    NVVAL(NVC37B, DMA, METHOD_OFFSET, (m) >> 2),		\
		    " mthd 0x%04x size %d - %s", (u32)(m), (u32)(c), __func__);	\
} while (0)

#define PUSH_MTHD_INC 4:4
#define PUSH_MTHD_HDR(p,s,m,c) PUSH_HDR(p,m,c)

#endif /* _LKPI_NV_NVIF_PUSHC37B_H */
