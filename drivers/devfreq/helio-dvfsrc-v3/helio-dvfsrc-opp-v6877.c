/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-qos.h>

#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif

#define V_VMODE_SHIFT 0
#define V_CT_SHIFT 5
#define V_CT_TEST_SHIFT 6
#define V_OPP_TYPE_SHIFT 20

static int dvfsrc_rsrv;

#ifndef CONFIG_MEDIATEK_DRAMC
static int mtk_dramc_get_steps_freq(unsigned int step)
{
	pr_info("get dram steps_freq fail\n");
	return 4266;
}
#endif


void dvfsrc_opp_level_mapping(void)
{
	set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_1, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_2, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_3, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_4, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_5, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_6, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_7, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_8, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_9, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_10, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_11, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_12, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_13, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_14, VCORE_OPP_4);
	set_vcore_opp(VCORE_DVFS_OPP_15, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_16, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_17, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_18, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_19, VCORE_OPP_4);
	set_vcore_opp(VCORE_DVFS_OPP_20, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_21, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_22, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_23, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_24, VCORE_OPP_4);
	set_vcore_opp(VCORE_DVFS_OPP_25, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_26, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_27, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_28, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_29, VCORE_OPP_4);

	set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_0);
	set_ddr_opp(VCORE_DVFS_OPP_1, DDR_OPP_1);
	set_ddr_opp(VCORE_DVFS_OPP_2, DDR_OPP_1);
	set_ddr_opp(VCORE_DVFS_OPP_3, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_4, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_5, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_6, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_7, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_8, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_9, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_10, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_11, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_12, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_13, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_14, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_15, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_16, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_17, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_18, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_19, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_20, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_21, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_22, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_23, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_24, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_25, DDR_OPP_7);
	set_ddr_opp(VCORE_DVFS_OPP_26, DDR_OPP_7);
	set_ddr_opp(VCORE_DVFS_OPP_27, DDR_OPP_7);
	set_ddr_opp(VCORE_DVFS_OPP_28, DDR_OPP_7);
	set_ddr_opp(VCORE_DVFS_OPP_29, DDR_OPP_7);
}

void dvfsrc_opp_table_init(void)
{
	int i;
	int vcore_opp, ddr_opp;

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++) {
		vcore_opp = get_vcore_opp(i);
		ddr_opp = get_ddr_opp(i);

		if (vcore_opp == VCORE_OPP_UNREQ || ddr_opp == DDR_OPP_UNREQ) {
			set_opp_table(i, 0, 0);
			continue;
		}
		set_opp_table(i, get_vcore_uv_table(vcore_opp),
			get_opp_ddr_freq(ddr_opp));
	}
}

static int __init dvfsrc_opp_init(void)
{
	struct device_node *dvfsrc_node = NULL;
	int vcore_opp_0_uv, vcore_opp_1_uv, vcore_opp_2_uv, vcore_opp_3_uv;
	int vcore_opp_4_uv;
	int is_vcore_ct = 0;
	int dvfs_v_mode = 0;
	int opp_type = 0;
	void __iomem *dvfsrc_base;

	set_pwrap_cmd(VCORE_OPP_0, 0);
	set_pwrap_cmd(VCORE_OPP_1, 1);
	set_pwrap_cmd(VCORE_OPP_2, 2);
	set_pwrap_cmd(VCORE_OPP_3, 3);
	set_pwrap_cmd(VCORE_OPP_4, 4);

	dvfsrc_node =
		of_find_compatible_node(NULL, NULL, "mediatek,dvfsrc");

	/* For Doe */
	if (dvfsrc_node) {
		dvfsrc_base = of_iomap(dvfsrc_node, 0);
		if (dvfsrc_base) {
			dvfsrc_rsrv = readl(dvfsrc_base + 0x610);
			iounmap(dvfsrc_base);
		}
		pr_info("%s: vcore_arg = %08x\n", __func__, dvfsrc_rsrv);
		dvfs_v_mode = (dvfsrc_rsrv >> V_VMODE_SHIFT) & 0x3;
		is_vcore_ct = (dvfsrc_rsrv >> V_CT_SHIFT) & 0x1;
		opp_type = (dvfsrc_rsrv >> V_OPP_TYPE_SHIFT) & 0x3;
	}

	if (opp_type == 0)
		vcore_opp_0_uv = 725000;
	else
		vcore_opp_0_uv = 750000;

	vcore_opp_1_uv = 725000;
	vcore_opp_2_uv = 650000;
	vcore_opp_3_uv = 600000;
	vcore_opp_4_uv = 550000;

	if (dvfs_v_mode == 3) {
		/* LV */
		vcore_opp_0_uv = rounddown((vcore_opp_0_uv * 95) / 100, 6250);
		vcore_opp_1_uv = rounddown((vcore_opp_1_uv * 95) / 100, 6250);
		vcore_opp_2_uv = rounddown((vcore_opp_2_uv * 95) / 100, 6250);
		vcore_opp_3_uv = rounddown((vcore_opp_3_uv * 95) / 100, 6250);
		vcore_opp_4_uv = rounddown((vcore_opp_4_uv * 95) / 100, 6250);
	} else if (dvfs_v_mode == 1) {
		/* HV */
		vcore_opp_0_uv = roundup((vcore_opp_0_uv * 105) / 100, 6250);
		vcore_opp_1_uv = roundup((vcore_opp_1_uv * 105) / 100, 6250);
		vcore_opp_2_uv = roundup((vcore_opp_2_uv * 105) / 100, 6250);
		vcore_opp_3_uv = roundup((vcore_opp_3_uv * 105) / 100, 6250);
		vcore_opp_4_uv = roundup((vcore_opp_4_uv * 105) / 100, 6250);
	}

	pr_info("%s: VMODE=%d, RSV4=%x\n",
			__func__,
			dvfs_v_mode,
			dvfsrc_rsrv);

	pr_info("%s: FINAL vcore_opp_uv: %d, %d, %d, %d, %d\n",
		__func__,
		vcore_opp_0_uv,
		vcore_opp_1_uv,
		vcore_opp_2_uv,
		vcore_opp_3_uv,
		vcore_opp_4_uv);

	set_vcore_uv_table(VCORE_OPP_0, vcore_opp_0_uv);
	set_vcore_uv_table(VCORE_OPP_1, vcore_opp_1_uv);
	set_vcore_uv_table(VCORE_OPP_2, vcore_opp_2_uv);
	set_vcore_uv_table(VCORE_OPP_3, vcore_opp_3_uv);
	set_vcore_uv_table(VCORE_OPP_4, vcore_opp_4_uv);

	return 0;
}

fs_initcall_sync(dvfsrc_opp_init)

static int __init dvfsrc_dram_opp_init(void)
{
	int i;

	for (i = 0; i < DDR_OPP_NUM; i++)
		set_opp_ddr_freq(i, mtk_dramc_get_steps_freq(i) * 1000);

	return 0;
}

device_initcall_sync(dvfsrc_dram_opp_init)
