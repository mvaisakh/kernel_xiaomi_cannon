/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __GED_BRIDGE_H__
#define __GED_BRIDGE_H__

#include "ged_base.h"
#include "ged_log.h"
#include "ged_type.h"

#include "ged_bridge_id.h"


/*****************************************************************************
 *  BRIDGE FUNCTIONS
 *****************************************************************************/

int ged_bridge_log_buf_get(
	struct GED_BRIDGE_IN_LOGBUFGET *psLogBufGetIN,
	struct GED_BRIDGE_OUT_LOGBUFGET *psLogBufGetOUT);

int ged_bridge_log_buf_write(
	struct GED_BRIDGE_IN_LOGBUFWRITE *psLogBufWriteIN,
	struct GED_BRIDGE_OUT_LOGBUFWRITE *psLogBufWriteOUT);

int ged_bridge_log_buf_reset(
	struct GED_BRIDGE_IN_LOGBUFRESET *psLogBufResetIn,
	struct GED_BRIDGE_OUT_LOGBUFRESET *psLogBufResetOUT);

int ged_bridge_boost_gpu_freq(
	struct GED_BRIDGE_IN_BOOSTGPUFREQ *psBoostGpuFreqIN,
	struct GED_BRIDGE_OUT_BOOSTGPUFREQ *psBoostGpuFreqOUT);

int ged_bridge_monitor_3D_fence(
	struct GED_BRIDGE_IN_MONITOR3DFENCE *psMonitor3DFenceINT,
	struct GED_BRIDGE_OUT_MONITOR3DFENCE *psMonitor3DFenceOUT);

int ged_bridge_query_info(
	struct GED_BRIDGE_IN_QUERY_INFO *psQueryInfoINT,
	struct GED_BRIDGE_OUT_QUERY_INFO *psQueryInfoOUT);

int ged_bridge_notify_vsync(
	struct GED_BRIDGE_IN_NOTIFY_VSYNC *psNotifyVsyncINT,
	struct GED_BRIDGE_OUT_NOTIFY_VSYNC *psNotifyVsyncOUT);

int ged_bridge_dvfs_probe(
	struct GED_BRIDGE_IN_DVFS_PROBE *psDVFSProbeINT,
	struct GED_BRIDGE_OUT_DVFS_PROBE *psDVFSProbeOUT);

int ged_bridge_dvfs_um_retrun(
	struct GED_BRIDGE_IN_DVFS_UM_RETURN *psDVFS_UM_returnINT,
	struct GED_BRIDGE_OUT_DVFS_UM_RETURN *psDVFS_UM_returnOUT);

int ged_bridge_event_notify(
	struct GED_BRIDGE_IN_EVENT_NOTIFY *psEVENT_NOTIFYINT,
	struct GED_BRIDGE_OUT_EVENT_NOTIFY *psEVENT_NOTIFYOUT);

int ged_bridge_gpu_hint_to_cpu(
	struct GED_BRIDGE_IN_GPU_HINT_TO_CPU *in,
	struct GED_BRIDGE_OUT_GPU_HINT_TO_CPU *out);

int ged_bridge_hint_force_mdp(
	struct GED_BRIDGE_IN_HINT_FORCE_MDP *psHintForceMdpIn,
	struct GED_BRIDGE_OUT_HINT_FORCE_MDP *psHintForceMdpOut);

int ged_bridge_gpu_timestamp(
	struct GED_BRIDGE_IN_GPU_TIMESTAMP *psGpuBeginINT,
	struct GED_BRIDGE_OUT_GPU_TIMESTAMP *psGpuBeginOUT);

int ged_bridge_query_dvfs_freq_pred(
	struct GED_BRIDGE_IN_QUERY_DVFS_FREQ_PRED *QueryDVFSFreqPredIn,
	struct GED_BRIDGE_OUT_QUERY_DVFS_FREQ_PRED *QueryDVFSFreqPredOut);

int ged_bridge_ge_alloc(
	struct GED_BRIDGE_IN_GE_ALLOC *psALLOC_IN,
	struct GED_BRIDGE_OUT_GE_ALLOC *psALLOC_OUT);

int ged_bridge_ge_get(
	struct GED_BRIDGE_IN_GE_GET *psGET_IN,
	struct GED_BRIDGE_OUT_GE_GET *psGET_OUT,
	int output_package_size);

int ged_bridge_ge_set(
	struct GED_BRIDGE_IN_GE_SET *psSET_IN,
	struct GED_BRIDGE_OUT_GE_SET *psSET_OUT,
	int input_package_size);

int ged_bridge_ge_info(
	struct GED_BRIDGE_IN_GE_INFO *psINFO_IN,
	struct GED_BRIDGE_OUT_GE_INFO *psINFO_OUT);

int ged_bridge_gpu_tuner_status(
	struct GED_BRIDGE_IN_GPU_TUNER_STATUS *in,
	struct GED_BRIDGE_OUT_GPU_TUNER_STATUS *out);

#ifdef ENABLE_FRR_FOR_MT6XXX_PLATFORM
int ged_bridge_vsync_wait(void *IN, void *OUT);
#endif

#endif
