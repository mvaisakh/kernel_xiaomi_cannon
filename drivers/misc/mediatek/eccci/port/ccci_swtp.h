/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
//BSP.System - 2020.11.29 - add key value begin
#define KEY_TABLE0		249
#define KEY_TABLE1		250
//BSP.System - 2020.11.29 - add key value begin
#ifndef __SWTP_H__
#define __SWTP_H__

/* modify MAX_PIN_NUM/DTS to support more gpio,
 * need to follow SOP for customization.
 */
#define MAX_PIN_NUM 1
#define SWTP_COMPATIBLE_DEVICE_ID "mediatek, swtp-eint"
#define SWTP1_COMPATIBLE_DEVICE_ID "mediatek, swtp1-eint"


#define SWTP_EINT_PIN_PLUG_IN	(1)
#define SWTP_EINT_PIN_PLUG_OUT	(0)

#define SWTP_DO_TX_POWER	(0)
#define SWTP_NO_TX_POWER	(1)

struct swtp_t {
	unsigned int	md_id;
	unsigned int	irq[MAX_PIN_NUM];
	unsigned int	gpiopin[MAX_PIN_NUM];
	unsigned int	setdebounce[MAX_PIN_NUM];
	unsigned int	eint_type[MAX_PIN_NUM];
	unsigned int	gpio_state[MAX_PIN_NUM];
	int	tx_power_mode;
	spinlock_t		spinlock;
	struct delayed_work delayed_work;
};
/*****************************************************************************/
/* External API Region called by ccci_swtp object */
/*****************************************************************************/
extern int swtp_init(int md_id);
#endif				/* __SWTP_H__ */
