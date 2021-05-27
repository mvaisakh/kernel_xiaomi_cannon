/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "apu_power_table.h"

// FIXME: update vpu power table in DVT stage
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 212},
	{APU_OPP_1, 176},
	{APU_OPP_2, 133},
	{APU_OPP_3, 98},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(vpu_power_table);

// FIXME: update mdla power table in DVT stage
/*  opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 0},
	{APU_OPP_1, 0},
	{APU_OPP_2, 0},
	{APU_OPP_3, 0},
	{APU_OPP_4, 0},
};
EXPORT_SYMBOL(mdla_power_table);
