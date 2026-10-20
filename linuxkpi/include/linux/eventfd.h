/* linux/eventfd.h -- Kestrel's Linux KPI: signalling user space's eventfds
 * (linuxkpi/src/chardev.c, on Kestrel's eventfds through the glue) */
#ifndef _LKPI_LINUX_EVENTFD_H
#define _LKPI_LINUX_EVENTFD_H

#include <linux/types.h>
#include <linux/err.h>

struct eventfd_ctx;

struct eventfd_ctx *eventfd_ctx_fdget(int fd);     /* ERR_PTR(-EBADF / -EINVAL) */
void eventfd_ctx_put(struct eventfd_ctx *ctx);     /* any context */
void eventfd_signal(struct eventfd_ctx *ctx);      /* any context */

#endif
