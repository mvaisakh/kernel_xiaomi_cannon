/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6833_H__
#define __MT6833_H__

#include <linux/delay.h>
#include <mtk_lpm_type.h>
#include <mt6833_common.h>

int mt6833_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req);

int mt6833_do_mcusys_prepare_on_ex(unsigned int clr_status);

int mt6833_do_mcusys_prepare_on(void);

#endif
