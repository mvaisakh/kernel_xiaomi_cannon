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
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_mdp_common.h"
#include "cmdq_sec_iwc_common.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/slab.h>
#ifdef COFNIG_MTK_IOMMU
#include "mtk_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif
#ifdef CONFIG_MTK_SMI_EXT
#include <mmdvfs_pmqos.h>
#include "smi_public.h"
#include "smi_port.h"

/* mdp */
struct mm_qos_request mdp_rdma0_request[MDP_TOTAL_THREAD];
struct mm_qos_request mdp_rdma1_request[MDP_TOTAL_THREAD];
struct mm_qos_request mdp_wrot0_request[MDP_TOTAL_THREAD];
struct mm_qos_request mdp_wrot1_request[MDP_TOTAL_THREAD];
/* isp */
struct mm_qos_request imgi_request[MDP_TOTAL_THREAD];
struct mm_qos_request img2o_request[MDP_TOTAL_THREAD];
struct mm_qos_request img3o_request[MDP_TOTAL_THREAD];
struct mm_qos_request vipi_request[MDP_TOTAL_THREAD];
struct mm_qos_request lcei_request[MDP_TOTAL_THREAD];
struct mm_qos_request smxi_request[MDP_TOTAL_THREAD];
struct mm_qos_request smxo_request[MDP_TOTAL_THREAD];

uint32_t cmdq_mdp_translate_port(uint32_t engineId)
{
	switch (engineId) {
	case CMDQ_ENG_MDP_RDMA0:
		return SMI_MDP_RDMA0;
	case CMDQ_ENG_MDP_RDMA1:
		return SMI_MDP_RDMA1;
	case CMDQ_ENG_MDP_WROT0:
		return SMI_MDP_WROT0_R;
	case CMDQ_ENG_MDP_WROT1:
		return SMI_MDP_WROT1_R;
	}

	if (engineId != CMDQ_ENG_MDP_CAMIN
#ifdef CMDQ_ENG_MDP_CAMIN2
		&& engineId != CMDQ_ENG_MDP_CAMIN2
#endif
		)
		CMDQ_ERR("pmqos invalid engineId %d\n", engineId);
	return 0;
}

struct mm_qos_request *cmdq_mdp_get_request(uint32_t thread_id, uint32_t port)
{
	if (!port)
		return NULL;

	switch (port) {
	case SMI_MDP_RDMA0:
		return &mdp_rdma0_request[thread_id];
	case SMI_MDP_RDMA1:
		return &mdp_rdma1_request[thread_id];
	case SMI_MDP_WROT0_R:
		return &mdp_wrot0_request[thread_id];
	case SMI_MDP_WROT1_R:
		return &mdp_wrot1_request[thread_id];
	case SMI_IMGI:
		return &imgi_request[thread_id];
	case SMI_IMG2O:
		return &img2o_request[thread_id];
	case SMI_IMG3O:
		return &img3o_request[thread_id];
	case SMI_VIPI:
		return &vipi_request[thread_id];
	case SMI_LCEI:
		return &lcei_request[thread_id];
	case SMI_SMXI:
		return &smxi_request[thread_id];
	case SMI_SMXO:
		return &smxo_request[thread_id];
	}

	CMDQ_ERR("pmqos invalid port%d\n", port);
	return NULL;
}

void cmdq_mdp_init_pmqos_mdp(s32 index, struct plist_head *owner_list)
{
	mm_qos_add_request(&owner_list[index],
		&mdp_rdma0_request[index], SMI_MDP_RDMA0);
	mm_qos_add_request(&owner_list[index],
		&mdp_rdma1_request[index], SMI_MDP_RDMA1);
	mm_qos_add_request(&owner_list[index],
		&mdp_wrot0_request[index], SMI_MDP_WROT0_R);
	mm_qos_add_request(&owner_list[index],
		&mdp_wrot1_request[index], SMI_MDP_WROT1_R);
}

void cmdq_mdp_init_pmqos_isp(s32 index, struct plist_head *owner_list)
{
	mm_qos_add_request(&owner_list[index],
		&imgi_request[index], SMI_IMGI);
	mm_qos_add_request(&owner_list[index],
		&img2o_request[index], SMI_IMG2O);
	mm_qos_add_request(&owner_list[index],
		&img3o_request[index], SMI_IMG3O);
	mm_qos_add_request(&owner_list[index],
		&vipi_request[index], SMI_VIPI);
	mm_qos_add_request(&owner_list[index],
		&lcei_request[index], SMI_LCEI);
	mm_qos_add_request(&owner_list[index],
		&smxi_request[index], SMI_SMXI);
	mm_qos_add_request(&owner_list[index],
		&smxo_request[index], SMI_SMXO);
}
#endif	/* CONFIG_MTK_SMI_EXT */

#include "cmdq_device.h"
struct CmdqMdpModuleBaseVA {
	long MDP_RDMA0;
	long MDP_RDMA1;
	long MDP_RSZ0;
	long MDP_RSZ1;
	long MDP_TDSHP;
	long MDP_COLOR;
	long MDP_AAL;
	long MDP_HDR;
	long MDP_WROT0;
	long MDP_WROT1;
	long VENC;
	long MM_MUTEX;
};
static struct CmdqMdpModuleBaseVA gCmdqMdpModuleBaseVA;

struct CmdqMdpModuleClock {
	struct clk *clk_CAM_MDP_TX;
	struct clk *clk_CAM_MDP_RX;
	struct clk *clk_CAM_MDP2_TX;
	struct clk *clk_CAM_MDP2_RX;
	struct clk *clk_MDP_RDMA0;
	struct clk *clk_MDP_RDMA1;
	struct clk *clk_MDP_RSZ0;
	struct clk *clk_MDP_RSZ1;
	struct clk *clk_MDP_WROT0;
	struct clk *clk_MDP_WROT1;
	struct clk *clk_MDP_TDSHP;
	struct clk *clk_MDP_COLOR;
	struct clk *clk_MDP_AAL;
	struct clk *clk_MDP_HDR;
};
static struct CmdqMdpModuleClock gCmdqMdpModuleClock;
#define IMP_ENABLE_MDP_HW_CLOCK(FN_NAME, HW_NAME)	\
uint32_t cmdq_mdp_enable_clock_##FN_NAME(bool enable)	\
{		\
	return cmdq_dev_enable_device_clock(enable,	\
		gCmdqMdpModuleClock.clk_##HW_NAME, #HW_NAME "-clk");	\
}
#define IMP_MDP_HW_CLOCK_IS_ENABLE(FN_NAME, HW_NAME)	\
bool cmdq_mdp_clock_is_enable_##FN_NAME(void)	\
{		\
	return cmdq_dev_device_clock_is_enable(	\
		gCmdqMdpModuleClock.clk_##HW_NAME);	\
}

/* for debug tdshp clock issue */
atomic_t mdp_tdshp_clk;
uint32_t cmdq_mdp_enable_clock_MDP_TDSHP0(bool enable)
{
	int result = 0;
	const char *clkName = "MDP_TDSHP-clk";

	if (IS_ERR(gCmdqMdpModuleClock.clk_MDP_TDSHP)) {
		CMDQ_ERR("clock module does not support:%s\n", clkName);
		return PTR_ERR(gCmdqMdpModuleClock.clk_MDP_TDSHP);
	}

	if (enable) {
		result = clk_prepare_enable(gCmdqMdpModuleClock.clk_MDP_TDSHP);
		if (result) {
			CMDQ_ERR("enable tdshp clk fail:%d\n", result);
			CMDQ_AEE("MDP", "enable tdshp clk fail:%d\n", result);
		} else {
			CMDQ_MSG("enable clock with module: %s, result:%d\n",
				clkName, result);
			atomic_inc(&mdp_tdshp_clk);
		}
	} else {
		clk_disable_unprepare(gCmdqMdpModuleClock.clk_MDP_TDSHP);
		CMDQ_MSG("disable clock with module:MDP_TDSHP-clk\n");
		atomic_dec(&mdp_tdshp_clk);
	}

	return result;
}


IMP_ENABLE_MDP_HW_CLOCK(CAM_MDP_TX, CAM_MDP_TX);
IMP_ENABLE_MDP_HW_CLOCK(CAM_MDP_RX, CAM_MDP_RX);
IMP_ENABLE_MDP_HW_CLOCK(CAM_MDP2_TX, CAM_MDP2_TX);
IMP_ENABLE_MDP_HW_CLOCK(CAM_MDP2_RX, CAM_MDP2_RX);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA0, MDP_RDMA0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA1, MDP_RDMA1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ0, MDP_RSZ0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ1, MDP_RSZ1);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL, MDP_AAL);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR, MDP_HDR);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT0, MDP_WROT0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT1, MDP_WROT1);
#if 0
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP0, MDP_TDSHP);
#endif
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR0, MDP_COLOR);
IMP_MDP_HW_CLOCK_IS_ENABLE(CAM_MDP_TX, CAM_MDP_TX);
IMP_MDP_HW_CLOCK_IS_ENABLE(CAM_MDP_RX, CAM_MDP_RX);
IMP_MDP_HW_CLOCK_IS_ENABLE(CAM_MDP2_TX, CAM_MDP2_TX);
IMP_MDP_HW_CLOCK_IS_ENABLE(CAM_MDP2_RX, CAM_MDP2_RX);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA0, MDP_RDMA0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA1, MDP_RDMA1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ0, MDP_RSZ0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ1, MDP_RSZ1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL, MDP_AAL);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR, MDP_HDR);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT0, MDP_WROT0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT1, MDP_WROT1);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP0, MDP_TDSHP);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR0, MDP_COLOR);
#undef IMP_ENABLE_MDP_HW_CLOCK
#undef IMP_MDP_HW_CLOCK_IS_ENABLE

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_ISP_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_DISP_GROUP_BITS,
	CMDQ_ENG_JPEG_GROUP_BITS,
	CMDQ_ENG_VENC_GROUP_BITS,
	CMDQ_ENG_DPE_GROUP_BITS,
	CMDQ_ENG_RSC_GROUP_BITS,
	CMDQ_ENG_GEPF_GROUP_BITS,
	CMDQ_ENG_WPE_GROUP_BITS,
	CMDQ_ENG_OWE_GROUP_BITS,
	CMDQ_ENG_MFB_GROUP_BITS
};

long cmdq_mdp_get_module_base_VA_MDP_RDMA0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA0;
}
long cmdq_mdp_get_module_base_VA_MDP_RDMA1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA1;
}
long cmdq_mdp_get_module_base_VA_MDP_RSZ0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ0;
}
long cmdq_mdp_get_module_base_VA_MDP_RSZ1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ1;
}
long cmdq_mdp_get_module_base_VA_MDP_TDSHP(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP;
}
long cmdq_mdp_get_module_base_VA_MDP_COLOR(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR;
}
long cmdq_mdp_get_module_base_VA_MDP_AAL(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL;
}
long cmdq_mdp_get_module_base_VA_MDP_HDR(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR;
}
long cmdq_mdp_get_module_base_VA_MDP_WROT0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT0;
}
long cmdq_mdp_get_module_base_VA_MDP_WROT1(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT1;
}
long cmdq_mdp_get_module_base_VA_VENC(void)
{
	return gCmdqMdpModuleBaseVA.VENC;
}

long cmdq_mdp_get_module_base_VA_MM_MUTEX(void)
{
	return gCmdqMdpModuleBaseVA.MM_MUTEX;
}

#define MMSYS_CONFIG_BASE	cmdq_mdp_get_module_base_VA_MMSYS_CONFIG()
#define MDP_RDMA0_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA0()
#define MDP_RDMA1_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA1()
#define MDP_RSZ0_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ0()
#define MDP_RSZ1_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ1()
#define MDP_TDSHP_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP()
#define MDP_COLOR_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR()
#define MDP_AAL_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL()
#define MDP_HDR_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR()
#define MDP_WROT0_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT0()
#define MDP_WROT1_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT1()
#define VENC_BASE		cmdq_mdp_get_module_base_VA_VENC()
#define MM_MUTEX_BASE		cmdq_mdp_get_module_base_VA_MM_MUTEX()
struct RegDef {
	int offset;
	const char *name;
};
void cmdq_mdp_dump_mmsys_config(void)
{
	int i = 0;
	uint32_t value = 0;
	static const struct RegDef configRegisters[] = {
		{0x100, "MMSYS_CG_CON0"},
		{0x110, "MMSYS_CG_CON1"},
		{0xF14, "IPU_MOUT_EN"},
		{0xF18, "ISP_MOUT_EN"},
		{0xF1C, "MDP_AAL_MOUT_EN"},
		{0xF20, "MDP_COLOR_MOUT_EN"},
		{0xF24, "MDP_RDMA0_MOUT_EN"},
		{0xF28, "MDP_RDMA1_MOUT_EN"},
		{0xF2C, "MDP_PRZ0_MOUT_EN"},
		{0xF30, "MDP_PRZ1_MOUT_EN"},
		{0xF4C, "DISP_TO_WROT_SOUT_SEL"},
		{0xF50, "MDP_CCORR_SOUT_SEL"},
		{0xF54, "MDP_COLOR_IN_SOUT_SEL"},
		{0xF58, "MDP_PATH0_SOUT_SEL"},
		{0xF5C, "MDP_PATH1_SOUT_SEL"},
		{0xF68, "DISP_COLOR_OUT_SEL_IN"},
		{0xF90, "MDP_AAL_SEL_IN"},
		{0xF94, "MDP_CCORR_SEL_IN"},
		{0xF98, "MDP_COLOR_OUT_SEL_IN"},
		{0xF9C, "MDP_COLOR_SEL_IN "},
		{0xFA0, "MDP_PATH0_SEL_IN"},
		{0xFA4, "MDP_PATH1_SEL_IN"},
		{0xFA8, "MDP_PRZ0_SEL_IN"},
		{0xFAC, "MDP_PRZ1_SEL_IN"},
		{0xFB0, "MDP_TDSHP_SEL_IN"},
		{0xFB4, "MDP_WROT0_SEL_IN"},
		{0xFB8, "MDP_WROT1_SEL_IN"},
		/* DISP */
		{0xF00, "DISP_DITHER0_MOUT_EN"},
		{0xF04, "DISP_OVL0_2L_MOUT_EN"},
		{0xF08, "DISP_OVL0_MOUT_EN"},
		{0xF0C, "DISP_OVL1_2L_MOUT_EN"},
		{0xF10, "DISP_RSZ_MOUT_EN"},
		{0xF84, "DISP_WDMA0_SEL_IN"},
		{0xF88, "DPI0_SEL_IN"},
		{0xF8C, "DSI0_SEL_IN"},
		{0xF44, "DISP_RDMA0_SOUT_SEL_IN"},
		{0xF48, "DISP_RDMA1_SOUT_SEL_IN"},
		{0x0F0, "MMSYS_MISC"},
		/* ACK and REQ related */
		{0x8CC, "MMSYS_DL_VALID_0"},
		{0x8D0, "MMSYS_DL_VALID_1"},
		{0x8D4, "MMSYS_DL_VALID_2"},
		{0x8D8, "MMSYS_DL_VALID_3"},
		{0x8DC, "MMSYS_DL_VALID_4"},
		{0x8E0, "MMSYS_DL_READY_0"},
		{0x8E4, "MMSYS_DL_READY_1"},
		{0x8E8, "MMSYS_DL_READY_2"},
		{0x8EC, "MMSYS_DL_READY_3"},
		{0x8F0, "MMSYS_DL_READY_4"},
		{0x8F8, "MMSYS_MOUT_MASK_0"},
		{0x8FC, "MMSYS_MOUT_MASK_1"},
		{0x900, "MMSYS_MOUT_MASK_2"},
		{0x100, "MMSYS_CG_CON0"},
		{0x110, "MMSYS_CG_CON1"},
		/* Async DL related */
		{0x938, "MDP_ASYNC_CFG_WD"},
		{0x93C, "MDP_ASYNC_CFG_RD"},
		{0x940, "MDP_ASYNC_IPU_CFG_WD"},
		{0x944, "MDP_ASYNC_IPU_CFG_RD"},
		{0x958, "MDP_ASYNC_ISP_CFG_OUT_RD"},
		{0x95C, "MDP_ASYNC_IPU_CFG_OUT_RD"},
		{0x960, "TOP_RELAY_FSM_RD"},
		{0x994, "ISP_RELAY_CFG_WD"},
		{0x998, "ISP_RELAY_CNT_RD"},
		{0x99C, "ISP_RELAY_CNT_LATCH_RD"},
		{0x9A0, "IPU_RELAY_CFG_WD"},
		{0x9A4, "IPU_RELAY_CNT_RD"},
		{0x9A8, "IPU_RELAY_CNT_LATCH_RD"}


	};
	for (i = 0; i < ARRAY_SIZE(configRegisters); i++) {
		value = CMDQ_REG_GET32(MMSYS_CONFIG_BASE +
			configRegisters[i].offset);
		CMDQ_ERR("%s: 0x%08x\n", configRegisters[i].name, value);
	}

	/*DISP_MUTEX MOD*/
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D0);
	CMDQ_ERR("%s: 0x%08x\n", "DISP_MUTEX5_MOD0", value);
}
int32_t cmdq_mdp_reset_with_mmsys(const uint64_t engineToResetAgain)
{
	long MMSYS_SW0_RST_B_REG = MMSYS_CONFIG_BASE + (0x140);
	long MMSYS_SW1_RST_B_REG = MMSYS_CONFIG_BASE + (0x144);
#ifdef CMDQ_MDP_COLOR
	int mdpColorResetBit = CMDQ_ENG_MDP_COLOR0;
#else
	int mdpColorResetBit = -1;
#endif
	int i = 0;
	uint32_t reset_bits0 = 0L;
	uint32_t reset_bits1 = 0L;
	int engineResetBit0[32] = {
		-1,		/* bit  0 : SMI COMMON */
		-1,		/* bit  1 : SMI LARB0 */
		-1,		/* bit  2 : SMI LARB1 */
		CMDQ_ENG_MDP_CAMIN,	/* bit  3 : CAM_MDP */
		CMDQ_ENG_MDP_CAMIN2,	/* bit  4 : CAM2_MDP */
		CMDQ_ENG_MDP_RDMA0,	/* bit  5 : MDP_RDMA0 */
		CMDQ_ENG_MDP_RDMA1,	/* bit  6 : MDP_RDMA1 */
		CMDQ_ENG_MDP_RSZ0,	/* bit  7 : MDP_RSZ0 */
		CMDQ_ENG_MDP_RSZ1,	/* bit  8 : MDP_RSZ1 */
		CMDQ_ENG_MDP_TDSHP0,	/* bit  9 : MDP_TDSHP0 */
		CMDQ_ENG_MDP_WROT0,	/* bit  10 : MDP_WROT0 */
		CMDQ_ENG_MDP_WROT1,	/* bit  11 : MDP_WROT1 */
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		mdpColorResetBit,	/* bit  19 : COLOR0 */
		[20 ... 31] = -1
	};

	int engineResetBit1[32] = {
		-1,			/* bit  0 : DISP_MUTEX */
		-1,			/* bit  1 : GALS_COMM0 */
		-1,			/* bit  2 : GALS_COMM1 */
		-1,			/* bit  3 : GALS_VDEC2MM */
		-1,			/* bit  4 : GALS_CAM2MM */
		-1,			/* bit  5 : GALS_VENC2MM */
		-1,			/* bit  6 : GALS_IMG2MM */
		-1,			/* bit  7 : GALS_IPU02MM */
		-1,			/* bit  8 : MMSYS_R2Y */
		-1,			/* bit  9 : MDP_COLOR_MOUT */
		CMDQ_ENG_MDP_CAMIN,	/* bit  10 : CAM_MDP */
		CMDQ_ENG_MDP_CAMIN2,	/* bit  11 : CAM2_MDP */
		-1,			/* bit  12 : MDP_COLOR_MOUT */
		-1,			/* bit  13 : MDP_COLOR_MOUT */
		CMDQ_ENG_MDP_AAL0,      /* bit  14 : MDP_AAL */
		CMDQ_ENG_MDP_HDR0,	/* bit  15 : MDP_HDR */
		[16 ... 31] = -1
	};

	for (i = 0; i < 32; ++i) {
		if (engineResetBit0[i] < 0)
			continue;
		if (engineToResetAgain & (1LL << engineResetBit0[i]))
			reset_bits0 |= (1 << i);
	}
	if (reset_bits0 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits0 = ~reset_bits0;
		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, reset_bits0);
		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}

	for (i = 0; i < 32; ++i) {
		if (engineResetBit1[i] < 0)
			continue;
		if (engineToResetAgain & (1LL << engineResetBit1[i]))
			reset_bits1 |= (1 << i);
	}
	if (reset_bits1 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits1 = ~reset_bits1;
		CMDQ_REG_SET32(MMSYS_SW1_RST_B_REG, reset_bits1);
		CMDQ_REG_SET32(MMSYS_SW1_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}
	return 0;
}

#ifdef COFNIG_MTK_IOMMU
mtk_iommu_callback_ret_t cmdq_TranslationFault_callback(
	int port, unsigned int mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("================= [MDP M4U] Dump Begin ================\n");
	CMDQ_ERR("[MDP M4U]fault call port=%d, mva=0x%x", port, mva);
	cmdq_core_dump_tasks_info();
	switch (port) {
	case M4U_PORT_MDP_RDMA0:
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
		break;
	case M4U_PORT_MDP_RDMA1:
		cmdq_mdp_dump_rdma(MDP_RDMA1_BASE, "RDMA1");
		break;
	case M4U_PORT_MDP_WROT0_R:
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
		break;
	case M4U_PORT_MDP_WROT1_R:
		cmdq_mdp_dump_rot(MDP_WROT1_BASE, "WROT1");
		break;
	default:
		CMDQ_ERR("[MDP M4U]fault callback function");
		break;
	}
	CMDQ_ERR(
		"=============== [MDP] Frame Information Begin ====================================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR(
		"=============== [MDP] Frame Information End ====================================\n");
	CMDQ_ERR(
		"================= [MDP M4U] Dump End ================\n");
	return MTK_IOMMU_CALLBACK_HANDLED;
}
#elif defined(CONFIG_MTK_M4U)
enum m4u_callback_ret_t cmdq_TranslationFault_callback(
	int port, unsigned int mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("================= [MDP M4U] Dump Begin ================\n");
	CMDQ_ERR("[MDP M4U]fault call port=%d, mva=0x%x", port, mva);
	cmdq_core_dump_tasks_info();
	switch (port) {
	case M4U_PORT_MDP_RDMA0:
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
		break;
	case M4U_PORT_MDP_RDMA1:
		cmdq_mdp_dump_rdma(MDP_RDMA1_BASE, "RDMA1");
		break;
	case M4U_PORT_MDP_WROT0_R:
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
		break;
	case M4U_PORT_MDP_WROT1_R:
		cmdq_mdp_dump_rot(MDP_WROT1_BASE, "WROT1");
		break;
	default:
		CMDQ_ERR("[MDP M4U]fault callback function");
		break;
	}
	CMDQ_ERR(
		"=============== [MDP] Frame Information Begin ====================================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR(
		"=============== [MDP] Frame Information End ====================================\n");
	CMDQ_ERR(
		"================= [MDP M4U] Dump End ================\n");
	return M4U_CALLBACK_HANDLED;
}
#endif

int32_t cmdqVEncDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_VIDEO_ENC))
		cmdq_mdp_dump_venc(VENC_BASE, "VENC");
	return 0;
}
void cmdq_mdp_init_module_base_VA(void)
{
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));

	gCmdqMdpModuleBaseVA.MDP_RDMA0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma0");
	gCmdqMdpModuleBaseVA.MDP_RDMA1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma1");
	gCmdqMdpModuleBaseVA.MDP_RSZ0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz0");
	gCmdqMdpModuleBaseVA.MDP_RSZ1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz1");
	gCmdqMdpModuleBaseVA.MDP_WROT0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot0");
	gCmdqMdpModuleBaseVA.MDP_WROT1 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot1");
	gCmdqMdpModuleBaseVA.MDP_TDSHP =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp0");
	gCmdqMdpModuleBaseVA.MDP_COLOR =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color0");
	gCmdqMdpModuleBaseVA.MDP_AAL =
		cmdq_dev_alloc_reference_VA_by_name("mdp_aal0");
	gCmdqMdpModuleBaseVA.MDP_HDR =
		cmdq_dev_alloc_reference_VA_by_name("mdp_hdr0");
	gCmdqMdpModuleBaseVA.VENC =
		cmdq_dev_alloc_reference_VA_by_name("venc");
	gCmdqMdpModuleBaseVA.MM_MUTEX =
		cmdq_dev_alloc_reference_VA_by_name("mm_mutex");
}
void cmdq_mdp_deinit_module_base_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT1());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_VENC());
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));
}
bool cmdq_mdp_clock_is_on(enum CMDQ_ENG_ENUM engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		return cmdq_mdp_clock_is_enable_CAM_MDP_TX();
	case CMDQ_ENG_MDP_CAMIN2:
		return cmdq_mdp_clock_is_enable_CAM_MDP2_TX();
	case CMDQ_ENG_MDP_RDMA0:
		return cmdq_mdp_clock_is_enable_MDP_RDMA0();
	case CMDQ_ENG_MDP_RDMA1:
		return cmdq_mdp_clock_is_enable_MDP_RDMA1();
	case CMDQ_ENG_MDP_RSZ0:
		return cmdq_mdp_clock_is_enable_MDP_RSZ0();
	case CMDQ_ENG_MDP_RSZ1:
		return cmdq_mdp_clock_is_enable_MDP_RSZ1();
	case CMDQ_ENG_MDP_WROT0:
		return cmdq_mdp_clock_is_enable_MDP_WROT0();
	case CMDQ_ENG_MDP_WROT1:
		return cmdq_mdp_clock_is_enable_MDP_WROT1();
	case CMDQ_ENG_MDP_TDSHP0:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP0();
	case CMDQ_ENG_MDP_COLOR0:
		return cmdq_mdp_clock_is_enable_MDP_COLOR0();
	case CMDQ_ENG_MDP_AAL0:
		return cmdq_mdp_clock_is_enable_MDP_AAL();
	case CMDQ_ENG_MDP_HDR0:
		return cmdq_mdp_clock_is_enable_MDP_HDR();
	default:
		CMDQ_ERR("try to query unknown mdp clock");
		return false;
	}
}

void cmdq_mdp_enable_clock(bool enable, enum CMDQ_ENG_ENUM engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		cmdq_mdp_enable_clock_CAM_MDP_TX(enable);
		cmdq_mdp_enable_clock_CAM_MDP_RX(enable);
		break;
	case CMDQ_ENG_MDP_CAMIN2:
		cmdq_mdp_enable_clock_CAM_MDP2_TX(enable);
		cmdq_mdp_enable_clock_CAM_MDP2_RX(enable);
		break;
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA0(enable);
		break;
	case CMDQ_ENG_MDP_RDMA1:
		cmdq_mdp_enable_clock_MDP_RDMA1(enable);
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ0(enable);
		break;
	case CMDQ_ENG_MDP_RSZ1:
		cmdq_mdp_enable_clock_MDP_RSZ1(enable);
		break;
	case CMDQ_ENG_MDP_WROT0:
		if (enable) {
#ifdef CONFIG_MTK_SMI_EXT
			smi_bus_prepare_enable(SMI_LARB1, "MDPSRAM");
#endif
			cmdq_mdp_enable_clock_MDP_WROT0(enable);
		} else {
			cmdq_mdp_enable_clock_MDP_WROT0(enable);
#ifdef CONFIG_MTK_SMI_EXT
			smi_bus_disable_unprepare(SMI_LARB1, "MDPSRAM");
#endif
		}
		break;
	case CMDQ_ENG_MDP_WROT1:
		if (enable) {
#ifdef CONFIG_MTK_SMI_EXT
			smi_bus_prepare_enable(SMI_LARB1, "MDPSRAM");
#endif
			cmdq_mdp_enable_clock_MDP_WROT1(enable);
		} else {
			cmdq_mdp_enable_clock_MDP_WROT1(enable);
#ifdef CONFIG_MTK_SMI_EXT
			smi_bus_disable_unprepare(SMI_LARB1, "MDPSRAM");
#endif
		}
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_TDSHP0(enable);
		break;
	case CMDQ_ENG_MDP_COLOR0:
#ifdef CMDQ_MDP_COLOR
		cmdq_mdp_enable_clock_MDP_COLOR0(enable);
#endif
		break;
	case CMDQ_ENG_MDP_AAL0:
		cmdq_mdp_enable_clock_MDP_AAL(enable);
		break;
	case CMDQ_ENG_MDP_HDR0:
		cmdq_mdp_enable_clock_MDP_HDR(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp clock");
		break;
	}
}
/* Common Clock Framework */
void cmdq_mdp_init_module_clk(void)
{
	cmdq_dev_get_module_clock_by_name("mmsys_config", "CAM_MDP_TX",
					  &gCmdqMdpModuleClock.clk_CAM_MDP_TX);
	cmdq_dev_get_module_clock_by_name("mmsys_config", "CAM_MDP_RX",
					  &gCmdqMdpModuleClock.clk_CAM_MDP_RX);
	cmdq_dev_get_module_clock_by_name("mmsys_config", "CAM_MDP2_TX",
					  &gCmdqMdpModuleClock.clk_CAM_MDP2_TX);
	cmdq_dev_get_module_clock_by_name("mmsys_config", "CAM_MDP2_RX",
					  &gCmdqMdpModuleClock.clk_CAM_MDP2_RX);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "MDP_RDMA0",
					  &gCmdqMdpModuleClock.clk_MDP_RDMA0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma1", "MDP_RDMA1",
					  &gCmdqMdpModuleClock.clk_MDP_RDMA1);
	cmdq_dev_get_module_clock_by_name("mdp_rsz0", "MDP_RSZ0",
					  &gCmdqMdpModuleClock.clk_MDP_RSZ0);
	cmdq_dev_get_module_clock_by_name("mdp_rsz1", "MDP_RSZ1",
					  &gCmdqMdpModuleClock.clk_MDP_RSZ1);
	cmdq_dev_get_module_clock_by_name("mdp_wrot0", "MDP_WROT0",
					  &gCmdqMdpModuleClock.clk_MDP_WROT0);
	cmdq_dev_get_module_clock_by_name("mdp_wrot1", "MDP_WROT1",
					  &gCmdqMdpModuleClock.clk_MDP_WROT1);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp0", "MDP_TDSHP",
					  &gCmdqMdpModuleClock.clk_MDP_TDSHP);
	cmdq_dev_get_module_clock_by_name("mdp_aal0", "MDP_AAL",
					  &gCmdqMdpModuleClock.clk_MDP_AAL);
	cmdq_dev_get_module_clock_by_name("mdp_hdr0", "MDP_HDR",
					  &gCmdqMdpModuleClock.clk_MDP_HDR);
#ifdef CMDQ_MDP_COLOR
	cmdq_dev_get_module_clock_by_name("mdp_color0", "MDP_COLOR",
					  &gCmdqMdpModuleClock.clk_MDP_COLOR);
#endif
}
/* MDP engine dump */
void cmdq_mdp_dump_rsz(const unsigned long base, const char *label)
{
	uint32_t value[12] = { 0 };
	uint32_t request[4] = { 0 };
	uint32_t state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x000);
	value[1] = CMDQ_REG_GET32(base + 0x004);
	value[2] = CMDQ_REG_GET32(base + 0x008);
	value[3] = CMDQ_REG_GET32(base + 0x010);
	value[4] = CMDQ_REG_GET32(base + 0x014);
	value[5] = CMDQ_REG_GET32(base + 0x018);
	value[6] = CMDQ_REG_GET32(base + 0x01C);
	CMDQ_REG_SET32(base + 0x044, 0x00000001);
	value[7] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000002);
	value[8] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000003);
	value[9] = CMDQ_REG_GET32(base + 0x048);
	value[10] = CMDQ_REG_GET32(base + 0x100);
	value[11] = CMDQ_REG_GET32(base + 0x200);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"PRZ_ENABLE: 0x%08x, RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		value[3], value[4]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[7], value[8], value[9]);
	CMDQ_ERR("PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		 value[10], value[11]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[8] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	CMDQ_ERR("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		cmdq_mdp_get_rsz_state(state));
}
void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label)
{
	uint32_t value[10] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x114);
	value[1] = CMDQ_REG_GET32(base + 0x11C);
	value[2] = CMDQ_REG_GET32(base + 0x104);
	value[3] = CMDQ_REG_GET32(base + 0x108);
	value[4] = CMDQ_REG_GET32(base + 0x10C);
	value[5] = CMDQ_REG_GET32(base + 0x110);
	value[6] = CMDQ_REG_GET32(base + 0x120);
	value[7] = CMDQ_REG_GET32(base + 0x124);
	value[8] = CMDQ_REG_GET32(base + 0x128);
	value[9] = CMDQ_REG_GET32(base + 0x12C);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);

	CMDQ_ERR("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, STATUS: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[5], value[6], value[8]);
	CMDQ_ERR("TDSHP OUTPUT_OFFSET: 0x%08x, BLANK_WIDTH: 0x%08x\n",
		value[7], value[9]);
	CMDQ_ERR("TDSHP clk count: %d\n", (s32)atomic_read(&mdp_tdshp_clk));
}

void cmdq_mdp_dump_aal(const unsigned long base, const char *label)
{
	uint32_t value[9] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_AAL_INTSTA       */
	value[1] = CMDQ_REG_GET32(base + 0x010);    /* MDP_AAL_STATUS       */
	value[2] = CMDQ_REG_GET32(base + 0x024);    /* MDP_AAL_INPUT_COUNT  */
	value[3] = CMDQ_REG_GET32(base + 0x028);    /* MDP_AAL_OUTPUT_COUNT */
	value[4] = CMDQ_REG_GET32(base + 0x030);    /* MDP_AAL_SIZE         */
	value[5] = CMDQ_REG_GET32(base + 0x034);    /* MDP_AAL_OUTPUT_SIZE  */
	value[6] = CMDQ_REG_GET32(base + 0x038);    /* MDP_AAL_OUTPUT_OFFSET*/
	value[7] = CMDQ_REG_GET32(base + 0x4EC);    /* MDP_AAL_TILE_00      */
	value[8] = CMDQ_REG_GET32(base + 0x4F0);    /* MDP_AAL_TILE_01      */
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("AAL_INTSTA: 0x%08x, AAL_STATUS: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR(
		"AAL_INPUT_COUNT: 0x%08x, AAL_OUTPUT_COUNT: 0x%08x, AAL_SIZE: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("AAL_OUTPUT_SIZE: 0x%08x, AAL_OUTPUT_OFFSET: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR("AAL_TILE_00: 0x%08x, AAL_TILE_01: 0x%08x\n",
		value[7], value[8]);
}
void cmdq_mdp_dump_hdr(const unsigned long base, const char *label)
{
	uint32_t value[15] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);    /* MDP_HDR_TOP            */
	value[1] = CMDQ_REG_GET32(base + 0x004);    /* MDP_HDR_RELAY          */
	value[2] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_HDR_INTSTA         */
	value[3] = CMDQ_REG_GET32(base + 0x010);    /* MDP_HDR_ENGSTA         */
	value[4] = CMDQ_REG_GET32(base + 0x020);    /* MDP_HDR_HIST_CTRL_0    */
	value[5] = CMDQ_REG_GET32(base + 0x024);    /* MDP_HDR_HIST_CTRL_1    */
	value[6] = CMDQ_REG_GET32(base + 0x014);    /* MDP_HDR_SIZE_0         */
	value[7] = CMDQ_REG_GET32(base + 0x018);    /* MDP_HDR_SIZE_1         */
	value[8] = CMDQ_REG_GET32(base + 0x01C);    /* MDP_HDR_SIZE_2         */
	value[9] = CMDQ_REG_GET32(base + 0x0EC);    /* MDP_HDR_CURSOR_CTRL    */
	value[10] = CMDQ_REG_GET32(base + 0x0F0);   /* MDP_HDR_CURSOR_POS     */
	value[11] = CMDQ_REG_GET32(base + 0x0F4);   /* MDP_HDR_CURSOR_COLOR   */
	value[12] = CMDQ_REG_GET32(base + 0x0F8);   /* MDP_HDR_TILE_POS       */
	value[13] = CMDQ_REG_GET32(base + 0x0FC);   /* MDP_HDR_CURSOR_BUF0    */
	value[14] = CMDQ_REG_GET32(base + 0x100);   /* MDP_HDR_CURSOR_BUF1    */
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("HDR_TOP: 0x%08x, HDR_RELAY: 0x%08x, HDR_INTSTA: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"HDR_ENGSTA: 0x%08x, HDR_HIST_CTRL0: 0x%08x, HDR_HIST_CTRL1: 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR("HDR_SIZE_0: 0x%08x, HDR_SIZE_1: 0x%08x, HDR_SIZE_2: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"HDR_CURSOR_CTRL: 0x%08x, HDR_CURSOR_POS: 0x%08x, HDR_CURSOR_COLOR: 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"HDR_TILE_POS: 0x%08x, HDR_CURSOR_BUF0: 0x%08x, HDR_CURSOR_BUF1: 0x%08x\n",
		value[12], value[13], value[14]);
}
int32_t cmdqMdpClockOn(uint64_t engineFlag)
{
	CMDQ_MSG("Enable MDP(0x%llx) clock begin\n", engineFlag);
#ifdef CMDQ_PWR_AWARE
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_CAMIN);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_CAMIN2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP0);
	if ((engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) &&
		!cmdq_mdp_clock_is_enable_MDP_TDSHP0()) {
		CMDQ_ERR("enable tdshp but not on\n");
		dump_stack();
	}
#ifdef CMDQ_MDP_COLOR
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR0);
#endif
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT1);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR0);
#else
	CMDQ_MSG("Force MDP clock all on\n");
	/* enable all bits in MMSYS_CG_CLR0 and MMSYS_CG_CLR1 */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x108, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x118, 0xFFFFFFFF);
#endif				/* #ifdef CMDQ_PWR_AWARE */
	CMDQ_MSG("Enable MDP(0x%llx) clock end\n", engineFlag);
	return 0;
}
struct MODULE_BASE {
	uint64_t engine;
	/* considering 64 bit kernel, use long type to store base addr */
	long base;
	const char *name;
};
#define DEFINE_MODULE(eng, base) {eng, base, #eng}
int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0))
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1))
		cmdq_mdp_dump_rdma(MDP_RDMA1_BASE, "RDMA1");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0))
		cmdq_mdp_dump_aal(MDP_AAL_BASE, "AAL0");
	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0))
		cmdq_mdp_dump_hdr(MDP_HDR_BASE, "HDR0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ0_BASE, "RSZ0");
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ1_BASE, "RSZ1");
	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP_BASE, "TDSHP");
#ifdef CMDQ_MDP_COLOR
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		CMDQ_ERR("COLOR : %s\n", "MDP");
		cmdq_mdp_dump_color(MDP_COLOR_BASE, "COLOR0");
	}
#else
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		CMDQ_ERR("COLOR : %s\n", "DISP");
		cmdq_mdp_dump_color(MDP_COLOR_BASE, "COLOR0");
	}
#endif

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0))
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1))
		cmdq_mdp_dump_rot(MDP_WROT1_BASE, "WROT1");
	/* verbose case, dump entire 1KB HW register region */
	/* for each enabled HW module. */
	if (logLevel >= 1) {
		int inner = 0;
		const struct MODULE_BASE bases[] = {
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA1, MDP_RDMA1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ0, MDP_RSZ0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ1, MDP_RSZ1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR0, MDP_COLOR_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT0, MDP_WROT0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT1, MDP_WROT1_BASE),
		};
		for (inner = 0; inner < ARRAY_SIZE(bases); ++inner) {
			if (engineFlag & (1LL << bases[inner].engine)) {
				CMDQ_ERR(
					"========= [CMDQ] %s dump base 0x%lx ========\n",
					bases[inner].name, bases[inner].base);
				print_hex_dump(KERN_ERR, "",
					DUMP_PREFIX_ADDRESS, 32, 4,
					(void *)bases[inner].base, 1024,
					false);
			}
		}
	}
	return 0;
}
enum MOUT_BITS {
	MOUT_BITS_IPU_MDP = 8,	/* bit  6: IPU_MDP multiple outupt reset */
	MOUT_BITS_ISP_MDP = 9,	/* bit  0: ISP_MDP multiple outupt reset */
	MOUT_BITS_MDP_AAL = 10,	/* bit  7: MDP_AAL multiple outupt reset */
	MOUT_BITS_MDP_COLOR = 11, /* bit  5: MDP_COLOR multiple outupt reset */
	MOUT_BITS_MDP_RDMA0 = 12, /* bit  1: MDP_RDMA0 multiple outupt reset */
	MOUT_BITS_MDP_RDMA1 = 13, /* bit  2: MDP_RDMA1 multiple outupt reset */
	MOUT_BITS_MDP_PRZ0 = 14, /* bit  3: MDP_PRZ0 multiple outupt reset */
	MOUT_BITS_MDP_PRZ1 = 15, /* bit  4: MDP_PRZ1 multiple outupt reset */
};
int32_t cmdqMdpResetEng(uint64_t engineFlag)
{
#ifndef CMDQ_PWR_AWARE
	return 0;
#else
	int status = 0;
	int64_t engineToResetAgain = 0LL;
	uint32_t mout_bits_old = 0L;
	uint32_t mout_bits = 0L;
	long MMSYS_MOUT_RST_REG = MMSYS_CONFIG_BASE + (0x050);

	CMDQ_PROF_START(0, "MDP_Rst");
	CMDQ_VERBOSE("Reset MDP(0x%llx) begin\n", engineFlag);
	/* After resetting each component, */
	/* we need also reset corresponding MOUT config. */
	mout_bits_old = CMDQ_REG_GET32(MMSYS_MOUT_RST_REG);
	mout_bits = 0;
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA0);
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA0,
			MDP_RDMA0_BASE + 0x8,
			MDP_RDMA0_BASE + 0x408, 0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA0);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1)) {
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA1);
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA1,
			MDP_RDMA1_BASE + 0x8,
			MDP_RDMA1_BASE + 0x408, 0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA1);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ0);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ1);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0))
		mout_bits |= (1 << MOUT_BITS_MDP_COLOR);

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0x010,
			MDP_WROT0_BASE + 0x014, 0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT0);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT1,
			MDP_WROT1_BASE + 0x010,
			MDP_WROT1_BASE + 0x014, 0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT1);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		/* MDP_CAMIN can only reset by mmsys, */
		/* so this is not a "error" */
		cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN2)) {
		/* MDP_CAMIN can only reset by mmsys, */
		/* so this is not a "error" */
		cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN2));
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_AAL);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL0)) {
			CMDQ_REG_SET32(MDP_AAL_BASE + 0x04, 0x1);
			CMDQ_REG_SET32(MDP_AAL_BASE + 0x04, 0x0);
		}
	}
	/**
	 * when MDP engines fail to reset,
	 * 1. print SMI debug log
	 * 2. try resetting from MMSYS to restore system state
	 * 3. report to QA by raising AEE warning
	 * this reset will reset all registers to power on state.
	 * but DpFramework always reconfigures register values,
	 * so there is no need to backup registers.
	 */
	if (engineToResetAgain != 0) {
		CMDQ_ERR(
			"Reset failed MDP engines(0x%llx), reset again with MMSYS_SW0_RST_B\n",
			engineToResetAgain);
		cmdq_mdp_reset_with_mmsys(engineToResetAgain);
		/* finally, raise AEE warning to report normal reset fail.
		 * we hope that reset MMSYS.
		 */
		CMDQ_AEE("MDP", "Disable 0x%llx engine failed\n",
			engineToResetAgain);
		status = -EFAULT;
	}
	/* MOUT configuration reset */
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old | mout_bits));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));
	CMDQ_MSG("Reset MDP(0x%llx) end\n", engineFlag);
	CMDQ_PROF_END(0, "MDP_Rst");
	return status;
#endif				/* #ifdef CMDQ_PWR_AWARE */
}
int32_t cmdqMdpClockOff(uint64_t engineFlag)
{
#ifdef CMDQ_PWR_AWARE
	CMDQ_MSG("Disable MDP(0x%llx) clock begin\n", engineFlag);
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0X010, MDP_WROT0_BASE + 0X014, 0x1,
			0x1, true);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT1)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT1,
			MDP_WROT1_BASE + 0X010, MDP_WROT1_BASE + 0X014, 0x1,
			0x1, true);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR0)) {
			CMDQ_MSG("Disable MDP_HDR clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL0)) {
			CMDQ_MSG("Disable MDP_AAL clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_MSG("Disable MDP_RSZ1 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ1);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_MSG("Disable MDP_RSZ0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ0);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA0,
			MDP_RDMA0_BASE + 0x008,
			MDP_RDMA0_BASE + 0x408, 0x7FF00, 0x100, false);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA1)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA1,
			MDP_RDMA1_BASE + 0x008,
			MDP_RDMA1_BASE + 0x408, 0x7FF00, 0x100, false);
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CAMIN)) {
			cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
			CMDQ_MSG("Disable MDP_CAMIN clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CAMIN);
		}
	}
	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CAMIN2)) {
			cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN2));
			CMDQ_MSG("Disable MDP_CAMIN clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CAMIN2);
		}
	}
#ifdef CMDQ_MDP_COLOR
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR0)) {
			CMDQ_MSG("Disable MDP_COLOR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR0);
		}
	}
#endif
	CMDQ_MSG("Disable MDP(0x%llx) clock end\n", engineFlag);
#endif				/* #ifdef CMDQ_PWR_AWARE */
	return 0;
}
void cmdqMdpInitialSetting(void)
{
#ifdef COFNIG_MTK_IOMMU
	char *data = kzalloc(MDP_DISPATCH_KEY_STR_LEN, GFP_KERNEL);

	/* Register ION Translation Fault function */
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_RDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_RDMA1,
		cmdq_TranslationFault_callback, (void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WROT0_R,
		cmdq_TranslationFault_callback, (void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WROT1_R,
		cmdq_TranslationFault_callback, (void *)data);
#elif defined(CONFIG_MTK_M4U)
	char *data = kzalloc(MDP_DISPATCH_KEY_STR_LEN, GFP_KERNEL);

	/* Register M4U Translation Fault function */
	m4u_register_fault_callback(M4U_PORT_MDP_RDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	m4u_register_fault_callback(M4U_PORT_MDP_RDMA1,
		cmdq_TranslationFault_callback, (void *)data);
	m4u_register_fault_callback(M4U_PORT_MDP_WROT0_R,
		cmdq_TranslationFault_callback, (void *)data);
	m4u_register_fault_callback(M4U_PORT_MDP_WROT1_R,
		cmdq_TranslationFault_callback, (void *)data);
#endif
}
uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void)
{
	return 0xF00;
}
uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}
uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

const char *cmdq_mdp_parse_error_module(const struct cmdqRecStruct *task)
{
	const char *module = NULL;
	const u32 ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) |
		 (1LL << CMDQ_ENG_ISP_IMGO))
	};
	const u32 WPE_ONLY = ((1LL << CMDQ_ENG_WPEI) | (1LL << CMDQ_ENG_WPEO));

	/* common part for both normal and secure path */
	/* for JPEG scenario, use HW flag is sufficient */
	if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
		module = "JPGENC";
	else if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
		module = "JPGDEC";
	else if ((ISP_ONLY[0] == task->engineFlag) ||
		(ISP_ONLY[1] == task->engineFlag))
		module = "ISP_ONLY";
	else if (task->engineFlag == WPE_ONLY)
		module = "WPE_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!task->secData.is_secure) {
			/* normal path */
			/* need parse current running instruciton */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(task->engineFlag)) {
			module = "MDP";
			break;
		} else if (CMDQ_ENG_DPE_GROUP_FLAG(task->engineFlag)) {
			module = "DPE";
			break;
		} else if (CMDQ_ENG_RSC_GROUP_FLAG(task->engineFlag)) {
			module = "RSC";
			break;
		} else if (CMDQ_ENG_GEPF_GROUP_FLAG(task->engineFlag)) {
			module = "GEPF";
			break;
		} else if (CMDQ_ENG_EAF_GROUP_FLAG(task->engineFlag)) {
			module = "EAF";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
}

u64 cmdq_mdp_get_engine_group_bits(u32 engine_group)
{
	return gCmdqEngineGroupBits[engine_group];
}
void testcase_clkmgr_mdp(void)
{
#if defined(CMDQ_PWR_AWARE)
	/* RDMA clk test with src buffer addr */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RDMA0,
		"CMDQ_TEST_MDP_RDMA0",
		MDP_RDMA0_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		0xAACCBBDD,
		MDP_RDMA0_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		true);
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RDMA1,
		"CMDQ_TEST_MDP_RDMA1",
		MDP_RDMA1_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		0xAACCBBDD,
		MDP_RDMA1_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		true);
	/* WROT clk test with dst buffer addr */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_WROT0,
		"CMDQ_TEST_MDP_WROT0",
		MDP_WROT0_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		0xAACCBBDD,
		MDP_WROT0_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		true);
	testcase_clkmgr_impl(CMDQ_ENG_MDP_WROT1,
		"CMDQ_TEST_MDP_WROT1",
		MDP_WROT1_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		0xAACCBBDD,
		MDP_WROT1_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		true);
	/* TDSHP clk test with input size */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_TDSHP0,
			     "CMDQ_TEST_MDP_TDSHP",
		MDP_TDSHP_BASE + 0x40, 0xAACCBBDD, MDP_TDSHP_BASE + 0x40,
		true);
	/* RSZ clk test with debug port */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RSZ0,
		"CMDQ_TEST_MDP_RSZ0",
		MDP_RSZ0_BASE + 0x040, 0x00000001, MDP_RSZ0_BASE + 0x044,
		false);
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RSZ1,
		"CMDQ_TEST_MDP_RSZ1",
		MDP_RSZ1_BASE + 0x040, 0x00000001, MDP_RSZ1_BASE + 0x044,
		false);
	/* COLOR clk test with debug port */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_COLOR0,
		"CMDQ_TEST_MDP_COLOR",
		MDP_COLOR_BASE + 0x438, 0x000001AB, MDP_COLOR_BASE + 0x438,
		true);
#endif
}

static void cmdq_mdp_enable_common_clock(bool enable)
{
#ifdef CMDQ_PWR_AWARE
#ifdef CONFIG_MTK_SMI_EXT
	if (enable) {
		/* Use SMI clock API */
		smi_bus_prepare_enable(SMI_LARB1, "MDP");

	} else {
		/* disable, reverse the sequence */
		smi_bus_disable_unprepare(SMI_LARB1, "MDP");
	}
#endif	/* CONFIG_MTK_SMI_EXT */
#endif	/* CMDQ_PWR_AWARE */
}


#define CMDQ_ENGINE_TRANS(eng_flags, eng_flags_sec, ENGINE) \
	do {	\
		if ((1LL << CMDQ_ENG_##ENGINE) & (eng_flags)) \
			(eng_flags_sec) |= (1LL << CMDQ_SEC_##ENGINE); \
	} while (0)

u64 cmdq_mdp_get_secure_engine(u64 engine_flags)
{
	u64 sec_eng_flag = 0;

	/* ISP */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMGI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_VIPI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_LCEI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMG2O);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMG3O);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_SMXIO);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_DMGI_DEPI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_IMGCI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, ISP_TIMGO);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, DPE);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, OWE);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEO);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEI2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEO2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, FDVT);

	return sec_eng_flag;
}

void cmdq_mdp_platform_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc = cmdq_mdp_get_func();

#ifdef CONFIG_MTK_SMI_EXT
	pFunc->translatePort = cmdq_mdp_translate_port;
	pFunc->getRequest = cmdq_mdp_get_request;
	pFunc->initPmqosMdp = cmdq_mdp_init_pmqos_mdp;
	pFunc->initPmqosIsp = cmdq_mdp_init_pmqos_isp;
#endif	/* CONFIG_MTK_SMI_EXT */
	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config;
	pFunc->vEncDumpInfo = cmdqVEncDumpInfo;
	pFunc->initModuleBaseVA = cmdq_mdp_init_module_base_VA;
	pFunc->deinitModuleBaseVA = cmdq_mdp_deinit_module_base_VA;
	pFunc->mdpClockIsOn = cmdq_mdp_clock_is_on;
	pFunc->enableMdpClock = cmdq_mdp_enable_clock;
	pFunc->initModuleCLK = cmdq_mdp_init_module_clk;
	pFunc->mdpDumpRsz = cmdq_mdp_dump_rsz;
	pFunc->mdpDumpTdshp = cmdq_mdp_dump_tdshp;
	pFunc->mdpClockOn = cmdqMdpClockOn;
	pFunc->mdpDumpInfo = cmdqMdpDumpInfo;
	pFunc->mdpResetEng = cmdqMdpResetEng;
	pFunc->mdpClockOff = cmdqMdpClockOff;
	pFunc->mdpInitialSet = cmdqMdpInitialSetting;
	pFunc->rdmaGetRegOffsetSrcAddr = cmdq_mdp_rdma_get_reg_offset_src_addr;
	pFunc->wrotGetRegOffsetDstAddr = cmdq_mdp_wrot_get_reg_offset_dst_addr;
	pFunc->parseErrModByEngFlag = cmdq_mdp_parse_error_module;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits;
	pFunc->testcaseClkmgrMdp = testcase_clkmgr_mdp;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock;
	pFunc->mdpGetSecEngine = cmdq_mdp_get_secure_engine;
}
