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

#ifndef _GRAPHICS_BASE_V1_1_H_
#define _GRAPHICS_BASE_V1_1_H_

enum android_pixel_format_v1_1 {
	HAL_PIXEL_FORMAT_DEPTH_16 = 48,
	HAL_PIXEL_FORMAT_DEPTH_24 = 49,
	HAL_PIXEL_FORMAT_DEPTH_24_STENCIL_8 = 50,
	HAL_PIXEL_FORMAT_DEPTH_32F = 51,
	HAL_PIXEL_FORMAT_DEPTH_32F_STENCIL_8 = 52,
	HAL_PIXEL_FORMAT_STENCIL_8 = 53,
	HAL_PIXEL_FORMAT_YCBCR_P010 = 54,
};

enum android_dataspace_v1_1 {
	/* ((STANDARD_BT2020 | TRANSFER_SMPTE_170M) | RANGE_LIMITED) */
	HAL_DATASPACE_BT2020_ITU = 281411584,
	/* ((STANDARD_BT2020 | TRANSFER_ST2084) | RANGE_LIMITED) */
	HAL_DATASPACE_BT2020_ITU_PQ = 298188800,
	/* ((STANDARD_BT2020 | TRANSFER_HLG) | RANGE_LIMITED) */
	HAL_DATASPACE_BT2020_ITU_HLG = 302383104,
	/* ((STANDARD_BT2020 | TRANSFER_HLG) | RANGE_FULL) */
	HAL_DATASPACE_BT2020_HLG = 168165376,
};

enum android_color_mode_v1_1 {
	HAL_COLOR_MODE_BT2020 = 10,
	HAL_COLOR_MODE_BT2100_PQ = 11,
	HAL_COLOR_MODE_BT2100_HLG = 12,
};

enum android_render_intent_v1_1 {
	HAL_RENDER_INTENT_COLORIMETRIC = 0,
	HAL_RENDER_INTENT_ENHANCE = 1,
	HAL_RENDER_INTENT_TONE_MAP_COLORIMETRIC = 2,
	HAL_RENDER_INTENT_TONE_MAP_ENHANCE = 3,
};

#endif  /* _GRAPHICS_BASE_V1_1_H_ */
