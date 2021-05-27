/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM apu_sched_events
#if !defined(_TRACE_APU_SCHED_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_APU_SCHED_EVENTS_H
#include <linux/tracepoint.h>
#include "mdw_rsc.h"
TRACE_EVENT(deadline_load,
	TP_PROTO(char name[MDW_DEV_NAME_SIZE],
		 u64 avg_load[3]),
	TP_ARGS(name, avg_load),
	TP_STRUCT__entry(
		__array(char, name, MDW_DEV_NAME_SIZE)
		__array(u64, avg_load, 3)
		),
	TP_fast_assign(
		memcpy(__entry->name, name,
		       MDW_DEV_NAME_SIZE * sizeof(char));
		memcpy(__entry->avg_load, avg_load, 3 * sizeof(u64));
	),
	TP_printk(
		"type=%s, avg_load[0]=%lld, avg_load[1]=%lld, avg_load[2]=%lld",
		__entry->name,
		__entry->avg_load[0],
		__entry->avg_load[1],
		__entry->avg_load[2])
);

TRACE_EVENT(deadline_task,
	TP_PROTO(char name[MDW_DEV_NAME_SIZE],
		 bool enter, u64 apu_loading),
	TP_ARGS(name, enter, apu_loading),
	TP_STRUCT__entry(
		__array(char, name, MDW_DEV_NAME_SIZE)
		__field(bool, enter)
		__field(u64, apu_loading)
		),
	TP_fast_assign(
		memcpy(__entry->name, name,
		       MDW_DEV_NAME_SIZE * sizeof(char));
		__entry->enter = enter;
		__entry->apu_loading = apu_loading;
	),
	TP_printk("type=%s, enter=%d, loading=%llu",
			__entry->name,
			__entry->enter,
			__entry->apu_loading)
);
#endif /* _TRACE_MET_MDLASYS_EVENTS_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE apu_sched_events
#include <trace/define_trace.h>


