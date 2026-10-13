/* dispnv50/core.h -- Kestrel's Linux KPI: nv50 KMS core channel and per-class method tables; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_CORE_H
#define _LKPI_NV_DISPNV50_CORE_H

#include "disp.h"
#include "atom.h"
#include <dispnv50/crc.h>                /* vendored next to the original: name it from the driver root */
#include <nouveau_encoder.h>

struct nv50_core {
	const struct nv50_core_func *func;
	struct nv50_dmac chan;
	bool assign_windows;
};

int nv50_core_new(struct nouveau_drm *, struct nv50_core **);
void nv50_core_del(struct nv50_core **);

struct nv50_core_func {
	int (*init)(struct nv50_core *);
	void (*ntfy_init)(struct nouveau_bo *, u32 offset);
	int (*caps_init)(struct nouveau_drm *, struct nv50_disp *);
	int (*ntfy_wait_done)(struct nouveau_bo *, u32 offset,
			      struct nvif_device *);
	int (*update)(struct nv50_core *, u32 *interlock, bool ntfy);

	struct {
		int (*owner)(struct nv50_core *);
	} wndw;

	const struct nv50_head_func *head;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	const struct nv50_crc_func *crc;
#endif
	/* per output-resource type (DAC, PIOR, SOR) methods */
	const struct nv50_outp_func {
		int (*ctrl)(struct nv50_core *, int or, u32 ctrl,
			    struct nv50_head_atom *);
		void (*get_caps)(struct nv50_disp *, struct nouveau_encoder *,
				 int or);
	} *dac, *pior, *sor;
};

/* NV50 (507d) */
int core507d_new(struct nouveau_drm *, s32, struct nv50_core **);
int core507d_new_(const struct nv50_core_func *, struct nouveau_drm *, s32,
		  struct nv50_core **);
int core507d_init(struct nv50_core *);
void core507d_ntfy_init(struct nouveau_bo *, u32);
int core507d_read_caps(struct nv50_disp *disp);
int core507d_caps_init(struct nouveau_drm *, struct nv50_disp *);
int core507d_ntfy_wait_done(struct nouveau_bo *, u32, struct nvif_device *);
int core507d_update(struct nv50_core *, u32 *, bool);
extern const struct nv50_outp_func dac507d;
extern const struct nv50_outp_func sor507d;
extern const struct nv50_outp_func pior507d;

/* G82 (827d) */
int core827d_new(struct nouveau_drm *, s32, struct nv50_core **);

/* GF110 (907d) */
int core907d_new(struct nouveau_drm *, s32, struct nv50_core **);
int core907d_caps_init(struct nouveau_drm *, struct nv50_disp *);
extern const struct nv50_outp_func dac907d;
extern const struct nv50_outp_func sor907d;

/* GK104 (917d) */
int core917d_new(struct nouveau_drm *, s32, struct nv50_core **);

/* GV100 (c37d) */
int corec37d_new(struct nouveau_drm *, s32, struct nv50_core **);
int corec37d_caps_init(struct nouveau_drm *, struct nv50_disp *);
int corec37d_ntfy_wait_done(struct nouveau_bo *, u32, struct nvif_device *);
int corec37d_update(struct nv50_core *, u32 *, bool);
int corec37d_wndw_owner(struct nv50_core *);
extern const struct nv50_outp_func sorc37d;

/* TU102 (c57d) */
int corec57d_new(struct nouveau_drm *, s32, struct nv50_core **);

#endif /* _LKPI_NV_DISPNV50_CORE_H */
