/* linux/stacktrace.h -- Kestrel's Linux KPI: stack traces (none) */
#ifndef _LKPI_LINUX_STACKTRACE_H
#define _LKPI_LINUX_STACKTRACE_H

static inline unsigned int stack_trace_save(unsigned long *store, unsigned int size, unsigned int skip) { return 0; }

#endif
