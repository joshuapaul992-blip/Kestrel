/* include/nvif/push006c.h -- Kestrel's Linux KPI: pushbuffer method headers for NV04-era (class 006c) channels; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NVIF_PUSH006C_H
#define _LKPI_NV_NVIF_PUSH006C_H

#include <nvif/push.h>

#include <nvhw/class/cl006c.h>

/*
 * Default subchannel assignments by object class.  A file that binds its
 * objects differently defines PUSH006C_SUBC and its own set beforehand.
 */
#ifndef PUSH006C_SUBC
#define PUSH006C_SUBC_NV06E	0	/* host */
#define PUSH006C_SUBC_NV176E	0
#define PUSH006C_SUBC_NV826F	0
#define PUSH006C_SUBC_NV042	0	/* 2D surfaces */
#define PUSH006C_SUBC_NV062	0
#define PUSH006C_SUBC_NV019	0	/* clip rectangle */
#define PUSH006C_SUBC_NV043	0	/* ROP */
#define PUSH006C_SUBC_NV044	0	/* pattern */
#define PUSH006C_SUBC_NV_SW	1	/* software object */
#define PUSH006C_SUBC_NV05F	2	/* image blit */
#define PUSH006C_SUBC_NV09F	2
#define PUSH006C_SUBC_NV04A	3	/* GDI rectangle/text */
#define PUSH006C_SUBC_NV502D	3	/* 2D engine */
#define PUSH006C_SUBC_NV039	4	/* M2MF */
#define PUSH006C_SUBC_NV5039	4
#define PUSH006C_SUBC_NV85B5	4	/* copy engine */
#define PUSH006C_SUBC_NV74C1	4	/* cipher */
#endif

/* Emit one method header word: opcode o, debug name n, subchannel s, method m, count c. */
#define PUSH_HDR(p,o,n,s,m,c) do {						\
	PUSH_ASSERT(!((s) & ~DRF_MASK(NV06C_METHOD_SUBCHANNEL)), "subc");	\
	PUSH_ASSERT(!((m) & ~DRF_SMASK(NV06C_METHOD_ADDRESS)), "mthd");		\
	PUSH_ASSERT(!((c) & ~DRF_MASK(NV06C_METHOD_COUNT)), "count");		\
	PUSH_DATA__((p),							\
		    NVVAL_X(NV06C_METHOD_ADDRESS, (m) >> 2) |			\
		    NVVAL_X(NV06C_METHOD_SUBCHANNEL, (s)) |			\
		    NVVAL_X(NV06C_METHOD_COUNT, (c)) |				\
		    NVVAL_X(NV06C_OPCODE, NV06C_OPCODE_##o),			\
		    " "n" subc %d mthd 0x%04x size %d - %s",			\
		    (u32)(s), (u32)(m), (u32)(c), __func__);			\
} while (0)

#define PUSH_MTHD_INC 4:4
#define PUSH_MTHD_HDR(p,c,m,n) \
	PUSH_HDR(p, METHOD, "incr", PUSH006C_SUBC_##c, m, n)

#define PUSH_NINC_INC 0:0
#define PUSH_NINC_HDR(p,c,m,n) \
	PUSH_HDR(p, NONINC_METHOD, "ninc", PUSH006C_SUBC_##c, m, n)

#define PUSH_JUMP(p,o) do {							\
	PUSH_ASSERT(!((o) & ~0x1fffffffcULL), "offset");			\
	PUSH_DATA__((p),							\
		    NVVAL_X(NV06C_OPCODE, NV06C_OPCODE_JUMP) |			\
		    NVVAL_X(NV06C_JUMP_OFFSET, (o) >> 2),			\
		    " jump 0x%08x - %s", (u32)(o), __func__);			\
} while (0)

#endif /* _LKPI_NV_NVIF_PUSH006C_H */
