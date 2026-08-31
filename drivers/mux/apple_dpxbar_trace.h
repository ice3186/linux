/* SPDX-License-Identifier: GPL-2.0-only OR MIT */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apple_dpxbar

#if !defined(_TRACE_APPLE_DPXBAR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_APPLE_DPXBAR_H

#include <linux/tracepoint.h>

TRACE_EVENT(apple_dpxbar_route,
	TP_PROTO(const char *devname, unsigned int control, int state,
		 int selected_before, int selected_after, int ret),
	TP_ARGS(devname, control, state, selected_before, selected_after, ret),

	TP_STRUCT__entry(__string(devname, devname)
		__field(unsigned int, control)
		__field(int, state)
		__field(int, dispext)
		__field(int, core)
		__field(int, selected_before)
		__field(int, selected_after)
		__field(int, ret)
	),

	TP_fast_assign(__assign_str(devname);
		__entry->control = control;
		__entry->state = state;
		__entry->dispext = state >= 0 ? state >> 1 : -1;
		__entry->core = state >= 0 ? state & 1 : -1;
		__entry->selected_before = selected_before;
		__entry->selected_after = selected_after;
		__entry->ret = ret;
	),

	TP_printk("%s: control=%s(%u) state=%d dispext=%d core=%d selected=%d->%d ret=%d",
		  __get_str(devname), __print_symbolic(__entry->control,
						   { 0, "dpphy" },
						   { 1, "dpin0" },
						   { 2, "dpin1" }),
		  __entry->control, __entry->state, __entry->dispext,
		  __entry->core, __entry->selected_before,
		  __entry->selected_after, __entry->ret)
);

TRACE_EVENT(apple_dpxbar_rmw,
	TP_PROTO(const void *xbar, u32 reg, u32 old, u32 mask, u32 set,
		 u32 value),
	TP_ARGS(xbar, reg, old, mask, set, value),

	TP_STRUCT__entry(__field(const void *, xbar)
		__field(u32, reg)
		__field(u32, old)
		__field(u32, mask)
		__field(u32, set)
		__field(u32, value)
	),

	TP_fast_assign(__entry->xbar = xbar;
		__entry->reg = reg;
		__entry->old = old;
		__entry->mask = mask;
		__entry->set = set;
		__entry->value = value;
	),

	TP_printk("xbar=%p reg=0x%03x old=0x%08x mask=0x%08x set=0x%08x new=0x%08x",
		  __entry->xbar, __entry->reg, __entry->old,
		  __entry->mask, __entry->set, __entry->value)
);

#endif /* _TRACE_APPLE_DPXBAR_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE apple_dpxbar_trace

#include <trace/define_trace.h>
