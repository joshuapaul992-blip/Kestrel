/* dispnv50/wndw.h -- Kestrel's Linux KPI: nv50 KMS windows (planes), their channels and method tables; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_DISPNV50_WNDW_H
#define _LKPI_NV_DISPNV50_WNDW_H

#define nv50_wndw(p) container_of((p), struct nv50_wndw, plane)

#include "disp.h"
#include "atom.h"
#include "lut.h"

/* A DMA object created for one framebuffer handle. */
struct nv50_wndw_ctxdma {
	struct list_head head;
	struct nvif_object object;
};

struct nv50_wndw {
	const struct nv50_wndw_func *func;
	const struct nv50_wimm_func *immd;
	int id;
	struct nv50_disp_interlock interlock;

	struct {
		struct nvif_object *parent;
		struct list_head list;
	} ctxdma;

	struct drm_plane plane;
	struct nv50_lut ilut;

	struct nv50_dmac wndw;	/* main channel */
	struct nv50_dmac wimm;	/* immediate channel */

	u16 ntfy;
	u16 sema;
	u32 data;
};

int nv50_wndw_new_(const struct nv50_wndw_func *, struct drm_device *,
		   enum drm_plane_type, const char *name, int index,
		   const u32 *format, u32 heads,
		   enum nv50_disp_interlock_type, u32 interlock_data,
		   struct nv50_wndw **);
void nv50_wndw_flush_set(struct nv50_wndw *, u32 *interlock,
			 struct nv50_wndw_atom *);
void nv50_wndw_flush_clr(struct nv50_wndw *, u32 *interlock, bool flush,
			 struct nv50_wndw_atom *);
void nv50_wndw_ntfy_enable(struct nv50_wndw *, struct nv50_wndw_atom *);
int nv50_wndw_wait_armed(struct nv50_wndw *, struct nv50_wndw_atom *);

/* Hardware-class specific window methods. */
struct nv50_wndw_func {
	int (*acquire)(struct nv50_wndw *, struct nv50_wndw_atom *asyw,
		       struct nv50_head_atom *asyh);
	void (*release)(struct nv50_wndw *, struct nv50_wndw_atom *asyw,
			struct nv50_head_atom *asyh);
	void (*prepare)(struct nv50_wndw *, struct nv50_head_atom *asyh,
			struct nv50_wndw_atom *asyw);

	int (*sema_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*sema_clr)(struct nv50_wndw *);
	void (*ntfy_reset)(struct nouveau_bo *, u32 offset);
	int (*ntfy_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*ntfy_clr)(struct nv50_wndw *);
	int (*ntfy_wait_begun)(struct nouveau_bo *, u32 offset,
			       struct nvif_device *);
	void (*ilut)(struct nv50_wndw *, struct nv50_wndw_atom *, int size);
	void (*csc)(struct nv50_wndw *, struct nv50_wndw_atom *,
		    const struct drm_color_ctm *);
	int (*csc_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*csc_clr)(struct nv50_wndw *);
	bool ilut_identity;
	int ilut_size;
	bool olut_core;
	int (*xlut_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*xlut_clr)(struct nv50_wndw *);
	int (*image_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*image_clr)(struct nv50_wndw *);
	int (*scale_set)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*blend_set)(struct nv50_wndw *, struct nv50_wndw_atom *);

	int (*update)(struct nv50_wndw *, u32 *interlock);
};

extern const struct drm_plane_funcs nv50_wndw;

void base507c_ntfy_reset(struct nouveau_bo *, u32);
int base507c_ntfy_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int base507c_ntfy_clr(struct nv50_wndw *);
int base507c_ntfy_wait_begun(struct nouveau_bo *, u32, struct nvif_device *);
int base507c_image_clr(struct nv50_wndw *);
int base507c_update(struct nv50_wndw *, u32 *);

void base907c_csc(struct nv50_wndw *, struct nv50_wndw_atom *,
		  const struct drm_color_ctm *);

/* Methods for the immediate (cursor / position) channel. */
struct nv50_wimm_func {
	int (*point)(struct nv50_wndw *, struct nv50_wndw_atom *);
	int (*update)(struct nv50_wndw *, u32 *interlock);
};

extern const struct nv50_wimm_func curs507a;
bool curs507a_space(struct nv50_wndw *);

/*
 * Immediate channels use PIO rather than a push buffer; "waiting for space"
 * there means polling the channel's free count.  @dmac must be the wimm
 * member of a struct nv50_wndw.
 */
static inline __must_check int
nvif_chan_wait(struct nv50_dmac *dmac, u32 size)
{
	struct nv50_wndw *w = container_of(dmac, typeof(*w), wimm);

	if (!curs507a_space(w))
		return -ETIMEDOUT;
	return 0;
}

/* GV100 (c37e) */
int wndwc37e_new(struct nouveau_drm *, enum drm_plane_type, int, s32,
		 struct nv50_wndw **);
int wndwc37e_new_(const struct nv50_wndw_func *, struct nouveau_drm *,
		  enum drm_plane_type type, int index, s32 oclass, u32 heads,
		  struct nv50_wndw **);
int wndwc37e_acquire(struct nv50_wndw *, struct nv50_wndw_atom *,
		     struct nv50_head_atom *);
void wndwc37e_release(struct nv50_wndw *, struct nv50_wndw_atom *,
		      struct nv50_head_atom *);
int wndwc37e_sema_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int wndwc37e_sema_clr(struct nv50_wndw *);
int wndwc37e_ntfy_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int wndwc37e_ntfy_clr(struct nv50_wndw *);
int wndwc37e_image_clr(struct nv50_wndw *);
int wndwc37e_blend_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int wndwc37e_update(struct nv50_wndw *, u32 *);

/* TU102 (c57e) */
int wndwc57e_new(struct nouveau_drm *, enum drm_plane_type, int, s32,
		 struct nv50_wndw **);
void wndwc57e_ilut(struct nv50_wndw *, struct nv50_wndw_atom *, int);
int wndwc57e_ilut_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int wndwc57e_ilut_clr(struct nv50_wndw *);
int wndwc57e_csc_set(struct nv50_wndw *, struct nv50_wndw_atom *);
int wndwc57e_csc_clr(struct nv50_wndw *);

/* GA102 (c67e) */
int wndwc67e_new(struct nouveau_drm *, enum drm_plane_type, int, s32,
		 struct nv50_wndw **);

int nv50_wndw_new(struct nouveau_drm *, enum drm_plane_type, int index,
		  struct nv50_wndw **);

#endif /* _LKPI_NV_DISPNV50_WNDW_H */
