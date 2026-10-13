/* linuxkpi/nouveau/kestrel.c -- nouveau's side of Kestrel's display client
 *
 * lkpi_drm_fb_where() puts in the log where a frame buffer Kestrel draws in
 * place really is: the buffer object's VRAM offset and BAR1 address, what
 * the first word of that VRAM holds read through the PRAMIN window (not
 * through BAR1), and what the head's window was told to scan out. When a
 * monitor stays blank, that line says whether Kestrel's writes reach the
 * buffer and whether the buffer is the one on screen. The display engine's
 * own record of the methods it took (core channel once, then each head's
 * base channel) follows, as nvkm prints it after a channel error. */
#include <drm/drm_crtc.h>
#include <drm/drm_plane.h>
#include <drm/drm_framebuffer.h>
#include "nouveau_drv.h"
#include "nouveau_bo.h"
#include "nouveau_gem.h"
#include <subdev/instmem.h>
#include <engine/disp.h>
#include "nvkm/engine/disp/chan.h"
#include "dispnv50/atom.h"

/* One word of VRAM through the PRAMIN window (BAR0 0x700000, 64 KiB at
 * 0x001700 << 16), under the instance memory's lock like nvkm's own use. */
static u32 pramin_rd32(struct nouveau_drm *drm, u64 addr)
{
	struct nvkm_device *device = nvxx_device(drm);
	struct nvkm_instmem *imem = device->imem;
	unsigned long flags;
	u32 old, v;

	if (imem) spin_lock_irqsave(&imem->lock, flags);
	old = nvkm_rd32(device, 0x001700);
	nvkm_wr32(device, 0x001700, addr >> 16);
	v = nvkm_rd32(device, 0x700000 + (addr & 0xfffc));
	nvkm_wr32(device, 0x001700, old);
	if (imem) spin_unlock_irqrestore(&imem->lock, flags);
	return v;
}

void lkpi_drm_fb_where(struct drm_crtc *crtc, struct drm_framebuffer *fb, size_t last);   /* drm_kestrel.c */
void lkpi_drm_fb_where(struct drm_crtc *crtc, struct drm_framebuffer *fb, size_t last)
{
	struct drm_device *dev = crtc->dev;
	struct nouveau_drm *drm;
	struct nouveau_bo *nvbo;
	struct ttm_resource *res;

	if (strcmp(dev->driver->name, "nouveau") || !fb || !fb->obj[0]) return;
	drm = nouveau_drm(dev);
	nvbo = nouveau_gem_object(fb->obj[0]);
	res = nvbo->bo.resource;
	if (!res) return;

	u64 vram = (u64)res->start << PAGE_SHIFT;
	NV_INFO(drm, "kestrel: %s: bo in %s at %llx (offset %llx, contig %d, kind %d, pinned %d), BAR1 %llx; "
		"PRAMIN reads %08x %08x\n", crtc->name, res->mem_type == TTM_PL_VRAM ? "VRAM" : "not VRAM",
		vram, nvbo->offset, nvbo->contig, nvbo->kind, nvbo->bo.pin_count,
		(u64)res->bus.offset, pramin_rd32(drm, vram), pramin_rd32(drm, vram + last));

	struct nvkm_disp *disp = nvxx_device(drm)->disp;
	static bool core_dumped;
	if (disp && disp->chan[0] && !core_dumped) {
		core_dumped = true;
		nv50_disp_chan_mthd(disp->chan[0], NV_DBG_ERROR);
	}
	for (int i = 1; disp && i < ARRAY_SIZE(disp->chan); i++) {
		struct nvkm_disp_chan *chan = disp->chan[i];
		if (chan && chan->mthd && chan->head == drm_crtc_index(crtc) && !strcmp(chan->mthd->name, "Base"))
			nv50_disp_chan_mthd(chan, NV_DBG_ERROR);
	}

	struct drm_plane_state *ps = crtc->primary ? crtc->primary->state : NULL;
	if (ps && ps->crtc == crtc) {
		struct nv50_wndw_atom *asyw = nv50_wndw_atom(ps);
		NV_INFO(drm, "kestrel: %s: %s shows fb %u (ours %u), visible %d, image at %llx pitch %u kind %d "
			"layout %d handle %08x, ilut %s, xlut handle %08x\n", crtc->name, crtc->primary->name,
			ps->fb ? ps->fb->base.id : 0, fb->base.id, asyw->visible, asyw->image.offset[0],
			asyw->image.pitch[0], asyw->image.kind, asyw->image.layout, asyw->image.handle[0],
			asyw->ilut ? "set" : "none", asyw->xlut.handle);
	} else {
		NV_INFO(drm, "kestrel: %s: its primary plane is not on it\n", crtc->name);
	}
	if (crtc->state) {
		struct nv50_head_atom *asyh = nv50_head_atom(crtc->state);
		NV_INFO(drm, "kestrel: %s: active %d, output LUT visible %d handle %08x at %llx, mode %d size %d\n",
			crtc->name, crtc->state->active, asyh->olut.visible, asyh->olut.handle,
			(u64)asyh->olut.offset, asyh->olut.mode, asyh->olut.size);
	}
}
