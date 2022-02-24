// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <mtk_lpm.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>
#include <mtk_dbg_common_v1.h>
#include <mt-plat/mtk_ccci_common.h>

#include "mt6853.h"
#include "mt6853_suspend.h"

unsigned int mt6853_suspend_status;
u64 before_md_sleep_time;
u64 after_md_sleep_time;
struct cpumask s2idle_cpumask;


void __attribute__((weak)) subsys_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) pll_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) gpio_dump_regs(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}

void mtk_suspend_gpio_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	gpio_dump_regs();
#endif
}
EXPORT_SYMBOL(mtk_suspend_gpio_dbg);

void mtk_suspend_clk_dbg(void){}
EXPORT_SYMBOL(mtk_suspend_clk_dbg);

#define MD_SLEEP_INFO_SMEM_OFFEST (4)
static u64 get_md_sleep_time(void)
{
	/* dump subsystem sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	u32 *share_mem = NULL;
	struct md_sleep_status md_data;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_LOW_POWER, NULL);
	if (share_mem == NULL) {
		printk_deferred("[name:spm&][%s:%d] - No MD share mem\n",
			 __func__, __LINE__);
		return 0;
	}
	share_mem = share_mem + MD_SLEEP_INFO_SMEM_OFFEST;
	memset(&md_data, 0, sizeof(struct md_sleep_status));
	memcpy(&md_data, share_mem, sizeof(struct md_sleep_status));

	return md_data.sleep_time;
#else
	return 0;
#endif
}

static inline int mt6853_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int mt6853_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

static int __mt6853_suspend_prompt(int type, int cpu,
				   const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

	mt6853_suspend_status = 0;

	printk_deferred("[name:spm&][%s:%d] - prepare suspend enter\n",
			__func__, __LINE__);

	ret = mt6853_suspend_common_enter(&mt6853_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6853_do_mcusys_prepare_pdn(mt6853_suspend_status, &spm_res);

	printk_deferred("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);

	/* Record md sleep time */
	before_md_sleep_time = get_md_sleep_time();


PLAT_LEAVE_SUSPEND:
	return ret;
}

static void __mt6853_suspend_reflect(int type, int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	printk_deferred("[name:spm&][%s:%d] - prepare suspend resume\n",
			__func__, __LINE__);

	mt6853_suspend_common_resume(mt6853_suspend_status);
	mt6853_do_mcusys_prepare_on();

	printk_deferred("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

	if (issuer)
		issuer->log(MT_LPM_ISSUER_SUSPEND, "suspend", NULL);

	/* show md sleep duration during AP suspend */
	after_md_sleep_time = get_md_sleep_time();
	if (after_md_sleep_time >= before_md_sleep_time)
		printk_deferred("[name:spm&][SPM] md_slp_duration = %llu",
			after_md_sleep_time - before_md_sleep_time);
}
int mt6853_suspend_system_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	return __mt6853_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
				       cpu, issuer);
}

void mt6853_suspend_system_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	return __mt6853_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					cpu, issuer);
}

int mt6853_suspend_s2idle_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;

	cpumask_set_cpu(cpu, &s2idle_cpumask);
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
#ifdef CONFIG_PM_SLEEP
		/* Notice
		 * Fix the rcu_idle workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore. But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NOIDLE maybe the right solution.
		 */
		RCU_NONIDLE(syscore_suspend());
#endif
		ret = __mt6853_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
	}
	return ret;
}

void mt6853_suspend_s2idle_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
		__mt6853_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
#ifdef CONFIG_PM_SLEEP
		/* Notice
		 * Fix the rcu_idle/timekeeping workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore.
		 */
		RCU_NONIDLE(syscore_resume());
		RCU_NONIDLE(pm_system_wakeup());
#endif
	}
	cpumask_clear_cpu(cpu, &s2idle_cpumask);
}

#define MT6853_SUSPEND_OP_INIT(_prompt, _enter, _resume, _reflect) ({\
	mt6853_model_suspend.op.prompt = _prompt;\
	mt6853_model_suspend.op.prepare_enter = _enter;\
	mt6853_model_suspend.op.prepare_resume = _resume;\
	mt6853_model_suspend.op.reflect = _reflect; })



struct mtk_lpm_model mt6853_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6853_suspend_system_prompt,
		.reflect = mt6853_suspend_system_reflect,
	}
};

#ifdef CONFIG_PM
static int mt6853_spm_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		printk_deferred(
		"[name:spm&][SPM] PM: suspend entry %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		printk_deferred(
		"[name:spm&][SPM] PM: suspend exit %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mt6853_spm_suspend_pm_notifier_func = {
	.notifier_call = mt6853_spm_suspend_pm_event,
	.priority = 0,
};
#endif

int __init mt6853_model_suspend_init(void)
{
	int ret;

	int suspend_type = mtk_lpm_suspend_type_get();

	if (suspend_type == MTK_LPM_SUSPEND_S2IDLE) {
		MT6853_SUSPEND_OP_INIT(mt6853_suspend_s2idle_prompt,
					NULL,
					NULL,
					mt6853_suspend_s2idle_reflect);
		mtk_lpm_suspend_registry("s2idle", &mt6853_model_suspend);
	} else {
		MT6853_SUSPEND_OP_INIT(mt6853_suspend_system_prompt,
					NULL,
					NULL,
					mt6853_suspend_system_reflect);
		mtk_lpm_suspend_registry("suspend", &mt6853_model_suspend);
	}

	cpumask_clear(&s2idle_cpumask);


#ifdef CONFIG_PM
	ret = register_pm_notifier(&mt6853_spm_suspend_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_SLEEP_DEBUG
	pm_print_times_enabled = false;
#endif
	return 0;
}
