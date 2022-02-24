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

#ifndef _MTK_VCODEC_ENC_PM_H_
#define _MTK_VCODEC_ENC_PM_H_

#include "mtk_vcodec_drv.h"

#define ENC_DVFS	1
#define ENC_EMI_BW	1

void mtk_venc_init_ctx_pm(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_init_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_release_enc_pm(struct mtk_vcodec_dev *dev);
void mtk_venc_deinit_ctx_pm(struct mtk_vcodec_ctx *ctx);

void mtk_vcodec_enc_clock_on(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_vcodec_enc_clock_off(struct mtk_vcodec_ctx *ctx, int core_id);

void mtk_prepare_venc_dvfs(void);
void mtk_unprepare_venc_dvfs(void);
void mtk_prepare_venc_emi_bw(void);
void mtk_unprepare_venc_emi_bw(void);

/* Frame based PMQoS */
void mtk_venc_pmqos_prelock(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx, int core_id);
void mtk_venc_pmqos_end_frame(struct mtk_vcodec_ctx *ctx, int core_id);

/* GCE version PMQoS */
void mtk_venc_pmqos_gce_flush(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);
void mtk_venc_pmqos_gce_done(struct mtk_vcodec_ctx *ctx, int core_id,
			int job_cnt);

int mtk_venc_ion_config_buff(struct dma_buf *dmabuf);

#endif /* _MTK_VCODEC_ENC_PM_H_ */
