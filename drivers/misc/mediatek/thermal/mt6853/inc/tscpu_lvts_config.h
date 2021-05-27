/*
 * Copyright (C) 2018 MediaTek Inc.
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
#ifndef __TSCPU_LVTS_SETTINGS_H__
#define __TSCPU_LVTS_SETTINGS_H__

/*=============================================================
 * Chip related
 *=============================================================
 */
/* chip dependent */
/* TODO: change to new reg addr. */
#define LVTS_ADDRESS_INDEX_1	116 /* 0x11C101C0 */
#define LVTS_ADDRESS_INDEX_2	117 /* 0x11C101C4 */
#define LVTS_ADDRESS_INDEX_3	118 /* 0x11C101C8 */
#define LVTS_ADDRESS_INDEX_4	119 /* 0x11C101CC */
#define LVTS_ADDRESS_INDEX_5	190 /* 0x11C101D0 */
#define LVTS_ADDRESS_INDEX_6	191 /* 0x11C101D4 */
#define LVTS_ADDRESS_INDEX_7	192 /* 0x11C101D8 */
#define LVTS_ADDRESS_INDEX_8	193 /* 0x11C101DC */
#define LVTS_ADDRESS_INDEX_9	194 /* 0x11C101E0 */
#define LVTS_ADDRESS_INDEX_10	195 /* 0x11C101E4 */
#define LVTS_ADDRESS_INDEX_11	196 /* 0x11C101E8 */
#define LVTS_ADDRESS_INDEX_12	197 /* 0x11C101EC */
#define LVTS_ADDRESS_INDEX_13	198 /* 0x11C101F0 */
#define LVTS_ADDRESS_INDEX_14	199 /* 0x11C101F4 */
#define LVTS_ADDRESS_INDEX_15	200 /* 0x11C101F8 */
#define LVTS_ADDRESS_INDEX_16	201 /* 0x11C101FC */
#define LVTS_ADDRESS_INDEX_17	202 /* 0x11C10200 */
#define LVTS_ADDRESS_INDEX_18	203 /* 0x11C10204 */
#define LVTS_ADDRESS_INDEX_19	204 /* 0x11C10208 */
#define LVTS_ADDRESS_INDEX_20	205 /* 0x11C1020C */
#define LVTS_ADDRESS_INDEX_21	206 /* 0x11C10210 */
#define LVTS_ADDRESS_INDEX_22	207 /* 0x11C10214 */




/**************************************************************************** */
/* LVTS related registers. */
/**************************************************************************** */
#define LVTSMONCTL0_0	(THERM_CTRL_BASE_2 + 0x000)
#define LVTSMONCTL1_0	(THERM_CTRL_BASE_2 + 0x004)
#define LVTSMONCTL2_0	(THERM_CTRL_BASE_2 + 0x008)
#define LVTSMONINT_0	(THERM_CTRL_BASE_2 + 0x00C)
#define LVTSMONINTSTS_0	(THERM_CTRL_BASE_2 + 0x010)
#define LVTSMONIDET0_0	(THERM_CTRL_BASE_2 + 0x014)
#define LVTSMONIDET1_0	(THERM_CTRL_BASE_2 + 0x018)
#define LVTSMONIDET2_0	(THERM_CTRL_BASE_2 + 0x01C)
#define LVTSMONIDET3_0	(THERM_CTRL_BASE_2 + 0x020)
#define LVTSH2NTHRE_0	(THERM_CTRL_BASE_2 + 0x024)
#define LVTSHTHRE_0	(THERM_CTRL_BASE_2 + 0x028)
#define LVTSCTHRE_0	(THERM_CTRL_BASE_2 + 0x02C)
#define LVTSOFFSETH_0	(THERM_CTRL_BASE_2 + 0x030)
#define LVTSOFFSETL_0	(THERM_CTRL_BASE_2 + 0x034)
#define LVTSMSRCTL0_0	(THERM_CTRL_BASE_2 + 0x038)
#define LVTSMSRCTL1_0	(THERM_CTRL_BASE_2 + 0x03C)
#define LVTSTSSEL_0     (THERM_CTRL_BASE_2 + 0x040)
#define LVTSDEVICETO_0	(THERM_CTRL_BASE_2 + 0x044)
#define LVTSCALSCALE_0	(THERM_CTRL_BASE_2 + 0x048)
#define LVTS_ID_0	(THERM_CTRL_BASE_2 + 0x04C)
#define LVTS_CONFIG_0	(THERM_CTRL_BASE_2 + 0x050)
#define LVTSEDATA00_0	(THERM_CTRL_BASE_2 + 0x054)
#define LVTSEDATA01_0	(THERM_CTRL_BASE_2 + 0x058)
#define LVTSEDATA02_0	(THERM_CTRL_BASE_2 + 0x05C)
#define LVTSEDATA03_0	(THERM_CTRL_BASE_2 + 0x060)
#define LVTSMSR0_0	(THERM_CTRL_BASE_2 + 0x090)
#define LVTSMSR1_0	(THERM_CTRL_BASE_2 + 0x094)
#define LVTSMSR2_0	(THERM_CTRL_BASE_2 + 0x098)
#define LVTSMSR3_0	(THERM_CTRL_BASE_2 + 0x09C)
#define LVTSIMMD0_0	(THERM_CTRL_BASE_2 + 0x0A0)
#define LVTSIMMD1_0	(THERM_CTRL_BASE_2 + 0x0A4)
#define LVTSIMMD2_0	(THERM_CTRL_BASE_2 + 0x0A8)
#define LVTSIMMD3_0	(THERM_CTRL_BASE_2 + 0x0AC)
#define LVTSRDATA0_0	(THERM_CTRL_BASE_2 + 0x0B0)
#define LVTSRDATA1_0	(THERM_CTRL_BASE_2 + 0x0B4)
#define LVTSRDATA2_0	(THERM_CTRL_BASE_2 + 0x0B8)
#define LVTSRDATA3_0	(THERM_CTRL_BASE_2 + 0x0BC)
#define LVTSPROTCTL_0	(THERM_CTRL_BASE_2 + 0x0C0)
#define LVTSPROTTA_0	(THERM_CTRL_BASE_2 + 0x0C4)
#define LVTSPROTTB_0	(THERM_CTRL_BASE_2 + 0x0C8)
#define LVTSPROTTC_0	(THERM_CTRL_BASE_2 + 0x0CC)
#define LVTSCLKEN_0	(THERM_CTRL_BASE_2 + 0x0E4)
#define LVTSDBGSEL_0	(THERM_CTRL_BASE_2 + 0x0E8)
#define LVTSDBGSIG_0	(THERM_CTRL_BASE_2 + 0x0EC)
#define LVTSSPARE0_0	(THERM_CTRL_BASE_2 + 0x0F0)
#define LVTSSPARE1_0	(THERM_CTRL_BASE_2 + 0x0F4)
#define LVTSSPARE2_0	(THERM_CTRL_BASE_2 + 0x0F8)
#define LVTSSPARE3_0	(THERM_CTRL_BASE_2 + 0x0FC)

#define THERMINTST	(THERM_CTRL_BASE_2 + 0xF04)

#endif	/* __TSCPU_LVTS_SETTINGS_H__ */
