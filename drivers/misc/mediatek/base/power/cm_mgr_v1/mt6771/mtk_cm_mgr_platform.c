/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>
#include <trace/events/mtk_events.h>
#include <linux/sched/clock.h>
#ifdef MET_SUPPORT
#include <mt-plat/met_drv.h>
#endif /* MET_SUPPORT */

#include <linux/of.h>
#include <linux/of_address.h>
#include <mtk_cm_mgr.h>

#define CREATE_TRACE_POINTS
#include "mtk_cm_mgr_platform_events.h"

#include <linux/fb.h>
#include <linux/notifier.h>

#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#include <mtk_spm_vcore_dvfs.h>
#ifdef USE_IDLE_NOTIFY
#include "mtk_idle.h"
#endif /* USE_IDLE_NOTIFY */

#ifdef MET_SUPPORT
struct cm_mgr_met_data {
	unsigned int cm_mgr_power[14];
	unsigned int cm_mgr_count[4];
	unsigned int cm_mgr_opp[6];
	unsigned int cm_mgr_loading[12];
	unsigned int cm_mgr_ratio[12];
	unsigned int cm_mgr_bw;
	unsigned int cm_mgr_valid;
};

/******************** MET BEGIN ********************/
typedef void (*cm_mgr_value_handler_t) (unsigned int cnt, unsigned int *value);

static struct cm_mgr_met_data met_data;
static cm_mgr_value_handler_t cm_mgr_power_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_count_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_opp_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_loading_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_ratio_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_bw_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_valid_dbg_handler;

#define CM_MGR_MET_REG_FN_VALUE(name)				\
	void cm_mgr_register_##name(cm_mgr_value_handler_t handler)	\
{								\
	name##_dbg_handler = handler;				\
}								\
EXPORT_SYMBOL(cm_mgr_register_##name)

CM_MGR_MET_REG_FN_VALUE(cm_mgr_power);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_count);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_opp);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_loading);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_ratio);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_bw);
CM_MGR_MET_REG_FN_VALUE(cm_mgr_valid);
/********************* MET END *********************/
#endif /* MET_SUPPORT */

void cm_mgr_update_met(void)
{
#ifdef MET_SUPPORT
	int cpu;

	met_data.cm_mgr_power[0] = cpu_power_up_array[0];
	met_data.cm_mgr_power[1] = cpu_power_up_array[1];
	met_data.cm_mgr_power[2] = cpu_power_down_array[0];
	met_data.cm_mgr_power[3] = cpu_power_down_array[1];
	met_data.cm_mgr_power[4] = cpu_power_up[0];
	met_data.cm_mgr_power[5] = cpu_power_up[1];
	met_data.cm_mgr_power[6] = cpu_power_down[0];
	met_data.cm_mgr_power[7] = cpu_power_down[1];
	met_data.cm_mgr_power[8] = cpu_power_up[0] + cpu_power_up[1];
	met_data.cm_mgr_power[9] = cpu_power_down[0] + cpu_power_down[1];
	met_data.cm_mgr_power[10] = (unsigned int)vcore_power_up;
	met_data.cm_mgr_power[11] = (unsigned int)vcore_power_down;
	met_data.cm_mgr_power[12] = v2f[0];
	met_data.cm_mgr_power[13] = v2f[1];

	met_data.cm_mgr_count[0] = count[0];
	met_data.cm_mgr_count[1] = count[1];
	met_data.cm_mgr_count[2] = count_ack[0];
	met_data.cm_mgr_count[3] = count_ack[1];

	met_data.cm_mgr_opp[0] = vcore_dram_opp;
	met_data.cm_mgr_opp[1] = vcore_dram_opp_cur;
	met_data.cm_mgr_opp[2] = cpu_opp_cur[0];
	met_data.cm_mgr_opp[3] = cpu_opp_cur[1];
	met_data.cm_mgr_opp[4] = debounce_times_up;
	met_data.cm_mgr_opp[5] = debounce_times_down;

	met_data.cm_mgr_loading[0] = cm_mgr_abs_load;
	met_data.cm_mgr_loading[1] = cm_mgr_rel_load;
	met_data.cm_mgr_loading[2] = max_load[0];
	met_data.cm_mgr_loading[3] = max_load[1];
	for_each_possible_cpu(cpu) {
		if (cpu >= CM_MGR_CPU_COUNT)
			break;
		met_data.cm_mgr_loading[4 + cpu] = cpu_load[cpu];
	}

	met_data.cm_mgr_ratio[0] = ratio_max[0];
	met_data.cm_mgr_ratio[1] = ratio_max[1];
	met_data.cm_mgr_ratio[2] = ratio_scale[0];
	met_data.cm_mgr_ratio[3] = ratio_scale[1];
	met_data.cm_mgr_ratio[4] = ratio[0];
	met_data.cm_mgr_ratio[5] = ratio[1];
	met_data.cm_mgr_ratio[6] = ratio[2];
	met_data.cm_mgr_ratio[7] = ratio[3];
	met_data.cm_mgr_ratio[8] = ratio[4];
	met_data.cm_mgr_ratio[9] = ratio[5];
	met_data.cm_mgr_ratio[10] = ratio[6];
	met_data.cm_mgr_ratio[11] = ratio[7];

	met_data.cm_mgr_bw = total_bw;

	met_data.cm_mgr_valid = cps_valid;

	if (cm_mgr_power_dbg_handler)
		cm_mgr_power_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_power),
				met_data.cm_mgr_power);
	if (cm_mgr_count_dbg_handler)
		cm_mgr_count_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_count),
				met_data.cm_mgr_count);
	if (cm_mgr_opp_dbg_handler)
		cm_mgr_opp_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_opp),
				met_data.cm_mgr_opp);
	if (cm_mgr_loading_dbg_handler)
		cm_mgr_loading_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_loading),
				met_data.cm_mgr_loading);
	if (cm_mgr_ratio_dbg_handler)
		cm_mgr_ratio_dbg_handler(ARRAY_SIZE(met_data.cm_mgr_ratio),
				met_data.cm_mgr_ratio);
	if (cm_mgr_bw_dbg_handler)
		cm_mgr_bw_dbg_handler(1, &met_data.cm_mgr_bw);
	if (cm_mgr_valid_dbg_handler)
		cm_mgr_valid_dbg_handler(1, &met_data.cm_mgr_valid);
#endif /* MET_SUPPORT */
}

#include <linux/cpu_pm.h>
static unsigned int cm_mgr_idle_mask = CLUSTER0_MASK | CLUSTER1_MASK;

void __iomem *mcucfg_mp0_counter_base;
void __iomem *mcucfg_mp2_counter_base;

spinlock_t cm_mgr_cpu_mask_lock;

#define diff_value_overflow(diff, a, b) do { \
	if ((a) >= (b)) \
		diff = (a) - (b); \
	else \
		diff = 0xffffffff - (b) + (a); \
} while (0) \

#define CM_MGR_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* #define USE_DEBUG_LOG */

struct stall_s {
	unsigned int clustor[CM_MGR_CPU_CLUSTER];
	unsigned long long stall_val[CM_MGR_CPU_COUNT];
	unsigned long long stall_val_diff[CM_MGR_CPU_COUNT];
	unsigned long long time_ns[CM_MGR_CPU_COUNT];
	unsigned long long time_ns_diff[CM_MGR_CPU_COUNT];
	unsigned long long ratio[CM_MGR_CPU_COUNT];
	unsigned int ratio_max[CM_MGR_CPU_COUNT];
	unsigned int cpu;
	unsigned int cpu_count[CM_MGR_CPU_CLUSTER];
};

static struct stall_s stall_all;
static struct stall_s *pstall_all = &stall_all;
static int cm_mgr_idx = -1;

#ifdef USE_DEBUG_LOG
void debug_stall(int cpu)
{
	pr_debug("%s: cpu number %d ################\n", __func__,
			cpu);
	pr_debug("%s: clustor[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->clustor[cpu / CM_MGR_CPU_LIMIT]);
	pr_debug("%s: stall_val[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->stall_val[cpu]);
	pr_debug("%s: stall_val_diff[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->stall_val_diff[cpu]);
	pr_debug("%s: time_ns[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->time_ns[cpu]);
	pr_debug("%s: time_ns_diff[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->time_ns_diff[cpu]);
	pr_debug("%s: ratio[%d] 0x%016llx\n", __func__,
			cpu, pstall_all->ratio[cpu]);
	pr_debug("%s: ratio_max[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->ratio_max[cpu / CM_MGR_CPU_LIMIT]);
	pr_debug("%s: cpu 0x%08x\n", __func__, pstall_all->cpu);
	pr_debug("%s: cpu_count[%d] 0x%08x\n", __func__,
			cpu / CM_MGR_CPU_LIMIT,
			pstall_all->cpu_count[cpu / CM_MGR_CPU_LIMIT]);
}

void debug_stall_all(void)
{
	int i;

	for (i = 0; i < CM_MGR_CPU_COUNT; i++)
		debug_stall(i);
}
#endif /* USE_DEBUG_LOG */

static int cm_mgr_check_dram_type(void)
{
#ifdef CONFIG_MTK_DRAMC
	int ddr_type = get_ddr_type();
	int ddr_hz = dram_steps_freq(0);

	if ((ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4)
			&& ddr_hz == 3600) {
		cm_mgr_idx = CM_MGR_LP4X_2CH_3600;
		vcore_power_ratio_up[0] = 100;
		vcore_power_ratio_down[0] = 100;
	} else if (ddr_type == TYPE_LPDDR4X || ddr_type == TYPE_LPDDR4)
		cm_mgr_idx = CM_MGR_LP4X_2CH_3200;
	else if (ddr_type == TYPE_LPDDR3)
		cm_mgr_idx = CM_MGR_LP3_1CH_1866;
	pr_info("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);
#else
	cm_mgr_idx = 0;
	pr_info("#@# %s(%d) NO CONFIG_MTK_DRAMC !!! set cm_mgr_idx to 0x%x\n",
			__func__, __LINE__, cm_mgr_idx);
#endif /* CONFIG_MTK_DRAMC */

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

	return cm_mgr_idx;
};

int cm_mgr_get_idx(void)
{
	if (cm_mgr_idx < 0)
		return cm_mgr_check_dram_type();
	else
		return cm_mgr_idx;
};

int cm_mgr_get_stall_ratio(int cpu)
{
	return pstall_all->ratio[cpu];
}

int cm_mgr_get_max_stall_ratio(int cluster)
{
	return pstall_all->ratio_max[cluster];
}

int cm_mgr_get_cpu_count(int cluster)
{
	return pstall_all->cpu_count[cluster];
}

static ktime_t cm_mgr_init_time;
static int cm_mgr_init_flag;

static unsigned int cm_mgr_read_stall(int cpu)
{
	unsigned int val = 0;
#ifdef CONFIG_MTK_DRAMC
	unsigned long spinlock_save_flags;
#endif /* CONFIG_MTK_DRAMC */

	if (cm_mgr_init_flag) {
		if (ktime_ms_delta(ktime_get(), cm_mgr_init_time) <
				CM_MGR_INIT_DELAY_MS)
			return val;
		cm_mgr_init_flag = 0;
	}

#ifdef CONFIG_MTK_DRAMC
	if (!spin_trylock_irqsave(&sw_zq_tx_lock, spinlock_save_flags))
		return val;
#endif /* CONFIG_MTK_DRAMC */

	if (cpu < CM_MGR_CPU_LIMIT) {
		if (cm_mgr_idle_mask & CLUSTER0_MASK)
			val = cm_mgr_read(MP0_CPU0_STALL_COUNTER + 4 * cpu);
	} else {
		if (cm_mgr_idle_mask & CLUSTER1_MASK)
			val = cm_mgr_read(CPU0_STALL_COUNTER +
					4 * (cpu - CM_MGR_CPU_LIMIT));
	}
#ifdef CONFIG_MTK_DRAMC
	spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
#endif /* CONFIG_MTK_DRAMC */

	return val;
}

int cm_mgr_check_stall_ratio(int mp0, int mp1)
{
	unsigned int i;
	unsigned int clustor;
	unsigned int stall_val_new;
	unsigned long long time_ns_new;

	pstall_all->clustor[0] = mp0;
	pstall_all->clustor[1] = mp1;
	pstall_all->cpu = 0;
	for (i = 0; i < CM_MGR_CPU_CLUSTER; i++) {
		pstall_all->ratio_max[i] = 0;
		pstall_all->cpu_count[i] = 0;
	}

	for (i = 0; i < CM_MGR_CPU_COUNT; i++) {
		pstall_all->ratio[i] = 0;
		clustor = i / CM_MGR_CPU_LIMIT;

		if (pstall_all->clustor[clustor] == 0)
			continue;

		stall_val_new = cm_mgr_read_stall(i);

		if (stall_val_new == 0 || stall_val_new == 0xdeadbeef) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! stall_val_new is 0x%08x\n",
					__func__, stall_val_new);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			continue;
		}

		time_ns_new = sched_clock();
		pstall_all->time_ns_diff[i] =
			time_ns_new - pstall_all->time_ns[i];
		pstall_all->time_ns[i] = time_ns_new;
		if (pstall_all->time_ns_diff[i] == 0)
			continue;

		diff_value_overflow(pstall_all->stall_val_diff[i],
				stall_val_new, pstall_all->stall_val[i]);
		pstall_all->stall_val[i] = stall_val_new;

		if (pstall_all->stall_val_diff[i] == 0) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d diff == 0\n", __func__, i);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			continue;
		}

#ifdef CONFIG_ARM64
		pstall_all->ratio[i] = pstall_all->stall_val_diff[i] * 100000 /
			pstall_all->time_ns_diff[i] /
			pstall_all->clustor[clustor];
#else
		pstall_all->ratio[i] = pstall_all->stall_val_diff[i] * 100000;
		do_div(pstall_all->ratio[i], pstall_all->time_ns_diff[i]);
		do_div(pstall_all->ratio[i], pstall_all->clustor[clustor]);
#endif /* CONFIG_ARM64 */
		if (pstall_all->ratio[i] > 100) {
#ifdef USE_DEBUG_LOG
			pr_debug("%s: WARN!!! cpu:%d ratio > 100\n",
					__func__, i);
			debug_stall(i);
#endif /* USE_DEBUG_LOG */
			pstall_all->ratio[i] = 100;
			/* continue; */
		}

		pstall_all->cpu |= (1 << i);
		pstall_all->cpu_count[clustor]++;
		pstall_all->ratio_max[clustor] =
			CM_MGR_MAX(pstall_all->ratio[i],
					pstall_all->ratio_max[clustor]);
#ifdef USE_DEBUG_LOG
		debug_stall(i);
#endif /* USE_DEBUG_LOG */
	}

#ifdef USE_DEBUG_LOG
	debug_stall_all();
#endif /* USE_DEBUG_LOG */
	return 0;
}

static void init_cpu_stall_counter(int cluster)
{
	unsigned int val;

	if (!timekeeping_suspended) {
		cm_mgr_init_time = ktime_get();
		cm_mgr_init_flag = 1;
	}

	if (cluster == 0) {
		val = 0x11000;
		cm_mgr_write(MP0_CPU_STALL_INFO, val);

		/* please check CM_MGR_INIT_DELAY_MS value */
		val = RG_FMETER_EN;
		val |= RG_MP0_AVG_STALL_PERIOD_1MS;
		val |= RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU0_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU1_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU2_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(MP0_CPU3_AVG_STALL_RATIO_CTRL, val);
	} else {
		val = CPUTOP_NON_WFX_COUNTER_EN;
		cm_mgr_write(CPUTOP_NON_WFX_COUNTER_CTRL, val);

		/* please check CM_MGR_INIT_DELAY_MS value */
		val = RG_FMETER_EN;
		val |= RG_MP0_AVG_STALL_PERIOD_1MS;
		val |= RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(CPU0_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(CPU1_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(CPU2_AVG_STALL_RATIO_CTRL, val);

		val = RG_CPU0_AVG_STALL_RATIO_EN |
			RG_CPU0_STALL_COUNTER_EN |
			RG_CPU0_NON_WFX_COUNTER_EN;
		cm_mgr_write(CPU3_AVG_STALL_RATIO_CTRL, val);
	}
}

static int cm_mgr_cpuhp_online(unsigned int cpu)
{
#ifdef CONFIG_MTK_DRAMC
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&sw_zq_tx_lock, spinlock_save_flags);
#endif /* CONFIG_MTK_DRAMC */

	if (((cm_mgr_idle_mask & CLUSTER0_MASK) == 0x0) &&
			(cpu < CM_MGR_CPU_LIMIT))
		init_cpu_stall_counter(0);
	else if (((cm_mgr_idle_mask & CLUSTER1_MASK) == 0x0) &&
			(cpu >= CM_MGR_CPU_LIMIT))
		init_cpu_stall_counter(1);
	cm_mgr_idle_mask |= (1 << cpu);

#ifdef CONFIG_MTK_DRAMC
	spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
#endif /* CONFIG_MTK_DRAMC */

	return 0;
}

static int cm_mgr_cpuhp_offline(unsigned int cpu)
{
#ifdef CONFIG_MTK_DRAMC
	unsigned long spinlock_save_flags;

	spin_lock_irqsave(&sw_zq_tx_lock, spinlock_save_flags);
#endif /* CONFIG_MTK_DRAMC */

	cm_mgr_idle_mask &= ~(1 << cpu);

#ifdef CONFIG_MTK_DRAMC
	spin_unlock_irqrestore(&sw_zq_tx_lock, spinlock_save_flags);
#endif /* CONFIG_MTK_DRAMC */

	return 0;
}

#ifdef CONFIG_CPU_PM
static int cm_mgr_sched_pm_notifier(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	unsigned int cpu = smp_processor_id();

	if (cmd == CPU_PM_EXIT)
		cm_mgr_cpuhp_online(cpu);
	else if (cmd == CPU_PM_ENTER)
		cm_mgr_cpuhp_offline(cpu);

	return NOTIFY_OK;
}

static struct notifier_block cm_mgr_sched_pm_notifier_block = {
	.notifier_call = cm_mgr_sched_pm_notifier,
};

static void cm_mgr_sched_pm_init(void)
{
	cpu_pm_register_notifier(&cm_mgr_sched_pm_notifier_block);
}

#else
static inline void cm_mgr_sched_pm_init(void) { }
#endif /* CONFIG_CPU_PM */

static int cm_mgr_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		pr_info("#@# %s(%d) SCREEN ON\n", __func__, __LINE__);
		cm_mgr_blank_status = 0;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 0);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		break;
	case FB_BLANK_POWERDOWN:
		pr_info("#@# %s(%d) SCREEN OFF\n", __func__, __LINE__);
		cm_mgr_blank_status = 1;
		cm_mgr_set_dram_level(-1);
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block cm_mgr_fb_notifier = {
	.notifier_call = cm_mgr_fb_notifier_callback,
};

#ifdef USE_IDLE_NOTIFY
static int cm_mgr_idle_cb(struct notifier_block *nfb,
				  unsigned long id,
				  void *arg)
{
	switch (id) {
	case NOTIFY_SOIDLE_ENTER:
	case NOTIFY_SOIDLE3_ENTER:
		if (get_cur_ddr_opp() != CM_MGR_EMI_OPP)
			check_cm_mgr_status_internal();
		break;
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cm_mgr_idle_notify = {
	.notifier_call = cm_mgr_idle_cb,
};
#endif /* USE_IDLE_NOTIFY */

struct timer_list cm_mgr_ratio_timer;
#define CM_MGR_RATIO_TIMER_MS	msecs_to_jiffies(1)

static void cm_mgr_ratio_timer_fn(unsigned long data)
{
	trace_CM_MGR__stall_ratio_0(
			(unsigned int)cm_mgr_read(MP0_CPU_AVG_STALL_RATIO));
	trace_CM_MGR__stall_ratio_1(
			(unsigned int)cm_mgr_read(CPU_AVG_STALL_RATIO));

	cm_mgr_ratio_timer.expires = jiffies + CM_MGR_RATIO_TIMER_MS;
	add_timer(&cm_mgr_ratio_timer);
}

void cm_mgr_ratio_timer_en(int enable)
{
	if (enable) {
		cm_mgr_ratio_timer.expires = jiffies + CM_MGR_RATIO_TIMER_MS;
		add_timer(&cm_mgr_ratio_timer);
	} else {
		del_timer(&cm_mgr_ratio_timer);
	}
}

static struct pm_qos_request ddr_opp_req;
static int debounce_times_perf_down_local;
static int pm_qos_update_request_status;
void cm_mgr_perf_platform_set_status(int enable)
{
	if (enable) {
		debounce_times_perf_down_local = 0;

		if (cm_mgr_perf_enable == 0)
			return;

		if (cm_mgr_idx == CM_MGR_LP4X_2CH_3600) {
			cpu_power_ratio_up[0] = 500;
			cpu_power_ratio_up[1] = 500;
			debounce_times_up_adb[1] = 0;
		} else if (cm_mgr_idx == CM_MGR_LP4X_2CH_3200) {
			cpu_power_ratio_up[0] = 500;
			cpu_power_ratio_up[1] = 500;
			debounce_times_up_adb[1] = 0;
		} else if (cm_mgr_idx == CM_MGR_LP3_1CH_1866) {
			cpu_power_ratio_up[0] = 500;
			cpu_power_ratio_up[1] = 500;
			debounce_times_up_adb[1] = 0;
		}

	} else {
		if (++debounce_times_perf_down_local < debounce_times_perf_down)
			return;

		if (cm_mgr_idx == CM_MGR_LP4X_2CH_3600) {
			cpu_power_ratio_up[0] = 100;
			cpu_power_ratio_up[1] = 100;
			debounce_times_up_adb[1] = 3;
		} else if (cm_mgr_idx == CM_MGR_LP4X_2CH_3200) {
			cpu_power_ratio_up[0] = 100;
			cpu_power_ratio_up[1] = 100;
			debounce_times_up_adb[1] = 3;
		} else if (cm_mgr_idx == CM_MGR_LP3_1CH_1866) {
			cpu_power_ratio_up[0] = 100;
			cpu_power_ratio_up[1] = 100;
			debounce_times_up_adb[1] = 3;
		}

		debounce_times_perf_down_local = 0;
	}
}

void cm_mgr_perf_platform_set_force_status(int enable)
{
	if (enable) {
		debounce_times_perf_down_local = 0;

		if (cm_mgr_perf_enable == 0)
			return;

		if ((cm_mgr_perf_force_enable == 0) ||
				(pm_qos_update_request_status == 1))
			return;

		if (cm_mgr_idx == CM_MGR_LP4X_2CH_3600)
			pm_qos_update_request(&ddr_opp_req, 0);
		else if (cm_mgr_idx == CM_MGR_LP4X_2CH_3200)
			pm_qos_update_request(&ddr_opp_req, 0);
		else if (cm_mgr_idx == CM_MGR_LP3_1CH_1866)
			pm_qos_update_request(&ddr_opp_req, 0);

		pm_qos_update_request_status = enable;
	} else {
		if (pm_qos_update_request_status == 0)
			return;

		if ((cm_mgr_perf_force_enable == 0) ||
				(++debounce_times_perf_down_local >=
				 debounce_times_perf_force_down)) {

			if (cm_mgr_idx == CM_MGR_LP4X_2CH_3600) {
				pm_qos_update_request(&ddr_opp_req,
						PM_QOS_EMI_OPP_DEFAULT_VALUE);
			} else if (cm_mgr_idx == CM_MGR_LP4X_2CH_3200) {
				pm_qos_update_request(&ddr_opp_req,
						PM_QOS_EMI_OPP_DEFAULT_VALUE);
			} else if (cm_mgr_idx == CM_MGR_LP3_1CH_1866) {
				pm_qos_update_request(&ddr_opp_req,
						PM_QOS_EMI_OPP_DEFAULT_VALUE);
			}

			pm_qos_update_request_status = enable;
			debounce_times_perf_down_local = 0;
		}
	}
}

int cm_mgr_register_init(void)
{
	struct device_node *node;
	int ret;
	const char *buf;

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,mcucfg_mp0_counter");
	if (!node)
		pr_info("find mcucfg_mp0_counter node failed\n");
	mcucfg_mp0_counter_base = of_iomap(node, 0);
	if (!mcucfg_mp0_counter_base) {
		pr_info("base mcucfg_mp0_counter_base failed\n");
		return -1;
	}

	if (node) {
		ret = of_property_read_string(node,
				"status", (const char **)&buf);

		if (ret == 0) {
			if (!strcmp(buf, "enable"))
				cm_mgr_enable = 1;
			else
				cm_mgr_enable = 0;
		}
	}

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,mcucfg_mp2_counter");
	if (!node)
		pr_info("find mcucfg_mp2_counter node failed\n");
	mcucfg_mp2_counter_base = of_iomap(node, 0);
	if (!mcucfg_mp2_counter_base) {
		pr_info("base mcucfg_mp2_counter_base failed\n");
		return -1;
	}

	return 0;
}

int cm_mgr_platform_init(void)
{
	int r;

	spin_lock_init(&cm_mgr_cpu_mask_lock);

	r = cm_mgr_register_init();
	if (r) {
		pr_info("FAILED TO CREATE REGISTER(%d)\n", r);
		return r;
	}

	cm_mgr_sched_pm_init();

	r = fb_register_client(&cm_mgr_fb_notifier);
	if (r) {
		pr_info("FAILED TO REGISTER FB CLIENT (%d)\n", r);
		return r;
	}

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"cm_mgr:online",
			cm_mgr_cpuhp_online,
			cm_mgr_cpuhp_offline);

#ifdef USE_IDLE_NOTIFY
	mtk_idle_notifier_register(&cm_mgr_idle_notify);
#endif /* USE_IDLE_NOTIFY */

	init_timer_deferrable(&cm_mgr_ratio_timer);
	cm_mgr_ratio_timer.function = cm_mgr_ratio_timer_fn;
	cm_mgr_ratio_timer.data = 0;

#ifdef CONFIG_MTK_CPU_FREQ
	mt_cpufreq_set_governor_freq_registerCB(check_cm_mgr_status);
#endif /* CONFIG_MTK_CPU_FREQ */

	pm_qos_add_request(&ddr_opp_req, PM_QOS_EMI_OPP,
			PM_QOS_EMI_OPP_DEFAULT_VALUE);

	return r;
}

void cm_mgr_set_dram_level(int level)
{
	int dram_level;

	if ((cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1) || (level < 0))
		dram_level = 0;
	else
		dram_level = level;
	dvfsrc_set_power_model_ddr_request(dram_level);
}

int cm_mgr_get_dram_opp(void)
{
	int dram_opp_cur;

	dram_opp_cur = get_cur_ddr_opp();
	if (dram_opp_cur < 0 || dram_opp_cur > CM_MGR_EMI_OPP)
		dram_opp_cur = 0;

	return dram_opp_cur;
}

int cm_mgr_check_bw_status(void)
{
	if (cm_mgr_get_bw() > CM_MGR_BW_VALUE)
		return 1;
	else
		return 0;
}

int cm_mgr_get_bw(void)
{
	return dvfsrc_get_bw(QOS_TOTAL);
}
