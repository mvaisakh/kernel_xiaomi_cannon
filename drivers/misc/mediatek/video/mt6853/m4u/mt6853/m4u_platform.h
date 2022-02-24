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

#ifndef __M4U_PORT_PRIV_H__
#define __M4U_PORT_PRIV_H__

static const char *const gM4U_SMILARB[] = {
	"mediatek,smi_larb0", "mediatek,smi_larb1", "mediatek,smi_larb2",
	"mediatek,smi_larb3",
	"mediatek,smi_larb4", "mediatek,smi_larb5", "mediatek,smi_larb6"
};

#define M4U0_PORT_INIT(name, slave, larb_id, smi_select_larb_id, port)  {\
		name, 0, slave, larb_id, port, \
		(((smi_select_larb_id)<<7)|((port)<<2)), 1\
}

struct m4u_port_t gM4uPort[] = {
	/*Larb0-10*/
	M4U0_PORT_INIT("DISP_OVL0", 0, 0, 0, 0),
	M4U0_PORT_INIT("DISP_2L_OVL0_LARB0", 0, 0, 0, 1),
	M4U0_PORT_INIT("DISP_2L_OVL1_LARB0", 0, 0, 0, 2),
	M4U0_PORT_INIT("DISP_RDMA0", 0, 0, 0, 3),
	M4U0_PORT_INIT("DISP_RDMA1", 0, 0, 0, 4),
	M4U0_PORT_INIT("DISP_WDMA0", 0, 0, 0, 5),
	M4U0_PORT_INIT("MDP_RDMA0", 0, 0, 0, 6),
	M4U0_PORT_INIT("MDP_WROT0", 0, 0, 0, 7),
	M4U0_PORT_INIT("MDP_WDMA0", 0, 0, 0, 8),
	M4U0_PORT_INIT("DISP_FAKE0", 0, 0, 0, 9),
	/*Larb1-7*/
	M4U0_PORT_INIT("VDEC_MC", 0, 1, 7, 0),
	M4U0_PORT_INIT("VDEC_PP", 0, 1, 7, 1),
	M4U0_PORT_INIT("VDEC_VLD", 0, 1, 7, 2),
	M4U0_PORT_INIT("VDEC_AVC_MV", 0, 1, 7, 3),
	M4U0_PORT_INIT("VDEC_PRED_RD", 0, 1, 7, 4),
	M4U0_PORT_INIT("VDEC_PRED_WR", 0, 1, 7, 5),
	M4U0_PORT_INIT("VDEC_PPWRAP", 0, 1, 7, 6),
	/*Larb2*/
	M4U0_PORT_INIT("IMG_IPUO", 0, 2, 5, 0),
	M4U0_PORT_INIT("IMG_IPU3O", 0, 2, 5, 1),
	M4U0_PORT_INIT("IMG_IPUI", 0, 2, 5, 2),
	/*Larb3*/
	M4U0_PORT_INIT("CAM_IPUO", 0, 3, 6, 0),
	M4U0_PORT_INIT("CAM_IPU2O", 0, 3, 6, 1),
	M4U0_PORT_INIT("CAM_IPU3O", 0, 3, 6, 2),
	M4U0_PORT_INIT("CAM_IPUI", 0, 3, 6, 3),
	M4U0_PORT_INIT("CAM_IPU2I", 0, 3, 6, 4),
	/*Larb4-11*/
	M4U0_PORT_INIT("VENC_RCPU", 0, 4, 1, 0),
	M4U0_PORT_INIT("VENC_REC", 0, 4, 1, 1),
	M4U0_PORT_INIT("VENC_BSDMA", 0, 4, 1, 2),
	M4U0_PORT_INIT("VENC_SV_COMV", 0, 4, 1, 3),
	M4U0_PORT_INIT("VENC_RD_COMV", 0, 4, 1, 4),
	M4U0_PORT_INIT("JPGENC_RDMA", 0, 4, 1, 5),
	M4U0_PORT_INIT("JPGENC_BSDMA", 0, 4, 1, 6),
	M4U0_PORT_INIT("VENC_CUR_LUMA", 0, 4, 1, 7),
	M4U0_PORT_INIT("VENC_CUR_CHROMA", 0, 4, 1, 8),
	M4U0_PORT_INIT("VENC_REF_LUMA", 0, 4, 1, 9),
	M4U0_PORT_INIT("VENC_REF_CHROMA", 0, 4, 1, 10),
	/*Larb5-25*/
	M4U0_PORT_INIT("CAM_IMGI", 0, 5, 2, 0),
	M4U0_PORT_INIT("CAM_IMG2O", 0, 5, 2, 1),
	M4U0_PORT_INIT("CAM_IMG3O", 0, 5, 2, 2),
	M4U0_PORT_INIT("CAM_VIPI", 0, 5, 2, 3),
	M4U0_PORT_INIT("CAM_LCEI", 0, 5, 2, 4),
	M4U0_PORT_INIT("CAM_SMXI", 0, 5, 2, 5),
	M4U0_PORT_INIT("CAM_SMXO", 0, 5, 2, 6),
	M4U0_PORT_INIT("CAM_WPE0_RDMA1", 0, 5, 2, 7),
	M4U0_PORT_INIT("CAM_WPE0_RDMA0", 0, 5, 2, 8),
	M4U0_PORT_INIT("CAM_WPE0_WDMA", 0, 5, 2, 9),

	M4U0_PORT_INIT("CAM_FDVT_RP", 0, 5, 2, 10),
	M4U0_PORT_INIT("CAM_FDVT_WR", 0, 5, 2, 11),
	M4U0_PORT_INIT("CAM_FDVT_RB", 0, 5, 2, 12),
	M4U0_PORT_INIT("CAM_WPE1_RDMA0", 0, 5, 2, 13),
	M4U0_PORT_INIT("CAM_WPE1_RDMA1", 0, 5, 2, 14),
	M4U0_PORT_INIT("CAM_WPE1_WDMA", 0, 5, 2, 15),
	M4U0_PORT_INIT("CAM_DPE_RDMA", 0, 5, 2, 16),
	M4U0_PORT_INIT("CAM_DPE_WDMA", 0, 5, 2, 17),
	M4U0_PORT_INIT("CAM_MFB_RDMA0", 0, 5, 2, 18),
	M4U0_PORT_INIT("CAM_MFB_RDMA1", 0, 5, 2, 19),

	M4U0_PORT_INIT("CAM_MFB_WDMA", 0, 5, 2, 20),
	M4U0_PORT_INIT("CAM_RSC_RDMA", 0, 5, 2, 21),
	M4U0_PORT_INIT("CAM_RSC_WDMA", 0, 5, 2, 22),
	M4U0_PORT_INIT("CAM_OWE_RDMA", 0, 5, 2, 23),
	M4U0_PORT_INIT("CAM_OWE_WDMA", 0, 5, 2, 24),

	/*Larb6-31*/
	M4U0_PORT_INIT("CAM_IMGO", 0, 6, 3, 0),
	M4U0_PORT_INIT("CAM_RRZO", 0, 6, 3, 1),
	M4U0_PORT_INIT("CAM_AAO", 0, 6, 3, 2),
	M4U0_PORT_INIT("CAM_AFO", 0, 6, 3, 3),
	M4U0_PORT_INIT("CAM_LSCI0", 0, 6, 3, 4),
	M4U0_PORT_INIT("CAM_LSCI1", 0, 6, 3, 5),
	M4U0_PORT_INIT("CAM_PDO", 0, 6, 3, 6),
	M4U0_PORT_INIT("CAM_BPCI", 0, 6, 3, 7),
	M4U0_PORT_INIT("CAM_LCSO", 0, 6, 3, 8),
	M4U0_PORT_INIT("CAM_RSSO_A", 0, 6, 3, 9),

	M4U0_PORT_INIT("CAM_UFEO", 0, 6, 3, 10),
	M4U0_PORT_INIT("CAM_SOCO", 0, 6, 3, 11),
	M4U0_PORT_INIT("CAM_SOC1", 0, 6, 3, 12),
	M4U0_PORT_INIT("CAM_SOC2", 0, 6, 3, 13),
	M4U0_PORT_INIT("CAM_CCUI", 0, 6, 3, 14),
	M4U0_PORT_INIT("CAM_CCUO", 0, 6, 3, 15),
	M4U0_PORT_INIT("CAM_RAWI_A", 0, 6, 3, 16),
	M4U0_PORT_INIT("CAM_CCUG", 0, 6, 3, 17),
	M4U0_PORT_INIT("CAM_PSO", 0, 6, 3, 18),
	M4U0_PORT_INIT("CAM_AFO_1", 0, 6, 3, 19),

	M4U0_PORT_INIT("CAM_LSCI_2", 0, 6, 3, 20),
	M4U0_PORT_INIT("CAM_PDI", 0, 6, 3, 21),
	M4U0_PORT_INIT("CAM_FLKO", 0, 6, 3, 22),
	M4U0_PORT_INIT("CAM_LMVO", 0, 6, 3, 23),
	M4U0_PORT_INIT("CAM_UFGO", 0, 6, 3, 24),
	M4U0_PORT_INIT("CAM_SPARE", 0, 6, 3, 25),
	M4U0_PORT_INIT("CAM_SPARE_2", 0, 6, 3, 26),
	M4U0_PORT_INIT("CAM_SPARE_3", 0, 6, 3, 27),
	M4U0_PORT_INIT("CAM_SPARE_4", 0, 6, 3, 28),
	M4U0_PORT_INIT("CAM_SPARE_5", 0, 6, 3, 29),
	M4U0_PORT_INIT("CAM_SPARE_6", 0, 6, 3, 30),

	M4U0_PORT_INIT("VPU0", 0, 7, 8, 0),
	M4U0_PORT_INIT("VPU1", 0, 7, 8, 1),
	M4U0_PORT_INIT("CCU0", 0, 6, 4, 0),
	M4U0_PORT_INIT("CCU1", 0, 6, 4, 1),

	M4U0_PORT_INIT("UNKNOWN", 0, 0, 0, 0)
};


#endif
