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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <mt-plat/upmu_common.h>
#include "vibrator.h"

struct vibrator_hw *pvib_cust;

static int debug_enable_vib_hal = 1;
/* #define pr_fmt(fmt) "[vibrator]"fmt */
#define VIB_DEBUG(format, args...) do { \
	if (debug_enable_vib_hal) {\
		pr_debug(format, ##args);\
	} \
} while (0)

#define OC_INTR_INIT_DELAY      (3)

void vibr_Enable_HW(void)
{
#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 1);
#endif
	mdelay(OC_INTR_INIT_DELAY);
#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
	pmic_enable_interrupt(INT_VIBR_OC, 1, "vibr");
#endif
}

void vibr_Disable_HW(void)
{
#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
	pmic_enable_interrupt(INT_VIBR_OC, 0, "vibr");
	pmic_set_register_value(PMIC_RG_LDO_VIBR_EN, 0);
#endif
}

void init_vibr_oc_handler(void (*vibr_oc_func)(void))
{
#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
	pmic_register_interrupt_callback(INT_VIBR_OC, vibr_oc_func);
#endif
}

/******************************************
 * Set RG_VIBR_VOSEL	Output voltage select
 *  hw->vib_vol:  Voltage selection
 * 4'b0000 :1.2V
 * 4'b0001 :1.3V
 * 4'b0010 :1.5V
 * 4'b0100 :1.8V
 * 4'b1001 :2.8V
 * 4'b1010 :2.9V
 * 4'b1011 :3.0V
 * 4'b1101 :3.3V
 ******************************************/
void init_cust_vibrator_dtsi(struct platform_device *pdev)
{
	int ret;

	if (pvib_cust == NULL) {
		pvib_cust = kmalloc(sizeof(struct vibrator_hw), GFP_KERNEL);
		if (pvib_cust == NULL) {
			VIB_DEBUG("%s kmalloc fail\n", __func__);
			return;
		}
		ret = of_property_read_u32(pdev->dev.of_node, "vib_timer",
						&(pvib_cust->vib_timer));
		if (!ret)
			VIB_DEBUG("The vibrator timer from dts is : %d\n",
							pvib_cust->vib_timer);
		else
			pvib_cust->vib_timer = 25;
#ifdef CUST_VIBR_LIMIT
		ret = of_property_read_u32(pdev->dev.of_node, "vib_limit",
						&(pvib_cust->vib_limit));
		if (!ret)
			VIB_DEBUG("The vibrator limit from dts is : %d\n",
							pvib_cust->vib_limit);
		else
			pvib_cust->vib_limit = 9;
#endif

#ifdef CUST_VIBR_VOL
		ret = of_property_read_u32(pdev->dev.of_node, "vib_vol",
							&(pvib_cust->vib_vol));
		if (!ret)
			VIB_DEBUG("The vibrator vol from dts is : %d\n",
							pvib_cust->vib_vol);
		else
			pvib_cust->vib_vol = 0x05;
#endif
		VIB_DEBUG("pvib_cust = %d, %d, %d\n", pvib_cust->vib_timer,
				pvib_cust->vib_limit, pvib_cust->vib_vol);
	}
}

struct vibrator_hw *get_cust_vibrator_dtsi(void)
{
	if (pvib_cust == NULL)
		VIB_DEBUG("%s fail, pvib_cust is NULL\n", __func__);
	return pvib_cust;
}

void vibr_power_set(void)
{
#ifdef CUST_VIBR_VOL
	struct vibrator_hw *hw = get_cust_vibrator_dtsi();

	if (hw != NULL) {
		VIB_DEBUG("vibrator set voltage = %d\n", hw->vib_vol);
#ifdef CONFIG_MTK_PMIC_CHIP_MT6358
		pmic_set_register_value(PMIC_RG_VIBR_VOSEL, hw->vib_vol);
#endif
	} else {
		VIB_DEBUG("can not get vibrator settings from dtsi!\n");
	}
#endif
}

struct vibrator_hw *mt_get_cust_vibrator_hw(void)
{
	struct vibrator_hw *hw = get_cust_vibrator_dtsi();
	return hw;
}
