/*
 * Copyright (C) 2017 MediaTek Inc.
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

/**
 * @file	mtk_gpufreq_core
 * @brief   Driver for GPU-DVFS
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/pm_qos.h>

#include "mtk_devinfo.h"
/* #define BRING_UP */

#include "mtk_gpufreq_core.h"
#include "upmu_common.h"
#include "sync_write.h"

/* #include "mtk_thermal_typedefs.h" */
#include "mtk_thermal.h"
#include "mtk_freqhopping.h"
#include "mtk_fhreg.h"
#include "upmu_sw.h"
#include "upmu_hw.h"
#include "mtk_pbm.h"
#include "mt6765_clkmgr.h"
/*#include "mtk_dramc.h"*/
#include "mtk_gpufreq.h"

#ifdef CONFIG_MTK_QOS_SUPPORT
#include "mtk_gpu_bw.h"
#endif

#ifdef MT_GPUFREQ_OPP_STRESS_TEST
#include <linux/random.h>
#endif /* ifdef MT_GPUFREQ_OPP_STRESS_TEST */
#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
#include "mtk_static_power.h"
#include "mtk_static_power_mt6765.h"
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

#include "helio-dvfsrc-opp.h"

/**
 * ===============================================
 * SECTION : Local functions declaration
 * ===============================================
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev);
static void __mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_volt_old,
	unsigned int vsram_volt_new);
static void __mt_gpufreq_set_fixed_volt(int fixed_volt);
static void __mt_gpufreq_set_fixed_freq(int fixed_freq);
static void __mt_gpufreq_bucks_enable(void);
static void __mt_gpufreq_bucks_disable(void);
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode);
static unsigned int __mt_gpufreq_get_cur_volt(void);
static unsigned int __mt_gpufreq_get_cur_freq(void);
static unsigned int __mt_gpufreq_get_cur_vsram_volt(void);
static int __mt_gpufreq_get_opp_idx_by_volt(unsigned int volt);
static unsigned int
__mt_gpufreq_get_vsram_volt_by_target_volt(unsigned int volt);
static unsigned int
__mt_gpufreq_get_limited_freq_by_power(unsigned int limited_power);
static enum g_post_divider_power_enum
__mt_gpufreq_get_post_divider_power(unsigned int freq, unsigned int efuse);
static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc);
static void __mt_gpufreq_kick_pbm(int enable);
static void __mt_gpufreq_clock_switch(unsigned int freq_new);
static void __mt_gpufreq_volt_switch(unsigned int volt_old,
	unsigned int volt_new, unsigned int vsram_volt_old,
	unsigned int vsram_volt_new);
static void
__mt_gpufreq_volt_switch_without_vsram_volt(unsigned int volt_old,
	unsigned int volt_new);
static void __mt_gpufreq_batt_oc_protect(unsigned int limited_idx);
#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
static void __mt_gpufreq_batt_percent_protect(unsigned int limited_index);
#endif
static void __mt_gpufreq_low_batt_protect(unsigned int limited_index);
static void __mt_update_gpufreqs_power_table(void);
static void __mt_gpufreq_update_max_limited_idx(void);
static unsigned int __mt_gpufreq_calculate_dds(unsigned int freq_khz,
		enum g_post_divider_power_enum post_divider_power);
static void __mt_gpufreq_setup_opp_power_table(int num);
static void __mt_gpufreq_calculate_springboard_opp_index(void);
static void
__mt_gpufreq_vsram_gpu_volt_switch(enum g_volt_switch_enum switch_way,
	unsigned int sfchg_rate, unsigned int volt_old, unsigned int volt_new);
static void
__mt_gpufreq_vgpu_volt_switch(enum g_volt_switch_enum switch_way,
	unsigned int sfchg_rate, unsigned int volt_old, unsigned int volt_new);


/**
 * ===============================================
 * SECTION : Local variables definition
 * ===============================================
 */

static struct mt_gpufreq_power_table_info *g_power_table;
static struct g_opp_table_info *g_opp_table;
static struct g_opp_table_info *g_opp_table_default;
static struct g_pmic_info *g_pmic;
static struct g_clk_info *g_clk;
static struct g_opp_table_info g_opp_table_segment1[] = {
GPUOP(SEG1_GPU_DVFS_FREQ0, SEG1_GPU_DVFS_VOLT0, SEG1_GPU_DVFS_VSRAM0, 0),
};
static struct g_opp_table_info g_opp_table_segment2[] = {
GPUOP(SEG2_GPU_DVFS_FREQ0, SEG2_GPU_DVFS_VOLT0, SEG2_GPU_DVFS_VSRAM0, 0),
GPUOP(SEG2_GPU_DVFS_FREQ1, SEG2_GPU_DVFS_VOLT1, SEG2_GPU_DVFS_VSRAM1, 1),
GPUOP(SEG2_GPU_DVFS_FREQ2, SEG2_GPU_DVFS_VOLT2, SEG2_GPU_DVFS_VSRAM2, 2),
};
static struct g_opp_table_info g_opp_table_segment3[] = {
GPUOP(SEG3_GPU_DVFS_FREQ0, SEG3_GPU_DVFS_VOLT0, SEG3_GPU_DVFS_VSRAM0, 0),
GPUOP(SEG3_GPU_DVFS_FREQ1, SEG3_GPU_DVFS_VOLT1, SEG3_GPU_DVFS_VSRAM1, 1),
GPUOP(SEG3_GPU_DVFS_FREQ2, SEG3_GPU_DVFS_VOLT2, SEG3_GPU_DVFS_VSRAM2, 2),
};
static struct g_opp_table_info g_opp_table_segment4[] = {
GPUOP(SEG4_GPU_DVFS_FREQ0, SEG4_GPU_DVFS_VOLT0, SEG4_GPU_DVFS_VSRAM0, 0),
GPUOP(SEG4_GPU_DVFS_FREQ1, SEG4_GPU_DVFS_VOLT1, SEG4_GPU_DVFS_VSRAM1, 1),
GPUOP(SEG4_GPU_DVFS_FREQ2, SEG4_GPU_DVFS_VOLT2, SEG4_GPU_DVFS_VSRAM2, 2),
};
static struct g_opp_table_info g_opp_table_segment5[] = {
GPUOP(SEG5_GPU_DVFS_FREQ0, SEG5_GPU_DVFS_VOLT0, SEG5_GPU_DVFS_VSRAM0, 0),
GPUOP(SEG5_GPU_DVFS_FREQ1, SEG5_GPU_DVFS_VOLT1, SEG5_GPU_DVFS_VSRAM1, 1),
GPUOP(SEG5_GPU_DVFS_FREQ2, SEG5_GPU_DVFS_VOLT2, SEG5_GPU_DVFS_VSRAM2, 2),
};
static const struct of_device_id g_gpufreq_of_match[] = {
	{ .compatible = "mediatek,mt6765-gpufreq" },
	{ /* sentinel */ }
};
static struct platform_driver g_gpufreq_pdrv = {
	.probe = __mt_gpufreq_pdrv_probe,
	.remove = NULL,
	.driver = {
		.name = "gpufreq",
		.owner = THIS_MODULE,
		.of_match_table = g_gpufreq_of_match,
	},
};

static bool g_parking;
static bool g_DVFS_is_paused_by_ptpod;
static bool g_volt_enable_state;
static bool g_keep_opp_freq_state;
static bool g_opp_stress_test_state;
static bool g_fixed_freq_volt_state;
static bool g_pbm_limited_ignore_state;
static bool g_thermal_protect_limited_ignore_state;
static unsigned int g_efuse_id;
static unsigned int g_segment_id;
static unsigned int g_opp_idx_num;
static unsigned int g_cur_opp_freq;
static unsigned int g_cur_opp_volt;
static unsigned int g_cur_opp_vsram_volt;
static unsigned int g_cur_opp_idx;
static unsigned int g_cur_opp_cond_idx;
static unsigned int g_keep_opp_freq;
static unsigned int g_keep_opp_freq_idx;
static unsigned int g_fixed_vsram_volt_idx;
static unsigned int g_fixed_freq;
static unsigned int g_fixed_volt;
static unsigned int g_max_limited_idx;
static unsigned int g_pbm_limited_power;
static unsigned int g_thermal_protect_power;
static unsigned int g_vgpu_sfchg_rrate;
static unsigned int g_vgpu_sfchg_frate;
static unsigned int g_vsram_sfchg_rrate;
static unsigned int g_vsram_sfchg_frate;
static unsigned int g_DVFS_off_by_ptpod_idx;
static unsigned int g_opp_springboard_idx;
#ifdef MT_GPUFREQ_BATT_OC_PROTECT
static bool g_batt_oc_limited_ignore_state;
static unsigned int g_batt_oc_level;
static unsigned int g_batt_oc_limited_idx;
static unsigned int g_batt_oc_limited_idx_lvl_0;
static unsigned int g_batt_oc_limited_idx_lvl_1;
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */
#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
static bool g_batt_percent_limited_ignore_state;
static unsigned int g_batt_percent_level;
static unsigned int g_batt_percent_limited_idx;
static unsigned int g_batt_percent_limited_idx_lv_0;
static unsigned int g_batt_percent_limited_idx_lv_1;
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static bool g_low_batt_limited_ignore_state;
static unsigned int g_low_battery_level;
static unsigned int g_low_batt_limited_idx;
static unsigned int g_low_batt_limited_idx_lvl_0;
static unsigned int g_low_batt_limited_idx_lvl_2;
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */
static enum g_post_divider_power_enum g_cur_post_divider_power;
static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);
static unsigned int g_limited_idx_array[NUMBER_OF_LIMITED_IDX] = { 0 };
static bool g_limited_ignore_array[NUMBER_OF_LIMITED_IDX] = { false };
static void __iomem *g_apmixed_base;
static void __iomem *g_efuse_base;
phys_addr_t gpu_fdvfs_virt_addr; /* for GED, legacy ?! */

/**
 * ===============================================
 * SECTION : API definition
 * ===============================================
 */

/*
 * API : handle frequency change request
 */
unsigned int mt_gpufreq_target(unsigned int idx)
{
	unsigned int target_freq;
	unsigned int target_volt;
	unsigned int target_idx;
	unsigned int target_cond_idx;

	mutex_lock(&mt_gpufreq_lock);

#ifdef MT_GPUFREQ_OPP_STRESS_TEST
	if (g_opp_stress_test_state) {
		get_random_bytes(&idx, sizeof(idx));
		idx = idx % g_opp_idx_num;
		gpufreq_pr_debug("@%s: OPP stress test index: %d\n",
			__func__, idx);
	}
#endif /* ifdef MT_GPUFREQ_OPP_STRESS_TEST */

	if (idx > (g_opp_idx_num - 1)) {
		gpufreq_pr_debug("@%s: OPP index (%d) is out of range\n",
			__func__, idx);
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	/* look up for the target OPP table */
	target_freq = g_opp_table[idx].gpufreq_khz;
	target_volt = g_opp_table[idx].gpufreq_volt;
	target_idx = g_opp_table[idx].gpufreq_idx;
	target_cond_idx = idx;

	gpufreq_pr_debug("@%s: receive freq: %d, index: %d\n",
		__func__, target_freq, target_idx);

	/* OPP freq is limited by Thermal/Power/PBM */
	if (g_max_limited_idx != 0) {
		if (target_freq > g_opp_table[g_max_limited_idx].gpufreq_khz) {
			target_freq =
				g_opp_table[g_max_limited_idx].gpufreq_khz;
			target_volt =
				g_opp_table[g_max_limited_idx].gpufreq_volt;
			target_idx = g_opp_table[g_max_limited_idx].gpufreq_idx;
			target_cond_idx = g_max_limited_idx;
			gpufreq_pr_debug("@%s: OPP freq is limited by Thermal/Power/PBM, g_max_limited_idx = %d\n",
					__func__, target_cond_idx);
		}
	}

	/* If /proc command keep OPP freq */
	if (g_keep_opp_freq_state) {
		target_freq = g_opp_table[g_keep_opp_freq_idx].gpufreq_khz;
		target_volt = g_opp_table[g_keep_opp_freq_idx].gpufreq_volt;
		target_idx = g_opp_table[g_keep_opp_freq_idx].gpufreq_idx;
		target_cond_idx = g_keep_opp_freq_idx;
		gpufreq_pr_debug(
			"@%s: keep OPP freq, freq = %d, volt = %d, idx = %d\n",
			__func__, target_freq, target_volt, target_cond_idx);
	}

	/* If /proc command fix the freq and volt */
	if (g_fixed_freq_volt_state) {
		target_freq = g_fixed_freq;
		target_volt = g_fixed_volt;
		target_idx = 0;
		target_cond_idx = 0;
		gpufreq_pr_debug(
			"@%s: fixed both freq and volt, freq = %d, volt = %d\n",
			__func__, target_freq, target_volt);
	}

	/* keep at max freq when PTPOD is initializing */
	if (g_DVFS_is_paused_by_ptpod) {
		target_freq = g_opp_table[g_DVFS_off_by_ptpod_idx].gpufreq_khz;
		target_volt = GPU_DVFS_PTPOD_DISABLE_VOLT;
		target_idx = g_opp_table[g_DVFS_off_by_ptpod_idx].gpufreq_idx;
		target_cond_idx = g_DVFS_off_by_ptpod_idx;
		gpufreq_pr_debug(
			"@%s: PTPOD disable DVFS, g_DVFS_off_by_ptpod_idx = %d\n",
			__func__, target_cond_idx);
	}

	/* target freq == current freq
	 * && target volt == current volt, skip it
	 */
	if (g_cur_opp_freq == target_freq && g_cur_opp_volt == target_volt) {
		gpufreq_pr_debug("@%s: Freq: %d ---> %d (skipped)\n",
			__func__, g_cur_opp_freq, target_freq);
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}

	/* set to the target frequency and voltage */
	__mt_gpufreq_set(g_cur_opp_freq, target_freq, g_cur_opp_volt,
		target_volt, g_cur_opp_vsram_volt,
		__mt_gpufreq_get_vsram_volt_by_target_volt(target_volt));

	g_cur_opp_idx = target_idx;
	g_cur_opp_cond_idx = target_cond_idx;

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

/*
 * enable MTCMOS
 */
void mt_gpufreq_enable_MTCMOS(bool bEnableHWAPM)
{
	/* enable PLL and set clk_mux to mfgpll */
	if (clk_prepare_enable(g_clk->clk_mux))
		gpufreq_perr("@%s: failed when enable top-clk\n",
		__func__);

	__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);

	if (clk_prepare_enable(g_clk->mtcmos_mfg_async))
		gpufreq_perr("@%s: failed when enable mtcmos_mfg_async\n",
		__func__);

	if (clk_prepare_enable(g_clk->mtcmos_mfg))
		gpufreq_perr("@%s: failed when enable mtcmos_mfg\n",
		__func__);

	if (!bEnableHWAPM) {
		if (clk_prepare_enable(g_clk->mtcmos_mfg_core0))
			gpufreq_perr(
			"@%s: failed when enable mtcmos_mfg_core0\n",
			__func__);

		gpufreq_pr_debug("@%s: enable MTCMOS done\n", __func__);
	}
}

/*
 * disable MTCMOS
 */
void mt_gpufreq_disable_MTCMOS(bool bEnableHWAPM)
{
	if (!bEnableHWAPM)
		clk_disable_unprepare(g_clk->mtcmos_mfg_core0);

	clk_disable_unprepare(g_clk->mtcmos_mfg);
	clk_disable_unprepare(g_clk->mtcmos_mfg_async);

	/* set clk_mux to 26M and turn-off gpupll reference */
	__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);
	clk_disable_unprepare(g_clk->clk_mux);

	gpufreq_pr_debug("@%s: disable MTCMOS done\n", __func__);
}

/*
 * API : GPU voltage on/off setting
 * 0 : off
 * 1 : on
 */
unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: begin, enable = %d, g_volt_enable_state = %d\n",
		__func__, enable, g_volt_enable_state);

	if (g_DVFS_is_paused_by_ptpod && enable == 0) {
		gpufreq_pr_info("@%s: DVFS is paused by PTPOD\n", __func__);
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	if (enable == 1) {
		__mt_gpufreq_bucks_enable();
		g_volt_enable_state = true;
		__mt_gpufreq_kick_pbm(1);
		gpufreq_pr_debug("@%s: VGPU/VSRAM_GPU is on\n", __func__);
	} else if (enable == 0)  {
		/* [MT6358] srclken high : 1ms, srclken low : 1T of 32K */
		/* ensure time interval of buck_on
		 * -> buck_off is at least 1ms
		 */
		__mt_gpufreq_bucks_disable();
		/* ensure time interval of buck_off
		 * -> buck_on is at least 1ms
		 */
		g_volt_enable_state = false;
		__mt_gpufreq_kick_pbm(0);
		gpufreq_pr_debug("@%s: VGPU/VSRAM_GPU is off\n", __func__);
	}

	gpufreq_pr_debug("@%s: end, enable = %d, g_volt_enable_state = %d\n",
			__func__, enable, g_volt_enable_state);

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

/*
 * API : enable DVFS for PTPOD initializing
 */
void mt_gpufreq_enable_by_ptpod(void)
{
#ifdef USE_STAND_ALONE_VGPU
	/* Set GPU Buck to leave PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_NORMAL);
#endif

	/* Freerun GPU DVFS */
	g_DVFS_is_paused_by_ptpod = false;

	/* Turn off GPU MTCMOS, neglect HWAPM */
	mt_gpufreq_disable_MTCMOS(false);

	/* Turn off GPU PMIC Buck */
	mt_gpufreq_voltage_enable_set(0);

	gpufreq_pr_debug("@%s: DVFS is enabled by ptpod\n", __func__);
}

/*
 * API : disable DVFS for PTPOD initializing
 */
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0;
	int target_idx = 0;

	/* Turn on GPU PMIC Buck */
	mt_gpufreq_voltage_enable_set(1);

	/* Turn on GPU MTCMOS, neglect HWAPM */
	mt_gpufreq_enable_MTCMOS(false);

	/* Pause GPU DVFS */
	g_DVFS_is_paused_by_ptpod = true;

	/* Fix GPU @ 0.8V */
	for (i = 0; i < g_opp_idx_num; i++) {
		if (g_opp_table_default[i].gpufreq_volt <=
			GPU_DVFS_PTPOD_DISABLE_VOLT) {
			target_idx = i;
			break;
		}
	}
	g_DVFS_off_by_ptpod_idx = (unsigned int)target_idx;
	mt_gpufreq_target(target_idx);

#ifdef USE_STAND_ALONE_VGPU
	/* Set GPU Buck to enter PWM mode */
	__mt_gpufreq_vgpu_set_mode(REGULATOR_MODE_FAST);
#endif

	gpufreq_pr_debug("@%s: DVFS is disabled by ptpod\n", __func__);
}

/*
 * API : update OPP and switch back to default voltage setting
 */
void mt_gpufreq_restore_default_volt(void)
{
	int i;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: restore OPP table to default voltage\n",
		__func__);

	for (i = 0; i < g_opp_idx_num; i++) {
		g_opp_table[i].gpufreq_volt
			= g_opp_table_default[i].gpufreq_volt;
		gpufreq_pr_debug("@%s: g_opp_table[%d].gpufreq_volt = %x\n",
				__func__, i, g_opp_table[i].gpufreq_volt);
	}

	__mt_gpufreq_calculate_springboard_opp_index();

	__mt_gpufreq_volt_switch_without_vsram_volt(g_cur_opp_volt,
		g_opp_table[g_cur_opp_cond_idx].gpufreq_volt);

	g_cur_opp_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_volt;
	g_cur_opp_vsram_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_vsram;

	mutex_unlock(&mt_gpufreq_lock);
}

/*
 * API : update OPP and set voltage because
 * PTPOD modified voltage table by PMIC wrapper
 */
unsigned int
mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size)
{
	int i;

	mutex_lock(&mt_gpufreq_lock);

	gpufreq_pr_debug("@%s: update OPP table to given voltage\n", __func__);

	for (i = 0; i < array_size; i++) {
		g_opp_table[i].gpufreq_volt = pmic_volt[i];
		gpufreq_pr_debug("@%s: g_opp_table[%d].gpufreq_volt = %d\n",
				__func__, i, g_opp_table[i].gpufreq_volt);
	}

	__mt_gpufreq_calculate_springboard_opp_index();

	__mt_gpufreq_volt_switch_without_vsram_volt(g_cur_opp_volt,
		g_opp_table[g_cur_opp_cond_idx].gpufreq_volt);

	g_cur_opp_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_volt;
	g_cur_opp_vsram_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_vsram;

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}

/* API : get OPP table index number */
unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return g_opp_idx_num;
}

/* API : get frequency via OPP table index */
unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	if (idx < g_opp_idx_num) {
		gpufreq_pr_debug("@%s: idx = %d, freq = %d\n", __func__, idx,
			g_opp_table[idx].gpufreq_khz);
		return g_opp_table[idx].gpufreq_khz;
	}

	gpufreq_pr_debug("@%s: not found, idx = %d\n", __func__, idx);
	return 0;
}

/* API : get voltage via OPP table index */
unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx)
{
	if (idx < g_opp_idx_num) {
		gpufreq_pr_debug("@%s: idx = %d, volt = %d\n", __func__,
			idx, g_opp_table[idx].gpufreq_volt);
		return g_opp_table[idx].gpufreq_volt;
	}

	gpufreq_pr_debug("@%s: not found, idx = %d\n", __func__, idx);
	return 0;
}

/* API : get max power on power table */
unsigned int mt_gpufreq_get_max_power(void)
{
	return (!g_power_table) ? 0 : g_power_table[0].gpufreq_power;
}

/* API : get min power on power table */
unsigned int mt_gpufreq_get_min_power(void)
{
	if (!g_power_table)
		return 0;
	return g_power_table[g_opp_idx_num - 1].gpufreq_power;
}

/* API : get static leakage power */
unsigned int mt_gpufreq_get_leakage_mw(void)
{
	int temp = 0;
#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
	unsigned int cur_vcore = __mt_gpufreq_get_cur_volt() / 100;
	int leak_power;
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
	leak_power = mt_spower_get_leakage(MTK_SPOWER_GPU, cur_vcore, temp);
	if (g_volt_enable_state && leak_power > 0)
		return leak_power;
	else
		return 0;
#else
	return 130;
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */
}

/*
 * API : get current Thermal/Power/PBM limited OPP table index
 */
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	gpufreq_pr_debug("@%s: current GPU Thermal/Power/PBM limit index is %d\n",
			__func__, g_max_limited_idx);
	return g_max_limited_idx;
}

/*
 * API : get current Thermal/Power/PBM limited OPP table frequency
 */
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	gpufreq_pr_debug("@%s: current GPU thermal limit freq is %d MHz\n",
			__func__,
			g_opp_table[g_max_limited_idx].gpufreq_khz / 1000);
	return g_opp_table[g_max_limited_idx].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/*
 * API : get current OPP table conditional index
 */
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	gpufreq_pr_debug("@%s: current OPP table conditional index is %d\n",
		__func__, g_cur_opp_cond_idx);
	return g_cur_opp_cond_idx;
}

/*
 * API : get current OPP table frequency
 */
unsigned int mt_gpufreq_get_cur_freq(void)
{
	gpufreq_pr_debug(
		"@%s: current frequency is %d MHz\n",
		__func__, g_cur_opp_freq / 1000);
	return g_cur_opp_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/*
 * API : get current voltage
 * This is exported API, which reports local backup for latest
 * updated VGPU directly
 * Since some non-preemptive thread could not use regulator_get,
 * This is not a redundant API
 */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return (g_volt_enable_state)?g_cur_opp_volt:0;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* API : get Thermal/Power/PBM limited OPP table index */
int mt_gpufreq_get_cur_ceiling_idx(void)
{
	return (int)mt_gpufreq_get_thermal_limit_index();
}

#ifdef MT_GPUFREQ_BATT_OC_PROTECT
/*
 * API : Over Currents(OC) Callback
 */
void mt_gpufreq_batt_oc_callback(BATTERY_OC_LEVEL battery_oc_level)
{
	if (g_batt_oc_limited_ignore_state) {
		gpufreq_pr_debug("@%s: ignore Over Currents(OC) protection\n",
			__func__);
		return;
	}

	gpufreq_pr_debug("@%s: battery_oc_level = %d\n",
		__func__, battery_oc_level);

	g_batt_oc_level = battery_oc_level;

	if (battery_oc_level == BATTERY_OC_LEVEL_1) {
		if (g_batt_oc_limited_idx != g_batt_oc_limited_idx_lvl_1) {
			g_batt_oc_limited_idx = g_batt_oc_limited_idx_lvl_1;
			__mt_gpufreq_batt_oc_protect(
				g_batt_oc_limited_idx_lvl_1);
		}
	} else {
		if (g_batt_oc_limited_idx != g_batt_oc_limited_idx_lvl_0) {
			g_batt_oc_limited_idx = g_batt_oc_limited_idx_lvl_0;
			__mt_gpufreq_batt_oc_protect(
				g_batt_oc_limited_idx_lvl_0);
		}
	}
}
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */

#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
/*
 * API : Battery Percentage Callback
 */
void
mt_gpufreq_batt_percent_callback(BATTERY_PERCENT_LEVEL battery_percent_level)
{
	if (g_batt_percent_limited_ignore_state) {
		gpufreq_pr_debug(
			"@%s: ignore Battery Percentage protection\n",
			__func__);
		return;
	}

	gpufreq_pr_debug("@%s: battery_percent_level = %d\n",
		__func__, battery_percent_level);

	g_batt_percent_level = battery_percent_level;

	/* BATTERY_PERCENT_LEVEL_1: <= 15%, BATTERY_PERCENT_LEVEL_0: >15% */
	if (battery_percent_level == BATTERY_PERCENT_LEVEL_1) {
		if (g_batt_percent_limited_idx !=
			g_batt_percent_limited_idx_lv_1) {
			g_batt_percent_limited_idx
				= g_batt_percent_limited_idx_lv_1;
			__mt_gpufreq_batt_percent_protect(
				g_batt_percent_limited_idx_lv_1);
		}
	} else {
		if (g_batt_percent_limited_idx !=
			g_batt_percent_limited_idx_lv_0) {
			g_batt_percent_limited_idx
				= g_batt_percent_limited_idx_lv_0;
			__mt_gpufreq_batt_percent_protect(
				g_batt_percent_limited_idx_lv_0);
		}
	}
}
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/*
 * API : Low Battery Volume Callback
 */
void mt_gpufreq_low_batt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	if (g_low_batt_limited_ignore_state) {
		gpufreq_pr_debug(
			"@%s: ignore Low Battery Volume protection\n",
			__func__);
		return;
	}

	gpufreq_pr_debug("@%s: low_battery_level = %d\n",
		__func__, low_battery_level);

	g_low_battery_level = low_battery_level;

	/*
	 * 3.25V HW issue int and is_low_battery = 1
	 * 3.10V HW issue int and is_low_battery = 2
	 */

	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		if (g_low_batt_limited_idx != g_low_batt_limited_idx_lvl_2) {
			g_low_batt_limited_idx = g_low_batt_limited_idx_lvl_2;
			__mt_gpufreq_low_batt_protect(
				g_low_batt_limited_idx_lvl_2);
		}
	} else {
		if (g_low_batt_limited_idx != g_low_batt_limited_idx_lvl_0) {
			g_low_batt_limited_idx = g_low_batt_limited_idx_lvl_0;
			__mt_gpufreq_low_batt_protect(
				g_low_batt_limited_idx_lvl_0);
		}
	}
}
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */

/*
 * API : set limited OPP table index for Thermal protection
 */
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = -1;
	unsigned int limited_freq;

	mutex_lock(&mt_gpufreq_power_lock);

	if (g_thermal_protect_limited_ignore_state) {
		gpufreq_pr_debug("@%s: ignore Thermal protection\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == g_thermal_protect_power) {
		gpufreq_pr_debug("@%s: limited_power(%d mW) not changed, skip it\n",
			__func__, limited_power);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	g_thermal_protect_power = limited_power;

#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
	__mt_update_gpufreqs_power_table();
#endif /* ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE */

	gpufreq_pr_debug("@%s: limited power = %d\n", __func__, limited_power);

	if (limited_power == 0) {
		g_limited_idx_array[IDX_THERMAL_PROTECT_LIMITED] = 0;
		__mt_gpufreq_update_max_limited_idx();
	} else {
		limited_freq =
			__mt_gpufreq_get_limited_freq_by_power(limited_power);
		for (i = 0; i < g_opp_idx_num; i++) {
			if (g_opp_table[i].gpufreq_khz <= limited_freq) {
			g_limited_idx_array[IDX_THERMAL_PROTECT_LIMITED] = i;
				__mt_gpufreq_update_max_limited_idx();
				if (g_cur_opp_freq > g_opp_table[i].gpufreq_khz)
					mt_gpufreq_target(i);
				break;
			}
		}
	}

	gpufreq_pr_debug("@%s: limited power index = %d\n", __func__, i);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/* API : set limited OPP table index by PBM */
void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power)
{
	int i = -1;
	unsigned int limited_freq;

	mutex_lock(&mt_gpufreq_power_lock);

	if (g_pbm_limited_ignore_state) {
		gpufreq_pr_debug("@%s: ignore PBM Power limited\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == g_pbm_limited_power) {
		gpufreq_pr_debug("@%s: limited_power(%d mW) not changed, skip it\n",
				__func__, limited_power);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	g_pbm_limited_power = limited_power;

	gpufreq_pr_debug("@%s: limited_power = %d\n", __func__, limited_power);

	if (limited_power == 0) {
		g_limited_idx_array[IDX_PBM_LIMITED] = 0;
		__mt_gpufreq_update_max_limited_idx();
	} else {
		limited_freq =
			__mt_gpufreq_get_limited_freq_by_power(limited_power);
		for (i = 0; i < g_opp_idx_num; i++) {
			if (g_opp_table[i].gpufreq_khz <= limited_freq) {
				g_limited_idx_array[IDX_PBM_LIMITED] = i;
				__mt_gpufreq_update_max_limited_idx();
				break;
			}
		}
	}

	gpufreq_pr_debug("@%s: limited power index = %d\n", __func__, i);

	mutex_unlock(&mt_gpufreq_power_lock);
}

/*
 * API : set GPU loading for SSPM
 */
void mt_gpufreq_set_loading(unsigned int gpu_loading)
{
	/* legacy */
}

/*
 * API : register GPU power limited notifiction callback
 */
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	/* legacy */
}

/**
 * ===============================================
 * SECTION : PROCFS interface for debugging
 * ===============================================
 */

#ifdef CONFIG_PROC_FS
/*
 * PROCFS : show OPP table
 */
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < g_opp_idx_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", g_opp_table[i].gpufreq_khz);
		seq_printf(m, "volt = %d, ", g_opp_table[i].gpufreq_volt);
		seq_printf(m, "vsram = %d, ", g_opp_table[i].gpufreq_vsram);
		seq_printf(m, "gpufreq_power = %d\n",
			g_power_table[i].gpufreq_power);
	}

	return 0;
}

/*
 * PROCFS : show OPP power table
 */
static int mt_gpufreq_power_dump_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < g_opp_idx_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", g_power_table[i].gpufreq_khz);
		seq_printf(m, "volt = %d, ", g_power_table[i].gpufreq_volt);
		seq_printf(m, "power = %d\n", g_power_table[i].gpufreq_power);
	}

	return 0;
}

/*
 * PROCFS : show important variables for debugging
 */
static int g_cur_vcore_opp = VCORE_OPP_0;
static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int gpu_loading = 0;

	/*mtk_get_gpu_loading(&gpu_loading);*/

	seq_printf(m, "g_cur_opp_idx = %d, g_cur_opp_cond_idx = %d\n",
			g_cur_opp_idx, g_cur_opp_cond_idx);
	seq_printf(m,
	"g_cur_opp_freq = %d, g_cur_opp_volt = %d, g_cur_opp_vsram_volt = %d\n",
		g_cur_opp_freq, g_cur_opp_volt, g_cur_opp_vsram_volt);
	seq_printf(m, "real freq = %d, real volt = %d, real vsram_volt = %d\n",
			__mt_gpufreq_get_cur_freq(),
			__mt_gpufreq_get_cur_volt(),
			__mt_gpufreq_get_cur_vsram_volt());
	seq_printf(m, "current vcore opp = %d\n", g_cur_vcore_opp);
	seq_printf(m, "clock freq = %d\n", mt_get_abist_freq(25));
	seq_printf(m, "g_segment_id = %d\n", g_segment_id);
	seq_printf(m, "g_volt_enable_state = %d\n", g_volt_enable_state);
	seq_printf(m, "g_opp_stress_test_state = %d\n",
		g_opp_stress_test_state);
	seq_printf(m, "g_DVFS_off_by_ptpod_idx = %d\n",
		g_DVFS_off_by_ptpod_idx);
	seq_printf(m, "g_max_limited_idx = %d\n", g_max_limited_idx);
	seq_printf(m, "g_opp_springboard_idx = %d\n", g_opp_springboard_idx);
	seq_printf(m, "gpu_loading = %d\n", gpu_loading);

	for (i = 0; i < NUMBER_OF_LIMITED_IDX; i++)
		seq_printf(m, "g_limited_idx_array[%d] = %d\n",
			i, g_limited_idx_array[i]);

	return 0;
}

#ifdef MT_GPUFREQ_OPP_STRESS_TEST
/*
 * PROCFS : show current opp stress test state
 */
static int mt_gpufreq_opp_stress_test_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_opp_stress_test_state = %d\n",
		g_opp_stress_test_state);
	return 0;
}

/*
 * PROCFS : opp stress test message setting
 * 0 : disable
 * 1 : enable
 */
static ssize_t mt_gpufreq_opp_stress_test_proc_write(struct file *file,
	const char __user *buffer,
	size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (!kstrtouint(buf, 10, &value)) {
		if (!value || !(value-1)) {
			ret = 0;
			g_opp_stress_test_state = value;
		}
	}

out:
	return (ret < 0) ? ret : count;
}
#endif /* ifdef MT_GPUFREQ_OPP_STRESS_TEST */

/*
 * PROCFS : show Thermal/Power/PBM limited ignore state
 * 0 : consider
 * 1 : ignore
 */
static int mt_gpufreq_power_limited_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "GPU-DVFS power limited state ....\n");
	seq_printf(m, "g_batt_oc_limited_ignore_state = %d\n",
		g_batt_oc_limited_ignore_state);
	seq_printf(m, "g_batt_percent_limited_ignore_state = %d\n",
		g_batt_percent_limited_ignore_state);
	seq_printf(m, "g_low_batt_limited_ignore_state = %d\n",
		g_low_batt_limited_ignore_state);
	seq_printf(m, "g_thermal_protect_limited_ignore_state = %d\n",
		g_thermal_protect_limited_ignore_state);
	seq_printf(m, "g_pbm_limited_ignore_state = %d\n",
		g_pbm_limited_ignore_state);
	return 0;
}

/*
 * PROCFS : ignore state or power value setting for Thermal/Power/PBM limit
 */
static ssize_t mt_gpufreq_power_limited_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int i;
	unsigned int size;
	unsigned int value = 0;
	static const char * const array[] = {
#ifdef MT_GPUFREQ_BATT_OC_PROTECT
		"ignore_batt_oc",
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */
#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
		"ignore_batt_percent",
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
		"ignore_low_batt",
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */
		"ignore_thermal_protect",
		"ignore_pbm_limited",
		"pbm_limited_power",
		"thermal_protect_power",
	};

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	size = ARRAY_SIZE(array);

	for (i = 0; i < size; i++) {
		if (strncmp(array[i], buf, MIN(strlen(array[i]), count)) == 0) {
			char cond_buf[64];

			snprintf(cond_buf, sizeof(cond_buf), "%s %%u",
				array[i]);
			if (sscanf(buf, cond_buf, &value) == 1) {
				ret = 0;
				if (strncmp(array[i], "pbm_limited_power",
						strlen(array[i])) == 0)
				mt_gpufreq_set_power_limit_by_pbm(value);
				else if (strncmp(array[i],
					"thermal_protect_power",
					strlen(array[i])) == 0)
					mt_gpufreq_thermal_protect(value);
#ifdef MT_GPUFREQ_BATT_OC_PROTECT
				else if (strncmp(array[i], "ignore_batt_oc",
						strlen(array[i])) == 0) {
					if (!value || !(value-1)) {
					g_batt_oc_limited_ignore_state =
							(value) ? true : false;
				g_limited_ignore_array[IDX_BATT_OC_LIMITED] =
						g_batt_oc_limited_ignore_state;
					}
				}
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */
#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
				else if (strncmp(array[i],
					"ignore_batt_percent",
						strlen(array[i])) == 0) {
					if (!value || !(value-1)) {
					g_batt_percent_limited_ignore_state =
							(value) ? true : false;
			g_limited_ignore_array[IDX_BATT_PERCENT_LIMITED] =
					g_batt_percent_limited_ignore_state;
					}
				}
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
				else if (strncmp(array[i], "ignore_low_batt",
						strlen(array[i])) == 0) {
				if (!value || !(value-1)) {
					g_low_batt_limited_ignore_state =
						(value) ? true : false;
				g_limited_ignore_array[IDX_LOW_BATT_LIMITED] =
					g_low_batt_limited_ignore_state;
					}
				}
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */
				else if (strncmp(array[i],
						"ignore_thermal_protect",
						strlen(array[i])) == 0) {
					if (!value || !(value-1)) {
					g_thermal_protect_limited_ignore_state
						= (value) ? true : false;
			g_limited_ignore_array[IDX_THERMAL_PROTECT_LIMITED] =
					g_thermal_protect_limited_ignore_state;
					}
				} else if (strncmp(array[i],
						"ignore_pbm_limited",
						strlen(array[i])) == 0) {
					if (!value || !(value-1)) {
					g_pbm_limited_ignore_state =
						(value) ? true : false;
					g_limited_ignore_array[IDX_PBM_LIMITED]
						= g_pbm_limited_ignore_state;
					}
				}
				break;
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current keeping OPP frequency state
 */
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	if (g_keep_opp_freq_state) {
		seq_puts(m, "Keeping OPP frequency is enabled\n");
		seq_printf(m, "[%d] ", g_keep_opp_freq_idx);
		seq_printf(m, "freq = %d, ",
			g_opp_table[g_keep_opp_freq_idx].gpufreq_khz);
		seq_printf(m, "volt = %d, ",
			g_opp_table[g_keep_opp_freq_idx].gpufreq_volt);
		seq_printf(m, "vsram = %d\n",
			g_opp_table[g_keep_opp_freq_idx].gpufreq_vsram);
	} else
		seq_puts(m, "Keeping OPP frequency is disabled\n");

	return 0;
}

/*
 * PROCFS : keeping OPP frequency setting
 * 0 : free run
 * 1 : keep OPP frequency
 */
static ssize_t mt_gpufreq_opp_freq_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;
	unsigned int i = 0;
	int ret = -EFAULT;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 0) {
			g_keep_opp_freq_state = false;
		} else {
			for (i = 0; i < g_opp_idx_num; i++) {
				if (value == g_opp_table[i].gpufreq_khz) {
					ret = 0;
					g_keep_opp_freq_idx = i;
					g_keep_opp_freq_state = true;
					g_keep_opp_freq = value;
					mt_gpufreq_voltage_enable_set(1);
					mt_gpufreq_target(i);
					break;
				}
			}
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : show current fixed freq & volt state
 */
static int mt_gpufreq_fixed_freq_volt_proc_show(struct seq_file *m, void *v)
{
	if (g_fixed_freq_volt_state) {
		seq_puts(m, "GPU-DVFS fixed freq & volt is enabled\n");
		seq_printf(m, "g_fixed_freq = %d\n", g_fixed_freq);
		seq_printf(m, "g_fixed_volt = %d\n", g_fixed_volt);
	} else
		seq_puts(m, "GPU-DVFS fixed freq & volt is disabled\n");

	return 0;
}

/*
 * PROCFS : fixed freq & volt state setting
 */
static ssize_t mt_gpufreq_fixed_freq_volt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = -EFAULT;
	unsigned int fixed_freq = 0;
	unsigned int fixed_volt = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);

	if (copy_from_user(buf, buffer, len))
		goto out;

	buf[len] = '\0';

	if (sscanf(buf, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		ret = 0;
		if ((fixed_freq == 0) && (fixed_volt == 0)) {
			g_fixed_freq_volt_state = false;
			g_fixed_freq = 0;
			g_fixed_volt = 0;
		} else {
			g_cur_opp_freq = __mt_gpufreq_get_cur_freq();
			fixed_volt = VOLT_NORMALIZATION(fixed_volt);
			if (fixed_freq > g_cur_opp_freq) {
				__mt_gpufreq_set_fixed_volt(fixed_volt);
				__mt_gpufreq_set_fixed_freq(fixed_freq);
			} else {
				__mt_gpufreq_set_fixed_freq(fixed_freq);
				__mt_gpufreq_set_fixed_volt(fixed_volt);
			}
			g_fixed_freq_volt_state = true;
		}
	}

out:
	return (ret < 0) ? ret : count;
}

/*
 * PROCFS : initialization
 */
PROC_FOPS_RW(gpufreq_opp_stress_test);
PROC_FOPS_RW(gpufreq_power_limited);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RO(gpufreq_power_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_fixed_freq_volt);
static int __mt_gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_opp_stress_test),
		PROC_ENTRY(gpufreq_power_limited),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_power_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_fixed_freq_volt),
	};

	dir = proc_mkdir("gpufreq", NULL);
	if (!dir) {
		gpufreq_perr("@%s: fail to create /proc/gpufreq\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0664, dir, entries[i].fops))
			gpufreq_perr("@%s: create /proc/gpufreq/%s failed\n",
				__func__, entries[i].name);
	}

	return 0;
}
#endif  /* ifdef CONFIG_PROC_FS */

/**
 * ===============================================
 * SECTION : Local functions definition
 * ===============================================
 */

/*
 * switch VGPU voltage via VCORE
 */

static void __mt_gpufreq_vcore_volt_switch(unsigned int volt_target)
{
	if (volt_target > 70000) {
		pm_qos_update_request(&g_pmic->pm_vgpu, VCORE_OPP_0);
		g_cur_vcore_opp = VCORE_OPP_0;
	} else if (volt_target > 65000) {
		pm_qos_update_request(&g_pmic->pm_vgpu, VCORE_OPP_1);
		g_cur_vcore_opp = VCORE_OPP_1;
	} else if (volt_target > 0) {
		pm_qos_update_request(&g_pmic->pm_vgpu, VCORE_OPP_3);
		g_cur_vcore_opp = VCORE_OPP_3;
	} else /* UNREQUEST */
		pm_qos_update_request(&g_pmic->pm_vgpu, VCORE_OPP_UNREQ);

}

/*
 * frequency ramp up/down handler
 * - frequency ramp up need to wait voltage settle
 * - frequency ramp down do not need to wait voltage settle
 */
static void __mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new,
	unsigned int volt_old, unsigned int volt_new,
	unsigned int vsram_volt_old, unsigned int vsram_volt_new)
{
	gpufreq_pr_debug(
		"@%s: freq: %d ---> %d, volt: %d ---> %d, vsram_volt: %d ---> %d\n",
		__func__, freq_old, freq_new, volt_old, volt_new,
		vsram_volt_old, vsram_volt_new);

	if (freq_new > freq_old) {
		if (vsram_volt_new > (volt_old + BUCK_VARIATION_MAX)) {
			__mt_gpufreq_volt_switch(volt_old,
			g_opp_table[g_opp_springboard_idx].gpufreq_volt,
			vsram_volt_old,
			g_opp_table[g_opp_springboard_idx].gpufreq_vsram);
			__mt_gpufreq_volt_switch(
			g_opp_table[g_opp_springboard_idx].gpufreq_volt,
			volt_new,
			g_opp_table[g_opp_springboard_idx].gpufreq_vsram,
				vsram_volt_new);
		} else
#ifdef USE_STAND_ALONE_VGPU
			__mt_gpufreq_volt_switch(volt_old, volt_new,
				vsram_volt_old, vsram_volt_new);
#else
		__mt_gpufreq_vcore_volt_switch(volt_new);
#endif
		__mt_gpufreq_clock_switch(freq_new);
	} else {
		__mt_gpufreq_clock_switch(freq_new);
#ifdef USE_STAND_ALONE_VGPU
		if (vsram_volt_old > (volt_new + BUCK_VARIATION_MAX)) {
			__mt_gpufreq_volt_switch(volt_old,
			g_opp_table[g_opp_springboard_idx].gpufreq_volt,
			vsram_volt_old,
			g_opp_table[g_opp_springboard_idx].gpufreq_vsram);
			__mt_gpufreq_volt_switch(
			g_opp_table[g_opp_springboard_idx].gpufreq_volt,
			volt_new,
			g_opp_table[g_opp_springboard_idx].gpufreq_vsram,
				vsram_volt_new);
		} else {
			__mt_gpufreq_volt_switch(volt_old,
				volt_new, vsram_volt_old, vsram_volt_new);
		}
#else
		__mt_gpufreq_vcore_volt_switch(volt_new);
#endif
	}

	gpufreq_pr_debug(
		"@%s: real_freq = %d, real_volt = %d, real_vsram_volt = %d\n",
		__func__, mt_get_ckgen_freq(9), __mt_gpufreq_get_cur_volt(),
		__mt_gpufreq_get_cur_vsram_volt());

	g_cur_opp_freq = freq_new;
	g_cur_opp_volt = volt_new;
	g_cur_opp_vsram_volt = vsram_volt_new;

	__mt_gpufreq_kick_pbm(1);
}

/*
 * switch clock(frequency) via PLL
 */
static void __mt_gpufreq_clock_switch(unsigned int freq_new)
{
	enum g_post_divider_power_enum post_divider_power;
	unsigned int cur_volt;
	unsigned int cur_freq;
	unsigned int dds;

	cur_volt = __mt_gpufreq_get_cur_volt();
	cur_freq = __mt_gpufreq_get_cur_freq();

	/* [MT6765] GPUPLL_CON1[24:26] is POST_DIVIDER
	 *    000 : /1
	 *    001 : /2
	 *    010 : /4
	 *    011 : /8
	 *    100 : /16
	 */
	post_divider_power = __mt_gpufreq_get_post_divider_power(freq_new, 0);
	dds = __mt_gpufreq_calculate_dds(freq_new, post_divider_power);

	gpufreq_pr_debug(
		"@%s: request GPU dds = 0x%x, cur_volt = %d, cur_freq = %d\n",
		__func__, dds, cur_volt, cur_freq);

	gpufreq_pr_debug("@%s: begin, freq = %d, GPUPLL_CON1 = 0x%x\n",
		__func__, freq_new, DRV_Reg32(GPUPLL_CON1));

#ifndef FHCTL_READY
	/* Force parking if FHCTL not ready */
	g_parking = true;
#endif
	if (g_parking) {
		/* mfgpll_ck to clk26m */
		__mt_gpufreq_switch_to_clksrc(CLOCK_SUB);
		/* dds = GPUPLL_CON1[21:0], POST_DIVIDER = GPUPLL_CON1[24:26] */
		DRV_WriteReg32(GPUPLL_CON1, (0x80000000) |
			(post_divider_power << POST_DIV_SHIFT) | dds);
		udelay(20);
		/* clk26m to mfgpll_ck */
		__mt_gpufreq_switch_to_clksrc(CLOCK_MAIN);
		g_parking = false;
	} else {
#ifdef FHCTL_READY
		mt_dfs_general_pll(3, dds);
#endif
	}
	gpufreq_pr_debug("@%s: end, freq = %d, GPUPLL_CON1 = 0x%x\n",
		__func__, freq_new, DRV_Reg32(GPUPLL_CON1));

}

/*
 * switch to target clock source
 */
static void __mt_gpufreq_switch_to_clksrc(enum g_clock_source_enum clksrc)
{
	int ret;

	ret = clk_prepare_enable(g_clk->clk_mux);
	if (ret)
		gpufreq_pr_debug(
		"@%s: enable clk_mux(TOP_MUX_MFG) failed, ret = %d\n",
		__func__, ret);

	if (clksrc == CLOCK_MAIN) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_main_parent);
		gpufreq_pr_debug("@%s: switch to main clock source (%d)\n",
			__func__, ret);
	} else if (clksrc == CLOCK_SUB) {
		ret = clk_set_parent(g_clk->clk_mux, g_clk->clk_sub_parent);
		gpufreq_pr_debug("@%s: switch to sub clock source (%d)\n",
			__func__, ret);
	} else {
		gpufreq_pr_debug(
			"@%s: clock source index is not valid, clksrc = %d\n",
			__func__, clksrc);
	}

	clk_disable_unprepare(g_clk->clk_mux);

}

/*
 * switch voltage and vsram via PMIC
 */
static void
__mt_gpufreq_volt_switch_without_vsram_volt(unsigned int volt_old,
	unsigned int volt_new)
{
#if 0
	unsigned int vsram_volt_new, vsram_volt_old;

	volt_new = VOLT_NORMALIZATION(volt_new);

	gpufreq_pr_debug("@%s: volt_new = %d, volt_old = %d\n",
		__func__, volt_new, volt_old);

	vsram_volt_new =
		__mt_gpufreq_get_vsram_volt_by_target_volt(volt_new);
	vsram_volt_old =
		__mt_gpufreq_get_vsram_volt_by_target_volt(volt_old);

	__mt_gpufreq_volt_switch(volt_old, volt_new,
		vsram_volt_old, vsram_volt_new);
#endif
}

/*
 * switch voltage and vsram via PMIC
 */
static void __mt_gpufreq_volt_switch(unsigned int volt_old,
		unsigned int volt_new, unsigned int vsram_volt_old,
		unsigned int vsram_volt_new)
{
	if (volt_new > volt_old) {
#ifdef USE_STAND_ALONE_VSRAM
		__mt_gpufreq_vsram_gpu_volt_switch(VOLT_RISING,
			g_vsram_sfchg_rrate, vsram_volt_old, vsram_volt_new);
#endif
#ifdef USE_STAND_ALONE_VGPU
		__mt_gpufreq_vgpu_volt_switch(VOLT_RISING,
			g_vgpu_sfchg_rrate, volt_old, volt_new);
#endif
	} else {
#ifdef USE_STAND_ALONE_VGPU
		__mt_gpufreq_vgpu_volt_switch(VOLT_FALLING, g_vgpu_sfchg_frate,
			volt_old, volt_new);
#endif
#ifdef USE_STAND_ALONE_VSRAM
		__mt_gpufreq_vsram_gpu_volt_switch(VOLT_FALLING,
			g_vsram_sfchg_frate, vsram_volt_old, vsram_volt_new);
#endif
	}
}

/*
 * calculate springboard opp index
 * to avoid buck variation, the voltage between VGPU
 * and VSRAM_GPU must be in 100mV?~ 250mV
 * (Vgpu +- 6.25%, Vgpu_sram +- 47mV)
 */
static void __mt_gpufreq_calculate_springboard_opp_index(void)
{
	unsigned int i, upper_volt;

	upper_volt = g_opp_table[g_opp_idx_num - 1].gpufreq_volt +
		BUCK_VARIATION_MAX;

	for (i = 0; i < g_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_vsram <= upper_volt) {
			g_opp_springboard_idx = i;
			break;
		}
	}

	gpufreq_pr_debug("@%s: upper_volt = %d, g_opp_springboard_idx = %d\n",
			__func__, upper_volt, g_opp_springboard_idx);
}

/*
 * switch VSRAM_GPU voltage via PMIC
 */
static void __mt_gpufreq_vsram_gpu_volt_switch(
	enum g_volt_switch_enum switch_way,
	unsigned int sfchg_rate,
	unsigned int volt_old, unsigned int volt_new)
{
#ifdef USE_STAND_ALONE_VSRAM
	unsigned int max_diff, steps;

	if (switch_way == VOLT_RISING)
		max_diff = (volt_new - volt_old);
	else
		max_diff = (volt_old - volt_new);

	steps = (max_diff / DELAY_FACTOR) + 1;

	regulator_set_voltage(g_pmic->reg_vsram_gpu,
		volt_new * 10, VSRAM_GPU_MAX_VOLT * 10 + 125);
	udelay(steps * sfchg_rate);

	gpufreq_pr_debug("@%s: udelay us(%d) = steps(%d) * sfchg_rate(%d)\n",
			__func__, steps * sfchg_rate, steps, sfchg_rate);
#endif
}

/*
 * switch VGPU voltage via PMIC
 */
static void __mt_gpufreq_vgpu_volt_switch(enum g_volt_switch_enum switch_way,
		unsigned int sfchg_rate,
		unsigned int volt_old, unsigned int volt_new)
{
#ifdef USE_STAND_ALONE_VGPU
	unsigned int max_diff, steps;


	if (switch_way == VOLT_RISING)
		max_diff = (volt_new - volt_old);
	else
		max_diff = (volt_old - volt_new);

	steps = (max_diff / DELAY_FACTOR) + 1;

	regulator_set_voltage(g_pmic->reg_vgpu,
		volt_new * 10, VGPU_MAX_VOLT * 10 + 125);
	udelay(PMIC_SRCLKEN_HIGH_TIME_US);

	gpufreq_pr_debug(
		"@%s: udelay us(%d) = steps(%d) * sfchg_rate(%d)\n",
		__func__, steps * sfchg_rate, steps, sfchg_rate);
#endif
}

/*
 * enable bucks (VGPU and VSRAM_GPU)
 */
static void __mt_gpufreq_bucks_enable(void)
{
	int ret;
#ifdef USE_STAND_ALONE_VSRAM

	if (regulator_is_enabled(g_pmic->reg_vsram_gpu) == 0) {
		ret = regulator_enable(g_pmic->reg_vsram_gpu);
		if (ret) {
			gpufreq_perr(
			"@%s: enable VSRAM_GPU failed, ret = %d\n",
			__func__, ret);
			return;
		}
	}
#endif
#ifdef USE_STAND_ALONE_VGPU
	if (regulator_is_enabled(g_pmic->reg_vgpu) == 0) {
		ret = regulator_enable(g_pmic->reg_vgpu);
		if (ret) {
			gpufreq_perr(
			"@%s: enable VGPU failed, ret = %d\n", __func__, ret);
			return;
		}
	}
	gpufreq_pr_debug("@%s: bucks is enabled\n", __func__);
#else
	__mt_gpufreq_vcore_volt_switch(g_cur_opp_volt);
#endif
}

/*
 * disable bucks (VGPU and VSRAM_GPU)
 */
static void __mt_gpufreq_bucks_disable(void)
{
	int ret;

#ifdef USE_STAND_ALONE_VGPU
	if (regulator_is_enabled(g_pmic->reg_vgpu) > 0) {
		ret = regulator_disable(g_pmic->reg_vgpu);
		if (ret) {
			gpufreq_perr(
			"@%s: disable VGPU failed, ret = %d\n", __func__, ret);
			return;
		}
	}
#else
	__mt_gpufreq_vcore_volt_switch(0);
#endif
#ifdef USE_STAND_ALONE_VSRAM
	if (regulator_is_enabled(g_pmic->reg_vsram_gpu) > 0) {
		ret = regulator_disable(g_pmic->reg_vsram_gpu);
		if (ret) {
			gpufreq_perr(
			"@%s: disable VSRAM_GPU failed, ret = %d\n",
			__func__, ret);
			return;
		}
	}
	gpufreq_pr_debug("@%s: bucks is disabled\n", __func__);
#endif
}

/*
 * set AUTO_MODE or PWM_MODE to PMIC(VGPU)
 * REGULATOR_MODE_FAST: PWM Mode
 * REGULATOR_MODE_NORMAL: Auto Mode
 */
static void __mt_gpufreq_vgpu_set_mode(unsigned int mode)
{
#if 0
	int ret;

	ret = regulator_set_mode(g_pmic->reg_vgpu, mode);
	if (ret == 0)
		gpufreq_pr_debug(
		"@%s: set AUTO_MODE(%d) or PWM_MODE(%d) to PMIC(VGPU), mode = %d\n",
		__func__, REGULATOR_MODE_NORMAL, REGULATOR_MODE_FAST, mode);
	else
		gpufreq_perr(
		"@%s: failed to configure mode, ret = %d, mode = %d\n",
		__func__, ret, mode);
#endif
}

/*
 * set fixed frequency for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_freq(int fixed_freq)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq = %d, g_fixed_volt = %d\n",
			__func__, g_fixed_freq, g_fixed_volt);
	g_fixed_freq = fixed_freq;
	g_fixed_volt = g_cur_opp_volt;
	mt_gpufreq_voltage_enable_set(1);
	gpufreq_pr_debug("@%s: now, g_fixed_freq = %d, g_fixed_volt = %d\n",
			__func__, g_fixed_freq, g_fixed_volt);
	__mt_gpufreq_clock_switch(g_fixed_freq);
	g_cur_opp_freq = g_fixed_freq;
}

/*
 * set fixed voltage for PROCFS: fixed_freq_volt
 */
static void __mt_gpufreq_set_fixed_volt(int fixed_volt)
{
	gpufreq_pr_debug("@%s: before, g_fixed_freq = %d, g_fixed_volt = %d\n",
			__func__, g_fixed_freq, g_fixed_volt);
	g_fixed_freq = g_cur_opp_freq;
	g_fixed_volt = fixed_volt;
	mt_gpufreq_voltage_enable_set(1);
	gpufreq_pr_debug("@%s: now, g_fixed_freq = %d, g_fixed_volt = %d\n",
			__func__, g_fixed_freq, g_fixed_volt);
#ifdef USE_STAND_ALONE_VGPU
	regulator_set_voltage(g_pmic->reg_vgpu, g_fixed_volt * 10,
		g_fixed_volt * 10 + 125);
#else
	regulator_set_voltage(g_pmic->reg_vcore, g_fixed_volt * 10,
		g_fixed_volt * 10 + 125);
#endif

	g_cur_opp_volt = g_fixed_volt;
	g_cur_opp_vsram_volt =
		__mt_gpufreq_get_vsram_volt_by_target_volt(g_fixed_volt);
}

/*
 * dds calculation for clock switching
 */
static unsigned int __mt_gpufreq_calculate_dds(unsigned int freq_khz,
		enum g_post_divider_power_enum post_divider_power)
{
	unsigned int dds = 0;

	gpufreq_pr_debug("@%s: request freq = %d, post_divider = %d\n",
		__func__, freq_khz, (1 << post_divider_power));

	/* [MT6765] dds is GPUPLL_CON1[21:0] */
	if ((freq_khz >= POST_DIV_16_MIN_FREQ) &&
		(freq_khz <= POST_DIV_4_MAX_FREQ)) {
		dds = (((freq_khz / TO_MHz_HEAD * (1 << post_divider_power))
		<< DDS_SHIFT) / GPUPLL_FIN + ROUNDING_VALUE) / TO_MHz_TAIL;
	} else {
		gpufreq_perr(
			"@%s: out of range, freq_khz = %d\n",
			__func__, freq_khz);
	}

	return dds;
}

/* power calculation for power table */
static void __mt_gpufreq_calculate_power(unsigned int idx,
	unsigned int freq, unsigned int volt, unsigned int temp)
{
	unsigned int p_total = 0;
	unsigned int p_dynamic = 0;
	unsigned int ref_freq = 0;
	unsigned int ref_volt = 0;
	int p_leakage = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq = GPU_ACT_REF_FREQ;
	ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
			((freq * 100) / ref_freq) *
			((volt * 100) / ref_volt) *
			((volt * 100) / ref_volt) / (100 * 100 * 100);

#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
	p_leakage = mt_spower_get_leakage(MTK_SPOWER_GPU, (volt / 100), temp);
	if (!g_volt_enable_state || p_leakage < 0)
		p_leakage = 0;
#else
	p_leakage = 71;
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

	p_total = p_dynamic + p_leakage;

	gpufreq_pr_debug(
	"@%s: idx = %d, p_dynamic = %d, p_leakage = %d, p_total = %d, temp = %d\n",
	__func__, idx, p_dynamic, p_leakage, p_total, temp);

	g_power_table[idx].gpufreq_power = p_total;
}

/*
 * VGPU slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vgpu_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vgpu = 1;
#if 0
	unsigned int sfchg_rate_reg;


	/* [MT6358] RG_BUCK_VGPU_SFCHG_RRATE and RG_BUCK_VGPU_SFCHG_FRATE
	 * Rising soft change rate
	 * Ref clock = 26MHz (0.038us)
	 * Step = ( code + 1 ) * 0.038 us
	 */

	if (isRising)
		pmic_read_interface(MT6358_BUCK_VGPU_CFG0, &sfchg_rate_reg,
				PMIC_RG_BUCK_VGPU_SFCHG_RRATE_MASK,
				PMIC_RG_BUCK_VGPU_SFCHG_RRATE_SHIFT);
	else
		pmic_read_interface(MT6358_BUCK_VGPU_CFG0, &sfchg_rate_reg,
				PMIC_RG_BUCK_VGPU_SFCHG_FRATE_MASK,
				PMIC_RG_BUCK_VGPU_SFCHG_FRATE_SHIFT);

	gpufreq_pr_debug(
		"@%s: isRising = %d, sfchg_rate_vgpu = %d, sfchg_rate_reg = 0x%x\n",
		__func__, isRising, sfchg_rate_vgpu, sfchg_rate_reg);

	sfchg_rate_vgpu = sfchg_rate_reg;
#endif
	return sfchg_rate_vgpu;
}

/*
 * VSRAM slew rate calculation
 * false : falling rate
 * true : rising rate
 */
static unsigned int __calculate_vsram_sfchg_rate(bool isRising)
{
	unsigned int sfchg_rate_vsram = 1;
#if 0
	unsigned int sfchg_rate_reg;


	/* [MT6358] RG_LDO_VSRAM_GPU_SFCHG_RRATE and
	 * RG_LDO_VSRAM_GPU_SFCHG_FRATE
	 *    7'd4 : 0.19us
	 *    7'd8 : 0.34us
	 *    7'd11 : 0.46us
	 *    7'd17 : 0.69us
	 *    7'd23 : 0.92us
	 *    7'd25 : 1us
	 */

	if (isRising)
		pmic_read_interface(MT6358_LDO_VSRAM_GPU_CFG0, &sfchg_rate_reg,
				PMIC_RG_LDO_VSRAM_GPU_SFCHG_RRATE_MASK,
				PMIC_RG_LDO_VSRAM_GPU_SFCHG_RRATE_SHIFT);
	else
		pmic_read_interface(MT6358_LDO_VSRAM_GPU_CFG0, &sfchg_rate_reg,
				PMIC_RG_LDO_VSRAM_GPU_SFCHG_FRATE_MASK,
				PMIC_RG_LDO_VSRAM_GPU_SFCHG_FRATE_SHIFT);

	gpufreq_pr_debug(
		"@%s: isRising = %d, sfchg_rate_vsram = %d, sfchg_rate_reg = 0x%x\n",
		__func__, isRising, sfchg_rate_vsram, sfchg_rate_reg);

	sfchg_rate_vsram = sfchg_rate_reg;
#endif
	return sfchg_rate_vsram;
}

/*
 * get post divider value
 * - VCO needs proper post divider value to get corresponding
 * dds value to adjust PLL value.
 * - e.g: In Vinson, VCO range is 2.0GHz - 4.0GHz, required frequency
 * is 900MHz, so post
 * divider could be 2(X), 4(3600/4), 8(X), 16(X).
 * - It may have special requiremt by DE in different efuse value
 * - e.g: In mt6757, efuse value(3'b001), VCO range is 1.5GHz - 3.8GHz,
 * required frequency
 * range is 375MHz - 900MHz, It can only use post divider 4, no post divider 2.
 */
static enum g_post_divider_power_enum
__mt_gpufreq_get_post_divider_power(unsigned int freq, unsigned int efuse)
{
	/* [MT6765]
	 *    VCO range: 1.5GHz - 3.8GHz by divider 1/2/4/8/16,
	 *    PLL range: 125MHz - 3.8GHz,
	 *    | VCO MAX | VCO MIN | POSTDIV | PLL OUT MAX | PLL OUT MIN |
	 *    |  3800   |  1500   |    1    |   3800MHz   |  1500MHz    | (X)
	 *    |  3800   |  1500   |    2    |   1900MHz   |   750MHz    | (X)
	 *    |  3800   |  1500   |    4    |   950MHz    |   375MHz    | (O)
	 *    |  3800   |  1500   |    8    |   475MHz    |   187.5MHz  | (O)
	 *    |  3800   |  2000   |   16    |   237.5MHz  |   125MHz    | (X)
	 */
	enum g_post_divider_power_enum post_divider_power = POST_DIV4;

	if (freq < 375000)
		post_divider_power = POST_DIV8;

	if (g_cur_post_divider_power != post_divider_power) {
		g_parking = true;
		g_cur_post_divider_power = post_divider_power;
	}

	gpufreq_pr_debug(
		"@%s: freq = %d, post_divider_power = %d, g_parking = %d\n",
		__func__, freq, post_divider_power, g_parking);

	return post_divider_power;
}

/*
 * get current frequency (KHZ)
 */
static unsigned int __mt_gpufreq_get_cur_freq(void)
{
	unsigned long mfgpll = 0;
	unsigned int post_divider_power = 0;
	unsigned int freq_khz = 0;
	unsigned long dds;

	mfgpll = DRV_Reg32(GPUPLL_CON1);
	dds = mfgpll & (0x3FFFFF);

	post_divider_power = (mfgpll & (0x7 << POST_DIV_SHIFT))
		>> POST_DIV_SHIFT;

	freq_khz = (((dds * TO_MHz_TAIL + ROUNDING_VALUE) * GPUPLL_FIN)
		>> DDS_SHIFT)
		/ (1 << post_divider_power) * TO_MHz_HEAD;

	gpufreq_pr_debug(
		"@%s: mfgpll = 0x%lx, freq = %d KHz, post_divider_power = %d\n",
		__func__, mfgpll, freq_khz, post_divider_power);

	return freq_khz;
}

/*
 * get current vsram voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_vsram_volt(void)
{
	unsigned int volt = 0;

	/* WARRNING: regulator_get_voltage prints uV */
#ifdef USE_STAND_ALONE_VSRAM
	volt = regulator_get_voltage(g_pmic->reg_vsram_gpu) / 10;

	gpufreq_pr_debug("@%s: volt = %d\n", __func__, volt);
#else
	gpufreq_pr_debug("@%s: volt = 87500 (FIXED VALUE)\n", __func__);
#endif

	return volt;
}

/*
 * get current voltage (mV * 100)
 */
static unsigned int __mt_gpufreq_get_cur_volt(void)
{
	unsigned int volt = 0;

	if (g_volt_enable_state) {
#ifdef USE_STAND_ALONE_VGPU
		/* WARRNING: regulator_get_voltage prints uV */
		volt = regulator_get_voltage(g_pmic->reg_vgpu) / 10;
#else
		volt = regulator_get_voltage(g_pmic->reg_vcore) / 10;
#endif
	}

	gpufreq_pr_debug("@%s: volt = %d\n", __func__, volt);

	return volt;
}

/*
 * get OPP table index by voltage (mV * 100)
 */
static int __mt_gpufreq_get_opp_idx_by_volt(unsigned int volt)
{
	int i = g_opp_idx_num - 1;

	while (i >= 0) {
		if (g_opp_table[i--].gpufreq_volt >= volt)
			goto EXIT;
	}

EXIT:
	return i+1;
}

/*
 * calculate vsram_volt via given volt
 */
static unsigned int
__mt_gpufreq_get_vsram_volt_by_target_volt(unsigned int volt)
{
	unsigned int target_vsram;

	target_vsram =
		g_opp_table[g_fixed_vsram_volt_idx].gpufreq_vsram;

	return target_vsram;
}

/*
 * get limited frequency by limited power (mW)
 */
static unsigned int
__mt_gpufreq_get_limited_freq_by_power(unsigned int limited_power)
{
	int i;
	unsigned int limited_freq;

	limited_freq = g_power_table[g_opp_idx_num - 1].gpufreq_khz;

	for (i = 0; i < g_opp_idx_num; i++) {
		if (g_power_table[i].gpufreq_power <= limited_power) {
			limited_freq = g_power_table[i].gpufreq_khz;
			break;
		}
	}

	gpufreq_pr_debug("@%s: limited_freq = %d\n", __func__, limited_freq);

	return limited_freq;
}

#ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE
/* update OPP power table */
static void __mt_update_gpufreqs_power_table(void)
{
	int i;
	int temp = 0;
	unsigned int freq = 0;
	unsigned int volt = 0;

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	gpufreq_pr_debug("@%s: temp = %d\n", __func__, temp);

	mutex_lock(&mt_gpufreq_lock);

	if ((temp >= -20) && (temp <= 125)) {
		for (i = 0; i < g_opp_idx_num; i++) {
			freq = g_power_table[i].gpufreq_khz;
			volt = g_power_table[i].gpufreq_volt;

			__mt_gpufreq_calculate_power(i, freq, volt, temp);

			gpufreq_pr_debug(
			"@%s: [%d] freq_khz = %d, volt = %d, power = %d\n",
				__func__, i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
		}
	} else {
		gpufreq_perr(
		"@%s: temp < -20 or temp > 125, NOT update power table!\n",
		__func__);
	}

	mutex_unlock(&mt_gpufreq_lock);
}
#endif /* ifdef MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE */

/* update OPP limited index for Thermal/Power/PBM protection */
static void __mt_gpufreq_update_max_limited_idx(void)
{
	int i = 0;
	unsigned int limited_idx = 0;

	/* Check lowest frequency index in all limitation */
	for (i = 0; i < NUMBER_OF_LIMITED_IDX; i++) {
		if (g_limited_idx_array[i] > limited_idx) {
			if (!g_limited_ignore_array[i])
				limited_idx = g_limited_idx_array[i];
		}
	}

	g_max_limited_idx = limited_idx;

	gpufreq_pr_debug("@%s: g_max_limited_idx = %d\n",
		__func__, g_max_limited_idx);
}

#ifdef MT_GPUFREQ_BATT_OC_PROTECT
/*
 * limit OPP index for Over Currents (OC) protection
 */
static void __mt_gpufreq_batt_oc_protect(unsigned int limited_idx)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug("@%s: limited_idx = %d\n", __func__, limited_idx);

	g_limited_idx_array[IDX_BATT_OC_LIMITED] = limited_idx;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
}
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */

#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
/*
 * limit OPP index for Battery Percentage protection
 */
static void __mt_gpufreq_batt_percent_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug("@%s: limited_index = %d\n", __func__, limited_index);

	g_limited_idx_array[IDX_BATT_PERCENT_LIMITED] = limited_index;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
}
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/*
 * limit OPP index for Low Battery Volume protection
 */
static void __mt_gpufreq_low_batt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_pr_debug("@%s: limited_index = %d\n", __func__, limited_index);

	g_limited_idx_array[IDX_LOW_BATT_LIMITED] = limited_index;
	__mt_gpufreq_update_max_limited_idx();

	mutex_unlock(&mt_gpufreq_power_lock);
}
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */

/*
 * kick Power Budget Manager(PBM) when OPP changed
 */
static void __mt_gpufreq_kick_pbm(int enable)
{
	unsigned int power;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int found = 0;
	int tmp_idx = -1;
	int i;

	cur_freq = __mt_gpufreq_get_cur_freq();
	cur_volt = __mt_gpufreq_get_cur_volt();

	if (enable) {
		for (i = 0; i < g_opp_idx_num; i++) {
			if (g_power_table[i].gpufreq_khz == cur_freq) {
				/* record idx since current voltage
				 * may not in DVFS table
				 */
				tmp_idx = i;

				if (g_power_table[i].gpufreq_volt == cur_volt) {
					power = g_power_table[i].gpufreq_power;
					found = 1;
					kicker_pbm_by_gpu(true,
						power, cur_volt / 100);
					gpufreq_pr_debug(
						"@%s: request GPU power = %d,",
						__func__, power);
					gpufreq_pr_debug(
				" cur_volt = %d uV, cur_freq = %d KHz\n",
					cur_volt * 10, cur_freq);
					return;
				}
			}
		}

		if (!found) {
			gpufreq_pr_debug("@%s: tmp_idx = %d\n",
				__func__, tmp_idx);
			if (tmp_idx != -1 && tmp_idx < g_opp_idx_num) {
				/* freq to find corresponding power budget */
				power = g_power_table[tmp_idx].gpufreq_power;
				kicker_pbm_by_gpu(true, power, cur_volt / 100);
				gpufreq_pr_debug("@%s: request GPU power = %d,",
				__func__, power);
				gpufreq_pr_debug(
				" cur_volt = %d uV, cur_freq = %d KHz\n",
				cur_volt * 10, cur_freq);
			} else {
				gpufreq_pr_debug(
				"@%s: Cannot found request power in power table",
				__func__);
				gpufreq_pr_debug(
				", cur_freq = %d KHz, cur_volt = %d uV\n",
				cur_freq, cur_volt * 10);
			}
		}
	} else {
		kicker_pbm_by_gpu(false, 0, cur_volt / 100);
	}
}

/*
 * (default) OPP table initialization
 */
static void __mt_gpufreq_setup_opp_table(struct g_opp_table_info *freqs,
	int num)
{
	int i = 0;

	g_opp_table = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	g_opp_table_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);

	if (g_opp_table == NULL || g_opp_table_default == NULL)
		return;

	for (i = 0; i < num; i++) {
		g_opp_table[i].gpufreq_khz = freqs[i].gpufreq_khz;
		g_opp_table[i].gpufreq_volt = freqs[i].gpufreq_volt;
		g_opp_table[i].gpufreq_vsram = freqs[i].gpufreq_vsram;
		g_opp_table[i].gpufreq_idx = freqs[i].gpufreq_idx;

		g_opp_table_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
		g_opp_table_default[i].gpufreq_volt = freqs[i].gpufreq_volt;
		g_opp_table_default[i].gpufreq_vsram = freqs[i].gpufreq_vsram;
		g_opp_table_default[i].gpufreq_idx = freqs[i].gpufreq_idx;

		gpufreq_pr_debug(
			"@%s: [%d] idx = %u, freq_khz = %u, volt = %u, vsram = %u\n",
			__func__, i, freqs[i].gpufreq_idx, freqs[i].gpufreq_khz,
			freqs[i].gpufreq_volt, freqs[i].gpufreq_vsram);
	}

	g_opp_idx_num = num;
	g_max_limited_idx = 0;

	__mt_gpufreq_calculate_springboard_opp_index();
	__mt_gpufreq_setup_opp_power_table(num);
}

/*
 * OPP power table initialization
 */
static void __mt_gpufreq_setup_opp_power_table(int num)
{
	int i = 0;
	int temp = 0;

	g_power_table = kzalloc((num) *
		sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);

	if (g_power_table == NULL)
		return;

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif /* ifdef CONFIG_THERMAL */

	gpufreq_pr_debug("@%s: temp = %d\n", __func__, temp);

	if ((temp < -20) || (temp > 125)) {
		gpufreq_pr_debug("@%s: temp < -20 or temp > 125!\n", __func__);
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		g_power_table[i].gpufreq_khz = g_opp_table[i].gpufreq_khz;
		g_power_table[i].gpufreq_volt = g_opp_table[i].gpufreq_volt;

		__mt_gpufreq_calculate_power(i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt, temp);

		gpufreq_pr_debug("@%s: [%d], freq_khz = %u, volt = %u, power = %u\n",
				__func__, i, g_power_table[i].gpufreq_khz,
				g_power_table[i].gpufreq_volt,
				g_power_table[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(g_power_table, num);
#endif /* ifdef CONFIG_THERMAL */
}

/*
 *Set default OPP index at driver probe function
 */
static void __mt_gpufreq_set_initial(void)
{
	unsigned int cur_volt = 0;
	unsigned int cur_freq = 0;
	unsigned int cur_vsram_volt = 0;

	mutex_lock(&mt_gpufreq_lock);

	/* default OPP index */
	g_cur_opp_cond_idx = 0;

	/* set POST_DIVIDER initial value */
	g_cur_post_divider_power = POST_DIV4;

	g_parking = false;

	gpufreq_pr_debug("@%s: initial opp index = %d\n",
		__func__, g_cur_opp_cond_idx);

	cur_vsram_volt = __mt_gpufreq_get_cur_vsram_volt();
	cur_volt = __mt_gpufreq_get_cur_volt();
	cur_freq = __mt_gpufreq_get_cur_freq();

	__mt_gpufreq_set(cur_freq, g_opp_table[g_cur_opp_cond_idx].gpufreq_khz,
			cur_volt, g_opp_table[g_cur_opp_cond_idx].gpufreq_volt,
			cur_vsram_volt,
			g_opp_table[g_cur_opp_cond_idx].gpufreq_vsram);

	g_cur_opp_freq = g_opp_table[g_cur_opp_cond_idx].gpufreq_khz;
	g_cur_opp_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_volt;
	g_cur_opp_vsram_volt = g_opp_table[g_cur_opp_cond_idx].gpufreq_vsram;
	g_cur_opp_idx = g_opp_table[g_cur_opp_cond_idx].gpufreq_idx;
	g_cur_opp_cond_idx = g_cur_opp_idx;

	mutex_unlock(&mt_gpufreq_lock);
}

/*
 * gpufreq driver probe
 */
static int __mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	struct device_node *apmixed_node;
	struct device_node *node;
	int i;

	gpufreq_pr_info("@%s: gpufreq driver probe, clock is %d KHz\n",
			__func__, mt_get_ckgen_freq(9));

	g_opp_stress_test_state = false;
	g_DVFS_off_by_ptpod_idx = 0;

	/* Pause GPU DVFS for debug */
	/* g_keep_opp_freq_state = true; */

	node = of_find_matching_node(NULL, g_gpufreq_of_match);
	if (!node)
		gpufreq_perr("@%s: find GPU node failed\n", __func__);

	g_clk = kzalloc(sizeof(struct g_clk_info), GFP_KERNEL);
	if (g_clk == NULL)
		return -ENOMEM;

	g_clk->clk_mux = devm_clk_get(&pdev->dev, "clk_mux");
	if (IS_ERR(g_clk->clk_mux)) {
		gpufreq_perr("@%s: cannot get clk_mux\n", __func__);
		return PTR_ERR(g_clk->clk_mux);
	}

	g_clk->clk_main_parent = devm_clk_get(&pdev->dev, "clk_main_parent");
	if (IS_ERR(g_clk->clk_main_parent)) {
		gpufreq_perr("@%s: cannot get clk_main_parent\n", __func__);
		return PTR_ERR(g_clk->clk_main_parent);
	}

	g_clk->clk_sub_parent = devm_clk_get(&pdev->dev, "clk_sub_parent");
	if (IS_ERR(g_clk->clk_sub_parent)) {
		gpufreq_perr("@%s: cannot get clk_sub_parent\n", __func__);
		return PTR_ERR(g_clk->clk_sub_parent);
	}


	g_clk->mtcmos_mfg_async = devm_clk_get(&pdev->dev, "mtcmos_mfg_async");
	if (IS_ERR(g_clk->mtcmos_mfg_async)) {
		gpufreq_perr("@%s: cannot get mtcmos_mfg_async\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_async);
	}

	g_clk->mtcmos_mfg = devm_clk_get(&pdev->dev, "mtcmos_mfg");
	if (IS_ERR(g_clk->mtcmos_mfg)) {
		gpufreq_perr("@%s: cannot get mtcmos_mfg\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg);
	}

	g_clk->mtcmos_mfg_core0 = devm_clk_get(&pdev->dev, "mtcmos_mfg_core0");
	if (IS_ERR(g_clk->mtcmos_mfg_core0)) {
		gpufreq_perr("@%s: cannot get mtcmos_mfg_core0\n", __func__);
		return PTR_ERR(g_clk->mtcmos_mfg_core0);
	}

	pr_info("[GPU/DVFS][INFO]@%s: clk_mux is at 0x%p, ",
		__func__, g_clk->clk_mux);
	pr_info("clk_main_parent is at 0x%p, \t", g_clk->clk_main_parent);
	pr_info("mtcmos_mfg_async is at 0x%p, \t", g_clk->mtcmos_mfg_async);
	pr_info("mtcmos_mfg is at 0x%p, mtcmos_mfg_core0 is at 0x%p, ",
		g_clk->mtcmos_mfg, g_clk->mtcmos_mfg_core0);


	/* check EFUSE register 0x11f10050[27:24] */
	/* Free Version : 4'b0000 */
	/* 1GHz Version : 4'b0001 */
	/* 950MHz Version : 4'b0010 */
	/* 900MHz Version : 4'b0011 (Segment1) */
	/* 850MHz Version : 4'b0100 */
	/* 800MHz Version : 4'b0101 (Segment2) */
	/* 750MHz Version : 4'b0110 */
	/* 700MHz Version : 4'b0111 (Segment3) */

	g_efuse_id = get_devinfo_with_index(30);
	if (g_efuse_id == 0x8 || g_efuse_id == 0xf) {
		/* 6762M */
		g_segment_id = MT6762M_SEGMENT;
	} else if (g_efuse_id == 0x1 || g_efuse_id == 0x7
		|| g_efuse_id == 0x9) {
		/* 6762 */
		g_segment_id = MT6762_SEGMENT;
	} else if (g_efuse_id == 0x2 || g_efuse_id == 0x5) {
		/* SpeedBin */
		g_segment_id = MT6765T_SEGMENT;
	} else if (g_efuse_id == 0x20) {
		/* 6762D */
		g_segment_id = MT6762D_SEGMENT;
	} else {
		/* Other Version, set default segment */
		g_segment_id = MT6765_SEGMENT;
	}
	gpufreq_pr_info("@%s: g_efuse_id = 0x%08X, g_segment_id = %d\n",
		__func__, g_efuse_id, g_segment_id);

	/* alloc PMIC regulator */
	g_pmic = kzalloc(sizeof(struct g_pmic_info), GFP_KERNEL);
	if (g_pmic == NULL)
		return -ENOMEM;
#ifdef USE_STAND_ALONE_VGPU
	g_pmic->reg_vgpu = regulator_get(&pdev->dev, "ext_buck_vgpu");
	if (IS_ERR(g_pmic->reg_vgpu)) {
		gpufreq_perr("@%s: cannot get VGPU\n", __func__);
		return PTR_ERR(g_pmic->reg_vgpu);
	}
#else
	pm_qos_add_request(&g_pmic->pm_vgpu,
	PM_QOS_VCORE_OPP, VCORE_OPP_0);

	g_pmic->reg_vcore = regulator_get(&pdev->dev, "vcore");
	if (IS_ERR(g_pmic->reg_vcore)) {
		gpufreq_perr("@%s: cannot get VCORE\n", __func__);
		return PTR_ERR(g_pmic->reg_vcore);
	}

#endif
#ifdef USE_STAND_ALONE_VSRAM
	g_pmic->reg_vsram_gpu = regulator_get(&pdev->dev, "vsram_gpu");
	if (IS_ERR(g_pmic->reg_vsram_gpu)) {
		gpufreq_perr("@%s: cannot get VSRAM_GPU\n", __func__);
		return PTR_ERR(g_pmic->reg_vsram_gpu);
	}
#endif

#ifdef MT_GPUFREQ_STATIC_PWR_READY2USE
	/* Initial leackage power usage */
	mt_spower_init();
#endif /* ifdef MT_GPUFREQ_STATIC_PWR_READY2USE */

	/* setup OPP table by device ID */
	if (g_segment_id == MT6762M_SEGMENT) {
		__mt_gpufreq_setup_opp_table(g_opp_table_segment1,
			ARRAY_SIZE(g_opp_table_segment1));
		g_fixed_vsram_volt_idx = 0;
	} else if (g_segment_id == MT6762_SEGMENT) {
		__mt_gpufreq_setup_opp_table(g_opp_table_segment2,
			ARRAY_SIZE(g_opp_table_segment2));
		g_fixed_vsram_volt_idx = 2;
	} else if (g_segment_id == MT6765_SEGMENT) {
		__mt_gpufreq_setup_opp_table(g_opp_table_segment3,
			ARRAY_SIZE(g_opp_table_segment3));
		g_fixed_vsram_volt_idx = 2;
	} else if (g_segment_id == MT6765T_SEGMENT) {
		__mt_gpufreq_setup_opp_table(g_opp_table_segment4,
			ARRAY_SIZE(g_opp_table_segment4));
		g_fixed_vsram_volt_idx = 2;
	} else if (g_segment_id == MT6762D_SEGMENT) {
		__mt_gpufreq_setup_opp_table(g_opp_table_segment5,
			ARRAY_SIZE(g_opp_table_segment5));
		g_fixed_vsram_volt_idx = 2;
	}

	/* setup PMIC init value */
#ifdef USE_STAND_ALONE_VGPU
	g_vgpu_sfchg_rrate = __calculate_vgpu_sfchg_rate(true);
	g_vgpu_sfchg_frate = __calculate_vgpu_sfchg_rate(false);
#endif
#ifdef USE_STAND_ALONE_VSRAM
	g_vsram_sfchg_rrate = __calculate_vsram_sfchg_rate(true);
	g_vsram_sfchg_frate = __calculate_vsram_sfchg_rate(false);
#endif

	/* set VSRAM_GPU */
#ifdef USE_STAND_ALONE_VSRAM
	regulator_set_voltage(g_pmic->reg_vsram_gpu,
		VSRAM_GPU_MAX_VOLT * 10, VSRAM_GPU_MAX_VOLT * 10 + 125);
#endif
	/* set VGPU */
#ifdef USE_STAND_ALONE_VGPU
	regulator_set_voltage(g_pmic->reg_vgpu, VGPU_MAX_VOLT * 10,
		VGPU_MAX_VOLT * 10 + 125);
#endif

	/* enable bucks (VGPU && VSRAM_GPU) enforcement */
#ifdef USE_STAND_ALONE_VSRAM
	if (regulator_enable(g_pmic->reg_vsram_gpu))
		gpufreq_perr("@%s: enable VSRAM_GPU failed\n", __func__);
#endif
#ifdef USE_STAND_ALONE_VGPU
	if (regulator_enable(g_pmic->reg_vgpu))
		gpufreq_perr("@%s: enable VGPU failed\n", __func__);
#endif

	/*pr_info("[GPU/DVFS][INFO]@%s: VGPU is enabled = %d (%d mV),"
	 *		" VSRAM_GPU is enabled = %d (%d mV)\n",
	 *		__func__, regulator_is_enabled(g_pmic->reg_vgpu),
	 *		(regulator_get_voltage(g_pmic->reg_vgpu) / 1000),
	 *		regulator_is_enabled(g_pmic->reg_vsram_gpu),
	 *		(regulator_get_voltage(g_pmic->reg_vsram_gpu) / 1000));
	 */

	g_volt_enable_state = true;

	/* init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		gpufreq_perr("@%s: APMIXED iomap failed", __func__);
		return -ENOENT;
	}

	/* setup initial frequency */
	__mt_gpufreq_set_initial();
	/*
	 * pr_info("[GPU/DVFS][INFO]@%s: current freq = %d KHz,"
	 *		" current volt = %d uV, \t"
	 *		"g_cur_opp_freq = %d, g_cur_opp_volt = %d,"
	 *		"g_cur_opp_vsram_volt = %d, \t"
	 *		"g_cur_opp_idx = %d, g_cur_opp_cond_idx = %d\n",
	 *		__func__, __mt_gpufreq_get_cur_freq(),
	 *		__mt_gpufreq_get_cur_volt() * 10, g_cur_opp_freq,
	 *		g_cur_opp_volt, g_cur_opp_vsram_volt, g_cur_opp_idx,
	 *		g_cur_opp_cond_idx);
	 */

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	g_low_batt_limited_idx_lvl_0 = 0;
	for (i = 0; i < g_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_khz <=
			MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ) {
			g_low_batt_limited_idx_lvl_2 = i;
			break;
		}
	}
	register_low_battery_notify(&mt_gpufreq_low_batt_callback,
		LOW_BATTERY_PRIO_GPU);
#endif /* ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */

#ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT
	g_batt_percent_limited_idx_lv_0 = 0;
	for (i = 0; i < g_opp_idx_num; i++) {
		g_batt_percent_limited_idx_lv_1 = 0;
		if (g_opp_table[i].gpufreq_khz ==
			MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ) {
			g_batt_percent_limited_idx_lv_1 = i;
			break;
		}
	}
	register_battery_percent_notify(&mt_gpufreq_batt_percent_callback,
		BATTERY_PERCENT_PRIO_GPU);
#endif /* ifdef MT_GPUFREQ_BATT_PERCENT_PROTECT */

#ifdef MT_GPUFREQ_BATT_OC_PROTECT
	g_batt_oc_limited_idx_lvl_0 = 0;
	for (i = 0; i < g_opp_idx_num; i++) {
		if (g_opp_table[i].gpufreq_khz <=
			MT_GPUFREQ_BATT_OC_LIMIT_FREQ) {
			g_batt_oc_limited_idx_lvl_1 = i;
			break;
		}
	}
	register_battery_oc_notify(&mt_gpufreq_batt_oc_callback,
		BATTERY_OC_PRIO_GPU);
#endif /* ifdef MT_GPUFREQ_BATT_OC_PROTECT */


	pr_info(
		"[GPU/DVFS][INFO]@%s: VGPU sfchg raising rate: %d us,",
		__func__, g_vgpu_sfchg_rrate);
	pr_info(
		" VGPU sfchg falling rate: %d us, \t", g_vgpu_sfchg_frate);
	pr_info(
		"VSRAM_GPU sfchg raising rate: %d us,", g_vsram_sfchg_rrate);
	pr_info(
		" VSRAM_GPU sfchg falling rate: %d us, \t",
		g_vsram_sfchg_frate);
	pr_info(
		"PMIC SRCLKEN_HIGH time: %d us\n", PMIC_SRCLKEN_HIGH_TIME_US);



#ifdef CONFIG_MTK_QOS_SUPPORT
	mt_gpu_bw_init();
#endif

	return 0;
}

/*
 * register the gpufreq driver
 */
static int __init __mt_gpufreq_init(void)
{
	int ret = 0;

#ifdef BRING_UP
	/* Skip driver init in bring up stage */
	return 0;
#endif

	gpufreq_pr_debug("@%s: start to initialize gpufreq driver\n", __func__);

#ifdef CONFIG_PROC_FS
	if (__mt_gpufreq_create_procfs())
		goto out;
#endif /* ifdef CONFIG_PROC_FS */

	/* register platform driver */
	ret = platform_driver_register(&g_gpufreq_pdrv);
	if (ret)
		gpufreq_perr("@%s: fail to register gpufreq driver\n",
		__func__);

out:
	return ret;
}

/*
 * unregister the gpufreq driver
 */
static void __exit __mt_gpufreq_exit(void)
{
	platform_driver_unregister(&g_gpufreq_pdrv);
}

#ifdef USE_STAND_ALONE_VGPU
late_initcall(__mt_gpufreq_init);
#else
module_init(__mt_gpufreq_init);
#endif
module_exit(__mt_gpufreq_exit);

MODULE_DEVICE_TABLE(of, g_gpufreq_of_match);
MODULE_DESCRIPTION("MediaTek GPU-DVFS driver");
MODULE_LICENSE("GPL");
