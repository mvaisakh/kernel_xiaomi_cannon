/*
 * Copyright (C) 2019 MediaTek Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _UFS_MTK_PLATFORM_H
#define _UFS_MTK_PLATFORM_H

#include "ufs.h"
#include "ufshcd.h"

#ifndef CONFIG_FPGA_EARLY_PORTING
/* If UPMU function not ready, comment this define */
#define UPMU_READY
#endif

#define UFS_REF_CLK_CTRL_BY_UFSHCI
#define HIE_CHANGE_KEY_IN_NORMAL_WORLD

/*
 * Platform dependent quirks
 */

/*
 * Platform dependent definitions
 */
#ifdef CONFIG_FPGA_EARLY_PORTING
/*
 * Bitfile fail in PWM mode, change to HS mode first.
 * If platform bitfile no this problem, can remove it.
 */
/* #define PWM_FAIL_WORKAROUND */
#endif

enum {
	REG_UFSHCI_SW_RST_SET       = 0x130,
	REG_UFSHCI_SW_RST_SET_BIT   = 15,
	REG_UFSHCI_SW_RST_CLR       = 0x134,
	REG_UFSHCI_SW_RST_CLR_BIT   = 15,

	REG_UNIPRO_SW_RST_SET       = 0x140,
	REG_UNIPRO_SW_RST_SET_BIT   = 7,
	REG_UNIPRO_SW_RST_CLR       = 0x144,
	REG_UNIPRO_SW_RST_CLR_BIT   = 7,

	REG_UFSPHY_SW_RST_SET       = 0x140,
	REG_UFSPHY_SW_RST_SET_BIT   = 9,
	REG_UFSPHY_SW_RST_CLR       = 0x144,
	REG_UFSPHY_SW_RST_CLR_BIT   = 9,

	REG_UFSCPT_SW_RST_SET       = 0x150,
	REG_UFSCPT_SW_RST_SET_BIT   = 21,
	REG_UFSCPT_SW_RST_CLR       = 0x154,
	REG_UFSCPT_SW_RST_CLR_BIT   = 21,
};

enum {
	SW_RST_TARGET_UFSHCI        = 0x1,
	SW_RST_TARGET_UNIPRO        = 0x2,
	SW_RST_TARGET_UFSCPT        = 0x4,
	SW_RST_TARGET_MPHY          = 0x8,
};
#define SW_RST_TARGET_ALL (SW_RST_TARGET_UFSHCI | \
	SW_RST_TARGET_UNIPRO | SW_RST_TARGET_UFSCPT)

/* infracfg_ao */
enum {
	CLK_CG_2_STA                = 0xAC,
	CLK_CG_3_STA                = 0xC8,
};

/* apmixed */
enum {
	PLL_MAINPLL                 = 0x0340,
	PLL_MSDCPLL                 = 0x0350,
};

/* topckgen */
enum {
	CLK_CFG_12                  = 0xD0,
};

/* INFRACFG_AO (0x10001000) MODULE_SW_CG_2 (0xac) */
#define INFRACFG_AO_UNIPRO_SYSCLK_CG    (1 << 11)
#define INFRACFG_AO_UNIPRO_TICK_CG      (1 << 12)
#define INFRACFG_AO_UFS_MP_SAP_BCLK_CG  (1 << 13)
#define INFRACFG_AO_UFS_CG              (1 << 28)
#define INFRACFG_AO_AES_UFSFDE_CG       (1 << 29)
#define INFRACFG_AO_UFS_TICK_CG         (1 << 30)

/* INFRACFG_AO (0x10001000) MODULE_SW_CG_3 (0xc8) */
#define INFRACFG_AO_UFS_AXI_CG          (1 << 5)

/*
 * Platform-dependent APIs
 */
void ufs_mtk_pltfrm_pwr_change_final_gear(struct ufs_hba *hba,
	struct ufs_pa_layer_attr *final);
int  ufs_mtk_pltfrm_ufs_device_reset(struct ufs_hba *hba);
int  ufs_mtk_pltfrm_host_sw_rst(struct ufs_hba *hba, u32 target);
int  ufs_mtk_pltfrm_bootrom_deputy(struct ufs_hba *hba);
int  ufs_mtk_pltfrm_deepidle_check_h8(void);
void ufs_mtk_pltfrm_deepidle_leave(void);
void ufs_mtk_pltfrm_deepidle_lock(struct ufs_hba *hba, bool lock);
int  ufs_mtk_pltfrm_ref_clk_ctrl(struct ufs_hba *hba, bool on);
int  ufs_mtk_pltfrm_init(void);
int  ufs_mtk_pltfrm_parse_dt(struct ufs_hba *hba);
int  ufs_mtk_pltfrm_resume(struct ufs_hba *hba);
int  ufs_mtk_pltfrm_suspend(struct ufs_hba *hba);
void ufs_mtk_pltfrm_gpio_trigger_and_debugInfo_dump(struct ufs_hba *hba);

#ifdef MTK_UFS_HQA
#ifdef UPMU_READY
#include <upmu_common.h>
#endif
void random_delay(struct ufs_hba *hba);
void wdt_pmic_full_reset(struct ufs_hba *hba);
extern unsigned short pmic_set_register_value_nolock(
	PMU_FLAGS_LIST_ENUM flagname, unsigned int val);
#endif

#endif /* _UFS_MTK_PLATFORM_H */

