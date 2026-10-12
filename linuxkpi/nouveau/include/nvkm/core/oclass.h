/* include/nvkm/core/oclass.h -- Kestrel's Linux KPI: nvkm object class descriptors (nvkm_sclass, nvkm_oclass); written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_INCLUDE_NVKM_CORE_OCLASS_H
#define _LKPI_NV_INCLUDE_NVKM_CORE_OCLASS_H
#include <core/os.h>
#include <core/debug.h>

struct nvkm_oclass;
struct nvkm_object;

/* a class a parent object can instantiate */
struct nvkm_sclass {
	int minver;
	int maxver;
	s32 oclass;
	const struct nvkm_object_func *func;
	int (*ctor)(const struct nvkm_oclass *, void *data, u32 size,
		    struct nvkm_object **);
};

/* everything a constructor needs to build one object */
struct nvkm_oclass {
	int (*ctor)(const struct nvkm_oclass *, void *data, u32 size,
		    struct nvkm_object **);
	struct nvkm_sclass base;
	const void *priv;
	const void *engn;
	u32 handle;
	u64 object;
	struct nvkm_client *client;
	struct nvkm_object *parent;
	struct nvkm_engine *engine;
};

#endif /* _LKPI_NV_INCLUDE_NVKM_CORE_OCLASS_H */
