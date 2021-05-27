/*
 *  Driver for Goodix Touchscreens
 *
 *  Copyright (c) 2014 Red Hat Inc.
 *  Copyright (c) 2015 K. Merker <merker@debian.org>
 *
 *  This code is based on gt9xx.c authored by andrew@goodix.com:
 *
 *  2010 - 2012 Goodix Technology.
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 of the License.
 */
#ifndef _GT9XX_CONFIG_H_
#define _GT9XX_CONFIG_H_

/*
 ***************************PART2:TODO
 * define**********************************
 * STEP_1(REQUIRED):Change config table.
 * TODO: puts the config info corresponded to your TP here,
 * the following is just
 * a sample config, send this config should cause the chip cannot work normally
 */
#define CTP_CFG_GROUP1                                                         \
	{                                                                      \
		0x41, 0x20, 0x03, 0x00, 0x05, 0x05, 0x38, 0x06, 0x01, 0x0F,    \
			0x28, 0x07, 0x55, 0x37, 0x03, 0x05, 0x00, 0x00, 0x00,  \
			0x00, 0x11, 0x11, 0x05, 0x0C, 0x0E, 0x0F, 0x0B, 0x8C,  \
			0x2E, 0x0E, 0x2F, 0x2D, 0x7F, 0x08, 0x00, 0x00, 0x00,  \
			0x9A, 0x02, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x5A, 0x94, 0xC5,  \
			0x02, 0x05, 0x00, 0x00, 0x04, 0xD9, 0x27, 0x00, 0xB6,  \
			0x2F, 0x00, 0x99, 0x39, 0x00, 0x85, 0x44, 0x00, 0x73,  \
			0x52, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x1A, 0x0D, 0x03, 0x06, 0x00, 0x21, 0x43, 0x06, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C,  \
			0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x18, 0x1C,  \
			0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x24, 0x26, 0x28,  \
			0x29, 0x2A, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C,  \
			0x0F, 0x10, 0x12, 0x13, 0x14, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
			0x00, 0x00, 0x00, 0x6E, 0x01,                          \
	}

/* default config for K82 */

#define CTP_CFG_GROUP1_CHARGER                                                 \
	{                                                                      \
	}

/* TODO puts your group2 config info here,if need. */
#define CTP_CFG_GROUP2                                                         \
	{                                                                      \
	}

/* TODO puts your group2 config info here,if need. */
#define CTP_CFG_GROUP2_CHARGER                                                 \
	{                                                                      \
	}

/* TODO puts your group3 config info here,if need. */
#define CTP_CFG_GROUP3                                                         \
	{                                                                      \
	}

/* TODO puts your group3 config info here,if need. */
#define CTP_CFG_GROUP3_CHARGER                                                 \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 3 here, if needed */
#define CTP_CFG_GROUP4                                                         \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 4 here, if needed */
#define CTP_CFG_GROUP5                                                         \
	{                                                                      \
	}

/* TODO: define your config for Sensor_ID == 5 here, if needed */
#define CTP_CFG_GROUP6                                                         \
	{                                                                      \
	}

#endif /* _GT9XX_CONFIG_H_ */
