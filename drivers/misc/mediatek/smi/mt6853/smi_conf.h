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

#ifndef __SMI_CONF_H__
#define __SMI_CONF_H__

#include <soc/mediatek/smi.h>
#include "smi_hw.h"
#include "../smi_reg.h"
#include "../smi_conf_dbg.h"

#define SMI_SCEN_NUM		1
#define SMI_ESL_INIT		0
#define SMI_ESL_VPWFD		(SMI_ESL_INIT)
#define SMI_ESL_ICFP		(SMI_ESL_INIT)
#define SMI_ESL_VR4K		(SMI_ESL_INIT)

static u32 smi_scen_map[SMI_BWC_SCEN_CNT] = {
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_INIT, SMI_ESL_INIT, SMI_ESL_INIT,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VPWFD, SMI_ESL_VPWFD,
	SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K, SMI_ESL_VR4K,
	SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_ICFP, SMI_ESL_INIT
};

static u32 smi_larb_cmd_gp_en_port[SMI_LARB_NUM][2] = {
	{5, 8}, {5, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static u32 smi_larb_bw_thrt_en_port[SMI_LARB_NUM][2] = { /* non-HRT */
	{SMI_LARB0_PORT_NUM - 1, SMI_LARB0_PORT_NUM},
	{SMI_LARB1_PORT_NUM - 1, SMI_LARB1_PORT_NUM},
	{0, SMI_LARB2_PORT_NUM}, {0, 0},
	{0, SMI_LARB4_PORT_NUM}, {0, 0}, {0, 0},
	{0, SMI_LARB7_PORT_NUM}, {0, 0},
	{0, SMI_LARB9_PORT_NUM}, {0, 0}, {0, SMI_LARB11_PORT_NUM}, {0, 0},
	{SMI_LARB13_PORT_NUM - 1, SMI_LARB13_PORT_NUM}, {0, 0},
	{0, 0}, {0, 0}, {0, 0}, {0, 0},
	{0, SMI_LARB19_PORT_NUM}, {0, SMI_LARB20_PORT_NUM},
};

/* conf */
#define SMI_COMM_CONF_NUM	(7)
struct mtk_smi_pair smi_comm_conf_pair[SMI_COMM_CONF_NUM] = {
	{SMI_L1LEN, 0xb}, {SMI_BUS_SEL, 0x4514}, {SMI_M4U_TH, 0xe100e10},
	{SMI_FIFO_TH1, 0x9100910}, {SMI_FIFO_TH2, 0x5060910},
	{SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},
};

#define SMI_SUB_COMM_CONF_NUM	(4)
struct mtk_smi_pair smi_sub_comm_conf_pair[SMI_SUB_COMM_CONF_NUM] = {
	{SMI_L1LEN, 0xa}, {SMI_PREULTRA_MASK1, 0x2105},
	{SMI_DCM, 0x4f1}, {SMI_DUMMY, 0x1},
};

#define SMI_LARB0_CONF_NUM	(2)
struct mtk_smi_pair smi_larb0_conf_pair[SMI_LARB0_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_SW_FLAG, 0x1},
};

#define SMI_LARB15_CONF_NUM	(2)
struct mtk_smi_pair smi_larb15_conf_pair[SMI_LARB15_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x300256}, {SMI_LARB_SW_FLAG, 0x1},
};

#define SMI_LARB16_CONF_NUM	(3)
struct mtk_smi_pair smi_larb16_conf_pair[SMI_LARB16_CONF_NUM] = {
	{SMI_LARB_CMD_THRT_CON, 0x370256}, {SMI_LARB_FORCE_ULTRA, 0x8000},
	{SMI_LARB_SW_FLAG, 0x1},
};

u32 smi_conf_pair_num[SMI_DEV_NUM] = {
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_LARB15_CONF_NUM, SMI_LARB16_CONF_NUM, SMI_LARB16_CONF_NUM,
	SMI_LARB16_CONF_NUM, SMI_LARB0_CONF_NUM, SMI_LARB0_CONF_NUM,
	SMI_COMM_CONF_NUM, SMI_SUB_COMM_CONF_NUM, SMI_SUB_COMM_CONF_NUM,
	SMI_SUB_COMM_CONF_NUM, SMI_SUB_COMM_CONF_NUM, SMI_SUB_COMM_CONF_NUM,
	SMI_SUB_COMM_CONF_NUM, SMI_SUB_COMM_CONF_NUM,
};

struct mtk_smi_pair *smi_conf_pair[SMI_DEV_NUM] = {
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb0_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_larb15_conf_pair, smi_larb16_conf_pair, smi_larb16_conf_pair,
	smi_larb16_conf_pair, smi_larb0_conf_pair, smi_larb0_conf_pair,
	smi_comm_conf_pair, smi_sub_comm_conf_pair, smi_sub_comm_conf_pair,
	smi_sub_comm_conf_pair, smi_sub_comm_conf_pair, smi_sub_comm_conf_pair,
	smi_sub_comm_conf_pair, smi_sub_comm_conf_pair,
};

/* scen: INIT */
struct mtk_smi_pair smi_comm_init_pair[SMI_COMM_MASTER_NUM] = {
	{SMI_L1ARB(0), 0x0}, {SMI_L1ARB(1), 0x0}, {SMI_L1ARB(2), 0x0},
	{SMI_L1ARB(3), 0x0}, {SMI_L1ARB(4), 0x0}, {SMI_L1ARB(5), 0x0},
	{SMI_L1ARB(6), 0x0}, {SMI_L1ARB(7), 0x0},
};

struct mtk_smi_pair smi_mdp_comm_init_pair[SMI_COMM_MASTER_NUM] = {
	{SMI_L1ARB(0), 0x0}, {SMI_L1ARB(1), 0x0}, {SMI_L1ARB(2), 0x0},
	{SMI_L1ARB(3), 0x0}, {SMI_L1ARB(4), 0x0}, {SMI_L1ARB(5), 0x0},
	{SMI_L1ARB(6), 0x0}, {SMI_L1ARB(7), 0x0},
};

struct mtk_smi_pair smi_sram_comm_init_pair[SMI_COMM_MASTER_NUM] = {
	{SMI_L1ARB(0), 0x0}, {SMI_L1ARB(1), 0x0}, {SMI_L1ARB(2), 0x0},
	{SMI_L1ARB(3), 0x0}, {SMI_L1ARB(4), 0x0}, {SMI_L1ARB(5), 0x0},
	{SMI_L1ARB(6), 0x0}, {SMI_L1ARB(7), 0x0},
};

struct mtk_smi_pair smi_sub_comm_init_pair[SMI_COMM_MASTER_NUM] = {
	{SMI_L1ARB(0), 0x0}, {SMI_L1ARB(1), 0x0}, {SMI_L1ARB(2), 0x0},
	{SMI_L1ARB(3), 0x0}, {SMI_L1ARB(4), 0x0}, {SMI_L1ARB(5), 0x0},
	{SMI_L1ARB(6), 0x0}, {SMI_L1ARB(7), 0x0},
};

struct mtk_smi_pair smi_larb0_init_pair[SMI_LARB0_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2},
	{SMI_LARB_OSTDL_PORT(1), 0x2},
	{SMI_LARB_OSTDL_PORT(2), 0x28},
	{SMI_LARB_OSTDL_PORT(3), 0x28},
};

struct mtk_smi_pair smi_larb1_init_pair[SMI_LARB1_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2},
	{SMI_LARB_OSTDL_PORT(1), 0x18},
	{SMI_LARB_OSTDL_PORT(2), 0xa},
	{SMI_LARB_OSTDL_PORT(3), 0xc},
	{SMI_LARB_OSTDL_PORT(4), 0x28},
};

struct mtk_smi_pair smi_larb2_init_pair[SMI_LARB2_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x5},
	{SMI_LARB_OSTDL_PORT(1), 0x5},
	{SMI_LARB_OSTDL_PORT(2), 0x5},
	{SMI_LARB_OSTDL_PORT(3), 0x5},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
};

struct mtk_smi_pair smi_larb3_init_pair[SMI_LARB3_PORT_NUM] = { };

struct mtk_smi_pair smi_larb4_init_pair[SMI_LARB4_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x28},
	{SMI_LARB_OSTDL_PORT(1), 0x19},
	{SMI_LARB_OSTDL_PORT(2), 0xb},
	{SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1},
	{SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1},
	{SMI_LARB_OSTDL_PORT(9), 0x4},
	{SMI_LARB_OSTDL_PORT(10), 0x1},
	{SMI_LARB_OSTDL_PORT(11), 0x16},
};

struct mtk_smi_pair smi_larb5_init_pair[SMI_LARB5_PORT_NUM] = { };

struct mtk_smi_pair smi_larb6_init_pair[SMI_LARB6_PORT_NUM] = { };

struct mtk_smi_pair smi_larb7_init_pair[SMI_LARB7_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1},
	{SMI_LARB_OSTDL_PORT(1), 0x3},
	{SMI_LARB_OSTDL_PORT(2), 0x2},
	{SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x5},
	{SMI_LARB_OSTDL_PORT(6), 0x2},
	{SMI_LARB_OSTDL_PORT(7), 0x12},
	{SMI_LARB_OSTDL_PORT(8), 0x13},
	{SMI_LARB_OSTDL_PORT(9), 0x4},
	{SMI_LARB_OSTDL_PORT(10), 0x4},
	{SMI_LARB_OSTDL_PORT(11), 0x1},
	{SMI_LARB_OSTDL_PORT(12), 0x4},
};

struct mtk_smi_pair smi_larb8_init_pair[SMI_LARB8_PORT_NUM] = { };

struct mtk_smi_pair smi_larb9_init_pair[SMI_LARB9_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0xa},
	{SMI_LARB_OSTDL_PORT(1), 0x7},
	{SMI_LARB_OSTDL_PORT(2), 0xf},
	{SMI_LARB_OSTDL_PORT(3), 0x8},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x8},
	{SMI_LARB_OSTDL_PORT(6), 0x9},
	{SMI_LARB_OSTDL_PORT(7), 0x3},
	{SMI_LARB_OSTDL_PORT(8), 0x3},
	{SMI_LARB_OSTDL_PORT(9), 0x6},
	{SMI_LARB_OSTDL_PORT(10), 0x7},
	{SMI_LARB_OSTDL_PORT(11), 0x4},
	{SMI_LARB_OSTDL_PORT(12), 0xa},
	{SMI_LARB_OSTDL_PORT(13), 0x3},
	{SMI_LARB_OSTDL_PORT(14), 0x4},
	{SMI_LARB_OSTDL_PORT(15), 0xe},
	{SMI_LARB_OSTDL_PORT(16), 0x1},
	{SMI_LARB_OSTDL_PORT(17), 0x7},
	{SMI_LARB_OSTDL_PORT(18), 0x1},
	{SMI_LARB_OSTDL_PORT(19), 0x1},
	{SMI_LARB_OSTDL_PORT(20), 0x1},
	{SMI_LARB_OSTDL_PORT(21), 0x1},
	{SMI_LARB_OSTDL_PORT(22), 0x1},
	{SMI_LARB_OSTDL_PORT(23), 0x1},
	{SMI_LARB_OSTDL_PORT(24), 0x1},
	{SMI_LARB_OSTDL_PORT(25), 0x1},
	{SMI_LARB_OSTDL_PORT(26), 0x1},
	{SMI_LARB_OSTDL_PORT(27), 0x1},
	{SMI_LARB_OSTDL_PORT(28), 0x1},
};

struct mtk_smi_pair smi_larb10_init_pair[SMI_LARB10_PORT_NUM] = { };

struct mtk_smi_pair smi_larb11_init_pair[SMI_LARB11_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1},
	{SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x1},
	{SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x1},
	{SMI_LARB_OSTDL_PORT(7), 0x1},
	{SMI_LARB_OSTDL_PORT(8), 0x1},
	{SMI_LARB_OSTDL_PORT(9), 0x1},
	{SMI_LARB_OSTDL_PORT(10), 0x1},
	{SMI_LARB_OSTDL_PORT(11), 0x1},
	{SMI_LARB_OSTDL_PORT(12), 0x1},
	{SMI_LARB_OSTDL_PORT(13), 0x1},
	{SMI_LARB_OSTDL_PORT(14), 0x1},
	{SMI_LARB_OSTDL_PORT(15), 0xe},
	{SMI_LARB_OSTDL_PORT(16), 0x1},
	{SMI_LARB_OSTDL_PORT(17), 0x7},
	{SMI_LARB_OSTDL_PORT(18), 0x8},
	{SMI_LARB_OSTDL_PORT(19), 0x7},
	{SMI_LARB_OSTDL_PORT(20), 0x7},
	{SMI_LARB_OSTDL_PORT(21), 0x1},
	{SMI_LARB_OSTDL_PORT(22), 0x6},
	{SMI_LARB_OSTDL_PORT(23), 0x2},
	{SMI_LARB_OSTDL_PORT(24), 0xf},
	{SMI_LARB_OSTDL_PORT(25), 0x8},
	{SMI_LARB_OSTDL_PORT(26), 0x1},
	{SMI_LARB_OSTDL_PORT(27), 0x1},
	{SMI_LARB_OSTDL_PORT(28), 0x1},
};

struct mtk_smi_pair smi_larb12_init_pair[SMI_LARB12_PORT_NUM] = { };

struct mtk_smi_pair smi_larb13_init_pair[SMI_LARB13_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2},
	{SMI_LARB_OSTDL_PORT(1), 0xc},
	{SMI_LARB_OSTDL_PORT(2), 0xc},
	{SMI_LARB_OSTDL_PORT(3), 0x1},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x1},
	{SMI_LARB_OSTDL_PORT(6), 0x6},
	{SMI_LARB_OSTDL_PORT(7), 0x6},
	{SMI_LARB_OSTDL_PORT(8), 0x6},
	{SMI_LARB_OSTDL_PORT(9), 0x12},
	{SMI_LARB_OSTDL_PORT(10), 0x6},
	{SMI_LARB_OSTDL_PORT(11), 0x1},
};

struct mtk_smi_pair smi_larb14_init_pair[SMI_LARB14_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x1},
	{SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x1},
	{SMI_LARB_OSTDL_PORT(3), 0x28},
	{SMI_LARB_OSTDL_PORT(4), 0x12},
	{SMI_LARB_OSTDL_PORT(5), 0x6},
};

struct mtk_smi_pair smi_larb15_init_pair[SMI_LARB15_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x28},
	{SMI_LARB_OSTDL_PORT(1), 0x1},
	{SMI_LARB_OSTDL_PORT(2), 0x2},
	{SMI_LARB_OSTDL_PORT(3), 0x28},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
};

struct mtk_smi_pair smi_larb16_init_pair[SMI_LARB16_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x28},
	{SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x2},
	{SMI_LARB_OSTDL_PORT(3), 0xc},
	{SMI_LARB_OSTDL_PORT(4), 0x18},
	{SMI_LARB_OSTDL_PORT(5), 0x4},
	{SMI_LARB_OSTDL_PORT(6), 0x28},
	{SMI_LARB_OSTDL_PORT(7), 0x14},
	{SMI_LARB_OSTDL_PORT(8), 0x4},
	{SMI_LARB_OSTDL_PORT(9), 0x4},
	{SMI_LARB_OSTDL_PORT(10), 0x4},
	{SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x4},
	{SMI_LARB_OSTDL_PORT(13), 0x2},
	{SMI_LARB_OSTDL_PORT(14), 0x8},
	{SMI_LARB_OSTDL_PORT(15), 0x4},
	{SMI_LARB_OSTDL_PORT(16), 0x4},
};

struct mtk_smi_pair smi_larb17_init_pair[SMI_LARB17_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x28},
	{SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x2},
	{SMI_LARB_OSTDL_PORT(3), 0xc},
	{SMI_LARB_OSTDL_PORT(4), 0x18},
	{SMI_LARB_OSTDL_PORT(5), 0x4},
	{SMI_LARB_OSTDL_PORT(6), 0x28},
	{SMI_LARB_OSTDL_PORT(7), 0x14},
	{SMI_LARB_OSTDL_PORT(8), 0x4},
	{SMI_LARB_OSTDL_PORT(9), 0x4},
	{SMI_LARB_OSTDL_PORT(10), 0x4},
	{SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x4},
	{SMI_LARB_OSTDL_PORT(13), 0x2},
	{SMI_LARB_OSTDL_PORT(14), 0x8},
	{SMI_LARB_OSTDL_PORT(15), 0x4},
	{SMI_LARB_OSTDL_PORT(16), 0x4},
};

struct mtk_smi_pair smi_larb18_init_pair[SMI_LARB18_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x28},
	{SMI_LARB_OSTDL_PORT(1), 0x14},
	{SMI_LARB_OSTDL_PORT(2), 0x2},
	{SMI_LARB_OSTDL_PORT(3), 0xc},
	{SMI_LARB_OSTDL_PORT(4), 0x18},
	{SMI_LARB_OSTDL_PORT(5), 0x4},
	{SMI_LARB_OSTDL_PORT(6), 0x28},
	{SMI_LARB_OSTDL_PORT(7), 0x14},
	{SMI_LARB_OSTDL_PORT(8), 0x4},
	{SMI_LARB_OSTDL_PORT(9), 0x4},
	{SMI_LARB_OSTDL_PORT(10), 0x4},
	{SMI_LARB_OSTDL_PORT(11), 0x2},
	{SMI_LARB_OSTDL_PORT(12), 0x4},
	{SMI_LARB_OSTDL_PORT(13), 0x2},
	{SMI_LARB_OSTDL_PORT(14), 0x8},
	{SMI_LARB_OSTDL_PORT(15), 0x4},
	{SMI_LARB_OSTDL_PORT(16), 0x4},
};

struct mtk_smi_pair smi_larb19_init_pair[SMI_LARB19_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x2},
	{SMI_LARB_OSTDL_PORT(1), 0x2},
	{SMI_LARB_OSTDL_PORT(2), 0x4},
	{SMI_LARB_OSTDL_PORT(3), 0x2},
};

struct mtk_smi_pair smi_larb20_init_pair[SMI_LARB20_PORT_NUM] = {
	{SMI_LARB_OSTDL_PORT(0), 0x9},
	{SMI_LARB_OSTDL_PORT(1), 0x9},
	{SMI_LARB_OSTDL_PORT(2), 0x5},
	{SMI_LARB_OSTDL_PORT(3), 0x5},
	{SMI_LARB_OSTDL_PORT(4), 0x1},
	{SMI_LARB_OSTDL_PORT(5), 0x1},
};

/* scen: ALL */
struct mtk_smi_pair *smi_comm_scen_pair[SMI_SCEN_NUM] = {
	smi_comm_init_pair,
};

struct mtk_smi_pair *smi_sub_comm_scen_pair[SMI_SCEN_NUM] = {
	smi_sub_comm_init_pair,
};

struct mtk_smi_pair *smi_larb0_scen_pair[SMI_SCEN_NUM] = {
	smi_larb0_init_pair,
};

struct mtk_smi_pair *smi_larb1_scen_pair[SMI_SCEN_NUM] = {
	smi_larb1_init_pair,
};

struct mtk_smi_pair *smi_larb2_scen_pair[SMI_SCEN_NUM] = {
	smi_larb2_init_pair,
};

struct mtk_smi_pair *smi_larb3_scen_pair[SMI_SCEN_NUM] = {
	smi_larb3_init_pair,
};

struct mtk_smi_pair *smi_larb4_scen_pair[SMI_SCEN_NUM] = {
	smi_larb4_init_pair,
};

struct mtk_smi_pair *smi_larb5_scen_pair[SMI_SCEN_NUM] = {
	smi_larb5_init_pair,
};

struct mtk_smi_pair *smi_larb6_scen_pair[SMI_SCEN_NUM] = {
	smi_larb6_init_pair,
};

struct mtk_smi_pair *smi_larb7_scen_pair[SMI_SCEN_NUM] = {
	smi_larb7_init_pair,
};

struct mtk_smi_pair *smi_larb8_scen_pair[SMI_SCEN_NUM] = {
	smi_larb8_init_pair,
};

struct mtk_smi_pair *smi_larb9_scen_pair[SMI_SCEN_NUM] = {
	smi_larb9_init_pair,
};

struct mtk_smi_pair *smi_larb10_scen_pair[SMI_SCEN_NUM] = {
	smi_larb10_init_pair,
};

struct mtk_smi_pair *smi_larb11_scen_pair[SMI_SCEN_NUM] = {
	smi_larb11_init_pair,
};

struct mtk_smi_pair *smi_larb12_scen_pair[SMI_SCEN_NUM] = {
	smi_larb12_init_pair,
};

struct mtk_smi_pair *smi_larb13_scen_pair[SMI_SCEN_NUM] = {
	smi_larb13_init_pair,
};

struct mtk_smi_pair *smi_larb14_scen_pair[SMI_SCEN_NUM] = {
	smi_larb14_init_pair,
};

struct mtk_smi_pair *smi_larb15_scen_pair[SMI_SCEN_NUM] = {
	smi_larb15_init_pair,
};

struct mtk_smi_pair *smi_larb16_scen_pair[SMI_SCEN_NUM] = {
	smi_larb16_init_pair,
};

struct mtk_smi_pair *smi_larb17_scen_pair[SMI_SCEN_NUM] = {
	smi_larb17_init_pair,
};

struct mtk_smi_pair *smi_larb18_scen_pair[SMI_SCEN_NUM] = {
	smi_larb18_init_pair,
};

struct mtk_smi_pair *smi_larb19_scen_pair[SMI_SCEN_NUM] = {
	smi_larb19_init_pair,
};

struct mtk_smi_pair *smi_larb20_scen_pair[SMI_SCEN_NUM] = {
	smi_larb20_init_pair,
};

u32 smi_scen_pair_num[SMI_DEV_NUM] = {
	SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM,
	SMI_LARB3_PORT_NUM, SMI_LARB4_PORT_NUM, SMI_LARB5_PORT_NUM,
	SMI_LARB6_PORT_NUM, SMI_LARB7_PORT_NUM, SMI_LARB8_PORT_NUM,
	SMI_LARB9_PORT_NUM,
	SMI_LARB10_PORT_NUM, SMI_LARB11_PORT_NUM, SMI_LARB12_PORT_NUM,
	SMI_LARB13_PORT_NUM, SMI_LARB14_PORT_NUM, SMI_LARB15_PORT_NUM,
	SMI_LARB16_PORT_NUM, SMI_LARB17_PORT_NUM, SMI_LARB18_PORT_NUM,
	SMI_LARB19_PORT_NUM, SMI_LARB20_PORT_NUM,
	SMI_COMM_MASTER_NUM, SMI_COMM_MASTER_NUM, SMI_COMM_MASTER_NUM,
	SMI_COMM_MASTER_NUM, SMI_COMM_MASTER_NUM, SMI_COMM_MASTER_NUM,
	SMI_COMM_MASTER_NUM, SMI_COMM_MASTER_NUM,
};

struct mtk_smi_pair **smi_scen_pair[SMI_DEV_NUM] = {
	smi_larb0_scen_pair, smi_larb1_scen_pair, smi_larb2_scen_pair,
	smi_larb3_scen_pair, smi_larb4_scen_pair, smi_larb5_scen_pair,
	smi_larb6_scen_pair, smi_larb7_scen_pair, smi_larb8_scen_pair,
	smi_larb9_scen_pair,
	smi_larb10_scen_pair, smi_larb11_scen_pair, smi_larb12_scen_pair,
	smi_larb13_scen_pair, smi_larb14_scen_pair, smi_larb15_scen_pair,
	smi_larb16_scen_pair, smi_larb17_scen_pair, smi_larb18_scen_pair,
	smi_larb19_scen_pair, smi_larb20_scen_pair,
	smi_comm_scen_pair, smi_sub_comm_scen_pair, smi_sub_comm_scen_pair,
	smi_sub_comm_scen_pair, smi_sub_comm_scen_pair, smi_sub_comm_scen_pair,
	smi_sub_comm_scen_pair, smi_sub_comm_scen_pair,
};

#define SMI_RSI_DEBUG_NUM	(29 * 2)
#define SMI_RSI_M0_OFFSET	(0x1000)
#define SMI_RSI_M1_OFFSET	(0x2000)
u32 smi_rsi_debug_offset[SMI_RSI_DEBUG_NUM] = {
	SMI_RSI_M0_OFFSET + RSI_INTLV_CON,
	SMI_RSI_M0_OFFSET + RSI_DCM_CON,
	SMI_RSI_M0_OFFSET + RSI_DS_PM_CON,
	SMI_RSI_M0_OFFSET + RSI_MISC_CON,
	SMI_RSI_M0_OFFSET + RSI_STA,
	SMI_RSI_M0_OFFSET + RSI_AWOSTD_S,
	SMI_RSI_M0_OFFSET + RSI_AWOSTD_M0,
	SMI_RSI_M0_OFFSET + RSI_AWOSTD_M1,
	SMI_RSI_M0_OFFSET + RSI_AWOSTD_PSEUDO,
	SMI_RSI_M0_OFFSET + RSI_WOSTD_S,
	SMI_RSI_M0_OFFSET + RSI_WOSTD_M0,
	SMI_RSI_M0_OFFSET + RSI_WOSTD_M1,
	SMI_RSI_M0_OFFSET + RSI_WOSTD_PSEUDO,
	SMI_RSI_M0_OFFSET + RSI_AROSTD_S,
	SMI_RSI_M0_OFFSET + RSI_AROSTD_M0,
	SMI_RSI_M0_OFFSET + RSI_AROSTD_M1,
	SMI_RSI_M0_OFFSET + RSI_AROSTD_PSEUDO,
	SMI_RSI_M0_OFFSET + RSI_WLAST_OWE_CNT_S,
	SMI_RSI_M0_OFFSET + RSI_WLAST_OWE_CNT_M0,
	SMI_RSI_M0_OFFSET + RSI_WLAST_OWE_CNT_M1,
	SMI_RSI_M0_OFFSET + RSI_WDAT_CNT_S,
	SMI_RSI_M0_OFFSET + RSI_WDAT_CNT_M0,
	SMI_RSI_M0_OFFSET + RSI_WDAT_CNT_M1,
	SMI_RSI_M0_OFFSET + RSI_RDAT_CNT_S,
	SMI_RSI_M0_OFFSET + RSI_RDAT_CNT_M0,
	SMI_RSI_M0_OFFSET + RSI_RDAT_CNT_M1,
	SMI_RSI_M0_OFFSET + RSI_AXI_DBG_S,
	SMI_RSI_M0_OFFSET + RSI_AXI_DBG_M0,
	SMI_RSI_M0_OFFSET + RSI_AXI_DBG_M1,

	SMI_RSI_M1_OFFSET + RSI_INTLV_CON,
	SMI_RSI_M1_OFFSET + RSI_DCM_CON,
	SMI_RSI_M1_OFFSET + RSI_DS_PM_CON,
	SMI_RSI_M1_OFFSET + RSI_MISC_CON,
	SMI_RSI_M1_OFFSET + RSI_STA,
	SMI_RSI_M1_OFFSET + RSI_AWOSTD_S,
	SMI_RSI_M1_OFFSET + RSI_AWOSTD_M0,
	SMI_RSI_M1_OFFSET + RSI_AWOSTD_M1,
	SMI_RSI_M1_OFFSET + RSI_AWOSTD_PSEUDO,
	SMI_RSI_M1_OFFSET + RSI_WOSTD_S,
	SMI_RSI_M1_OFFSET + RSI_WOSTD_M0,
	SMI_RSI_M1_OFFSET + RSI_WOSTD_M1,
	SMI_RSI_M1_OFFSET + RSI_WOSTD_PSEUDO,
	SMI_RSI_M1_OFFSET + RSI_AROSTD_S,
	SMI_RSI_M1_OFFSET + RSI_AROSTD_M0,
	SMI_RSI_M1_OFFSET + RSI_AROSTD_M1,
	SMI_RSI_M1_OFFSET + RSI_AROSTD_PSEUDO,
	SMI_RSI_M1_OFFSET + RSI_WLAST_OWE_CNT_S,
	SMI_RSI_M1_OFFSET + RSI_WLAST_OWE_CNT_M0,
	SMI_RSI_M1_OFFSET + RSI_WLAST_OWE_CNT_M1,
	SMI_RSI_M1_OFFSET + RSI_WDAT_CNT_S,
	SMI_RSI_M1_OFFSET + RSI_WDAT_CNT_M0,
	SMI_RSI_M1_OFFSET + RSI_WDAT_CNT_M1,
	SMI_RSI_M1_OFFSET + RSI_RDAT_CNT_S,
	SMI_RSI_M1_OFFSET + RSI_RDAT_CNT_M0,
	SMI_RSI_M1_OFFSET + RSI_RDAT_CNT_M1,
	SMI_RSI_M1_OFFSET + RSI_AXI_DBG_S,
	SMI_RSI_M1_OFFSET + RSI_AXI_DBG_M0,
	SMI_RSI_M1_OFFSET + RSI_AXI_DBG_M1,
};
#endif
