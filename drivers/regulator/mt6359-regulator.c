/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wen Su <wen.su@mediatek.com>
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

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6359/registers.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6359-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6359_BUCK_MODE_AUTO		0
#define MT6359_BUCK_MODE_FORCE_PWM	1
#define MT6359_BUCK_MODE_NORMAL		0
#define MT6359_BUCK_MODE_LP		1

/*
 * MT6359 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @constraints: standard fields of regulator constraints.
 * @da_reg: for query status of regulators.
 * @qi: Mask for query enable signal status of regulators.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_shift: SHIFT for operating modeset register.
 */
struct mt6359_regulator_info {
	struct regulator_desc desc;
	struct regulation_constraints constraints;
	u32 da_vsel_reg;
	u32 da_vsel_mask;
	u32 da_vsel_shift;
	u32 da_reg;
	u32 qi;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 modeset_shift;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 lp_mode_shift;
};

/*
 * MTK regulators' init data
 *
 * @id: chip id
 * @size: num of regulators
 * @regulator_info: regulator info.
 */
struct mt_regulator_init_data {
	u32 id;
	u32 size;
	struct mt6359_regulator_info *regulator_info;
};

#define MT_BUCK_EN		(REGULATOR_CHANGE_STATUS)
#define MT_BUCK_VOL		(REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN		(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE)
#define MT_BUCK_VOL_EN_MODE	(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE |	\
				 REGULATOR_CHANGE_MODE)

#define MT_LDO_EN		(REGULATOR_CHANGE_STATUS)
#define MT_LDO_VOL		(REGULATOR_CHANGE_VOLTAGE)
#define MT_LDO_VOL_EN		(REGULATOR_CHANGE_STATUS |	\
				 REGULATOR_CHANGE_VOLTAGE)

#define MT_BUCK(match, _name, min, max, step,				\
		min_sel, volt_ranges, _enable_reg,			\
		_da_reg,						\
		_da_vsel_reg, _da_vsel_mask, _da_vsel_shift,		\
		_vsel_reg, _vsel_mask,					\
		mode, _lp_mode_reg, _lp_mode_shift,			\
		_modeset_reg, _modeset_shift)				\
[MT6359_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6359_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6359_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.uV_step = (step),					\
		.linear_min_sel = (min_sel),				\
		.n_voltages = ((max) - (min)) / (step) + 1,		\
		.min_uV = (min),					\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = _vsel_reg,					\
		.vsel_mask = _vsel_mask,				\
		.enable_reg = _enable_reg,				\
		.enable_mask = BIT(0),					\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
		.valid_modes_mask =					\
			(REGULATOR_MODE_NORMAL |			\
			 REGULATOR_MODE_FAST |				\
			 REGULATOR_MODE_IDLE),				\
	},								\
	.da_vsel_reg = _da_vsel_reg,					\
	.da_vsel_mask = _da_vsel_mask,					\
	.da_vsel_shift = _da_vsel_shift,				\
	.da_reg = _da_reg,						\
	.qi = BIT(0),							\
	.lp_mode_reg = _lp_mode_reg,					\
	.lp_mode_mask = BIT(_lp_mode_shift),				\
	.lp_mode_shift = _lp_mode_shift,				\
	.modeset_reg = _modeset_reg,					\
	.modeset_mask = BIT(_modeset_shift),				\
	.modeset_shift = _modeset_shift					\
}

#define MT_LDO_REGULAR(match, _name, min, max, step,			\
		       min_sel, volt_ranges, _enable_reg,		\
		       _da_reg,						\
		       _da_vsel_reg, _da_vsel_mask, _da_vsel_shift,	\
		       _vsel_reg, _vsel_mask,				\
		       mode)						\
[MT6359_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6359_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6359_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.uV_step = (step),					\
		.linear_min_sel = (min_sel),				\
		.n_voltages = ((max) - (min)) / (step) + 1,		\
		.min_uV = (min),					\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = _vsel_reg,					\
		.vsel_mask = _vsel_mask,				\
		.enable_reg = _enable_reg,				\
		.enable_mask = BIT(0),					\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
	},								\
	.da_vsel_reg = _da_vsel_reg,					\
	.da_vsel_mask = _da_vsel_mask,					\
	.da_vsel_shift = _da_vsel_shift,				\
	.da_reg = _da_reg,						\
	.qi = BIT(0),							\
}

#define MT_LDO_NON_REGULAR(match, _name,				\
			   _volt_table, _enable_reg, _enable_mask,	\
			   _da_reg,					\
			   _vsel_reg, _vsel_mask,			\
			   mode)					\
[MT6359_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6359_volt_table_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6359_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(_volt_table),			\
		.volt_table = _volt_table,				\
		.vsel_reg = _vsel_reg,					\
		.vsel_mask = _vsel_mask,				\
		.enable_reg = _enable_reg,				\
		.enable_mask = BIT(_enable_mask),			\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
	},								\
	.da_reg = _da_reg,						\
	.qi = BIT(0),							\
}

#define MT_REG_FIXED(match, _name, _enable_reg,				\
		     _da_reg,						\
		     _fixed_volt, mode)					\
[MT6359_ID_##_name] = {							\
	.desc = {							\
		.name = #_name,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6359_volt_fixed_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6359_ID_##_name,				\
		.owner = THIS_MODULE,					\
		.n_voltages = 1,					\
		.enable_reg = _enable_reg,				\
		.enable_mask = BIT(0),					\
		.fixed_uV = (_fixed_volt),				\
	},								\
	.constraints = {						\
		.valid_ops_mask = (mode),				\
	},								\
	.da_reg = _da_reg,						\
	.qi = BIT(0),							\
}

//vs1
static const struct regulator_linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(800000, 0, 0x70, 12500),
};

static const struct regulator_linear_range mt_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(400000, 0, 0x7f, 6250),
};

static const struct regulator_linear_range mt_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(400000, 0, 0x70, 6250),
};

//vs2
static const struct regulator_linear_range mt_volt_range4[] = {
	REGULATOR_LINEAR_RANGE(800000, 0, 0x40, 12500),
};

//vpa
static const struct regulator_linear_range mt_volt_range5[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x3F, 50000),
};

static const struct regulator_linear_range mt_volt_range6[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x6f, 6250),
};

static const struct regulator_linear_range mt_volt_range7[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x60, 6250),
};

static const u32 vaud18_voltages[] = {
	1800000,
};

static const u32 vsim1_voltages[] = {
	0,
	0,
	0,
	1700000,
	1800000,
	0,
	0,
	0,
	2700000,
	0,
	0,
	3000000,
	3100000,
};

static const u32 vibr_voltages[] = {
	1200000,
	1300000,
	1500000,
	0,
	1800000,
	2000000,
	0,
	0,
	2700000,
	2800000,
	0,
	3000000,
	0,
	3300000,
};

static const u32 vrf12_voltages[] = {
	0,
	0,
	1100000,
	1200000,
	1300000,
};

static const u32 vusb_voltages[] = {
	3000000,
};

static const u32 vio18_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
};

static const u32 vcamio_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
};

static const u32 vcn18_voltages[] = {
	1800000,
};

static const u32 vfe28_voltages[] = {
	2800000,
};

static const u32 vcn13_voltages[] = {
	900000,
	1000000,
	0,
	1200000,
	1300000,
};

static const u32 vcn33_1_bt_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2800000,
	0,
	0,
	0,
	3300000,
	3400000,
	3500000,
};

static const u32 vcn33_1_wifi_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2800000,
	0,
	0,
	0,
	3300000,
	3400000,
	3500000,
};

static const u32 vaux18_voltages[] = {
	1800000,
};

static const u32 vefuse_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
	2000000,
};

static const u32 vxo22_voltages[] = {
	1800000,
	0,
	0,
	0,
	2200000,
};

static const u32 vrfck_voltages[] = {
	0,
	0,
	1500000,
	0,
	0,
	0,
	0,
	1600000,
	0,
	0,
	0,
	0,
	1700000,
};

static const u32 vbif28_voltages[] = {
	2800000,
};

static const u32 vio28_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2800000,
	2900000,
	3000000,
	3100000,
	3300000,
};

static const u32 vemc_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2900000,
	3000000,
	0,
	3300000,
};

static const u32 vcn33_2_bt_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2800000,
	0,
	0,
	0,
	3300000,
	3400000,
	3500000,
};

static const u32 vcn33_2_wifi_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	2800000,
	0,
	0,
	0,
	3300000,
	3400000,
	3500000,
};

static const u32 va12_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	1200000,
	1300000,
};

static const u32 va09_voltages[] = {
	0,
	0,
	800000,
	900000,
	0,
	0,
	1200000,
};

static const u32 vrf18_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1810000,
};

static const u32 vufs_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
};

static const u32 vm18_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
};

static const u32 vbbck_voltages[] = {
	0,
	0,
	0,
	0,
	1100000,
	0,
	0,
	0,
	1150000,
	0,
	0,
	0,
	1200000,
};

static const u32 vsim2_voltages[] = {
	0,
	0,
	0,
	1700000,
	1800000,
	0,
	0,
	0,
	2700000,
	0,
	0,
	3000000,
	3100000,
};

static int mt6359_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0;

	if (rdev->use_count == 0) {
		dev_notice(&rdev->dev, "%s:%s should not be disable.(use_count=0)\n"
			, __func__
			, rdev->desc->name);
		ret = -1;
	} else {
		ret = regulator_disable_regmap(rdev);
	}

	return ret;
}

static int mt6359_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6359_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, regval = 0;

	ret = regmap_read(rdev->regmap, info->da_vsel_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6359 regulator voltage: %d\n", ret);
		return ret;
	}

	ret = (regval >> info->da_vsel_shift) & info->da_vsel_mask;

	return ret;
}

static unsigned int mt6359_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6359_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, regval = 0;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6359 buck mode: %d\n", ret);
		return ret;
	}

	if ((regval & info->modeset_mask) >> info->modeset_shift ==
		MT6359_BUCK_MODE_FORCE_PWM)
		return REGULATOR_MODE_FAST;


	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to get mt6359 buck lp mode: %d\n", ret);
		return ret;
	}

	if (regval & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6359_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6359_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, val;
	int curr_mode;

	curr_mode = mt6359_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		if (curr_mode == REGULATOR_MODE_IDLE) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is LP mode, can't FPWM\n",
				   rdev->desc->name);
			return -EIO;
		}
		val = MT6359_BUCK_MODE_FORCE_PWM;
		val <<= info->modeset_shift;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 val);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			val = MT6359_BUCK_MODE_AUTO;
			val <<= info->modeset_shift;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 val);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			val = MT6359_BUCK_MODE_NORMAL;
			val <<= info->lp_mode_shift;
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 val);
			udelay(100);
		}
		break;
	case REGULATOR_MODE_IDLE:
		if (curr_mode == REGULATOR_MODE_FAST) {
			WARN_ON(1);
			dev_notice(&rdev->dev,
				   "BUCK %s is FPWM mode, can't enter LP\n",
				   rdev->desc->name);
			return -EIO;
		}
		val = MT6359_BUCK_MODE_LP;
		val <<= info->lp_mode_shift;
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 val);
		break;
	default:
		ret = -EINVAL;
		goto err_mode;
	}

err_mode:
	if (ret != 0) {
		dev_notice(&rdev->dev,
			"Failed to set mt6359 buck mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int mt6359_get_status(struct regulator_dev *rdev)
{
	int ret;
	u32 regval = 0;
	struct mt6359_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->da_reg, &regval);
	if (ret != 0) {
		dev_notice(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static const struct regulator_ops mt6359_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6359_regulator_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = mt6359_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6359_get_status,
	.set_mode = mt6359_regulator_set_mode,
	.get_mode = mt6359_regulator_get_mode,
};

static const struct regulator_ops mt6359_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = mt6359_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6359_get_status,
};

static const struct regulator_ops mt6359_volt_fixed_ops = {
	.enable = regulator_enable_regmap,
	.disable = mt6359_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6359_get_status,
};

/* The array is indexed by id(MT6359_ID_XXX) */
static struct mt6359_regulator_info mt6359_regulators[] = {
	MT_BUCK("buck_vs1", VS1, 800000, 2200000, 12500,
		0, mt_volt_range1, PMIC_RG_BUCK_VS1_EN_ADDR,
		PMIC_DA_VS1_EN_ADDR,
		PMIC_DA_VS1_VOSEL_ADDR,
		PMIC_DA_VS1_VOSEL_MASK,
		PMIC_DA_VS1_VOSEL_SHIFT,
		PMIC_RG_BUCK_VS1_VOSEL_ADDR,
		PMIC_RG_BUCK_VS1_VOSEL_MASK <<
		PMIC_RG_BUCK_VS1_VOSEL_SHIFT,
		MT_BUCK_VOL_EN,
		PMIC_RG_BUCK_VS1_LP_ADDR, PMIC_RG_BUCK_VS1_LP_SHIFT,
		PMIC_RG_VS1_FPWM_ADDR, PMIC_RG_VS1_FPWM_SHIFT),
	MT_BUCK("buck_vgpu11", VGPU11, 400000, 1193750, 6250,
		0, mt_volt_range2, PMIC_RG_BUCK_VGPU11_EN_ADDR,
		PMIC_DA_VGPU11_EN_ADDR,
		PMIC_DA_VGPU11_VOSEL_ADDR,
		PMIC_DA_VGPU11_VOSEL_MASK,
		PMIC_DA_VGPU11_VOSEL_SHIFT,
		PMIC_RG_BUCK_VGPU11_VOSEL_ADDR,
		PMIC_RG_BUCK_VGPU11_VOSEL_MASK <<
		PMIC_RG_BUCK_VGPU11_VOSEL_SHIFT,
		MT_BUCK_VOL_EN_MODE,
		PMIC_RG_BUCK_VGPU11_LP_ADDR, PMIC_RG_BUCK_VGPU11_LP_SHIFT,
		PMIC_RG_VGPU11_FCCM_ADDR, PMIC_RG_VGPU11_FCCM_SHIFT),
	MT_BUCK("buck_vmodem", VMODEM, 400000, 1100000, 6250,
		0, mt_volt_range3, PMIC_RG_BUCK_VMODEM_EN_ADDR,
		PMIC_DA_VMODEM_EN_ADDR,
		PMIC_DA_VMODEM_VOSEL_ADDR,
		PMIC_DA_VMODEM_VOSEL_MASK,
		PMIC_DA_VMODEM_VOSEL_SHIFT,
		PMIC_RG_BUCK_VMODEM_VOSEL_ADDR,
		PMIC_RG_BUCK_VMODEM_VOSEL_MASK <<
		PMIC_RG_BUCK_VMODEM_VOSEL_SHIFT,
		MT_BUCK_VOL_EN,
		PMIC_RG_BUCK_VMODEM_LP_ADDR, PMIC_RG_BUCK_VMODEM_LP_SHIFT,
		PMIC_RG_VMODEM_FCCM_ADDR, PMIC_RG_VMODEM_FCCM_SHIFT),
	MT_BUCK("buck_vpu", VPU, 400000, 1193750, 6250,
		0, mt_volt_range2, PMIC_RG_BUCK_VPU_EN_ADDR,
		PMIC_DA_VPU_EN_ADDR,
		PMIC_DA_VPU_VOSEL_ADDR,
		PMIC_DA_VPU_VOSEL_MASK,
		PMIC_DA_VPU_VOSEL_SHIFT,
		PMIC_RG_BUCK_VPU_VOSEL_ADDR,
		PMIC_RG_BUCK_VPU_VOSEL_MASK <<
		PMIC_RG_BUCK_VPU_VOSEL_SHIFT,
		MT_BUCK_VOL_EN_MODE,
		PMIC_RG_BUCK_VPU_LP_ADDR, PMIC_RG_BUCK_VPU_LP_SHIFT,
		PMIC_RG_VPU_FCCM_ADDR, PMIC_RG_VPU_FCCM_SHIFT),
	MT_BUCK("buck_vcore", VCORE, 400000, 1193750, 6250,
		0, mt_volt_range2, PMIC_RG_BUCK_VCORE_EN_ADDR,
		PMIC_DA_VCORE_EN_ADDR,
		PMIC_DA_VCORE_VOSEL_ADDR,
		PMIC_DA_VCORE_VOSEL_MASK,
		PMIC_DA_VCORE_VOSEL_SHIFT,
		PMIC_RG_BUCK_VCORE_VOSEL_ADDR,
		PMIC_RG_BUCK_VCORE_VOSEL_MASK <<
		PMIC_RG_BUCK_VCORE_VOSEL_SHIFT,
		MT_BUCK_VOL_EN_MODE,
		PMIC_RG_BUCK_VCORE_LP_ADDR, PMIC_RG_BUCK_VCORE_LP_SHIFT,
		PMIC_RG_VCORE_FCCM_ADDR, PMIC_RG_VCORE_FCCM_SHIFT),
	MT_BUCK("buck_vs2", VS2, 800000, 1600000, 12500,
		0, mt_volt_range4, PMIC_RG_BUCK_VS2_EN_ADDR,
		PMIC_DA_VS2_EN_ADDR,
		PMIC_DA_VS2_VOSEL_ADDR,
		PMIC_DA_VS2_VOSEL_MASK,
		PMIC_DA_VS2_VOSEL_SHIFT,
		PMIC_RG_BUCK_VS2_VOSEL_ADDR,
		PMIC_RG_BUCK_VS2_VOSEL_MASK <<
		PMIC_RG_BUCK_VS2_VOSEL_SHIFT,
		MT_BUCK_VOL_EN,
		PMIC_RG_BUCK_VS2_LP_ADDR, PMIC_RG_BUCK_VS2_LP_SHIFT,
		PMIC_RG_VS2_FPWM_ADDR, PMIC_RG_VS2_FPWM_SHIFT),
	MT_BUCK("buck_vpa", VPA, 500000, 3650000, 50000,
		0, mt_volt_range5, PMIC_RG_BUCK_VPA_EN_ADDR,
		PMIC_DA_VPA_EN_ADDR,
		PMIC_DA_VPA_VOSEL_ADDR,
		PMIC_DA_VPA_VOSEL_MASK,
		PMIC_DA_VPA_VOSEL_SHIFT,
		PMIC_RG_BUCK_VPA_VOSEL_ADDR,
		PMIC_RG_BUCK_VPA_VOSEL_MASK <<
		PMIC_RG_BUCK_VPA_VOSEL_SHIFT,
		MT_BUCK_VOL_EN,
		PMIC_RG_BUCK_VPA_LP_ADDR, PMIC_RG_BUCK_VPA_LP_SHIFT,
		PMIC_RG_VPA_MODESET_ADDR, PMIC_RG_VPA_MODESET_SHIFT),
	MT_BUCK("buck_vproc2", VPROC2, 400000, 1193750, 6250,
		0, mt_volt_range2, PMIC_RG_BUCK_VPROC2_EN_ADDR,
		PMIC_DA_VPROC2_EN_ADDR,
		PMIC_DA_VPROC2_VOSEL_ADDR,
		PMIC_DA_VPROC2_VOSEL_MASK,
		PMIC_DA_VPROC2_VOSEL_SHIFT,
		PMIC_RG_BUCK_VPROC2_VOSEL_ADDR,
		PMIC_RG_BUCK_VPROC2_VOSEL_MASK <<
		PMIC_RG_BUCK_VPROC2_VOSEL_SHIFT,
		MT_BUCK_VOL_EN_MODE,
		PMIC_RG_BUCK_VPROC2_LP_ADDR, PMIC_RG_BUCK_VPROC2_LP_SHIFT,
		PMIC_RG_VPROC2_FCCM_ADDR, PMIC_RG_VPROC2_FCCM_SHIFT),
	MT_BUCK("buck_vproc1", VPROC1, 400000, 1193750, 6250,
		0, mt_volt_range2, PMIC_RG_BUCK_VPROC1_EN_ADDR,
		PMIC_DA_VPROC1_EN_ADDR,
		PMIC_DA_VPROC1_VOSEL_ADDR,
		PMIC_DA_VPROC1_VOSEL_MASK,
		PMIC_DA_VPROC1_VOSEL_SHIFT,
		PMIC_RG_BUCK_VPROC1_VOSEL_ADDR,
		PMIC_RG_BUCK_VPROC1_VOSEL_MASK <<
		PMIC_RG_BUCK_VPROC1_VOSEL_SHIFT,
		MT_BUCK_VOL_EN_MODE,
		PMIC_RG_BUCK_VPROC1_LP_ADDR, PMIC_RG_BUCK_VPROC1_LP_SHIFT,
		PMIC_RG_VPROC1_FCCM_ADDR, PMIC_RG_VPROC1_FCCM_SHIFT),

	MT_REG_FIXED("ldo_vaud18", VAUD18, PMIC_RG_LDO_VAUD18_EN_ADDR,
		PMIC_DA_VAUD18_B_EN_ADDR,
		1800000, MT_LDO_EN),
	MT_LDO_NON_REGULAR("ldo_vsim1", VSIM1,
		vsim1_voltages,
		PMIC_RG_LDO_VSIM1_EN_ADDR,
		PMIC_RG_LDO_VSIM1_EN_SHIFT,
		PMIC_DA_VSIM1_B_EN_ADDR,
		PMIC_RG_VSIM1_VOSEL_ADDR,
		PMIC_RG_VSIM1_VOSEL_MASK <<
		PMIC_RG_VSIM1_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vibr", VIBR,
		vibr_voltages,
		PMIC_RG_LDO_VIBR_EN_ADDR,
		PMIC_RG_LDO_VIBR_EN_SHIFT,
		PMIC_DA_VIBR_B_EN_ADDR,
		PMIC_RG_VIBR_VOSEL_ADDR,
		PMIC_RG_VIBR_VOSEL_MASK <<
		PMIC_RG_VIBR_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vrf12", VRF12,
		vrf12_voltages,
		PMIC_RG_LDO_VRF12_EN_ADDR,
		PMIC_RG_LDO_VRF12_EN_SHIFT,
		PMIC_DA_VRF12_B_EN_ADDR,
		PMIC_RG_VRF12_VOSEL_ADDR,
		PMIC_RG_VRF12_VOSEL_MASK <<
		PMIC_RG_VRF12_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_REG_FIXED("ldo_vusb", VUSB, PMIC_RG_LDO_VUSB_EN_0_ADDR,
		PMIC_DA_VUSB_B_EN_ADDR,
		3000000, MT_LDO_EN),
	MT_LDO_REGULAR("ldo_vsram_proc2", VSRAM_PROC2, 500000, 1193750, 6250,
		0, mt_volt_range6, PMIC_RG_LDO_VSRAM_PROC2_EN_ADDR,
		PMIC_DA_VSRAM_PROC2_B_EN_ADDR,
		PMIC_DA_VSRAM_PROC2_VOSEL_ADDR,
		PMIC_DA_VSRAM_PROC2_VOSEL_MASK,
		PMIC_DA_VSRAM_PROC2_VOSEL_SHIFT,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_ADDR,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_MASK <<
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vio18", VIO18,
		vio18_voltages,
		PMIC_RG_LDO_VIO18_EN_ADDR,
		PMIC_RG_LDO_VIO18_EN_SHIFT,
		PMIC_DA_VIO18_B_EN_ADDR,
		PMIC_RG_VIO18_VOSEL_ADDR,
		PMIC_RG_VIO18_VOSEL_MASK <<
		PMIC_RG_VIO18_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vcamio", VCAMIO,
		vcamio_voltages,
		PMIC_RG_LDO_VCAMIO_EN_ADDR,
		PMIC_RG_LDO_VCAMIO_EN_SHIFT,
		PMIC_DA_VCAMIO_B_EN_ADDR,
		PMIC_RG_VCAMIO_VOSEL_ADDR,
		PMIC_RG_VCAMIO_VOSEL_MASK <<
		PMIC_RG_VCAMIO_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_REG_FIXED("ldo_vcn18", VCN18, PMIC_RG_LDO_VCN18_EN_ADDR,
		PMIC_DA_VCN18_B_EN_ADDR,
		1800000, MT_LDO_EN),
	MT_REG_FIXED("ldo_vfe28", VFE28, PMIC_RG_LDO_VFE28_EN_ADDR,
		PMIC_DA_VFE28_B_EN_ADDR,
		2800000, MT_LDO_EN),
	MT_LDO_NON_REGULAR("ldo_vcn13", VCN13,
		vcn13_voltages,
		PMIC_RG_LDO_VCN13_EN_ADDR,
		PMIC_RG_LDO_VCN13_EN_SHIFT,
		PMIC_DA_VCN13_B_EN_ADDR,
		PMIC_RG_VCN13_VOSEL_ADDR,
		PMIC_RG_VCN13_VOSEL_MASK <<
		PMIC_RG_VCN13_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vcn33_1_bt", VCN33_1_BT,
		vcn33_1_bt_voltages,
		PMIC_RG_LDO_VCN33_1_EN_0_ADDR,
		PMIC_RG_LDO_VCN33_1_EN_0_SHIFT,
		PMIC_DA_VCN33_1_B_EN_ADDR,
		PMIC_RG_VCN33_1_VOSEL_ADDR,
		PMIC_RG_VCN33_1_VOSEL_MASK <<
		PMIC_RG_VCN33_1_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vcn33_1_wifi", VCN33_1_WIFI,
		vcn33_1_wifi_voltages,
		PMIC_RG_LDO_VCN33_1_EN_1_ADDR,
		PMIC_RG_LDO_VCN33_1_EN_1_SHIFT,
		PMIC_DA_VCN33_1_B_EN_ADDR,
		PMIC_RG_VCN33_1_VOSEL_ADDR,
		PMIC_RG_VCN33_1_VOSEL_MASK <<
		PMIC_RG_VCN33_1_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_REG_FIXED("ldo_vaux18", VAUX18, PMIC_RG_LDO_VAUX18_EN_ADDR,
		PMIC_DA_VAUX18_B_EN_ADDR,
		1800000, MT_LDO_EN),
	MT_LDO_REGULAR("ldo_vsram_others", VSRAM_OTHERS, 500000, 1193750, 6250,
		0, mt_volt_range6, PMIC_RG_LDO_VSRAM_OTHERS_EN_ADDR,
		PMIC_DA_VSRAM_OTHERS_B_EN_ADDR,
		PMIC_DA_VSRAM_OTHERS_VOSEL_ADDR,
		PMIC_DA_VSRAM_OTHERS_VOSEL_MASK,
		PMIC_DA_VSRAM_OTHERS_VOSEL_SHIFT,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_MASK <<
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vefuse", VEFUSE,
		vefuse_voltages,
		PMIC_RG_LDO_VEFUSE_EN_ADDR,
		PMIC_RG_LDO_VEFUSE_EN_SHIFT,
		PMIC_DA_VEFUSE_B_EN_ADDR,
		PMIC_RG_VEFUSE_VOSEL_ADDR,
		PMIC_RG_VEFUSE_VOSEL_MASK <<
		PMIC_RG_VEFUSE_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vxo22", VXO22,
		vxo22_voltages,
		PMIC_RG_LDO_VXO22_EN_ADDR,
		PMIC_RG_LDO_VXO22_EN_SHIFT,
		PMIC_DA_VXO22_B_EN_ADDR,
		PMIC_RG_VXO22_VOSEL_ADDR,
		PMIC_RG_VXO22_VOSEL_MASK <<
		PMIC_RG_VXO22_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vrfck", VRFCK,
		vrfck_voltages,
		PMIC_RG_LDO_VRFCK_EN_ADDR,
		PMIC_RG_LDO_VRFCK_EN_SHIFT,
		PMIC_DA_VRFCK_B_EN_ADDR,
		PMIC_RG_VRFCK_VOSEL_ADDR,
		PMIC_RG_VRFCK_VOSEL_MASK <<
		PMIC_RG_VRFCK_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_REG_FIXED("ldo_vbif28", VBIF28, PMIC_RG_LDO_VBIF28_EN_ADDR,
		PMIC_DA_VBIF28_B_EN_ADDR,
		2800000, MT_LDO_EN),
	MT_LDO_NON_REGULAR("ldo_vio28", VIO28,
		vio28_voltages,
		PMIC_RG_LDO_VIO28_EN_ADDR,
		PMIC_RG_LDO_VIO28_EN_SHIFT,
		PMIC_DA_VIO28_B_EN_ADDR,
		PMIC_RG_VIO28_VOSEL_ADDR,
		PMIC_RG_VIO28_VOSEL_MASK <<
		PMIC_RG_VIO28_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vemc", VEMC,
		vemc_voltages,
		PMIC_RG_LDO_VEMC_EN_ADDR,
		PMIC_RG_LDO_VEMC_EN_SHIFT,
		PMIC_DA_VEMC_B_EN_ADDR,
		PMIC_RG_VEMC_VOSEL_ADDR,
		PMIC_RG_VEMC_VOSEL_MASK <<
		PMIC_RG_VEMC_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vcn33_2_bt", VCN33_2_BT,
		vcn33_2_bt_voltages,
		PMIC_RG_LDO_VCN33_2_EN_0_ADDR,
		PMIC_RG_LDO_VCN33_2_EN_0_SHIFT,
		PMIC_DA_VCN33_2_B_EN_ADDR,
		PMIC_RG_VCN33_2_VOSEL_ADDR,
		PMIC_RG_VCN33_2_VOSEL_MASK <<
		PMIC_RG_VCN33_2_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vcn33_2_wifi", VCN33_2_WIFI,
		vcn33_2_wifi_voltages,
		PMIC_RG_LDO_VCN33_2_EN_1_ADDR,
		PMIC_RG_LDO_VCN33_2_EN_1_SHIFT,
		PMIC_DA_VCN33_2_B_EN_ADDR,
		PMIC_RG_VCN33_2_VOSEL_ADDR,
		PMIC_RG_VCN33_2_VOSEL_MASK <<
		PMIC_RG_VCN33_2_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_va12", VA12,
		va12_voltages,
		PMIC_RG_LDO_VA12_EN_ADDR,
		PMIC_RG_LDO_VA12_EN_SHIFT,
		PMIC_DA_VA12_B_EN_ADDR,
		PMIC_RG_VA12_VOSEL_ADDR,
		PMIC_RG_VA12_VOSEL_MASK <<
		PMIC_RG_VA12_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_va09", VA09,
		va09_voltages,
		PMIC_RG_LDO_VA09_EN_ADDR,
		PMIC_RG_LDO_VA09_EN_SHIFT,
		PMIC_DA_VA09_B_EN_ADDR,
		PMIC_RG_VA09_VOSEL_ADDR,
		PMIC_RG_VA09_VOSEL_MASK <<
		PMIC_RG_VA09_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vrf18", VRF18,
		vrf18_voltages,
		PMIC_RG_LDO_VRF18_EN_ADDR,
		PMIC_RG_LDO_VRF18_EN_SHIFT,
		PMIC_DA_VRF18_B_EN_ADDR,
		PMIC_RG_VRF18_VOSEL_ADDR,
		PMIC_RG_VRF18_VOSEL_MASK <<
		PMIC_RG_VRF18_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_REGULAR("ldo_vsram_md", VSRAM_MD, 500000, 1100000, 6250,
		0, mt_volt_range7, PMIC_RG_LDO_VSRAM_MD_EN_ADDR,
		PMIC_DA_VSRAM_MD_B_EN_ADDR,
		PMIC_DA_VSRAM_MD_VOSEL_ADDR,
		PMIC_DA_VSRAM_MD_VOSEL_MASK,
		PMIC_DA_VSRAM_MD_VOSEL_SHIFT,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_ADDR,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_MASK <<
		PMIC_RG_LDO_VSRAM_MD_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vufs", VUFS,
		vufs_voltages,
		PMIC_RG_LDO_VUFS_EN_ADDR,
		PMIC_RG_LDO_VUFS_EN_SHIFT,
		PMIC_DA_VUFS_B_EN_ADDR,
		PMIC_RG_VUFS_VOSEL_ADDR,
		PMIC_RG_VUFS_VOSEL_MASK <<
		PMIC_RG_VUFS_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vm18", VM18,
		vm18_voltages,
		PMIC_RG_LDO_VM18_EN_ADDR,
		PMIC_RG_LDO_VM18_EN_SHIFT,
		PMIC_DA_VM18_B_EN_ADDR,
		PMIC_RG_VM18_VOSEL_ADDR,
		PMIC_RG_VM18_VOSEL_MASK <<
		PMIC_RG_VM18_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vbbck", VBBCK,
		vbbck_voltages,
		PMIC_RG_LDO_VBBCK_EN_ADDR,
		PMIC_RG_LDO_VBBCK_EN_SHIFT,
		PMIC_DA_VBBCK_B_EN_ADDR,
		PMIC_RG_VBBCK_VOSEL_ADDR,
		PMIC_RG_VBBCK_VOSEL_MASK <<
		PMIC_RG_VBBCK_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_REGULAR("ldo_vsram_proc1", VSRAM_PROC1, 500000, 1193750, 6250,
		0, mt_volt_range6, PMIC_RG_LDO_VSRAM_PROC1_EN_ADDR,
		PMIC_DA_VSRAM_PROC1_B_EN_ADDR,
		PMIC_DA_VSRAM_PROC1_VOSEL_ADDR,
		PMIC_DA_VSRAM_PROC1_VOSEL_MASK,
		PMIC_DA_VSRAM_PROC1_VOSEL_SHIFT,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_ADDR,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_MASK <<
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
	MT_LDO_NON_REGULAR("ldo_vsim2", VSIM2,
		vsim2_voltages,
		PMIC_RG_LDO_VSIM2_EN_ADDR,
		PMIC_RG_LDO_VSIM2_EN_SHIFT,
		PMIC_DA_VSIM2_B_EN_ADDR,
		PMIC_RG_VSIM2_VOSEL_ADDR,
		PMIC_RG_VSIM2_VOSEL_MASK <<
		PMIC_RG_VSIM2_VOSEL_SHIFT,
		MT_LDO_VOL_EN),
};

static const struct mt_regulator_init_data mt6359_regulator_init_data = {
	.id = MT6359_SWCID,
	.size = MT6359_MAX_REGULATOR,
	.regulator_info = &mt6359_regulators[0],
};

static const struct platform_device_id mt6359_platform_ids[] = {
	{"mt6359-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6359_platform_ids);

static const struct of_device_id mt6359_of_match[] = {
	{
		.compatible = "mediatek,mt6359-regulator",
		.data = &mt6359_regulator_init_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mt6359_of_match);

static int mt6359_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct mt6358_chip *mt6358 = dev_get_drvdata(pdev->dev.parent);
	struct mt6359_regulator_info *mt_regulators;
	struct mt_regulator_init_data *regulator_init_data;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regulation_constraints *c;
	int i;
	u32 reg_value = 0;

	of_id = of_match_device(mt6359_of_match, &pdev->dev);
	if (!of_id || !of_id->data)
		return -ENODEV;
	regulator_init_data = (struct mt_regulator_init_data *)of_id->data;
	mt_regulators = regulator_init_data->regulator_info;

	/* Read PMIC chip revision to update constraints and voltage table */
	if (regmap_read(mt6358->regmap, MT6359_SWCID, &reg_value) < 0) {
		dev_notice(&pdev->dev, "Failed to read Chip ID\n");
		return -EIO;
	}
	dev_info(&pdev->dev, "Chip ID = 0x%x\n", reg_value);

	for (i = 0; i < regulator_init_data->size; i++) {
		config.dev = &pdev->dev;
		config.driver_data = (mt_regulators + i);
		config.regmap = mt6358->regmap;
		rdev = devm_regulator_register(&pdev->dev,
				&(mt_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_notice(&pdev->dev, "failed to register %s\n",
				(mt_regulators + i)->desc.name);
			return PTR_ERR(rdev);
		}

		c = rdev->constraints;
		c->valid_ops_mask |=
			(mt_regulators + i)->constraints.valid_ops_mask;
		c->valid_modes_mask |=
			(mt_regulators + i)->constraints.valid_modes_mask;
	}

	return 0;
}

static struct platform_driver mt6359_regulator_driver = {
	.driver = {
		.name = "mt6359-regulator",
		.of_match_table = of_match_ptr(mt6359_of_match),
	},
	.probe = mt6359_regulator_probe,
	.id_table = mt6359_platform_ids,
};

module_platform_driver(mt6359_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6359 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt6359-regulator");
