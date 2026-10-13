/* linux/tracepoint.h -- Kestrel's Linux KPI: tracepoints (none: Kestrel has no tracing)
 *
 * A trace header's TRACE_EVENT()s become empty inline trace_<name>()
 * functions, and trace/define_trace.h (its second, generating read) is
 * empty, so CREATE_TRACE_POINTS makes nothing. */
#ifndef _LKPI_LINUX_TRACEPOINT_H
#define _LKPI_LINUX_TRACEPOINT_H

#include <linux/types.h>

#define PARAMS(args...)         args
#define TP_PROTO(args...)       args
#define TP_ARGS(args...)        args
#define TP_CONDITION(args...)   args

#define DECLARE_TRACE(name, proto, args)                                         \
    static inline void trace_##name(proto) { }                                   \
    static inline bool trace_##name##_enabled(void) { return false; }
#define TRACE_EVENT(name, proto, args, tstruct, assign, print)                   \
    DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
#define TRACE_EVENT_CONDITION(name, proto, args, cond, tstruct, assign, print)   \
    DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args)                                \
    DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_PRINT(template, name, proto, args, print)                   \
    DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_CONDITION(template, name, proto, args, cond)                \
    DECLARE_TRACE(name, PARAMS(proto), PARAMS(args))
#define TRACE_DEFINE_ENUM(x)
#define TRACE_DEFINE_SIZEOF(x)

#endif
