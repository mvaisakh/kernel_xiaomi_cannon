/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
//#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "smi_public.h"
#include "mt6853/smi_port.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#endif

#include "swpm_me.h"

#if ENC_DVFS
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 249
#define STD_LUMA_BW 70
#define STD_CHROMA_BW 35
static struct pm_qos_request venc_qos_req_f;
static u64 venc_freq;
static u32 venc_freq_step_size;
static u64 venc_freq_steps[MAX_FREQ_STEP];
static struct codec_history *venc_hists;
static struct codec_job *venc_jobs;
#endif

#if ENC_EMI_BW
#include <mtk_smi.h>
#include <mtk_qos_bound.h>
#include "vcodec_bw.h"
static long long venc_start_time;
static struct vcodec_bw *venc_bw;
static struct plist_head venc_rlist;
static struct mm_qos_request venc_rcpu;
static struct mm_qos_request venc_rec;
static struct mm_qos_request venc_bsdma;
static struct mm_qos_request venc_sv_comv;
static struct mm_qos_request venc_rd_comv;
static struct mm_qos_request venc_cur_luma;
static struct mm_qos_request venc_cur_chroma;
static struct mm_qos_request venc_ref_luma;
static struct mm_qos_request venc_ref_chroma;
struct pm_qos_request venc_qos_req_bw;
#endif

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
	ctx->async_mode = 0;
}

int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
	int ret = 0;
#ifndef FPGA_PWRCLK_API_DISABLE
	struct device_node *node;
	struct platform_device *pdev;
	struct device *dev;
	struct mtk_vcodec_pm *pm;

	pdev = mtkdev->plat_dev;
	pm = &mtkdev->pm;
	memset(pm, 0, sizeof(struct mtk_vcodec_pm));
	pm->mtkdev = mtkdev;
	pm->dev = &pdev->dev;
	dev = &pdev->dev;

	pm->chip_node = of_find_compatible_node(NULL,
		NULL, "mediatek,mt6853-vcodec-enc");
	node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!node) {
		mtk_v4l2_err("no mediatek,larb found");
		return -1;
	}
	pdev = of_find_device_by_node(node);
	if (!pdev) {
		mtk_v4l2_err("no mediatek,larb device found");
		return -1;
	}
	pm->larbvenc = &pdev->dev;

	pdev = mtkdev->plat_dev;
	pm->dev = &pdev->dev;

	pm->clk_MT_CG_VENC = devm_clk_get(&pdev->dev, "MT_CG_VENC");
	if (IS_ERR(pm->clk_MT_CG_VENC)) {
		mtk_v4l2_err("[VCODEC][ERROR] Unable to devm_clk_get MT_CG_VENC\n");
		ret = PTR_ERR(pm->clk_MT_CG_VENC);
	}
#endif
	return ret;
}

void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *mtkdev)
{
	//free_all_bw(&venc_bw);
}

void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx)
{
}

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id)
{
#ifdef CONFIG_MTK_PSEUDO_M4U
	int i, larb_port_num, larb_id;
	struct M4U_PORT_STRUCT port;
#endif
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;
	int ret;

	smi_bus_prepare_enable(SMI_LARB7, "VENC");
	ret = clk_prepare_enable(pm->clk_MT_CG_VENC);
	if (ret)
		mtk_v4l2_err("clk_prepare_enable CG_VENC fail %d", ret);

	set_swpm_venc_active(true);
#endif

#ifdef CONFIG_MTK_PSEUDO_M4U
	time_check_start(MTK_FMT_ENC, core_id);
	if (core_id == MTK_VENC_CORE_0) {
		larb_port_num = SMI_LARB7_PORT_NUM;
		larb_id = 7;
	} else {
		larb_port_num = 0;
		larb_id = 0;
		mtk_v4l2_err("invalid core_id %d", core_id);
		time_check_end(MTK_FMT_ENC, core_id, 50);
		return;
	}

	//enable 34bits port configs & sram settings
	for (i = 0; i < larb_port_num; i++) {
		port.ePortID = MTK_M4U_ID(larb_id, i);
		port.Direction = 0;
		port.Distance = 1;
		port.domain = 0;
		port.Security = 0;
		port.Virtuality = 1;
		m4u_config_port(&port);
	}
	time_check_end(MTK_FMT_ENC, core_id, 50);
#endif
}

void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id)
{
#ifndef FPGA_PWRCLK_API_DISABLE
	struct mtk_vcodec_pm *pm = &ctx->dev->pm;

	set_swpm_venc_active(false);
	clk_disable_unprepare(pm->clk_MT_CG_VENC);
	smi_bus_disable_unprepare(SMI_LARB7, "VENC");
#endif
}


void mtk_prepare_venc_dvfs(void)
{
#if ENC_DVFS
	int ret;

	pm_qos_add_request(&venc_qos_req_f, PM_QOS_VENC_FREQ,
				PM_QOS_DEFAULT_VALUE);
	venc_freq_step_size = 1;
	ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ, &venc_freq_steps[0],
					&venc_freq_step_size);
	if (ret < 0)
		pr_debug("Failed to get venc freq steps (%d)\n", ret);
#endif
}

void mtk_unprepare_venc_dvfs(void)
{
#if ENC_DVFS
	int freq_idx = 0;

	freq_idx = (venc_freq_step_size == 0) ? 0 : (venc_freq_step_size - 1);
	pm_qos_update_request(&venc_qos_req_f, venc_freq_steps[freq_idx]);
	pm_qos_remove_request(&venc_qos_req_f);
	free_hist(&venc_hists, 0);
	/* TODO: jobs error handle */
#endif
}

void mtk_release_pmqos(struct mtk_vcodec_ctx *ctx)
{
#if ENC_DVFS
	free_hist_by_handle(&ctx->id, &venc_hists);
#endif
}

void mtk_prepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	plist_head_init(&venc_rlist);
	mm_qos_add_request(&venc_rlist, &venc_rcpu, M4U_PORT_L7_VENC_RCPU);
	mm_qos_add_request(&venc_rlist, &venc_rec, M4U_PORT_L7_VENC_REC);
	mm_qos_add_request(&venc_rlist, &venc_bsdma, M4U_PORT_L7_VENC_BSDMA);
	mm_qos_add_request(&venc_rlist, &venc_sv_comv,
					M4U_PORT_L7_VENC_SV_COMV);
	mm_qos_add_request(&venc_rlist, &venc_rd_comv,
					M4U_PORT_L7_VENC_RD_COMV);
	mm_qos_add_request(&venc_rlist, &venc_cur_luma,
					M4U_PORT_L7_VENC_CUR_LUMA);
	mm_qos_add_request(&venc_rlist, &venc_cur_chroma,
					M4U_PORT_L7_VENC_CUR_CHROMA);
	mm_qos_add_request(&venc_rlist, &venc_ref_luma,
					M4U_PORT_L7_VENC_REF_LUMA);
	mm_qos_add_request(&venc_rlist, &venc_ref_chroma,
					M4U_PORT_L7_VENC_REF_CHROMA);
#endif
}

void mtk_unprepare_venc_emi_bw(void)
{
#if ENC_EMI_BW
	pm_qos_remove_request(&venc_qos_req_bw);
#endif
}


void mtk_venc_dvfs_begin(struct mtk_vcodec_ctx *ctx)
{
#if ENC_DVFS
	int target_freq = 0;
	u64 target_freq_64 = 0;
	struct codec_job *venc_cur_job = 0;

	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = move_job_to_head(&ctx->id, &venc_jobs);
	if (venc_cur_job != 0) {
		venc_cur_job->start = get_time_us();
		target_freq = est_freq(venc_cur_job->handle, &venc_jobs,
					venc_hists);
		target_freq_64 = match_freq(target_freq, &venc_freq_steps[0],
					venc_freq_step_size);

		if (ctx->async_mode == 1 && target_freq_64 > 458)
			target_freq_64 = 458;

		if (target_freq > 0) {
			venc_freq = target_freq;
			if (venc_freq > target_freq_64)
				venc_freq = target_freq_64;

			venc_cur_job->mhz = (int)target_freq_64;
			pm_qos_update_request(&venc_qos_req_f, target_freq_64);
		}
	} else {
		target_freq_64 = match_freq(DEFAULT_MHZ, &venc_freq_steps[0],
						venc_freq_step_size);
		pm_qos_update_request(&venc_qos_req_f, target_freq_64);
	}
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

#if ENC_DVFS
static int mtk_venc_get_exec_cnt(struct mtk_vcodec_ctx *ctx)
{
	return (int)((readl(ctx->dev->enc_reg_base[VENC_SYS] + 0x17C) &
			0x7FFFFFFF) / 1000);
}
#endif

void mtk_venc_dvfs_end(struct mtk_vcodec_ctx *ctx)
{
#if ENC_DVFS
	int freq_idx = 0;
	long long interval = 0;
	struct codec_job *venc_cur_job = 0;

	/* venc dvfs */
	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	venc_cur_job = venc_jobs;
	if (venc_cur_job != 0 && (venc_cur_job->handle == &ctx->id)) {
		venc_cur_job->end = get_time_us();
		if (ctx->async_mode == 0) {
			if ((ctx->enc_params.operationrate == 60 ||
				((ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom) >= 59 &&
				(ctx->enc_params.framerate_num /
				ctx->enc_params.framerate_denom) <= 60) ||
				(ctx->q_data[MTK_Q_DATA_SRC].visible_width
					== 3840 &&
				ctx->q_data[MTK_Q_DATA_SRC].visible_height
					== 2160))) {
				/* Set allowed time for 60fps/4K recording */
				/* Use clock cycle if single instance */
				if (venc_hists != 0 && venc_hists->next == 0) {
					venc_cur_job->hw_kcy =
						mtk_venc_get_exec_cnt(ctx);
				}
				if (ctx->enc_params.operationrate > 0) {
					interval = (long long)(1000 * 1000 /
					(int)ctx->enc_params.operationrate);
				} else {
					interval = (long long)(1000 * 1000 /
					(int)(ctx->enc_params.framerate_num /
					ctx->enc_params.framerate_denom));
				}
				update_hist(venc_cur_job, &venc_hists,
					interval);
			} else {
				update_hist(venc_cur_job, &venc_hists, 0);
			}
		} else {
			/* Set allowed time for slowmotion 4 buffer pack */
			interval = (long long)(1000 * 1000 * 4 /
					(int)ctx->enc_params.operationrate);
			update_hist(venc_cur_job, &venc_hists, interval);
		}
		venc_jobs = venc_jobs->next;
		kfree(venc_cur_job);
	} else {
		/* print error log */
		pr_debug("no job at venc_dvfs_end, reset freq only");
	}

	freq_idx = (venc_freq_step_size == 0) ? 0 : (venc_freq_step_size - 1);
	pm_qos_update_request(&venc_qos_req_f, venc_freq_steps[freq_idx]);
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_emi_bw_begin(struct mtk_vcodec_ctx *ctx)
{
#if ENC_EMI_BW
	int f_type = 1; /* TODO */
	struct vcodec_bw *cur_bw = 0;
	int variable_bw = 0;
	int boost_perc = 0;
	int rcpu_bw = 2;
	int rec_bw = 0;
	int bsdma_bw = 10;
	int sv_comv_bw = 2;
	int rd_comv_bw = 8;
	int cur_luma_bw = 0;
	int cur_chroma_bw = 0;
	int ref_luma_bw = 0;
	int ref_chroma_bw = 0;

	cur_bw = add_bw_by_id(&venc_bw, ctx->id);

	if (ctx->async_mode == 1 ||
		ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265 ||
		(ctx->q_data[MTK_Q_DATA_SRC].visible_width == 3840 &&
		ctx->q_data[MTK_Q_DATA_SRC].visible_height == 2160)) {
		boost_perc = 100;
	} else if (ctx->enc_params.operationrate == 60) {
		boost_perc = 30;
	}

	/* Input BW scaling to venc_freq & config */
	cur_luma_bw = STD_LUMA_BW * venc_freq * (100 + boost_perc)
			/ STD_VENC_FREQ / 100;
	cur_chroma_bw = STD_CHROMA_BW * venc_freq * (100 + boost_perc)
			/ STD_VENC_FREQ / 100;

	if (validate_bw(cur_bw, f_type) == true) { /* Refer to prev frame */
		/* BW in STD_VENC_FREQ */
		if (cur_bw->smi_bw_mon[f_type] > 0) {
			variable_bw = cur_bw->smi_bw_mon[f_type] -
					 ((STD_LUMA_BW + STD_CHROMA_BW)
					* (100 + boost_perc) / 100);
		} else {
			variable_bw = (STD_LUMA_BW + STD_CHROMA_BW) * 5
					* (100 + boost_perc) / 100;
		}

		/* BW in venc_freq */
		if (f_type == 0) {
			rec_bw = variable_bw * venc_freq / STD_VENC_FREQ;
		} else {
			rec_bw = variable_bw * venc_freq / STD_VENC_FREQ / 5;
			ref_luma_bw = variable_bw * 8 * venc_freq /
					STD_VENC_FREQ / 15;
			ref_chroma_bw = variable_bw * 4 * venc_freq /
					STD_VENC_FREQ / 15;
		}
	} else { /* First frame estimate */
		/* BW in venc_freq */
		if (f_type == 0) {
			rec_bw = cur_luma_bw + cur_chroma_bw;
		} else {
			rec_bw = cur_luma_bw + cur_chroma_bw;
			ref_luma_bw = rec_bw * 3;
			ref_chroma_bw = ref_luma_bw / 2;
		}
	}

	if (1) { /* UFO */
		ref_chroma_bw += ref_luma_bw;
		ref_luma_bw = 0;
	}

	if (ctx->enc_params.operationrate == 60 ||
		ctx->async_mode == 1 ||
		(ctx->q_data[MTK_Q_DATA_SRC].visible_width == 3840 &&
		ctx->q_data[MTK_Q_DATA_SRC].visible_height == 2160)) {
		/* 60fps / 4K30 / SMVR  recording use theoretical max BW */
		cur_luma_bw = STD_LUMA_BW * venc_freq * (100 + boost_perc)
				/ STD_VENC_FREQ / 100;
		cur_chroma_bw = STD_CHROMA_BW * venc_freq * (100 + boost_perc)
				/ STD_VENC_FREQ / 100;
		rec_bw = cur_luma_bw + cur_chroma_bw;
		ref_luma_bw = cur_luma_bw * 4;
		ref_chroma_bw = cur_chroma_bw * 4;
		if (1) { /* UFO */
			ref_chroma_bw += ref_luma_bw;
			ref_luma_bw = 0;
		}
	}
	mm_qos_set_request(&venc_rcpu, rcpu_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_bsdma, bsdma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sv_comv, sv_comv_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rd_comv, rd_comv_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_luma, cur_luma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_chroma, cur_chroma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rec, rec_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_luma, ref_luma_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_chroma, ref_chroma_bw, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&venc_rlist);

	venc_start_time = get_time_us();
#endif
}

void mtk_venc_emi_bw_end(struct mtk_vcodec_ctx *ctx)
{
#if ENC_EMI_BW
	/* read */
	int f_type = 1; /* TODO */
	struct vcodec_bw *cur_bw = 0;
	struct qos_bound *bound = 0;
	long long venc_end_time = 0;
	int venc_dur = 0;
	int i = 0;
	int idx = 0;
	unsigned short venc_bw_ms = 0;
	unsigned int venc_bw_sum = 0;
	u64 target_freq_64 = 0;

	venc_end_time = get_time_us();
	venc_dur = (int)((venc_end_time - venc_start_time) / 1000);
	if (venc_dur < 1)
		venc_dur = 1;

	if (venc_dur > 32)
		venc_dur = 32;

	bound = get_qos_bound();
	if (bound != 0) {
		idx = (bound->idx + QOS_BOUND_BUF_SIZE - venc_dur) & 0x3F;
		for (i = 0; i < venc_dur; i++, idx = (idx + 1) & 0x3F) {
			venc_bw_ms =
			bound->stats[idx].smibw_mon[QOS_SMIBM_VENC];
			if (venc_bw_ms == 0xFFFF) {
				venc_bw_sum = 0xFFFFFFFF;
				break;
			}
			venc_bw_sum += (unsigned int)venc_bw_ms;
		}

		if (venc_bw_sum != 0xFFFFFFFF) {
			target_freq_64 = match_freq(venc_freq,
					&venc_freq_steps[0],
					venc_freq_step_size);

			venc_bw_sum = (unsigned int)((u64)venc_bw_sum *
					STD_VENC_FREQ / ((u64)venc_dur) /
					target_freq_64);
		}

		cur_bw = find_bw_by_id(venc_bw, ctx->id);
		if (cur_bw != 0)
			cur_bw->smi_bw_mon[f_type] = venc_bw_sum;
	}

	mm_qos_set_request(&venc_rcpu, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rec, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_bsdma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_sv_comv, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_rd_comv, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_luma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_cur_chroma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_luma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&venc_ref_chroma, 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&venc_rlist);
#endif
}

void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id)
{
#if ENC_DVFS
	mutex_lock(&ctx->dev->enc_dvfs_mutex);
	add_job(&ctx->id, &venc_jobs);
	mutex_unlock(&ctx->dev->enc_dvfs_mutex);
#endif
}

void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
	mtk_venc_dvfs_begin(ctx);
	mtk_venc_emi_bw_begin(ctx);
}

void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id)
{
	mtk_venc_dvfs_end(ctx);
	mtk_venc_emi_bw_end(ctx);
}

void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt)
{

}

void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt)
{

}


