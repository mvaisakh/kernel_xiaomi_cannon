/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _MTK_SYNC_H
#define _MTK_SYNC_H

/*
 * TIMEOUT_NEVER may be passed to the wait method to indicate that it
 * should wait indefinitely for the fence to signal.
 */
#define TIMEOUT_NEVER -1

/* ---------------------------------------------------------------------------
 */

#ifdef __KERNEL__

#include <../../../../drivers/dma-buf/sync_debug.h>
#include <linux/version.h>

/*
 * sync_timeline, sync_fence data structure
 */

struct fence_data {
	__u32 value;
	char name[32];
	__s32 fence; /* fd of new fence */
};

#ifdef CONFIG_DEBUG_FS
/*
 * sync_timeline, sync_fence API
 */

extern struct sync_timeline *sync_timeline_create(const char *name);

/*
 * sync_timeline, sync_fence API
 */

/**
 * mtk_sync_timeline_create() - creates a sync object
 * @name:   sync_timeline name
 *
 * The mtk_sync_timeline_create() function creates a sync object named @name,
 * which represents a 32-bit monotonically increasing counter.
 */
struct sync_timeline *mtk_sync_timeline_create(const char *name);
#else
void mtk_sync_timeline_create(const char *name);
#endif

/**
 * mtk_sync_timeline_destroy() - releases a sync object
 * @obj:    sync_timeline obj
 *
 * The mtk_sync_timeline_destroy() function releases a sync object.
 * The remaining active points would be put into signaled list,
 * and their statuses are set to VENOENT.
 */
void mtk_sync_timeline_destroy(struct sync_timeline *obj);

/**
 * mtk_sync_timeline_inc() - increases timeline
 * @obj:    sync_timeline obj
 * @value:  the increment to a sync object
 *
 * The mtk_sync_timeline_inc() function increase the counter of @obj by @value
 * Each sync point contains a value. A sync point on a parent timeline transits
 * from active to signaled status when the counter of a timeline reaches
 * to that of a sync point.
 */
void mtk_sync_timeline_inc(struct sync_timeline *obj, u32 value);

/**
 * mtk_sync_fence_create() - create a fence
 * @obj:    sync_timeline obj
 * @data:   fence struct with its name and the number a sync point bears
 *
 * The mtk_sync_fence_create() function creates a new sync point with
 * @data->value,
 * and assign the sync point to a newly created fence named @data->name.
 * A file descriptor binded with the fence is stored in @data->fence.
 */
int mtk_sync_fence_create(struct sync_timeline *obj, struct fence_data *data);

#endif /* __KERNEL __ */

#endif /* _MTK_SYNC_H */
