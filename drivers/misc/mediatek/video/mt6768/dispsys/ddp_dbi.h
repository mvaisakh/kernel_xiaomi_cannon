/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __DBI_DRV_H__
#define __DBI_DRV_H__



#include "lcm_drv.h"
#include "ddp_hal.h"
#include "fbconfig_kdebug.h"
#include "disp_drv_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UINT8 unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
#define INT32	int

/* -------------------------------------------------------------------------- */

#define DBI_CHECK_RET(expr) \
		do { \
			DBI_STATUS ret = (expr); \
			ASSERT(ret == DBI_STATUS_OK); \
		} while (0)

/* -------------------------------------------------------------------------- */

	enum DBI_STATUS {
		DBI_STATUS_OK = 0,
		DBI_STATUS_ERROR,
	};

	enum DBI_STATE {
		DBI_STATE_IDLE = 0,
		DBI_STATE_BUSY,
		DBI_STATE_POWER_OFF,
	};

	enum DBI_IF_ID {
		DBI_IF_PARALLEL_0 = 0,
		DBI_IF_PARALLEL_1 = 1,
		DBI_IF_PARALLEL_2 = 2,
		DBI_IF_SERIAL_0 = 3,
		DBI_IF_SERIAL_1 = 4,

		DBI_IF_ALL = 0xFF,
	};

	enum DBI_IF_PARALLEL_BITS {
		DBI_IF_PARALLEL_8BITS = 0,
		DBI_IF_PARALLEL_9BITS = 1,
		DBI_IF_PARALLEL_16BITS = 2,
		DBI_IF_PARALLEL_18BITS = 3,
		DBI_IF_PARALLEL_24BITS = 4,
	};

	enum DBI_IF_SERIAL_BITS {
		DBI_IF_SERIAL_8BITS = 0,
		DBI_IF_SERIAL_9BITS = 1,
		DBI_IF_SERIAL_16BITS = 2,
		DBI_IF_SERIAL_18BITS = 3,
		DBI_IF_SERIAL_24BITS = 4,
		DBI_IF_SERIAL_32BITS = 5,
	};

	enum DBI_IF_PARALLEL_CLK_DIV {
		DBI_IF_PARALLEL_CLK_DIV_1 = 0,
		DBI_IF_PARALLEL_CLK_DIV_2,
		DBI_IF_PARALLEL_CLK_DIV_4,
	};

	enum DBI_IF_A0_MODE {
		DBI_IF_A0_LOW = 0,
		DBI_IF_A0_HIGH = 1,
	};

	enum DBI_IF_MCU_WRITE_BITS {
		DBI_IF_MCU_WRITE_8BIT = 8,
		DBI_IF_MCU_WRITE_16BIT = 16,
		DBI_IF_MCU_WRITE_32BIT = 32,
	};

	enum DBI_IF_FORMAT {
		DBI_IF_FORMAT_RGB332 = 0,
		DBI_IF_FORMAT_RGB444 = 1,
		DBI_IF_FORMAT_RGB565 = 2,
		DBI_IF_FORMAT_RGB666 = 3,
		DBI_IF_FORMAT_RGB888 = 4,
	};

	enum DBI_IF_WIDTH {
		DBI_IF_WIDTH_8_BITS = 0,
		DBI_IF_WIDTH_9_BITS = 2,
		DBI_IF_WIDTH_16_BITS = 1,
		DBI_IF_WIDTH_18_BITS = 3,
		DBI_IF_WIDTH_24_BITS = 4,
		DBI_IF_WIDTH_32_BITS = 5,
	};

	enum DBI_FB_ID {
		DBI_FB_0 = 0,
		DBI_FB_1 = 1,
		DBI_FB_2 = 2,
		DBI_FB_NUM,
	};

	enum DBI_FB_FORMAT {
		DBI_FB_FORMAT_RGB565 = 0,
		DBI_FB_FORMAT_RGB888 = 1,
		DBI_FB_FORMAT_ARGB8888 = 2,
		DBI_FB_FORMAT_NUM,
	};

	enum DBI_OUTPUT_MODE {
		DBI_OUTPUT_TO_LCM = (1 << 0),
		DBI_OUTPUT_TO_MEM = (1 << 1),
		DBI_OUTPUT_TO_TVROT = (1 << 2),
	};

	enum DBI_LAYER_ID {
		DBI_LAYER_0 = 0,
		DBI_LAYER_1 = 1,
		DBI_LAYER_2 = 2,
		DBI_LAYER_3 = 3,
		DBI_LAYER_NUM,
		DBI_LAYER_ALL = 0xFFFFFFFF,
	};

#define DBI_COLOR_BASE 30
	enum DBI_LAYER_FORMAT {
		DBI_LAYER_FORMAT_RGB888 = 0,
		DBI_LAYER_FORMAT_RGB565 = 1,
		DBI_LAYER_FORMAT_ARGB8888 = 2,
		DBI_LAYER_FORMAT_PARGB8888 = 3,
		DBI_LAYER_FORMAT_xRGB8888 = 4,
		DBI_LAYER_FORMAT_YUYV = 8,
		DBI_LAYER_FORMAT_UYVY = 9,
		DBI_LAYER_FORMAT_YVYU = 10,
		DBI_LAYER_FORMAT_VYUY = 11,
		DBI_LAYER_FORMAT_YUV444 = 15,
		DBI_LAYER_FORMAT_UNKNOWN = 16,

		DBI_LAYER_FORMAT_ABGR8888 =
			DBI_LAYER_FORMAT_ARGB8888 + DBI_COLOR_BASE,
		DBI_LAYER_FORMAT_BGR888 =
			DBI_LAYER_FORMAT_RGB888 + DBI_COLOR_BASE,
		DBI_LAYER_FORMAT_BGR565 =
			DBI_LAYER_FORMAT_RGB565 + DBI_COLOR_BASE,
		DBI_LAYER_FORMAT_PABGR8888 =
			DBI_LAYER_FORMAT_PARGB8888 + DBI_COLOR_BASE,
		DBI_LAYER_FORMAT_xBGR8888 =
			DBI_LAYER_FORMAT_xRGB8888 + DBI_COLOR_BASE,
	};

	enum DBI_LAYER_ROTATION {
		DBI_LAYER_ROTATE_NONE = 0,
		DBI_LAYER_ROTATE_0 = 0,
		DBI_LAYER_ROTATE_90 = 1,
		DBI_LAYER_ROTATE_180 = 2,
		DBI_LAYER_ROTATE_270 = 3,
		DBI_LAYER_ROTATE_MIRROR_0 = 4,
		DBI_LAYER_ROTATE_MIRROR_90 = 5,
		DBI_LAYER_ROTATE_MIRROR_180 = 6,
		DBI_LAYER_ROTATE_MIRROR_270 = 7,
	};

	enum DBI_LAYER_TRIGGER_MODE {
		DBI_SW_TRIGGER = 0,
		DBI_HW_TRIGGER_BUFFERING,
		DBI_HW_TRIGGER_DIRECT_COUPLE,
	};

	enum DBI_HW_TRIGGER_SRC {
		DBI_HW_TRIGGER_SRC_IRT1 = 0,
		DBI_HW_TRIGGER_SRC_IBW1 = 1,
		DBI_HW_TRIGGER_SRC_IBW2 = 3,
	};

	enum DBI_TE_MODE {
		DBI_TE_MODE_VSYNC_ONLY = 0,
		DBI_TE_MODE_VSYNC_OR_HSYNC = 1,
	};

	enum DBI_TE_VS_WIDTH_CNT_DIV {
		DBI_TE_VS_WIDTH_CNT_DIV_8 = 0,
		DBI_TE_VS_WIDTH_CNT_DIV_16 = 1,
		DBI_TE_VS_WIDTH_CNT_DIV_32 = 2,
		DBI_TE_VS_WIDTH_CNT_DIV_64 = 3,
	};

	enum DBI_IF_FMT_COLOR_ORDER {
		DBI_IF_FMT_COLOR_ORDER_RGB = 0,
		DBI_IF_FMT_COLOR_ORDER_BGR = 1,
	};

	enum DBI_IF_FMT_TRANS_SEQ {
		DBI_IF_FMT_TRANS_SEQ_MSB_FIRST = 0,
		DBI_IF_FMT_TRANS_SEQ_LSB_FIRST = 1,
	};

	enum DBI_IF_FMT_PADDING {
		DBI_IF_FMT_PADDING_ON_LSB = 0,
		DBI_IF_FMT_PADDING_ON_MSB = 1,
	};

	enum DBI_IO_DATA_DRIVING {
		DBI_IO_DATA_DRIVING_4MA = 0,
		DBI_IO_DATA_DRIVING_8MA = 1,
		DBI_IO_DATA_DRIVING_12MA = 2,
		DBI_IO_DATA_DRIVING_16MA = 3,
	};

	enum DBI_IO_CTL_DRIVING {
		DBI_IO_CTL_DRIVING_3MA = 0,
		DBI_IO_CTL_DRIVING_6MA = 1,
		DBI_IO_CTL_DRIVING_9MA = 2,
		DBI_IO_CTL_DRIVING_12MA = 3,
		DBI_IO_CTL_DRIVING_15MA = 4,
		DBI_IO_CTL_DRIVING_18MA = 5,
		DBI_IO_CTL_DRIVING_21MA = 6,
		DBI_IO_CTL_DRIVING_24MA = 7,
	};



	void dbi_analysis(enum DISP_MODULE_ENUM module);
	int ddp_dbi_start(enum DISP_MODULE_ENUM module, void *cmdq);
	enum DBI_STATUS DBI_DumpRegisters(enum DISP_MODULE_ENUM module,
			int level);

	int ddp_dbi_trigger(enum DISP_MODULE_ENUM module, void *cmdq);
	void ddp_dbi_dump_reg(enum DISP_MODULE_ENUM module);

	int dbi_enable_irq(enum DISP_MODULE_ENUM module, void *handle,
			unsigned int enable);
	int ddp_dbi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);

	enum DBI_STATUS _dbi_gpio_pinmux(enum DISP_MODULE_ENUM module,
			void *cmdq);
	enum DBI_STATUS _dbi_set_DrivingCurrent(enum DISP_MODULE_ENUM module,
			void *cmdq);



#ifdef __cplusplus
}
#endif
#endif				/* __DBI_DRV_H__ */
