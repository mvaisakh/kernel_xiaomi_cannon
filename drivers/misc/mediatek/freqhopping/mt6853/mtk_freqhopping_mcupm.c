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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/sched_clock.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include "mtk_freqhopping.h"
#include "mtk_fhreg.h"
#include "sync_write.h"
#include "mtk_freqhopping_drv.h"
#ifdef HP_EN_REG_SEMAPHORE_PROTECT
#include "mtk_cpufreq_hybrid.h"
#endif
#include <mt-plat/mtk_tinysys_ipi.h>
#include <mt-plat/aee.h>
#include "mcupm_ipi_id.h"
#include <linux/seq_file.h>
#include <linux/of_address.h>

#define MASK22b (0x3FFFFF)

unsigned int ack_data;
/***********************************/
/* Other global variable           */
/***********************************/
static unsigned int g_initialize;	/* [True]: Init done */
static DEFINE_SPINLOCK(g_fh_lock);

/*********************************/
/* FHCTL related IP base address */
/*********************************/

static void __iomem *g_fhctl_base;
static void __iomem *g_apmixed_base;
static void __iomem *g_reg_tr;

/*********************************/
/* Utility Macro */
/*********************************/
#define VALIDATE_PLLID(id) WARN_ON(((id) < 0) \
			|| ((id) >= FH_PLL_NUM) \
			|| (g_reg_pll_con0[(id)] == REG_PLL_NOT_SUPPORT))

#define IS_PLLID_VALID(id) ((id) >= 0 \
				&& (id) < FH_PLL_NUM \
				&& g_reg_pll_con0[(id)] != REG_PLL_NOT_SUPPORT)

#define VALIDATE_DDS(dds)  WARN_ON(dds > MASK22b)
#define PERCENT_TO_DDSLMT(dDS, pERCENT_M10) (((dDS * pERCENT_M10) >> 5) / 100)

/*********************************/
/* FHCTL PLL Setting ID */
/*********************************/
#define PLL_SETTING_IDX__USER	(0x9)	/* Magic number*/
#define PLL_SETTING_IDX__DEF    (0x1)	/* Default Setting, Magic number*/


/*********************************/
/* Track the status of all FHCTL PLL */
/*********************************/
static struct fh_pll_t g_fh_pll[FH_PLL_NUM] = { };	/* fake_pll_status. */

/*********************************/
/* FHCTL PLL name                */
/*********************************/
static const char *g_pll_name[FH_PLL_NUM] = {
	"ARMPLL_LL",
	"ARMPLL_BL0",
	"ARMPLL_BL1",
	"ARMPLL_BL2",
	"NPUPLL", /*tell EM MEMPLL cannot do hopping*/
	"CCIPLL",
	"MFGPLL",
	"MEMPLL",
	"MPLL",
	"MMPLL",
	"MAINPLL",
	"MSDCPLL",
	"ADSPPLL",
	"APUPLL",
	"TVDPLL"
};

/*********************************/
/* FHCTL PLL SSC Setting Table   */
/*********************************/

/* Should be setting according to HQA de-sense result.  */
static int g_pll_ssc_init_tbl[FH_PLL_NUM] = {
/*
 *  [FH_SSC_DEF_DISABLE]: Default SSC disable,
 *  [FH_SSC_DEF_ENABLE_SSC]: Default enable SSC.
 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL0 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL1 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL2 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL3 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL4 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL5 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL6 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL7 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL8 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL9 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL10 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL11 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL12 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL13 */
	FH_SSC_DEF_DISABLE,	/* FHCTL PLL14 */

};

static const struct freqhopping_ssc g_pll_ssc_setting_tbl[FH_PLL_NUM][4] = {
	/* FH PLL0 ARMPLL_LL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL1 ARMPLL_BL0*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL2 ARMPLL_BL1*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL3 ARMPLL_BL2*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL4 NPUPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL5 CCIPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 1, UNINIT_DDS},	/* 0% ~ -1% */
	},

	/* FH PLL6 MFGPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL7 MEMPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL8 MPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},
	/* FH PLL9 MMPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL10 MAINPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},
	/* FH PLL11 MSDCPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL12 ADSPPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},
	/* FH PLL13 APUPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},

	/* FH PLL14 TVDPLL*/
	{
		{0, 0, 0, 0, 0, 0},
		{PLL_SETTING_IDX__DEF, 0, 9, 0, 0, UNINIT_DDS},	/* 0% ~ -0% */
	},
};


/***********************************/
/*FHCTL HP CON Register */
/***********************************/

/*fake userdefine setting*/	/* freq, dt, df, upbnd, lowbnd, dds */
static struct freqhopping_ssc mt_ssc_fhpll_userdefined[FH_PLL_NUM];


/*************************************/
/* FHCTL Register table              */
/* - Dynamic assign address based on */
/*   Device tree IP address          */
/*************************************/

static unsigned long g_reg_dds[FH_PLL_NUM];
static unsigned long g_reg_cfg[FH_PLL_NUM];
static unsigned long g_reg_updnlmt[FH_PLL_NUM];
static unsigned long g_reg_mon[FH_PLL_NUM];
static unsigned long g_reg_dvfs[FH_PLL_NUM];
static unsigned long g_reg_pll_con0[FH_PLL_NUM];
static unsigned long g_reg_pll_con1[FH_PLL_NUM];

/***********************************/
/* In memory log */
/***********************************/
struct fh_log_entry {
	unsigned long long	serial;
	unsigned int		target_dds;
	unsigned int		before_dds;
	unsigned int		after_dds;
};

#define FH_LOG_LIST_SIZE 128

struct fh_log_entry fh_log_list_mfgpll[FH_LOG_LIST_SIZE];
struct fh_log_entry fh_log_list_apupll[FH_LOG_LIST_SIZE];
unsigned int fh_log_list_idx_table[FH_PLL_NUM] = {0};
struct fh_log_entry *fh_log_list_table[FH_PLL_NUM] = {
	NULL,			//ARMPLL_LL
	NULL,			//ARMPLL_BL0
	NULL,			//ARMPLL_BL1
	NULL,			//ARMPLL_BL2
	NULL,			//ARMPLL_BL3
	NULL,			//CCIPLL
	fh_log_list_mfgpll,	//MFGPLL
	NULL,			//MEMPLL
	NULL,			//MPLL
	NULL,			//MPLL
	NULL,			//MAINPLL
	NULL,			//MSDCPLL
	NULL,			//ADSPPLL
	fh_log_list_apupll,	//APUPLL
	NULL			//TVDPLL
};

struct fh_log_entry fh_log_list_mfgpll_fail[FH_LOG_LIST_SIZE];
struct fh_log_entry fh_log_list_apupll_fail[FH_LOG_LIST_SIZE];
unsigned int fh_log_list_idx_table_fail[FH_PLL_NUM] = {0};
struct fh_log_entry *fh_log_list_table_fail[FH_PLL_NUM] = {
	NULL,			//ARMPLL_LL
	NULL,			//ARMPLL_BL0
	NULL,			//ARMPLL_BL1
	NULL,			//ARMPLL_BL2
	NULL,			//ARMPLL_BL3
	NULL,			//CCIPLL
	fh_log_list_mfgpll_fail,//MFGPLL
	NULL,			//MEMPLL
	NULL,			//MPLL
	NULL,			//MPLL
	NULL,			//MAINPLL
	NULL,			//MSDCPLL
	NULL,			//ADSPPLL
	fh_log_list_apupll_fail,//APUPLL
	NULL			//TVDPLL
};

unsigned long long fh_log_list_serial[FH_PLL_NUM] = {0};

unsigned int fh_ipi_hopping_serial;
static DEFINE_MUTEX(fh_ipi_lock);

/*****************************************************************************/
/* Function */
/*****************************************************************************/
#define F2M_CMD_ERR_MSG \
	"[Error]mtk_ipi_send_compl error(%x) ret:%d - %d\n"

#define F2M_ACK_ERR_MSG \
	"[Error]cmd(%x) return error(%d)\n"

#define FHCTL_D_LEN (9)

static void ipi_get_data(unsigned int cmd)
{
	struct fhctl_ipi_data ipi_data;
	int ret;

	FH_MSG("[FH] cmd<%x>\n", cmd);
	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.cmd = cmd;

	/* 3 sec for debug */
	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
			IPI_SEND_POLLING, &ipi_data,
			FHCTL_D_LEN, 3000);
	FH_MSG("[FH] ret<%d>, ack_data<%x>\n", ret, ack_data);
}

static void fhctl_get_mcupm_log(void)
{
	if (g_reg_tr)
		FH_MSG("[FH] reg_tr<%x>\n", readl(g_reg_tr));

	ipi_get_data(FH_DBG_CMD_TR_BEGIN_LOW);
	ipi_get_data(FH_DBG_CMD_TR_BEGIN_HIGH);
	ipi_get_data(FH_DBG_CMD_TR_END_LOW);
	ipi_get_data(FH_DBG_CMD_TR_END_HIGH);
	ipi_get_data(FH_DBG_CMD_TR_ID);
	ipi_get_data(FH_DBG_CMD_TR_VAL);
}

static int fhctl_to_mcupm_ack_data(unsigned int cmd,
		struct fhctl_ipi_data *ipi_data, int *retVal)
{

	FH_MSG_DEBUG("send ipi command %x", cmd);

	switch (cmd) {
	case FH_DCTL_CMD_GET_PLL_STRUCT:
		ipi_data->cmd = cmd;

		*retVal = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
				IPI_SEND_POLLING, ipi_data, FHCTL_D_LEN, 10);

		if (*retVal != 0) {
			FH_MSG(F2M_CMD_ERR_MSG, cmd, *retVal, ack_data);
			FH_MSG("[FH] check uart status: %d",
				mt_get_uartlog_status());

			/*timeout when uart enable may be false alarm*/
			if (!mt_get_uartlog_status())
				aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
					"[FH] IPI to CPUEB\n",
					"IPI timeout");
		} else if (ack_data < 0)
			FH_MSG(F2M_ACK_ERR_MSG, cmd, ack_data);
		break;
	default:
		FH_MSG("[Error]Undefined IPI command");
		break;
	}

	FH_MSG_DEBUG("send ipi command %x, response: ack_data: %d",
			cmd, ack_data);
	return ack_data;
}


static int fhctl_to_mcupm_command(unsigned int cmd,
		struct fhctl_ipi_data *ipi_data)
{
	int retVal = 0;

	FH_MSG_DEBUG("send ipi command %x", cmd);

	switch (cmd) {
	case FH_DCTL_CMD_DVFS:
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:
	case FH_DCTL_CMD_DVFS_SSC_DISABLE:
	case FH_DCTL_CMD_SSC_ENABLE:
	case FH_DCTL_CMD_SSC_DISABLE:
	case FH_DCTL_CMD_GENERAL_DFS:
	case FH_DCTL_CMD_ARM_DFS:
	case FH_DCTL_CMD_MM_DFS:
	case FH_DCTL_CMD_FH_CONFIG:
	case FH_DCTL_CMD_SSC_TBL_CONFIG:
	case FH_DCTL_CMD_SET_PLL_STRUCT:
//	case FH_DCTL_CMD_GET_PLL_STRUCT:
	case FH_DCTL_CMD_PLL_PAUSE:
		ipi_data->cmd = cmd;

		retVal = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_FHCTL,
				IPI_SEND_POLLING, ipi_data, FHCTL_D_LEN, 10);

		if (retVal != 0)
			FH_MSG(F2M_CMD_ERR_MSG, cmd, retVal, ack_data);

		if (ack_data < 0)
			FH_MSG(F2M_ACK_ERR_MSG, cmd, ack_data);
		break;
	default:
		FH_MSG("[Error]Undefined IPI command");
		break;
	}

	FH_MSG_DEBUG("send ipi command %x, response: ack_data: %d",
			cmd, ack_data);
	return retVal;
}

static void mt_fh_pll_struct_set(int pll_id, int field, int value)
{
	struct fhctl_ipi_data ipi_data;

	ipi_data.u.args[0] = pll_id;
	ipi_data.u.args[1] = field;
	ipi_data.u.args[2] = value;
	fhctl_to_mcupm_command(FH_DCTL_CMD_SET_PLL_STRUCT, &ipi_data);
}

static int mt_fh_pll_struct_get(int pll_id, int field)
{
	struct fhctl_ipi_data ipi_data;
	int retval = 0;
	int ack_data = 0;

	ipi_data.u.args[0] = pll_id;
	ipi_data.u.args[1] = field;

	ack_data = fhctl_to_mcupm_ack_data(FH_DCTL_CMD_GET_PLL_STRUCT,
				&ipi_data, &retval);

	if (retval == 0)
		return ack_data;
	else
		return -1;
}

/* Just to use special index pattern to find right setting. */
static noinline int __freq_to_index(enum FH_PLL_ID pll_id,
		int pattern)
{
	unsigned int retVal = 0;
	unsigned int i = PLL_SETTING_IDX__DEF;	/* start from 1 */
	const unsigned int size =
		ARRAY_SIZE(g_pll_ssc_setting_tbl[pll_id]);

	while (i < size) {
		if (pattern == g_pll_ssc_setting_tbl[pll_id][i].idx_pattern) {
			retVal = i;
			break;
		}
		++i;
	}

	return retVal;
}

/* Hook to g_fh_hal_drv.mt_fh_hal_ctrl function point.
 * Common drv freqhopping_config() will call the HAL API.
 */
static int __freqhopping_ctrl(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
	unsigned int ssc_id = 0;
	int retVal = 1;
	struct fhctl_ipi_data ipi_data;
	struct fh_pll_t fh_pll;

	if (fh_ctl == NULL)
		return -1;
	FH_MSG("%s for pll %d", __func__, fh_ctl->pll_id);

	if (!IS_PLLID_VALID(fh_ctl->pll_id)) {
		FH_MSG("(ERROR) %s [pll_id]:%d freqhop is not supported ",
				__func__, fh_ctl->pll_id);
		return -1;
	}
	if (enable == true) {
		fh_pll.user_defined =
			mt_fh_pll_struct_get(fh_ctl->pll_id, USER_DEFINED);
		if (fh_pll.user_defined == true) {
			FH_MSG("Apply user defined setting");
			pSSC_setting =
				&mt_ssc_fhpll_userdefined[fh_ctl->pll_id];
			ssc_id = PLL_SETTING_IDX__USER;
		} else {

			fh_pll.setting_idx_pattern =
				mt_fh_pll_struct_get(fh_ctl->pll_id,
						SETTING_IDX_PATTERN);

			if (fh_pll.setting_idx_pattern > 0) {
				ssc_id =
					__freq_to_index(fh_ctl->pll_id,
						fh_pll.setting_idx_pattern);

			} else {
				ssc_id = 0;
			}
			if (ssc_id == 0) {
				FH_MSG("No corresponding setting found !!!");
				/* just disable FH & exit */
				goto Exit;
			}
			pSSC_setting =
				&g_pll_ssc_setting_tbl[fh_ctl->pll_id][ssc_id];
		}
		if (pSSC_setting == NULL) {
			FH_MSG("SSC_setting is NULL!");

			/* disable FH & exit */
			goto Exit;
		}
		mt_fh_pll_struct_set(fh_ctl->pll_id, SETTING_ID, ssc_id);

		memset(&ipi_data, 0,
				sizeof(struct fhctl_ipi_data));
		memcpy(&ipi_data.u.fh_ctl, fh_ctl,
				sizeof(struct freqhopping_ioctl));
		memcpy(&ipi_data.u.fh_ctl.ssc_setting, pSSC_setting,
				sizeof(struct freqhopping_ssc));
		fhctl_to_mcupm_command(FH_DCTL_CMD_SSC_TBL_CONFIG, &ipi_data);
	}

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.u.args[0] = fh_ctl->pll_id;
	ipi_data.u.args[2] = enable;
	fhctl_to_mcupm_command(FH_DCTL_CMD_FH_CONFIG, &ipi_data);

Exit:
	return retVal;
}

static void mt_fh_hal_default_conf(void)
{
	int id;

	FH_MSG_DEBUG("%s", __func__);

	/* According to setting to enable PLL SSC during init FHCTL. */
	for (id = 0; id < FH_PLL_NUM; id++) {
		/*TODO implement MCUPM control*/
		if (g_pll_ssc_init_tbl[id] == FH_SSC_DEF_ENABLE_SSC) {
			FH_MSG("[Default ENABLE SSC] PLL_ID:%d", id);
			mt_fh_pll_struct_set(id, FH_STATUS, FH_FH_ENABLE_SSC);
			freqhopping_config(id, PLL_SETTING_IDX__DEF, true);
		} else {
			FH_MSG("[Default DISABLE SSC] PLL_ID:%d", id);
			mt_fh_pll_struct_set(id, FH_STATUS, FH_FH_DISABLE);
		}
	}
}

static void mt_fh_dump_register(void)
{
	int i;

	FH_MSG("HP_EN:%08x\n", fh_read32(REG_FHCTL_HP_EN));

	for (i = 0 ; i < FH_PLL_NUM ; i++) {
		FH_MSG("P:%s C:%08x DV:%08x DDS:%08x M:%08x CON1:%08x\n",
		g_pll_name[i],
		fh_read32(g_reg_cfg[i]),
		fh_read32(g_reg_dvfs[i]),
		fh_read32(g_reg_dds[i]),
		fh_read32(g_reg_mon[i]),
		g_reg_pll_con1[i] == REG_PLL_NOT_SUPPORT ?
			REG_PLL_NOT_SUPPORT : fh_read32(g_reg_pll_con1[i]));
	}
}

/* General purpose PLL hopping and SSC enable API. */
#define HOPPING_FORBIDDEN_PLL_MSG \
	"ERROR! The [PLL_ID]:%d was forbidden hopping by MT6758 FHCTL."

static int mt_fh_hal_general_pll_dfs(enum FH_PLL_ID pll_id,
		unsigned int target_dds)
{
	struct fhctl_ipi_data ipi_data;
	int retVal = 0;
	unsigned int log_idx, log_idx_f;
	unsigned int cur_dds;
	u64 time_ns;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready. ", __func__);
		return -1;
	}

	if (!IS_PLLID_VALID(pll_id)) {
		FH_MSG("(ERROR) %s [pll_id]: %d freqhop isn't supported ",
				__func__, pll_id);
		WARN_ON(1);
		return -1;
	}

	if (target_dds > FH_DDS_MASK) {
		FH_MSG("[ERROR] Overflow! [%s] [pll_id]:%d [dds]:0x%x",
				__func__, pll_id, target_dds);
		/* Check dds overflow (21 bit) */
		WARN_ON(1);
	}

	/* [MT6765/MT6789 Only] All new platform should confirm again!!! */
	switch (pll_id) {
	case FH_MEM_PLLID:
		/* MEMPLL Confirmed with DRAM SA that MEMPLL hopping
		 * will only control by clk DIV
		 */
		FH_MSG(HOPPING_FORBIDDEN_PLL_MSG, pll_id);
		WARN_ON(1);
		break;
	default:
		break;
	}

	FH_MSG_DEBUG("%s, [Pll_ID]:%d [cur dds(CON1)]:0x%x, [target dds]:%d",
			__func__, pll_id,
			(fh_read32(g_reg_pll_con1[pll_id]) & FH_DDS_MASK),
			target_dds);

	mutex_lock(&fh_ipi_lock);

	//FHCTL IN MEM LOG
	if (fh_log_list_table[pll_id] != NULL) {
		//get log idx
		log_idx = fh_log_list_idx_table[pll_id];

		//recording
		fh_log_list_table[pll_id][log_idx].serial =
			fh_log_list_serial[pll_id]++;

		fh_log_list_table[pll_id][log_idx].before_dds =
			fh_read32(g_reg_pll_con1[pll_id]) & (0x3FFFFF);

		fh_log_list_table[pll_id][log_idx].target_dds =
			target_dds;
	}

	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.u.args[0] = pll_id;
	ipi_data.u.args[1] = target_dds;
	ipi_data.u.args[7] = ++fh_ipi_hopping_serial;
	time_ns = ktime_to_ns(ktime_get());
	retVal = fhctl_to_mcupm_command(FH_DCTL_CMD_GENERAL_DFS, &ipi_data);

	//read back CON1 after hopping IPI complete
	cur_dds = fh_read32(g_reg_pll_con1[pll_id]) & (0x3FFFFF);

	if ((cur_dds != target_dds) || (retVal != 0)) {
		FH_MSG("[FH] time_ns %lx", time_ns);
		FH_MSG("[FH] hopping fail, cur %x, tgt %x, s %d\n",
			cur_dds, target_dds, fh_ipi_hopping_serial);
		FH_MSG("[FH] serial %d, ack %d\n", fh_ipi_hopping_serial, ack_data);
		fhctl_get_mcupm_log();
		mt_fh_dump_register();
		FH_MSG("[FH] check uart status: %d", mt_get_uartlog_status());
		/*timeout when uart enable may be false alarm*/
		if (!mt_get_uartlog_status())
			aee_kernel_warning_api(__FILE__, __LINE__,
				DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
				"[FH] IPI to CPUEB\n",
				"IPI timeout");
		//BUG();
	}

	//FHCTL IN MEM LOG
	if (fh_log_list_table[pll_id] != NULL) {
		fh_log_list_table[pll_id][log_idx].after_dds = cur_dds;

		//validate result
		if (cur_dds != target_dds) {
			//log to fail list
			log_idx_f = fh_log_list_idx_table_fail[pll_id];

			fh_log_list_table_fail[pll_id][log_idx_f].serial =
				fh_log_list_table[pll_id][log_idx].serial;
			fh_log_list_table_fail[pll_id][log_idx_f].before_dds =
				fh_log_list_table[pll_id][log_idx].before_dds;
			fh_log_list_table_fail[pll_id][log_idx_f].target_dds =
				fh_log_list_table[pll_id][log_idx].target_dds;
			fh_log_list_table_fail[pll_id][log_idx_f].after_dds =
				fh_log_list_table[pll_id][log_idx].after_dds;

			//increament log idx
			if (++log_idx_f >= FH_LOG_LIST_SIZE)
				log_idx_f = 0;

			fh_log_list_idx_table_fail[pll_id] = log_idx_f;
		}

		//increament log idx
		if (++log_idx >= FH_LOG_LIST_SIZE)
			log_idx = 0;

		fh_log_list_idx_table[pll_id] = log_idx;
	}

	mutex_unlock(&fh_ipi_lock);

	return retVal;

}
#undef HOPPING_FORBIDDEN_PLL_MSG

/*
 *   armpll dfs mdoe
 */
static int mt_fh_hal_dfs_armpll(unsigned int coreid, unsigned int dds)
{
	struct fhctl_ipi_data ipi_data;
	int retVal = 0;
	memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
	ipi_data.u.args[0] = coreid;
	ipi_data.u.args[1] = dds;
	retVal = fhctl_to_mcupm_command(FH_DCTL_CMD_ARM_DFS, &ipi_data);
	return retVal;
}


/* #define UINT_MAX (unsigned int)(-1) */
static int fh_dumpregs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;
	static unsigned int dds_max[FH_PLL_NUM] = { 0 };
	static unsigned int dds_min[FH_PLL_NUM] = { 0 };

	if (g_initialize != 1) {
		FH_MSG("[ERROR] %s fhctl didn't init. Please check!!!",
				__func__);
		return -1;
	}

	FH_MSG("EN: %s", __func__);

	FH_MSG_DEBUG("REG ADDR TABLE:");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		FH_MSG_DEBUG("(%d): 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx",
				i, g_reg_mon[i], g_reg_cfg[i], g_reg_updnlmt[i],
				g_reg_dvfs[i], g_reg_dds[i], g_reg_pll_con0[i],
				g_reg_pll_con1[i]);
	}

	for (i = 0; i < FH_PLL_NUM; ++i) {
		const unsigned int mon = fh_read32(g_reg_mon[i]);
		const unsigned int dds = mon & MASK22b;

		seq_printf(m, "FHCTL%d CFG, UPDNLMT, DVFS, DDS, MON\r\n", i);
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			fh_read32(g_reg_cfg[i]), fh_read32(g_reg_updnlmt[i]),
			fh_read32(g_reg_dvfs[i]), fh_read32(g_reg_dds[i]), mon);

		if (dds > dds_max[i])
			dds_max[i] = dds;
		if ((dds < dds_min[i]) || (dds_min[i] == 0))
			dds_min[i] = dds;
	}

	seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n",
			fh_read32(REG_FHCTL_HP_EN));
	seq_printf(m, "\r\nFHCTL_CLK_CON:\r\n0x%08x\r\n",
			fh_read32(REG_FHCTL_CLK_CON));

	seq_puts(m, "\r\nPLL_CON0 :\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		if (g_reg_pll_con0[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;Not Support ", i);
		else
			seq_printf(m, "PLL%d;0x%08x ",
					i, fh_read32(g_reg_pll_con0[i]));
	}


	seq_puts(m, "\r\nPLL_CON1 :\r\n");
	for (i = 0; i < FH_PLL_NUM; ++i) {
		if (g_reg_pll_con1[i] == REG_PLL_NOT_SUPPORT)
			seq_printf(m, "PLL%d;Not Support ", i);
		else
			seq_printf(m, "PLL%d;0x%08x ",
					i, fh_read32(g_reg_pll_con1[i]));
	}

	seq_puts(m, "\r\nRecorded dds range\r\n");

	for (i = 0; i < FH_PLL_NUM; ++i)
		seq_printf(m, "Pll%d dds max 0x%06x, min 0x%06x\r\n",
				i, dds_max[i], dds_min[i]);

	return 0;
}

static void __reg_tbl_init(void)
{
	int id = 0;

	/****************************************/
	/* Should porting for specific platform. */
	/****************************************/

	const unsigned long reg_dds[] = {
		REG_FHCTL0_DDS, REG_FHCTL1_DDS, REG_FHCTL2_DDS,
		REG_FHCTL3_DDS, REG_FHCTL4_DDS, REG_FHCTL5_DDS,
		REG_FHCTL6_DDS, REG_FHCTL7_DDS, REG_FHCTL8_DDS,
		REG_FHCTL9_DDS, REG_FHCTL10_DDS, REG_FHCTL11_DDS,
		REG_FHCTL12_DDS, REG_FHCTL13_DDS, REG_FHCTL14_DDS
	};

	const unsigned long reg_cfg[] = {
		REG_FHCTL0_CFG, REG_FHCTL1_CFG, REG_FHCTL2_CFG,
		REG_FHCTL3_CFG, REG_FHCTL4_CFG, REG_FHCTL5_CFG,
		REG_FHCTL6_CFG, REG_FHCTL7_CFG, REG_FHCTL8_CFG,
		REG_FHCTL9_CFG, REG_FHCTL10_CFG, REG_FHCTL11_CFG,
		REG_FHCTL12_CFG, REG_FHCTL13_CFG, REG_FHCTL14_CFG
	};

	const unsigned long reg_updnlmt[] = {
		REG_FHCTL0_UPDNLMT, REG_FHCTL1_UPDNLMT, REG_FHCTL2_UPDNLMT,
		REG_FHCTL3_UPDNLMT, REG_FHCTL4_UPDNLMT, REG_FHCTL5_UPDNLMT,
		REG_FHCTL6_UPDNLMT, REG_FHCTL7_UPDNLMT, REG_FHCTL8_UPDNLMT,
		REG_FHCTL9_UPDNLMT, REG_FHCTL10_UPDNLMT, REG_FHCTL11_UPDNLMT,
		REG_FHCTL12_UPDNLMT, REG_FHCTL13_UPDNLMT, REG_FHCTL14_UPDNLMT
	};

	const unsigned long reg_mon[] = {
		REG_FHCTL0_MON, REG_FHCTL1_MON, REG_FHCTL2_MON,
		REG_FHCTL3_MON, REG_FHCTL4_MON, REG_FHCTL5_MON,
		REG_FHCTL6_MON, REG_FHCTL7_MON, REG_FHCTL8_MON,
		REG_FHCTL9_MON, REG_FHCTL10_MON, REG_FHCTL11_MON,
		REG_FHCTL12_MON, REG_FHCTL13_MON, REG_FHCTL14_MON
	};

	const unsigned long reg_dvfs[] = {
		REG_FHCTL0_DVFS, REG_FHCTL1_DVFS, REG_FHCTL2_DVFS,
		REG_FHCTL3_DVFS, REG_FHCTL4_DVFS, REG_FHCTL5_DVFS,
		REG_FHCTL6_DVFS, REG_FHCTL7_DVFS, REG_FHCTL8_DVFS,
		REG_FHCTL9_DVFS, REG_FHCTL10_DVFS, REG_FHCTL11_DVFS,
		REG_FHCTL12_DVFS, REG_FHCTL13_DVFS, REG_FHCTL14_DVFS
	};

	const unsigned long reg_pll_con0[] = {
		REG_FH_PLL0_CON0, REG_FH_PLL1_CON0, REG_FH_PLL2_CON0,
		REG_FH_PLL3_CON0, REG_FH_PLL4_CON0, REG_FH_PLL5_CON0,
		REG_FH_PLL6_CON0, REG_FH_PLL7_CON0, REG_FH_PLL8_CON0,
		REG_FH_PLL9_CON0, REG_FH_PLL10_CON0, REG_FH_PLL11_CON0,
		REG_FH_PLL12_CON0, REG_FH_PLL13_CON0, REG_FH_PLL14_CON0
	};

	const unsigned long reg_pll_con1[] = {
		REG_FH_PLL0_CON1, REG_FH_PLL1_CON1, REG_FH_PLL2_CON1,
		REG_FH_PLL3_CON1, REG_FH_PLL4_CON1, REG_FH_PLL5_CON1,
		REG_FH_PLL6_CON1, REG_FH_PLL7_CON1, REG_FH_PLL8_CON1,
		REG_FH_PLL9_CON1, REG_FH_PLL10_CON1, REG_FH_PLL11_CON1,
		REG_FH_PLL12_CON1, REG_FH_PLL13_CON1, REG_FH_PLL14_CON1
	};

	/****************************************/

	FH_MSG_DEBUG("EN: %s", __func__);


	for (id = 0; id < FH_PLL_NUM; ++id) {
		g_reg_dds[id] = reg_dds[id];
		g_reg_cfg[id] = reg_cfg[id];
		g_reg_updnlmt[id] = reg_updnlmt[id];
		g_reg_mon[id] = reg_mon[id];
		g_reg_dvfs[id] = reg_dvfs[id];
		g_reg_pll_con0[id] = reg_pll_con0[id];
		g_reg_pll_con1[id] = reg_pll_con1[id];
	}
}

/* Device Tree Initialize */
static int __reg_base_addr_init(void)
{
	struct device_node *fhctl_node;
	struct device_node *apmixed_node;
	void __iomem *reg_tr = (void __iomem *)(0xC8 + 0x4);

	FH_MSG("(b) g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("(b) g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);

	/* Init FHCTL base address */
	fhctl_node = of_find_compatible_node(NULL, NULL, "mediatek,fhctl");
	g_fhctl_base = of_iomap(fhctl_node, 0);
	if (!g_fhctl_base) {
		FH_MSG_DEBUG("Error, FHCTL iomap failed");
		WARN_ON(1);
		return -ENODEV;
	}

	/* Init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL,
					"mediatek,mt6853-apmixedsys");
	if (!apmixed_node)
		apmixed_node = of_find_compatible_node(NULL, NULL,
				"mediatek,mt6833-apmixedsys");

	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		FH_MSG_DEBUG("Error, APMIXED iomap failed");
		WARN_ON(1);
		return -ENODEV;
	}

	g_reg_tr = g_fhctl_base + (unsigned int)reg_tr;

	FH_MSG("g_fhctl_base:0x%lx", (unsigned long)g_fhctl_base);
	FH_MSG("g_apmixed_base:0x%lx", (unsigned long)g_apmixed_base);
	FH_MSG("g_reg_tr:0x%lx", (unsigned long)g_reg_tr);
	__reg_tbl_init();

	return 0;
}

static void __global_var_init(void)
{

}

/* TODO: __init void mt_freqhopping_init(void) */
static int mt_fh_hal_init(void)
{
	int of_init_result = 0;
	int ret;

	FH_MSG_DEBUG("EN: %s", __func__);

	if (g_initialize == 1)
		return 0;

	/* Register MCUPM ipi device*/
	ack_data = 0;
	ret = mtk_ipi_register(&mcupm_ipidev, CH_S_FHCTL, NULL,
			NULL, (void *)&ack_data);
	if (ret) {
		pr_err("[MCUPM] ipi_register fail, ret %d\n", ret);
		return -1;
	}

	/* Init relevant register base address by device tree */
	of_init_result = __reg_base_addr_init();
	if (of_init_result != 0)
		return of_init_result;

	/* Global Variable Init */
	__global_var_init();

	g_initialize = 1;

	FH_MSG("%s done", __func__);
	return 0;
}

static void mt_fh_hal_lock(unsigned long *flags)
{
	/*spin_lock(&g_fh_lock);*/
	spin_lock_irqsave(&g_fh_lock, *flags);
}

static void mt_fh_hal_unlock(unsigned long *flags)
{
	/*spin_unlock(&g_fh_lock);*/
	spin_unlock_irqrestore(&g_fh_lock, *flags);
}

static int mt_fh_hal_get_init(void)
{
	return g_initialize;
}

/* TODO: module_init(mt_freqhopping_init); */
/* TODO: module_exit(cpufreq_exit); */
#define FH_DEBUG_PROC_READ_MSG_STATUS \
	"[1st Status] FH_FH_UNINIT:0, FH_FH_DISABLE: 1, FH_FH_ENABLE_SSC:2 \r\n"

#define FH_DEBUG_PROC_READ_MSG_SETTING_IDX \
	"[2nd Setting_id] Disable:0, Default:1, PLL_SETTING_IDX__USER:9 \r\n"

/* Engineer mode will use the proc msg to create UI!!! */
static int __fh_debug_proc_read(struct seq_file *m, void *v,
		struct fh_pll_t *pll)
{
	int id;

	FH_MSG("EN: %s", __func__);

	/* [WWK] Should remove PLL name to save porting time. */
	/* [WWK] Could print ENG ID and PLL mapping */

	seq_puts(m, "\r\n[freqhopping debug flag]\r\n");
	seq_puts(m, FH_DEBUG_PROC_READ_MSG_STATUS);
	seq_puts(m, FH_DEBUG_PROC_READ_MSG_SETTING_IDX);
	seq_puts(m, "===============================================\r\n");

	/****** String Format sensitive for EM mode ******/
	seq_puts(m, "id");
	for (id = 0; id < FH_PLL_NUM; ++id)
		seq_printf(m, "=%s", g_pll_name[id]);

	seq_puts(m, "\r\n");



	for (id = 0; id < FH_PLL_NUM; ++id) {
		/* "  =%04d==%04d==%04d==%04d=\r\n" */
		if (id == 0)
			seq_puts(m, "  =");
		else
			seq_puts(m, "==");

		/*seq_printf(m, "%04d", pll[id].fh_status);*/
		seq_printf(m, "%04d", mt_fh_pll_struct_get(id, FH_STATUS));

		if (id == (FH_PLL_NUM - 1))
			seq_puts(m, "=");
	}
	seq_puts(m, "\r\n");


	for (id = 0; id < FH_PLL_NUM; ++id) {
		/* "  =%04d==%04d==%04d==%04d=\r\n" */
		if (id == 0)
			seq_puts(m, "  =");
		else
			seq_puts(m, "==");

		/*seq_printf(m, "%04d", pll[id].setting_id);*/
		seq_printf(m, "%04d", mt_fh_pll_struct_get(id, SETTING_ID));

		if (id == (FH_PLL_NUM - 1))
			seq_puts(m, "=");
	}
	/*************************************************/

	seq_puts(m, "\r\n");

	return 0;
}
#undef FH_DEBUG_PROC_READ_MSG_STATUS
#undef FH_DEBUG_PROC_READ_MSG_SETTING_IDX

static void __ioctl(unsigned int ctlid, void *arg)
{
	struct freqhopping_ioctl *fh_ctl = arg;
	struct fhctl_ipi_data ipi_data;

	switch (ctlid) {
	case FH_IO_PROC_READ:
		{
			struct FH_IO_PROC_READ_T *tmp =
				(struct FH_IO_PROC_READ_T *) (arg);

			__fh_debug_proc_read(tmp->m, tmp->v, tmp->pll);
		}
		break;
	case FH_DCTL_CMD_DVFS:	/* PLL DVFS */
	case FH_DCTL_CMD_DVFS_SSC_ENABLE:	/* PLL DVFS and enable SSC */
	case FH_DCTL_CMD_DVFS_SSC_DISABLE:	/* PLL DVFS and disable SSC */
	case FH_DCTL_CMD_SSC_ENABLE:	/* SSC enable */
	case FH_DCTL_CMD_SSC_DISABLE:	/* SSC disable */
		if (g_reg_pll_con0[fh_ctl->pll_id] == REG_PLL_NOT_SUPPORT) {
			FH_MSG("(ERROR) %s [pll_id]:%d freqhop isn't supported",
					__func__, fh_ctl->pll_id);
		} else {
			memset(&ipi_data, 0, sizeof(struct fhctl_ipi_data));
			memcpy(&ipi_data.u.fh_ctl, fh_ctl,
					sizeof(struct freqhopping_ioctl));
			fhctl_to_mcupm_command(ctlid, &ipi_data);
		}
		break;
	case FH_DCTL_CMD_GENERAL_DFS:
		if (g_reg_pll_con0[fh_ctl->pll_id] == REG_PLL_NOT_SUPPORT)
			FH_MSG("(ERROR) %s [pll_id]:%d freqhop isn't supported",
					__func__, fh_ctl->pll_id);
		else
			mt_fh_hal_general_pll_dfs(fh_ctl->pll_id,
					fh_ctl->ssc_setting.dds);
		break;

	default:
		FH_MSG("Unrecognized ctlid %d", ctlid);
		break;
	};
}

static struct mt_fh_hal_driver g_fh_hal_drv = {
	.fh_pll = g_fh_pll,
	//.fh_usrdef = mt_ssc_fhpll_userdefined,
	.fh_pll_set = mt_fh_pll_struct_set,
	.fh_pll_get = mt_fh_pll_struct_get,
	/*.fh_usrdef = mt_fh_usrdef_set,*/
	.pll_cnt = FH_PLL_NUM,
	.mt_fh_hal_dumpregs_read = fh_dumpregs_proc_read,
	//.proc.dvfs_read = fh_dvfs_proc_read,
	//.proc.dvfs_write = fh_dvfs_proc_write,
	.mt_fh_hal_init = mt_fh_hal_init,
	.mt_fh_hal_ctrl = __freqhopping_ctrl,
	.mt_fh_lock = mt_fh_hal_lock,
	.mt_fh_unlock = mt_fh_hal_unlock,
	.mt_fh_get_init = mt_fh_hal_get_init,
	//.mt_fh_popod_restore = mt_fh_hal_popod_restore,
	//.mt_fh_popod_save = mt_fh_hal_popod_save,
	//.mt_l2h_mempll = NULL,
	//.mt_h2l_mempll = NULL,
	.mt_dfs_armpll = mt_fh_hal_dfs_armpll,
	//.mt_dfs_mmpll = mt_fh_hal_dfs_mmpll,
	//.mt_dfs_vencpll = mt_fh_hal_dfs_vencpll,
	//.mt_is_support_DFS_mode = mt_fh_hal_is_support_DFS_mode,
	//.mt_l2h_dvfs_mempll = mt_fh_hal_l2h_dvfs_mempll,
	//.mt_h2l_dvfs_mempll = mt_fh_hal_h2l_dvfs_mempll,
	//.mt_dram_overclock = mt_fh_hal_dram_overclock,
	//.mt_get_dramc = mt_fh_hal_get_dramc,
	.mt_fh_hal_default_conf = mt_fh_hal_default_conf,
	.mt_dfs_general_pll = mt_fh_hal_general_pll_dfs,
	.ioctl = __ioctl
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	return &g_fh_hal_drv;
}

/* SS13 request to provide the pause ARMPLL API */
/* [Purpose]: control PLL for each cluster */
int mt_pause_armpll(unsigned int pll, unsigned int pause)
{
	/* unsigned long flags = 0; */
	unsigned long reg_cfg = 0;
	struct fhctl_ipi_data ipi_data;
	int retVal = 0;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready.", __func__);
		return -1;
	}
	if (g_reg_pll_con0[pll] == REG_PLL_NOT_SUPPORT) {
		FH_MSG("(ERROR) %s [pll_id]:%d freqhop isn't supported",
				__func__, pll);
		return -1;
	}
	/* pause[0]: pause, pause[4]:susepend */
	FH_MSG_DEBUG("%s for pll %d pause %d", __func__, pll, pause);

	switch (pll) {
	case FH_PLL0:
	case FH_PLL1:
	case FH_PLL2:
	case FH_PLL3:
	case FH_PLL4:
		reg_cfg = g_reg_cfg[pll];
		FH_MSG_DEBUG("(FHCTLx_CFG): 0x%x", fh_read32(g_reg_cfg[pll]));
		break;
	default:
		FH_MSG("(ERROR) %s [pll_id]:%d not ARM PLL",
				__func__, pll);
		WARN_ON(1);
		return 1;
	};
	ipi_data.u.args[0] = pll;
	ipi_data.u.args[1] = (pause & 0x00000001) ? 1 : 0;
	retVal = fhctl_to_mcupm_command(FH_DCTL_CMD_PLL_PAUSE, &ipi_data);
	return retVal;
}
