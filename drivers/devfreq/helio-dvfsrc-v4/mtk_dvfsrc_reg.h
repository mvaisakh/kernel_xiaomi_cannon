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

#ifndef __MTK_DVFSRC_REG_H
#define __MTK_DVFSRC_REG_H

#if defined(CONFIG_MACH_MT6775)

#include "mtk_dvfsrc_reg_mt6775.h"

#elif defined(CONFIG_MACH_MT6771)

#include "mtk_dvfsrc_reg_mt6771.h"

#elif defined(CONFIG_MACH_MT8168)

#include "mtk_dvfsrc_reg_mt8168.h"

#endif

#endif /* __MTK_DVFSRC_REG_H */


