/* nouveau_svm.h -- Kestrel's Linux KPI: nouveau shared virtual memory (SVM) interface; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_NOUVEAU_SVM_H
#define _LKPI_NV_NOUVEAU_SVM_H

#include <nvif/os.h>
#include <linux/mmu_notifier.h>

struct drm_device;
struct drm_file;
struct nouveau_drm;

/* A client's GPU address space mirrored from a process mm. */
struct nouveau_svmm {
	struct mmu_notifier notifier;
	struct nouveau_vmm *vmm;
	struct {
		unsigned long start;
		unsigned long limit;
	} unmanaged;
	struct mutex mutex;
};

#if IS_ENABLED(CONFIG_DRM_NOUVEAU_SVM)
void nouveau_svm_init(struct nouveau_drm *);
void nouveau_svm_fini(struct nouveau_drm *);
void nouveau_svm_suspend(struct nouveau_drm *);
void nouveau_svm_resume(struct nouveau_drm *);

int nouveau_svmm_init(struct drm_device *, void *, struct drm_file *);
void nouveau_svmm_fini(struct nouveau_svmm **);
int nouveau_svmm_join(struct nouveau_svmm *, u64 inst);
void nouveau_svmm_part(struct nouveau_svmm *, u64 inst);
int nouveau_svmm_bind(struct drm_device *, void *, struct drm_file *);
void nouveau_svmm_invalidate(struct nouveau_svmm *, u64 start, u64 limit);

u64 *nouveau_pfns_alloc(unsigned long npages);
void nouveau_pfns_free(u64 *pfns);
void nouveau_pfns_map(struct nouveau_svmm *, struct mm_struct *,
		      unsigned long addr, u64 *pfns, unsigned long npages);
#else
/* SVM compiled out: setup is a no-op and the ioctls report ENOSYS. */
static inline void nouveau_svm_init(struct nouveau_drm *drm) { }
static inline void nouveau_svm_fini(struct nouveau_drm *drm) { }
static inline void nouveau_svm_suspend(struct nouveau_drm *drm) { }
static inline void nouveau_svm_resume(struct nouveau_drm *drm) { }

static inline int
nouveau_svmm_init(struct drm_device *dev, void *data, struct drm_file *file)
{
	return -ENOSYS;
}

static inline void nouveau_svmm_fini(struct nouveau_svmm **psvmm) { }

static inline int
nouveau_svmm_join(struct nouveau_svmm *svmm, u64 inst)
{
	return 0;
}

static inline void nouveau_svmm_part(struct nouveau_svmm *svmm, u64 inst) { }

static inline int
nouveau_svmm_bind(struct drm_device *dev, void *data, struct drm_file *file)
{
	return -ENOSYS;
}
#endif /* CONFIG_DRM_NOUVEAU_SVM */

#endif /* _LKPI_NV_NOUVEAU_SVM_H */
