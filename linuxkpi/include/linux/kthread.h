/* linux/kthread.h -- Kestrel's Linux KPI: kernel threads (linuxkpi/src/kthread.c)
 *
 * kthread_create() makes a Kestrel thread that waits until the first
 * wake_up_process(), as in Linux; kthread_run() also wakes it. */
#ifndef _LKPI_LINUX_KTHREAD_H
#define _LKPI_LINUX_KTHREAD_H

#include <linux/err.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

struct completion;

__printf(4, 5) struct task_struct *kthread_create_on_node(int (*threadfn)(void *data), void *data, int node,
                                                         const char namefmt[], ...);
#define kthread_create(threadfn, data, namefmt, arg...) \
    kthread_create_on_node(threadfn, data, -1, namefmt, ##arg)
#define kthread_run(threadfn, data, namefmt, ...) ({ \
        struct task_struct *__k = kthread_create(threadfn, data, namefmt, ##__VA_ARGS__); \
        if (!IS_ERR(__k)) wake_up_process(__k); \
        __k; })
#define kthread_create_on_cpu(fn, data, cpu, namefmt) kthread_create(fn, data, namefmt, cpu)
#define kthread_bind(k, cpu)            do { (void)(k); } while (0)
/* ask the thread to stop, wait for it to return: its result */
int  kthread_stop(struct task_struct *k);
int  kthread_stop_put(struct task_struct *k);
bool kthread_should_stop(void);
bool kthread_should_park(void);
int  kthread_park(struct task_struct *k);
void kthread_unpark(struct task_struct *k);
void kthread_parkme(void);
void *kthread_data(struct task_struct *k);
__noreturn void kthread_exit(long result);
__noreturn void kthread_complete_and_exit(struct completion *c, long result);
bool kthread_freezable_should_stop(bool *was_frozen);
#define set_freezable()                 do { } while (0)
#define try_to_freeze()                 false
#define freezing(t)                     false

/* kthread workers: an ordered workqueue under another name */
struct kthread_work;
typedef void (*kthread_work_func_t)(struct kthread_work *work);
struct kthread_worker {
    struct workqueue_struct *wq;
    struct task_struct *task;           /* NULL: the work runs on a workqueue thread */
};
struct kthread_work {
    struct work_struct ws;
    kthread_work_func_t func;
    struct kthread_worker *worker;
};
void lkpi_kthread_work_fn(struct work_struct *ws);
static inline void kthread_init_work(struct kthread_work *work, kthread_work_func_t fn)
{ INIT_WORK(&work->ws, lkpi_kthread_work_fn); work->func = fn; work->worker = NULL; }
__printf(2, 3) struct kthread_worker *kthread_create_worker(unsigned int flags, const char namefmt[], ...);
void kthread_destroy_worker(struct kthread_worker *worker);
static inline bool kthread_queue_work(struct kthread_worker *worker, struct kthread_work *work)
{ work->worker = worker; return queue_work(worker->wq, &work->ws); }
static inline bool kthread_cancel_work_sync(struct kthread_work *work) { return cancel_work_sync(&work->ws); }
static inline void kthread_flush_work(struct kthread_work *work) { flush_work(&work->ws); }
static inline void kthread_flush_worker(struct kthread_worker *worker) { flush_workqueue(worker->wq); }


#endif
