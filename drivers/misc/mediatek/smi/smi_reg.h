/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __SMI_REG_H__
#define __SMI_REG_H__

#define SMI_LARB_NUM_MAX	(1 << 3)
#define SMI_PORT_NUM_MAX	(1 << 5)

/* MMSYS */
#define MMSYS_CG_CON0		(0x100)
#define MMSYS_CG_CON1		(0x110)
#define MMSYS_HW_DCM_1ST_DIS0	(0x120)
#define MMSYS_HW_DCM_1ST_DIS_SET0	(0x124)
#define MMSYS_HW_DCM_2ND_DIS0	(0x130)
#define MMSYS_SW0_RST_B		(0x140)
#define DISP_GALS_DBG(x)	(0x520 + ((x) << 2))
#define MMSYS_GALS_DBG(x)	(0x914 + ((x) << 2))

/* COMM */
#define SMI_L1LEN		(0x100)
#define SMI_L1ARB(master)	(0x104 + ((master) << 2))
#define SMI_MON_AXI_ENA		(0x1a0)
#define SMI_MON_AXI_CLR		(0x1a4)
#define SMI_MON_AXI_ACT_CNT	(0x1c0)
#define SMI_BUS_SEL		(0x220)
#define SMI_WRR_REG0		(0x228)
#define SMI_WRR_REG1		(0x22c)
#define SMI_READ_FIFO_TH	(0x230)
#define SMI_M4U_TH		(0x234)
#define SMI_FIFO_TH1		(0x238)
#define SMI_FIFO_TH2		(0x23c)
#define SMI_PREULTRA_MASK0	(0x240)
#define SMI_PREULTRA_MASK1	(0x244)

#define SMI_DCM				(0x300)
#define SMI_ELA				(0x304)
#define SMI_Mx_RWULTRA_WRRy(x, rw, y) \
	(0x308 + (((x) - 1) << 4) + ((rw) << 3) + ((y) << 2))
#define SMI_COMMON_CLAMP_EN		(0x3c0)
#define SMI_COMMON_CLAMP_EN_SET		(0x3c4)
#define SMI_COMMON_CLAMP_EN_CLR		(0x3c8)

#define SMI_DEBUG_S(slave)	(0x400 + ((slave) << 2))
#define SMI_DEBUG_EXT(slave)	(0x420 + ((slave) << 2))
#define SMI_DEBUG_M0		(0x430)
#define SMI_DEBUG_M1		(0x434)
#define SMI_DEBUG_EXT4		(0x438)
#define SMI_DEBUG_EXT5		(0x43c)
#define SMI_DEBUG_MISC		(0x440)
#define SMI_DUMMY		(0x444)
#define SMI_DEBUG_EXT6		(0x448)
#define SMI_DEBUG_EXT7		(0x44c)

#define SMI_AST_EN	(0x700)
#define SMI_AST_CLR	(0x704)
#define SMI_SW_TRIG	(0x708)
#define SMI_AST_COND	(0x70c)
#define SMI_TIMEOUT	(0x710)
#define SMI_TIMEOUT_CNT	(0x714)
#define SMI_AST_STA	(0x718)
#define SMI_AST_STA_CLR	(0x71c)

/* LARB */
#define SMI_LARB_STAT			(0x0)
#define SMI_LARB_IRQ_EN			(0x4)
#define SMI_LARB_IRQ_STATUS		(0x8)
#define SMI_LARB_SLP_CON		(0xc)
#define SMI_LARB_CON			(0x10)
#define SMI_LARB_CON_SET		(0x14)
#define SMI_LARB_CON_CLR		(0x18)
#define SMI_LARB_VC_PRI_MODE		(0x20)
#define SMI_LARB_CMD_THRT_CON		(0x24)
#define SMI_LARB_SW_FLAG		(0x40)
#define SMI_LARB_BWL_EN			(0x50)
#define SMI_LARB_BWL_CON		(0x58)
#define SMI_LARB_OSTDL_EN		(0x60)
#define SMI_LARB_ULTRA_DIS		(0x70)
#define SMI_LARB_PREULTRA_DIS		(0x74)
#define SMI_LARB_FORCE_ULTRA		(0x78)
#define SMI_LARB_FORCE_PREULTRA		(0x7c)
#define SMI_LARB_SPM_ULTRA_MASK		(0x80)
#define SMI_LARB_SPM_STA		(0x84)
#define SMI_LARB_EXT_GREQ_VIO		(0xa0)
#define SMI_LARB_INT_GREQ_VIO		(0xa4)
#define SMI_LARB_OSTD_UDF_VIO		(0xa8)
#define SMI_LARB_OSTD_CRS_VIO		(0xac)
#define SMI_LARB_FIFO_STAT		(0xb0)
#define SMI_LARB_BUS_STAT		(0xb4)
#define SMI_LARB_CMD_THRT_STAT		(0xb8)
#define SMI_LARB_MON_REQ		(0xbc)
#define SMI_LARB_REQ_MASK		(0xc0)
#define SMI_LARB_REQ_DET		(0xc4)
#define SMI_LARB_EXT_ONGOING		(0xc8)
#define SMI_LARB_INT_ONGOING		(0xcc)
#define SMI_LARB_DBG_CON		(0xf0)
#define SMI_LARB_WRR_PORT(p)		(0x100 + ((p) << 2))
#define SMI_LARB_OSTDL_PORT(p)		(0x200 + ((p) << 2))
#define SMI_LARB_OSTD_MON_PORT(p)	(0x280 + ((p) << 2))
#define SMI_LARB_NON_SEC_CON(p)		(0x380 + ((p) << 2))
#define SMI_LARB_MON_EN			(0x400)
#define SMI_LARB_MON_CLR		(0x404)
#define SMI_LARB_MON_PORT		(0x408)
#define SMI_LARB_MON_CON		(0x40c)
#define SMI_LARB_MON_ACT_CNT		(0x410)
#define SMI_LARB_MON_REQ_CNT		(0x414)
#define SMI_LARB_MON_BEAT_CNT		(0x418)
#define SMI_LARB_MON_BYTE_CNT		(0x41c)

#define INT_SMI_LARB_CMD_THRT_CON	(0x500 + (SMI_LARB_CMD_THRT_CON))
#define INT_SMI_LARB_DBG_CON		(0x500 + (SMI_LARB_DBG_CON))
#define INT_SMI_LARB_OSTD_MON_PORT(p)	(0x500 + SMI_LARB_OSTD_MON_PORT(p))

/* RSI */
#define RSI_INTLV_CON			(0x0)
#define RSI_DCM_CON			(0x4)
#define RSI_DS_PM_CON			(0x8)
#define RSI_MISC_CON			(0xc)
#define RSI_STA				(0x10)

#define RSI_AWOSTD_S			(0x80)
#define RSI_AWOSTD_M0			(0x84)
#define RSI_AWOSTD_M1			(0x88)
#define RSI_AWOSTD_PSEUDO		(0x8c)

#define RSI_WOSTD_S			(0x90)
#define RSI_WOSTD_M0			(0x94)
#define RSI_WOSTD_M1			(0x98)
#define RSI_WOSTD_PSEUDO		(0x9c)

#define RSI_AROSTD_S			(0xa0)
#define RSI_AROSTD_M0			(0xa4)
#define RSI_AROSTD_M1			(0xa8)
#define RSI_AROSTD_PSEUDO		(0xac)

#define RSI_WLAST_OWE_CNT_S		(0xb0)
#define RSI_WLAST_OWE_CNT_M0		(0xb4)
#define RSI_WLAST_OWE_CNT_M1		(0xb8)

#define RSI_WDAT_CNT_S			(0xc0)
#define RSI_WDAT_CNT_M0			(0xc4)
#define RSI_WDAT_CNT_M1			(0xc8)

#define RSI_RDAT_CNT_S			(0xd0)
#define RSI_RDAT_CNT_M0			(0xd4)
#define RSI_RDAT_CNT_M1			(0xd8)

#define RSI_AXI_DBG_S			(0xf0)
#define RSI_AXI_DBG_M0			(0xf4)
#define RSI_AXI_DBG_M1			(0xf8)
#endif
