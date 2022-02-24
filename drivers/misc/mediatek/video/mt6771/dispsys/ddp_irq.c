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


#define LOG_TAG "IRQ"

#include "ddp_log.h"
#include "ddp_debug.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/sched/clock.h>

#if defined(CONFIG_MTK_SMI_EXT)
#include <smi_public.h>
#endif

/* #include <mach/mt_irq.h> */
#include "ddp_reg.h"
#include "ddp_irq.h"
#include "ddp_aal.h"
#include "ddp_drv.h"
#include "disp_helper.h"
#include "ddp_dsi.h"
#include "ddp_dbi.h"
#include "disp_drv_log.h"
#include "ddp_manager.h"
#include "primary_display.h"

/* IRQ log print kthread */
static struct task_struct *disp_irq_log_task;
static wait_queue_head_t disp_irq_log_wq;
static int disp_irq_log_module;

static int irq_init;

static unsigned int cnt_rdma_underflow[2];
static unsigned int cnt_rdma_abnormal[2];
static unsigned int cnt_ovl_underflow[OVL_NUM];
static unsigned int cnt_wdma_underflow[2];

unsigned long long rdma_start_time[2] = { 0 };
unsigned long long rdma_end_time[2] = { 0 };

#define DISP_MAX_IRQ_CALLBACK   10

static DDP_IRQ_CALLBACK
	irq_module_callback_table[DISP_MODULE_NUM][DISP_MAX_IRQ_CALLBACK];

static DDP_IRQ_CALLBACK irq_callback_table[DISP_MAX_IRQ_CALLBACK];

atomic_t ESDCheck_byCPU = ATOMIC_INIT(0);

/* dsi read by cpu should keep esd_check_bycmdq = 0.  */
/* dsi read by cmdq should keep esd_check_bycmdq = 1. */
static atomic_t esd_check_bycmdq = ATOMIC_INIT(1);

#define RDMA_UNDERFLOW_CNT_AEE 5
static long long rdma_underflow_ts[RDMA_UNDERFLOW_CNT_AEE];

unsigned int display_ut_status;

void disp_irq_esd_cust_bycmdq(int enable)
{
	atomic_set(&esd_check_bycmdq, enable);
}

int disp_irq_esd_cust_get(void)
{
	return atomic_read(&esd_check_bycmdq);
}

int disp_register_irq_callback(DDP_IRQ_CALLBACK cb)
{
	int i = 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == cb)
			break;
	}
	if (i < DISP_MAX_IRQ_CALLBACK)
		return 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == NULL)
			break;
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPPR_ERR("not enough irq callback entries for module\n");
		return -1;
	}
	DDPMSG("register callback on %d\n", i);
	irq_callback_table[i] = cb;
	return 0;
}

int disp_unregister_irq_callback(DDP_IRQ_CALLBACK cb)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i] == cb) {
			irq_callback_table[i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPPR_ERR("Try to unregister callback function %p which was not registered\n",
		       cb);
		return -1;
	}
	return 0;
}

int disp_register_module_irq_callback(enum DISP_MODULE_ENUM module,
				      DDP_IRQ_CALLBACK cb)
{
	int i;

	if (module >= DISP_MODULE_NUM) {
		DDPERR("Register IRQ with invalid module ID. module=%d\n",
		       module);
		return -1;
	}
	if (cb == NULL) {
		DDPPR_ERR("Register IRQ with invalid cb.\n");
		return -1;
	}
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == cb)
			break;
	}
	if (i < DISP_MAX_IRQ_CALLBACK)
		return 0;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == NULL)
			break;
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPPR_ERR("No enough callback entries for module %d.\n",
				module);
		return -1;
	}

	irq_module_callback_table[module][i] = cb;
	return 0;
}

int disp_unregister_module_irq_callback(enum DISP_MODULE_ENUM module,
					DDP_IRQ_CALLBACK cb)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_module_callback_table[module][i] == cb) {
			irq_module_callback_table[module][i] = NULL;
			break;
		}
	}
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPPR_ERR("try to unregister callback function but not registered. module=%d cb=%p\n",
		       module, cb);
		return -1;
	}
	return 0;
}

void disp_invoke_irq_callbacks(enum DISP_MODULE_ENUM module, unsigned int param)
{
	int i;

	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
		if (irq_callback_table[i])
			irq_callback_table[i](module, param);

		if (irq_module_callback_table[module][i])
			irq_module_callback_table[module][i](module, param);
	}
}

/* TODO: move each irq to module driver */
unsigned int rdma_start_irq_cnt[2] = { 0, 0 };
unsigned int rdma_done_irq_cnt[2] = { 0, 0 };
unsigned int rdma_underflow_irq_cnt[2] = { 0, 0 };
unsigned int rdma_targetline_irq_cnt[2] = { 0, 0 };

irqreturn_t disp_irq_handler(int irq, void *dev_id)
{
	enum DISP_MODULE_ENUM module = DISP_MODULE_UNKNOWN;
	unsigned int reg_val = 0;
	unsigned int index = 0;
	unsigned int reg_temp_val = 0;

	if (irq == ddp_get_module_irq(DISP_MODULE_DSI0)) {
		if (ddp_get_module_irq(DISP_MODULE_DSI0) == irq) {
			index = 0;
			module = DISP_MODULE_DSI0;
		} else if (ddp_get_module_irq(DISP_MODULE_DSI1) == irq) {
			index = 1;
			module = DISP_MODULE_DSI1;
		}

		if (module == DISP_MODULE_DSI0)
			reg_val = (DISP_REG_GET(DISPSYS_DSI0_BASE + 0xC) &
				   0xffff);
		else
			reg_val = (DISP_REG_GET(DISPSYS_DSI1_BASE + 0xC) &
				   0xffff);

		reg_temp_val = reg_val;
		/*
		 * rd_rdy don't clear and wait for ESD &
		 * Read LCM will clear the bit.
		 */
		if (disp_irq_esd_cust_get() == 1)
			reg_temp_val = reg_val & 0xfffe;
		if (module == DISP_MODULE_DSI0)
			DISP_CPU_REG_SET(DISPSYS_DSI0_BASE + 0xC,
					 ~reg_temp_val);
		else
			DISP_CPU_REG_SET(DISPSYS_DSI1_BASE + 0xC,
					 ~reg_temp_val);

		DDPIRQ("%s irq_status = 0x%x\n",
				ddp_get_module_name(module), reg_val);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_OVL0) ||
		   irq == ddp_get_module_irq(DISP_MODULE_OVL0_2L) ||
		   irq == ddp_get_module_irq(DISP_MODULE_OVL1_2L)) {
		module = disp_irq_to_module(irq);
		index = ovl_to_index(module);
		reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA +
				       ovl_base_addr(module));
		DISP_CPU_REG_SET(DISP_REG_OVL_INTSTA + ovl_base_addr(module),
				~reg_val);

		DDPIRQ("%s irq_status = 0x%x\n",
		       ddp_get_module_name(module), reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: %s reg commit!\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 1))
			DDPIRQ("IRQ: %s frame done!\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 2)) {
			DDPPR_ERR("IRQ: %s frame underflow! cnt=%d\n",
			       ddp_get_module_name(module),
			       cnt_ovl_underflow[index]);

			cnt_ovl_underflow[index]++;
			set_display_ut_status(DISP_UT_ERROR_OVL);
		}

		if (reg_val & (1 << 3))
			DDPIRQ("IRQ: %s sw reset done\n",
			       ddp_get_module_name(module));

		if (reg_val & (1 << 4))
			DDPPR_ERR("IRQ: %s hw reset done\n",
			       ddp_get_module_name(module));

		mmprofile_log_ex(ddp_mmp_get_events()->OVL_IRQ[index],
				 MMPROFILE_FLAG_PULSE, reg_val, 0);
		if (reg_val & 0x1e0)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
					 MMPROFILE_FLAG_PULSE,
					 (index << 16) | reg_val, module);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_WDMA0)) {
		static unsigned char wdma_diagnosed;

		index = 0;
		module = DISP_MODULE_WDMA0;

		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA);
		/* clear intr */
		DISP_CPU_REG_SET(DISP_REG_WDMA_INTSTA, ~reg_val);

		DDPIRQ("%s irq_status = 0x%x\n", ddp_get_module_name(module),
		       reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: WDMA%d frame done!\n", index);

		if (reg_val & (1 << 1)) {
			DDPPR_ERR("IRQ: WDMA%d underrun! cnt=%d\n",
			       index, cnt_wdma_underflow[index]);
			cnt_wdma_underflow[index]++;
			if (!wdma_diagnosed) {
#if defined(CONFIG_MTK_SMI_EXT)
				smi_debug_bus_hang_detect(false, "DISP");
#endif
				wdma_diagnosed = 1;
			}
			disp_irq_log_module |= 1 << module;
			set_display_ut_status(DISP_UT_ERROR_WDMA);
			primary_display_set_recovery_module(DISP_MODULE_WDMA0);
			/* The DISP_RECOVERY event waiting by recovery thread
			 * is bundled with main display path.
			 * So we fill the main display path module:
			 * MODULE_RDMA0 here to signal the awaking
			 * event in recovery thread.
			 */
			dpmgr_module_notify(DISP_MODULE_RDMA0,
					DISP_PATH_EVENT_DISP_RECOVERY);
		}
		mmprofile_log_ex(ddp_mmp_get_events()->WDMA_IRQ[index],
				 MMPROFILE_FLAG_PULSE, reg_val,
				 DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
		if (reg_val & 0x2)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
					 MMPROFILE_FLAG_PULSE,
					 (cnt_wdma_underflow[index] << 24) |
					 (index << 16) | reg_val, module);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_RDMA0) ||
		   irq == ddp_get_module_irq(DISP_MODULE_RDMA1)) {
		if (ddp_get_module_irq(DISP_MODULE_RDMA0) == irq) {
			index = 0;
			module = DISP_MODULE_RDMA0;
		} else if (ddp_get_module_irq(DISP_MODULE_RDMA1) == irq) {
			index = 1;
			module = DISP_MODULE_RDMA1;
		}

		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS +
				       index * DISP_RDMA_INDEX_OFFSET);
		/* clear intr */
		DISP_CPU_REG_SET(DISP_REG_RDMA_INT_STATUS +
				index * DISP_RDMA_INDEX_OFFSET, ~reg_val);

		DDPIRQ("%s irq_status = 0x%x\n", ddp_get_module_name(module),
		       reg_val);

		if (reg_val & (1 << 0))
			DDPIRQ("IRQ: RDMA%d reg update done!\n", index);

		if (reg_val & (1 << 2)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_END, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));

			rdma_end_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame done!\n", index);
			rdma_done_irq_cnt[index]++;
		}
		if (reg_val & (1 << 1)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_START, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));

			rdma_start_time[index] = sched_clock();
			DDPIRQ("IRQ: RDMA%d frame start!\n", index);
			rdma_start_irq_cnt[index]++;
		}
		if (reg_val & (1 << 3)) {
			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_PULSE, reg_val,
				DISP_REG_GET(DISPSYS_RDMA0_BASE + 0x4));

			DDPPR_ERR("IRQ: RDMA%d abnormal! cnt=%d\n",
				index, cnt_rdma_abnormal[index]);
			cnt_rdma_abnormal[index]++;
			disp_irq_log_module |= 1 << module;
		}
		if (reg_val & (1 << 4)) {
			unsigned int in_p_cnt, in_l_cnt, out_p_cnt, out_l_cnt;

			in_p_cnt = DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index);
			in_l_cnt = DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index);
			out_p_cnt = DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index);
			out_l_cnt = DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index);

			mmprofile_log_ex(
				ddp_mmp_get_events()->SCREEN_UPDATE[index],
				MMPROFILE_FLAG_PULSE, reg_val, 1);
#if 0
			DDPMSG("rdma%d, pix(%d,%d,%d,%d)\n",
			       index,
			       DISP_REG_GET(DISP_REG_RDMA_IN_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_IN_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_OUT_P_CNT +
					    DISP_RDMA_INDEX_OFFSET * index),
			       DISP_REG_GET(DISP_REG_RDMA_OUT_LINE_CNT +
					    DISP_RDMA_INDEX_OFFSET * index));
#endif
			cnt_rdma_underflow[index]++;
			DDPPR_ERR("IRQ: RDMA%d underflow! cnt=%d, pix(%d,%d,%d,%d)\n",
				index, cnt_rdma_underflow[index],
				in_p_cnt, in_l_cnt, out_p_cnt, out_l_cnt);
			if (disp_helper_get_option(
						DISP_OPT_RDMA_UNDERFLOW_AEE)) {
				static int offset;
				static long long db_t;
				int offset_n =
					(offset + 1) % RDMA_UNDERFLOW_CNT_AEE;

				/* trigger aee 5 RDMA underflow happen in 1s */
				/* diff current and the newest timestamp */

				rdma_underflow_ts[offset] = sched_clock();
				if (rdma_underflow_ts[offset] -
				rdma_underflow_ts[offset_n] < 1000000000 &&
				rdma_underflow_ts[offset] - db_t > 1000000000) {
					DDPAEE("RDMA%d underflow!cnt=%d\n",
					index, cnt_rdma_underflow[index]);
					db_t = rdma_underflow_ts[offset];
				}
				offset = offset_n;
			}
			disp_irq_log_module |= 1 << module;
			rdma_underflow_irq_cnt[index]++;
			set_display_ut_status(DISP_UT_ERROR_RDMA);
		}
		if (reg_val & (1 << 5)) {
			DDPIRQ("IRQ: RDMA%d target line!\n", index);
			rdma_targetline_irq_cnt[index]++;
		}
		mmprofile_log_ex(ddp_mmp_get_events()->RDMA_IRQ[index],
				 MMPROFILE_FLAG_PULSE, reg_val, 0);
		if (reg_val & 0x18)
			mmprofile_log_ex(ddp_mmp_get_events()->ddp_abnormal_irq,
					 MMPROFILE_FLAG_PULSE,
					 (rdma_underflow_irq_cnt[index] << 24) |
					 (index << 16) | reg_val, module);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_MUTEX)) {
		/* mutex0: primary disp */
		/* mutex1: sub disp */
		/* mutex2: aal */
		unsigned int m_id = 0;
		unsigned int reg_val1;

		module = DISP_MODULE_MUTEX;
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) &
					DISP_MUTEX_INT_MSK;
		reg_val1 = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA_1);
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA_1, ~reg_val1);

		DDPIRQ("%s, irq_status = 0x%x, irq_status1 = 0x%x\n",
			ddp_get_module_name(module), reg_val, reg_val1);

		for (m_id = 0; m_id < DISP_MUTEX_DDP_COUNT; m_id++) {
			if (reg_val & (0x1 << m_id)) {
				DDPIRQ("IRQ: mutex%d sof!\n", m_id);
				mmprofile_log_ex(
					ddp_mmp_get_events()->MUTEX_IRQ[m_id],
					MMPROFILE_FLAG_PULSE, reg_val, 0);
			}
			if (reg_val & (0x1 << (m_id + DISP_MUTEX_TOTAL))) {
				DDPIRQ("IRQ: mutex%d eof!\n", m_id);
				mmprofile_log_ex(
					ddp_mmp_get_events()->MUTEX_IRQ[m_id],
					MMPROFILE_FLAG_PULSE, reg_val, 1);
			}
		}
	} else if (irq == ddp_get_module_irq(DISP_MODULE_AAL0)) {
		module = DISP_MODULE_AAL0;
		reg_val = DISP_REG_GET(DISP_AAL_INTSTA);
		disp_aal_on_end_of_frame();
	} else if (irq == ddp_get_module_irq(DISP_MODULE_CCORR0)) {
		module = DISP_MODULE_CCORR0;
		reg_val = DISP_REG_GET(DISP_REG_CCORR_INTSTA);
		disp_ccorr_on_end_of_frame();
	} else if (irq == ddp_get_module_irq(DISP_MODULE_CONFIG)) {
		/* MMSYS error intr */
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MMSYS_INTSTA) & 0x7;
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MMSYS_INTSTA, ~reg_val);
		if (reg_val & (1 << 0))
			DDPPR_ERR("MMSYS to MFG APB TX Error, MMSYS clock off but MFG clock on!\n");

		if (reg_val & (1 << 1))
			DDPPR_ERR("MMSYS to MJC APB TX Error, MMSYS clock off but MJC clock on!\n");

		if (reg_val & (1 << 2))
			DDPPR_ERR("PWM APB TX Error!\n");
	} else if (irq == ddp_get_module_irq(DISP_MODULE_DPI)) {
		module = DISP_MODULE_DPI;
		reg_val = DISP_REG_GET(DISP_REG_DPI_INSTA) & 0x7;
		DISP_CPU_REG_SET(DISP_REG_DPI_INSTA, ~reg_val);
	} else if (irq == ddp_get_module_irq(DISP_MODULE_DBI)) {
		module = DISP_MODULE_DBI;
		reg_val = (DISP_REG_GET(DISPSYS_DBI_BASE + 0x8) & 0x3f);
		DISP_CPU_REG_SET(DISPSYS_DBI_BASE + 0x8, ~reg_val);
		DDPIRQ("%s irq_status = 0x%x\n",
			ddp_get_module_name(module), reg_val);
	} else {
		module = DISP_MODULE_UNKNOWN;
		reg_val = 0;
		DDPPR_ERR("invalid irq=%d\n ", irq);
	}

	disp_invoke_irq_callbacks(module, reg_val);
	if (disp_irq_log_module != 0)
		wake_up_interruptible(&disp_irq_log_wq);

	mmprofile_log_ex(ddp_mmp_get_events()->DDP_IRQ, MMPROFILE_FLAG_END,
			 irq, reg_val);
	return IRQ_HANDLED;
}

static int disp_irq_log_kthread_func(void *data)
{
	unsigned int i = 0;

	while (1) {
		wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
		DDPMSG("%s: dump intr register: disp_irq_log_module=%d\n",
		       __func__, disp_irq_log_module);

		for (i = 0; i < DISP_MODULE_NUM; i++) {
			if (disp_irq_log_module & (1 << i))
				ddp_dump_reg(i);
		}
		disp_irq_log_module = 0;
	}
	return 0;
}

int disp_init_irq(void)
{
	if (irq_init)
		return 0;

	irq_init = 1;
	DDPMSG("%s\n", __func__);

	/* create irq log thread */
	init_waitqueue_head(&disp_irq_log_wq);
	disp_irq_log_task = kthread_create(disp_irq_log_kthread_func,
					   NULL, "ddp_irq_log_kthread");
	if (IS_ERR(disp_irq_log_task))
		DDPPR_ERR(" can not create disp_irq_log_task kthread\n");

	/* wake_up_process(disp_irq_log_task); */
	return 0;
}
unsigned int get_display_ut_status(void)
{
	return display_ut_status;
}

void set_display_ut_status(enum DISP_UT_ERROR err)
{
	display_ut_status |= err;
}

void reset_display_ut_status(void)
{
	display_ut_status &= 0xFFFF0000;
}

void disable_display_ut(int id)
{
	display_ut_status |= 1 << (id + 16);
}

void enable_display_ut(int id)
{
	if (display_ut_status & (1 << (id + 16)))
		display_ut_status -= (1 << (id + 16));
}
