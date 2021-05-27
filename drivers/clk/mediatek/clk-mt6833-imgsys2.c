/*
 * Copyright (c) 2020 MediaTek Inc.
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

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6833-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP			1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(13), BIT(13));

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate imgsys2_clks[] = {
	GATE_IMGSYS2(CLK_IMGSYS2_LARB9, "imgsys2_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS2(CLK_IMGSYS2_LARB10, "imgsys2_larb10",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS2(CLK_IMGSYS2_MFB, "imgsys2_mfb",
			"img1_ck"/* parent */, 6),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"img1_ck"/* parent */, 7),
	GATE_IMGSYS2(CLK_IMGSYS2_MSS, "imgsys2_mss",
			"img1_ck"/* parent */, 8),
	GATE_IMGSYS2(CLK_IMGSYS2_GALS, "imgsys2_gals",
			"img1_ck"/* parent */, 12),
};

static int clk_mt6833_imgsys2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMGSYS2_NR_CLK);

	mtk_clk_register_gates(node, imgsys2_clks, ARRAY_SIZE(imgsys2_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6833_imgsys2[] = {
	{ .compatible = "mediatek,mt6833-imgsys2", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_imgsys2_drv = {
	.probe = clk_mt6833_imgsys2_probe,
	.driver = {
		.name = "clk-mt6833-imgsys2",
		.of_match_table = of_match_clk_mt6833_imgsys2,
	},
};

builtin_platform_driver(clk_mt6833_imgsys2_drv);

#else

static struct platform_driver clk_mt6833_imgsys2_drv = {
	.probe = clk_mt6833_imgsys2_probe,
	.driver = {
		.name = "clk-mt6833-imgsys2",
		.of_match_table = of_match_clk_mt6833_imgsys2,
	},
};
static int __init clk_mt6833_imgsys2_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_imgsys2_drv);
}
arch_initcall(clk_mt6833_imgsys2_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
