/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __I2C_CHARGER_RT9458_H
#define __I2C_CHARGER_RT9458_H

struct rt9458_platform_data {
	const char *chg_name;
	u32 ichg;
	u32 aicr;
	u32 mivr;
	u32 ieoc;
	u32 voreg;
	u32 vmreg;
	int intr_gpio;
	u8 enable_te:1;
	u8 enable_eoc_shdn:1;
};

enum rt9458_chg_stat {
	RT9458_CHG_STAT_READY = 0,
	RT9458_CHG_STAT_PROGRESS,
	RT9458_CHG_STAT_DONE,
	RT9458_CHG_STAT_FAULT,
	RT9458_CHG_STAT_MAX,
};

static const char *rt9458_chg_stat_name[RT9458_CHG_STAT_MAX] = {
	"ready", "progress", "done", "fault",
};

#define RT9458_REG_CTRL1	(0x00)
#define RT9458_REG_CTRL2	(0x01)
#define RT9458_REG_CTRL3	(0x02)
#define RT9458_REG_DEVID	(0x03)
#define RT9458_REG_CTRL4	(0x04)
#define RT9458_REG_CTRL5	(0x05)
#define RT9458_REG_CTRL6	(0x06)
#define RT9458_REG_CTRL7	(0x07)
#define RT9458_REG_IRQ1		(0x08)
#define RT9458_REG_IRQ2		(0x09)
#define RT9458_REG_IRQ3		(0x0A)
#define RT9458_REG_MASK1	(0x0B)
#define RT9458_REG_MASK2	(0x0C)
#define RT9458_REG_MASK3	(0x0D)
#define RT9458_REG_CTRL8	(0x11)

#define RT9458_IRQ_REGNUM (RT9458_REG_IRQ3 - RT9458_REG_IRQ1 + 1)
enum {
	RT9458_IRQ_BATAB = 0,
	RT9458_IRQ_VINOVPI = 6,
	RT9458_IRQ_TSDI,
	RT9458_IRQ_CHMIVRI = 8,
	RT9458_IRQ_CHTREGI,
	RT9458_IRQ_CH32MI,
	RT9458_IRQ_CHRCHGI,
	RT9458_IRQ_CHTERMI,
	RT9458_IRQ_CHBATOVI,
	RT9458_IRQ_CHRVPI = 15,
	RT9458_IRQ_BST32SI = 19,
	RT9458_IRQ_BSTLOWVI = 21,
	RT9458_IRQ_BSTOLI,
	RT9458_IRQ_BSTVINOVI,
	RT9458_IRQ_MAX,
};

/* RT9458_REG_CTRL1 : 0x00 */
#define RT9458_STAT_MASK	(0x30)
#define RT9458_STAT_SHFT	(4)
/* RT9458_REG_CTRL2 : 0x01 */
#define RT9458_OPA_MASK		(0x01)
#define RT9458_TESHDN_MASK	(0x20)
#define RT9458_TERM_MASK	(0x08)
#define RT9458_IAICR_MASK	(0xc0)
#define RT9458_IAICR_SHFT	(6)
#define RT9458_IAICRINT_MASK	(0x04)
#define RT9458_HZ_MASK		(0x02)

/* RT9458_REG_CTRL3 : 0x02 */
#define RT9458_VOREG_MASK	(0xfc)
#define RT9458_VOREG_SHFT	(2)
#define RT9458_VOREG_MAXVAL	(0x2f)

/* RT9458_REG_CTRL5 : 0x05 */
#define RT9458_TMREN_MASK	(0x80)
#define RT9458_IEOC_MASK	(0x07)
#define RT9458_IEOC_SHFT	(0)
#define RT9458_IEOC_MAXVAL	(7)

/* RT9458_REG_CTRL6 : 0x06 */
#define RT9458_IAICRSEL_MASK	(0x80)
#define RT9458_ICHG_MASK	(0x70)
#define RT9458_ICHG_SHFT	(4)
#define RT9458_ICHG_MAXVAL	(7)

/* RT9458_REG_CTRL7 : 0x07 */
#define RT9458_CHGEN_MASK	(0x10)
#define RT9458_VMREG_MASK	(0x0f)
#define RT9458_VMREG_SHFT	(0)
#define RT9458_VMREG_MAXVAL	(0x0c)

/* RT9458_REG_CTRL8 : 0x11 */
#define RT9458_MIVR_MASK	(0x70)
#define RT9458_MIVR_SHFT	(4)
#define RT9458_MIVR_MAXVAL	(6)
#endif /* #ifndef __I2C_CHARGER_RT9458_H */
