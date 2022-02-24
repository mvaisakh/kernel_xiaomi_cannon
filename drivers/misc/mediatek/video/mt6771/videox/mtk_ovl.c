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


#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include "debug.h"

#include "disp_drv_log.h"
#include "disp_utils.h"

#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_clkmgr.h"
#include "disp_helper.h"
#include "disp_session.h"
#include "primary_display.h"
#include "disp_lowpower.h"

#include "m4u.h"
#include "m4u_port.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "ddp_manager.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "ddp_mmp.h"
#include "mtk_ovl.h"

#include "mtkfb_fence.h"
#include <linux/device.h>
#include <linux/pm_wakeup.h>

#include <linux/atomic.h>

#include "extd_platform.h"


struct wakeup_source mem_wk_lock;
static int is_context_inited;
static int ovl2mem_layer_num;

#ifdef MTKFB_M4U_SUPPORT
static int ovl2mem_use_m4u = 1;
#endif
static int ovl2mem_use_cmdq = CMDQ_ENABLE;

struct ovl2mem_path_context {
	int state;
	unsigned int session;
	unsigned int lcm_fps;
	int max_layer;
	int need_trigger_path;
	struct mutex lock;
	struct cmdqRecStruct *cmdq_handle_config;
	struct cmdqRecStruct *cmdq_handle_trigger;
	disp_path_handle dpmgr_handle;
	char *mutex_locker;
	cmdqBackupSlotHandle ovl2mem_cur_config_fence;
	cmdqBackupSlotHandle ovl2mem_subtractor_when_free;
};

atomic_t g_trigger_ticket = ATOMIC_INIT(1);
atomic_t g_release_ticket = ATOMIC_INIT(1);

#ifdef pgc
#undef pgc
#endif
#define pgc	__get_context()

#define MEMORY_SESSION_ID 0x30002

static struct ovl2mem_path_context *__get_context(void)
{
	static struct ovl2mem_path_context g_context;

	if (!is_context_inited) {
		memset((void *)&g_context, 0, sizeof(g_context));
		mutex_init(&g_context.lock);
		is_context_inited = 1;
		wakeup_source_init(&mem_wk_lock, "mem_disp_wakelock");
	}

	return &g_context;
}

enum CMDQ_SWITCH ovl2mem_cmdq_enabled(void)
{
	return ovl2mem_use_cmdq;
}

void ovl2mem_path_lock(const char *caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&pgc->lock);
	pgc->mutex_locker = (char *)caller;
}

void ovl2mem_path_unlock(const char *caller)
{
	pgc->mutex_locker = NULL;
	disp_sw_mutex_unlock(&pgc->lock);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

void ovl2mem_context_init(void)
{
	is_context_inited = 0;
	ovl2mem_layer_num = 0;
}

void ovl2mem_setlayernum(int layer_num)
{
	ovl2mem_layer_num = layer_num;
}

int ovl2mem_get_info(void *info)
{
	int size;
	struct disp_session_info *dispif_info;

	dispif_info = (struct disp_session_info *)info;
	memset((void *)dispif_info, 0, sizeof(*dispif_info));

	/*
	 * FIXME: for decouple mode, should dynamic return 4 or 8,
	 * please refer to primary_display_get_info()
	 */
	dispif_info->maxLayerNum = ovl2mem_layer_num;
	dispif_info->displayType = DISP_IF_TYPE_DPI;
	dispif_info->displayMode = DISP_IF_MODE_VIDEO;
	dispif_info->isHwVsyncAvailable = 1;
	dispif_info->displayFormat = DISP_IF_FORMAT_RGB888;

	dispif_info->displayWidth = primary_display_get_width();
	dispif_info->displayHeight = primary_display_get_height();

	dispif_info->vsyncFPS = pgc->lcm_fps;

	size = dispif_info->displayWidth * dispif_info->displayHeight;

	if (size <= 240 * 432)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (size <= 320 * 480)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else if (size <= 480 * 854)
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;
	else
		dispif_info->physicalHeight = dispif_info->physicalWidth = 0;

	dispif_info->isConnected = 1;

	return 0;
}

void *mtk_ovl_get_dpmgr_handle(void)
{
	return pgc->dpmgr_handle;
}

static int _convert_disp_input_to_ovl(struct OVL_CONFIG_STRUCT *dst,
				      struct disp_input_config *src)
{
	int ret = 0;
	int force_disable_alpha = 0;
	enum UNIFIED_COLOR_FMT tmp_fmt;
	unsigned int Bpp = 0;

	if (!src || !dst) {
		DISPERR("%s src(0x%p) or dst(0x%p) is null\n",
			__func__, src, dst);
		return -1;
	}

	dst->layer = src->layer_id;
	dst->isDirty = 1;
	dst->buff_idx = src->next_buff_idx;
	dst->layer_en = src->layer_enable;

	/* if layer is disable, we just need to config above params. */
	if (!src->layer_enable)
		return 0;

	tmp_fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	/*
	 * display does not support X channel, like XRGB8888
	 * we need to enable const_bld
	 */
	ufmt_disable_X_channel(tmp_fmt, &dst->fmt, &dst->const_bld);
#if 0
	if (tmp_fmt != dst->fmt)
		force_disable_alpha = 1;
#endif
	Bpp = UFMT_GET_Bpp(dst->fmt);

	dst->addr = (unsigned long)(src->src_phy_addr);
	dst->vaddr = (unsigned long)(src->src_base_addr);
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->src_pitch = src->src_pitch * Bpp;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;

	/* dst W/H should <= src W/H */
	dst->dst_w = min(src->src_width, src->tgt_width);
	dst->dst_h = min(src->src_height, src->tgt_height);

	dst->keyEn = src->src_use_color_key;
	dst->key = src->src_color_key;

	dst->aen = force_disable_alpha ? 0 : src->alpha_enable;
	dst->sur_aen = force_disable_alpha ? 0 : src->sur_aen;

	dst->alpha = src->alpha;
	dst->src_alpha = src->src_alpha;
	dst->dst_alpha = src->dst_alpha;

	dst->identity = src->identity;
	dst->connected_type = src->connected_type;
	dst->security = src->security;
	dst->yuv_range = src->yuv_range;

	if (src->buffer_source == DISP_BUFFER_ALPHA) {
		/* dim layer, constant alpha */
		dst->source = OVL_LAYER_SOURCE_RESERVED;
	} else if (src->buffer_source == DISP_BUFFER_ION ||
		   src->buffer_source == DISP_BUFFER_MVA) {
		dst->source = OVL_LAYER_SOURCE_MEM; /* from memory */
	} else {
		DISPERR("unknown source = %d", src->buffer_source);
		dst->source = OVL_LAYER_SOURCE_MEM;
	}

	dst->ext_sel_layer = src->ext_sel_layer;

	return ret;
}

static int ovl2mem_callback(unsigned int userdata)
{
	int fence_idx = 0;
	int layid = 0;
	int subtractor = 0;

	DISPFUNC();

	ovl2mem_path_lock(__func__);

	DISPDBG("%s(0x%08x), current tick=%d, release tick: %d\n", __func__,
		pgc->session, get_ovl2mem_ticket(), userdata);
	for (layid = 0; layid < MEMORY_SESSION_INPUT_LAYER_COUNT; layid++) {
		cmdqBackupReadSlot(pgc->ovl2mem_cur_config_fence,
				layid, &fence_idx);
		cmdqBackupReadSlot(pgc->ovl2mem_subtractor_when_free,
				layid, &subtractor);

		mtkfb_release_fence(pgc->session, layid,
				fence_idx - subtractor);
	}

	layid = disp_sync_get_output_timeline_id();
	fence_idx = mtkfb_query_idx_by_ticket(pgc->session, layid, userdata);
	if (fence_idx >= 0) {
		if (pgc->dpmgr_handle) {
			struct disp_ddp_path_config *data_config =
				dpmgr_path_get_last_config(pgc->dpmgr_handle);

			if (data_config) {
				struct WDMA_CONFIG_STRUCT wdma_layer;

				wdma_layer.idx = 0;
				wdma_layer.dstAddress =
					mtkfb_query_buf_mva(pgc->session,
					layid, fence_idx);
				wdma_layer.outputFormat =
					data_config->wdma_config.outputFormat;
				wdma_layer.srcWidth =
					data_config->wdma_config.srcWidth;
				wdma_layer.srcHeight =
					data_config->wdma_config.srcHeight;
				wdma_layer.dstPitch =
					data_config->wdma_config.dstPitch;

				dprec_mmp_dump_wdma_layer(&wdma_layer, 1);
			}
		}

		mtkfb_release_fence(pgc->session, layid, fence_idx);
	}

	/* Update ticket */
	atomic_set(&g_release_ticket, userdata);
	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_PULSE, 0x05,
			 (atomic_read(&g_trigger_ticket) << 16) |
			 atomic_read(&g_release_ticket));

	ovl2mem_path_unlock(__func__);
	DISPINFO("%s done\n", __func__);

	return 0;
}

int get_ovl2mem_ticket(void)
{
	return atomic_read(&g_trigger_ticket);

}

static int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val)
{
	int i;

	cmdqBackupAllocateSlot(pSlot, count);

	for (i = 0; i < count; i++)
		cmdqBackupWriteSlot(*pSlot, i, init_val);

	return 0;
}

static void deinit_cmdq_slots(cmdqBackupSlotHandle hSlot)
{
	cmdqBackupFreeSlot(hSlot);
}

static int ovl2mem_cmdq_dump(uint64_t engineFlag, int level)
{
	DISPFUNC();

	if (pgc->dpmgr_handle != NULL)
		dpmgr_check_status(pgc->dpmgr_handle);
	else
		DISPMSG("ovl2mem dpmgr_handle == NULL\n");

	return 0;
}

int ovl2mem_init(unsigned int session)
{
	int ret = -1;
#ifdef MTKFB_M4U_SUPPORT
	struct M4U_PORT_STRUCT sPort;
#endif
	DISPFUNC();

	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_PULSE, 0x01, 0);

	dpmgr_init();

	ovl2mem_path_lock(__func__);

	if (pgc->state > 0) {
		DISPERR("path has created, state%d\n", pgc->state);
		goto exit;
	}

	if (pgc->state == 0) {
		init_cmdq_slots(&(pgc->ovl2mem_cur_config_fence),
				MEMORY_SESSION_INPUT_LAYER_COUNT, 0);
		init_cmdq_slots(&(pgc->ovl2mem_subtractor_when_free),
				MEMORY_SESSION_INPUT_LAYER_COUNT, 0);
	}

	/* Register memory session cmdq dump callback */
	dpmgr_register_cmdq_dump_callback(ovl2mem_cmdq_dump);

	if (!pgc->cmdq_handle_config) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP,
				    &pgc->cmdq_handle_config);
		if (ret) {
			DISPERR("cmdqRecCreate FAIL, ret=%d\n", ret);
			goto exit;
		} else {
			DISPDBG("cmdqRecCreate SUCCESS, cmdq_handle=%p\n",
				pgc->cmdq_handle_config);
		}
	}
	/* Set fake cmdq engineflag for judge path scenario */
	cmdqRecSetEngine(pgc->cmdq_handle_config,
			 (1LL << CMDQ_ENG_DISP_2L_OVL1) |
			 (1LL << CMDQ_ENG_DISP_WDMA0));

	cmdqRecReset(pgc->cmdq_handle_config);
	cmdqRecClearEventToken(pgc->cmdq_handle_config,
			       CMDQ_EVENT_DISP_WDMA0_EOF);

	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_OVL_MEMOUT,
					      pgc->cmdq_handle_config);

	if (pgc->dpmgr_handle) {
		DISPDBG("dpmgr create path SUCCESS(%p)\n", pgc->dpmgr_handle);
	} else {
		DISPERR("dpmgr create path FAIL\n");
		goto exit;
	}

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, false);

	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);

#ifdef MTKFB_M4U_SUPPORT
	sPort.ePortID = M4U_PORT_DISP_2L_OVL1_LARB0;
	sPort.Virtuality = ovl2mem_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret == 0) {
		DISPDBG("config M4U Port %s to %s SUCCESS\n",
			ddp_get_module_name(DISP_MODULE_OVL1_2L),
			ovl2mem_use_m4u ? "virtual" : "physical");
	} else {
		DISPERR("config M4U Port %s to %s FAIL(ret=%d)\n",
			ddp_get_module_name(DISP_MODULE_OVL1_2L),
			ovl2mem_use_m4u ? "virtual" : "physical", ret);
		goto exit;
	}

	sPort.ePortID = M4U_PORT_DISP_WDMA0;
	sPort.Virtuality = ovl2mem_use_m4u;
	sPort.Security = 0;
	sPort.Distance = 1;
	sPort.Direction = 0;
	ret = m4u_config_port(&sPort);
	if (ret == 0) {
		DISPDBG("config M4U Port %s to %s SUCCESS\n",
			ddp_get_module_name(DISP_MODULE_WDMA0),
			ovl2mem_use_m4u ? "virtual" : "physical");
	} else {
		DISPERR("config M4U Port %s to %s FAIL(ret=%d)\n",
			ddp_get_module_name(DISP_MODULE_WDMA0),
			ovl2mem_use_m4u ? "virtual" : "physical", ret);
		goto exit;
	}
#endif
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_COMPLETE);

	pgc->max_layer = 4;
	pgc->state = 1;
	pgc->session = session;
	atomic_set(&g_trigger_ticket, 1);
	atomic_set(&g_release_ticket, 0);
	__pm_stay_awake(&mem_wk_lock);

exit:
	ovl2mem_path_unlock(__func__);
	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_PULSE, 0x01, 1);

	DISPMSG("%s done\n", __func__);

	return ret;
}

int ovl2mem_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = -1;

	DISPFUNC();

	if (pgc->need_trigger_path == 0) {
		DISPMSG("%s do not trigger\n", __func__);
		DISPMSG("%s(%x), configue input, but does not config output!!\n",
				__func__, pgc->session);
		return ret;
	}

	cmdqRecClearEventToken(pgc->cmdq_handle_config,
			       CMDQ_SYNC_DISP_EXT_STREAM_EOF);
	cmdqRecClearEventToken(pgc->cmdq_handle_config,
			       CMDQ_EVENT_DISP_WDMA0_EOF);
	dpmgr_path_start(pgc->dpmgr_handle, ovl2mem_cmdq_enabled());

	dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_config,
			   ovl2mem_cmdq_enabled());

	cmdqRecWait(pgc->cmdq_handle_config, CMDQ_EVENT_DISP_WDMA0_SOF);
	cmdqRecWait(pgc->cmdq_handle_config, CMDQ_EVENT_DISP_WDMA0_EOF);
	cmdqRecSetEventToken(pgc->cmdq_handle_config,
			     CMDQ_SYNC_DISP_EXT_STREAM_EOF);
	dpmgr_path_stop(pgc->dpmgr_handle, ovl2mem_cmdq_enabled());

	/* cmdqRecDumpCommand(pgc->cmdq_handle_config); */

	cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config,
				  (CmdqAsyncFlushCB)ovl2mem_callback,
				  atomic_read(&g_trigger_ticket));

	cmdqRecReset(pgc->cmdq_handle_config);

	pgc->need_trigger_path = 0;
	atomic_add(1, &g_trigger_ticket);

	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_PULSE, 0x02,
			 (atomic_read(&g_trigger_ticket)<<16) |
			 atomic_read(&g_release_ticket));

	DISPINFO("%s done %d\n", __func__, get_ovl2mem_ticket());

	return ret;
}

static int ovl2mem_frame_cfg_input(struct disp_frame_cfg_t *cfg)
{
	int ret = -1;
	int i = 0;
	int config_layer_id = 0;
	struct disp_ddp_path_config *data_config;
	struct ddp_io_golden_setting_arg gset_arg;
	unsigned int ext_last_fence, ext_cur_fence, ext_sub;

	DISPFUNC();

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->p_golden_setting_context = get_golden_setting_pgc();

	/* hope we can use only 1 input struct for input config,
	 * just set layer number
	 */
	for (i = 0; i < cfg->input_layer_num; i++) {
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG,
			cfg->input_cfg[i].layer_id |
			(cfg->input_cfg[i].layer_enable << 16),
			(unsigned long)(cfg->input_cfg[i].src_phy_addr));

		config_layer_id = cfg->input_cfg[i].layer_id;
		_convert_disp_input_to_ovl(
			&(data_config->ovl_config[config_layer_id]),
			&(cfg->input_cfg[i]));
		dprec_mmp_dump_ovl_layer(
			&(data_config->ovl_config[config_layer_id]),
			config_layer_id, 3);

		data_config->ovl_dirty = 1;
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG,
			cfg->input_cfg[i].src_offset_x,
			cfg->input_cfg[i].src_offset_y);
	}

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
			DISP_PATH_EVENT_FRAME_COMPLETE, HZ / 5);

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
		pgc->cmdq_handle_config);

	memset(&gset_arg, 0, sizeof(gset_arg));
	gset_arg.dst_mod_type =
			dpmgr_path_get_dst_module_type(pgc->dpmgr_handle);
	gset_arg.is_decouple_mode = 1;

	dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config,
			 DDP_OVL_GOLDEN_SETTING, &gset_arg);

	for (i = 0; i < cfg->input_layer_num; i++) {
		cmdqBackupReadSlot(pgc->ovl2mem_cur_config_fence, i,
			&ext_last_fence);
		ext_cur_fence = cfg->input_cfg[i].next_buff_idx;

		if (ext_cur_fence != -1 && ext_cur_fence > ext_last_fence) {
			cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
				pgc->ovl2mem_cur_config_fence,
				i, ext_cur_fence);
		}

		/*
		 * there's no need keep last layer fence for memory session,
		 * it's due to memory session, would not switch path
		 * during memory session
		 */
		ext_sub = 0;

		cmdqRecBackupUpdateSlot(pgc->cmdq_handle_config,
			pgc->ovl2mem_subtractor_when_free,
			i, ext_sub);
	}

	DISPINFO("ovl2mem_input_config done\n");
	return ret;
}

static int ovl2mem_frame_cfg_output(struct disp_frame_cfg_t *cfg)
{
	int ret = -1;
	unsigned int dst_mva = 0;
	struct disp_ddp_path_config *data_config;
	unsigned int session_id = cfg->session_id;

	DISPFUNC();

	if (cfg->output_cfg.pa) {
		dst_mva = (unsigned long)(cfg->output_cfg.pa);
	} else {
		dst_mva = mtkfb_query_buf_mva(session_id,
				disp_sync_get_output_timeline_id(),
				(unsigned int)(cfg->output_cfg.buff_idx));
	}

	/* Update output buffer ticket */
	mtkfb_update_buf_ticket(session_id,
				disp_sync_get_output_timeline_id(),
				cfg->output_cfg.buff_idx,
				get_ovl2mem_ticket());

	/* all dirty should be cleared in dpmgr_path_get_last_config() */
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config->dst_dirty = 1;
	data_config->dst_h = cfg->output_cfg.height;
	data_config->dst_w = cfg->output_cfg.width;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 1;
	/* set_overlay will not use fence+ion handle */
#if defined(MTK_FB_ION_SUPPORT)
	if (cfg->output_cfg.pa != NULL)
		data_config->wdma_config.dstAddress =
					(unsigned long)(cfg->output_cfg.pa);
	else
		data_config->wdma_config.dstAddress = (unsigned long)dst_mva;

#else
	data_config->wdma_config.dstAddress = (unsigned long)cfg->output_cfg.pa;
#endif
	data_config->wdma_config.srcHeight = cfg->output_cfg.height;
	data_config->wdma_config.srcWidth = cfg->output_cfg.width;
	data_config->wdma_config.clipX = cfg->output_cfg.x;
	data_config->wdma_config.clipY = cfg->output_cfg.y;
	data_config->wdma_config.clipHeight = cfg->output_cfg.height;
	data_config->wdma_config.clipWidth = cfg->output_cfg.width;
	data_config->wdma_config.outputFormat =
				disp_fmt_to_unified_fmt(cfg->output_cfg.fmt);
	data_config->wdma_config.dstPitch =
					cfg->output_cfg.pitch * UFMT_GET_Bpp(
					data_config->wdma_config.outputFormat);
	data_config->wdma_config.useSpecifiedAlpha = 1;
	data_config->wdma_config.alpha = 0xFF;
	data_config->wdma_config.security = cfg->output_cfg.security;
	data_config->p_golden_setting_context = get_golden_setting_pgc();

	if (dpmgr_path_is_busy(pgc->dpmgr_handle))
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_DONE, HZ / 5);

	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config,
				pgc->cmdq_handle_config);

	pgc->need_trigger_path = 1;

	return ret;
}

int ovl2mem_frame_cfg(struct disp_frame_cfg_t *cfg)
{
	int ret = 0;
	unsigned int session_id = 0;
	struct disp_session_sync_info *session_info;
	struct dprec_logger_event *input_event, *output_event, *trigger_event;

	session_info = disp_get_session_sync_info_for_debug(cfg->session_id);
	ovl2mem_path_lock(__func__);

	if (pgc->state == 0) {
		DISPERR("ovl2mem is already slept\n");
		ovl2mem_path_unlock(__func__);
		return 0;
	}

	session_id = cfg->session_id;

	if (session_info) {
		input_event = &session_info->event_setinput;
		output_event = &session_info->event_setoutput;
		trigger_event = &session_info->event_trigger;
	} else {
		input_event = output_event = trigger_event = NULL;
	}

	/* set input */
	dprec_start(input_event, cfg->overlap_layer_num, cfg->input_layer_num);
	ovl2mem_frame_cfg_input(cfg);
	dprec_done(input_event, 0, 0);

	if (cfg->output_en) {
		dprec_start(output_event, cfg->output_cfg.buff_idx, 0);
		ovl2mem_frame_cfg_output(cfg);
		dprec_done(output_event, 0, 0);
	}

	if (trigger_event) {
		/* to debug UI thread or MM thread */
		unsigned int proc_name = (current->comm[0] << 24) |
			    (current->comm[1] << 16) | (current->comm[2] << 8) |
			    (current->comm[3] << 0);
		dprec_start(trigger_event, proc_name, 0);
	}
	DISPPR_FENCE("T+/M%d/t%d\n", DISP_SESSION_DEV(session_id),
		  get_ovl2mem_ticket());
	ovl2mem_trigger(0, NULL, 0);

	dprec_done(trigger_event, 0, 0);

	ovl2mem_path_unlock(__func__);
	return ret;

}

int ovl2mem_get_max_layer(void)
{
	return MEMORY_SESSION_INPUT_LAYER_COUNT;
}

void ovl2mem_wait_done(void)
{
	int loop_cnt = 0;

	if ((atomic_read(&g_trigger_ticket) -
	     atomic_read(&g_release_ticket)) <= 1)
		return;

	DISPFUNC();

	while ((atomic_read(&g_trigger_ticket) -
		atomic_read(&g_release_ticket)) > 1) {
		dpmgr_wait_event_timeout(pgc->dpmgr_handle,
					 DISP_PATH_EVENT_FRAME_COMPLETE,
					 HZ / 30);

		if (loop_cnt > 5)
			break;

		loop_cnt++;
	}

	DISPINFO("%s loop %d, trigger tick:%d, release tick:%d\n",
		 __func__, loop_cnt, atomic_read(&g_trigger_ticket),
		 atomic_read(&g_release_ticket));
}

int ovl2mem_deinit(void)
{
	int ret = -1;
	int loop_cnt = 0;
	int i = 0;

	DISPFUNC();

	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_START, 0x03,
			 (atomic_read(&g_trigger_ticket)<<16) |
			 atomic_read(&g_release_ticket));

	ovl2mem_path_lock(__func__);

	if (pgc->state == 0) {
		DISPERR("path exit, state%d\n", pgc->state);
		goto Exit;
	}

	/* ovl2mem_wait_done(); */
	ovl2mem_layer_num = 0;
	while (((atomic_read(&g_trigger_ticket) -
		atomic_read(&g_release_ticket)) != 1) && (loop_cnt < 10)) {
		ovl2mem_path_unlock(__func__);
		usleep_range(5000, 6000);
		ovl2mem_path_lock(__func__);
		/* wait the last configuration done */
		loop_cnt++;
	}
	if (loop_cnt >= 10)
		DISPMSG("%s loop_cnt >= 10, g_trigger_ticket=%d, g_release_ticket=%d\n",
				__func__,
				atomic_read(&g_trigger_ticket),
				atomic_read(&g_release_ticket));

	/*[SVP]switch ddp mosule to nonsec when deinit the extension path*/
	switch_module_to_nonsec(pgc->dpmgr_handle,
			NULL, DISP_MODULE_NUM, __func__);

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);

	dpmgr_destroy_path_handle(pgc->dpmgr_handle);
	cmdqRecDestroy(pgc->cmdq_handle_config);

	DISPMSG("ovl2mem_release_all_fence");
	/* release input layer all fence */
	for (i = 0; i < MEMORY_SESSION_INPUT_LAYER_COUNT; i++)
		mtkfb_release_layer_fence(pgc->session, i);
	/* release output layer all fence */
	mtkfb_release_layer_fence(pgc->session,
			disp_sync_get_output_timeline_id());

	if (pgc->state == 1) {
		deinit_cmdq_slots(pgc->ovl2mem_cur_config_fence);
		deinit_cmdq_slots(pgc->ovl2mem_subtractor_when_free);
	}

	/* Unregister memory session cmdq dump callback */
	dpmgr_unregister_cmdq_dump_callback(ovl2mem_cmdq_dump);

	pgc->dpmgr_handle = NULL;
	pgc->cmdq_handle_config = NULL;
	pgc->state = 0;
	pgc->need_trigger_path = 0;
	atomic_set(&g_trigger_ticket, 1);
	atomic_set(&g_release_ticket, 0);
	__pm_relax(&mem_wk_lock);

Exit:
	ovl2mem_path_unlock(__func__);
	mmprofile_log_ex(ddp_mmp_get_events()->ovl_trigger,
			 MMPROFILE_FLAG_END, 0x03, (loop_cnt<<24)|1);

	DISPMSG("%s done\n", __func__);
	return ret;
}
