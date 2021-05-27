/*
 * Copyright (c) 2015 MediaTek Inc.
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

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct tianma {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;
};

static char bl_tb0[] = {0x51, 0xf, 0xff};

#define tianma_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct tianma *panel_to_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct tianma, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int tianma_dcs_read(struct tianma *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void tianma_dcs_write(struct tianma *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	msleep(200);

	/* set PLL to 190M */
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xB6, 0x51, 0x00, 0x06, 0x23, 0x8A,
		0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

	tianma_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1], bl_tb0[2]);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x44, 0x08, 0x66);
	tianma_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x00, 0x04, 0x37);
	tianma_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00, 0x09, 0x23);

	msleep(200);

	tianma_dcs_write_seq_static(ctx, 0xB0, 0x80);
	tianma_dcs_write_seq_static(ctx, 0xD4, 0x93);
	tianma_dcs_write_seq_static(ctx, 0x50, 0x42, 0x58, 0x81, 0x2D, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x6B, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0xFF, 0xD4,
			0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x53,
			0x18, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

	/* set PPS to table and choose table 0 */
	tianma_dcs_write_seq_static(ctx, 0xF7, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xF8, 0x11, 0x00, 0x00, 0x89, 0x30,
		0x80, 0x09, 0x24, 0x04, 0x38, 0x00, 0x14, 0x02, 0x1c, 0x02,
		0x1c, 0x02, 0x00, 0x02, 0x0e, 0x00, 0x20, 0x01, 0xe8, 0x00,
		0x07, 0x00, 0x0c, 0x05, 0x0e, 0x05, 0x16, 0x18, 0x00, 0x10,
		0xf0, 0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e,
		0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79,
		0x7b, 0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09,
		0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8, 0x1a, 0x38, 0x1a,
		0x78, 0x1a, 0xb6, 0x2a, 0xf6, 0x2b, 0x34, 0x2b, 0x74, 0x3b,
		0x74, 0x6b, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00);

	/* Turn on DSC */
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xEB, 0x8B, 0x8B);

	/* Flash QE setting */
	tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x40);
	tianma_dcs_write_seq_static(ctx, 0xF3, 0x50, 0x00, 0x00, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xF2, 0x11);

	usleep_range(10 * 1000, 15 * 1000);

	tianma_dcs_write_seq_static(ctx, 0xF3, 0x01, 0x00, 0x00, 0x00, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xF4, 0x00, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xF2, 0x19);

	usleep_range(20 * 1000, 25 * 1000);

	tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x42);
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
	tianma_dcs_write_seq_static(ctx, 0xE6, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x11);

	msleep(120);

	tianma_dcs_write_seq_static(ctx, 0x29);
}

static int tianma_disable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int tianma_unprepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->prepared)
		return 0;

	tianma_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	tianma_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		tianma_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

	return ret;
}

static int tianma_enable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 163530,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 10,
	.htotal = 1080 + 40 + 10 + 20,
	.vdisplay = 2340,
	.vsync_start = 2340 + 20,
	.vsync_end = 2340 + 20 + 2,
	.vtotal = 2340 + 20 + 2 + 8,
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 327060,
	.hdisplay = 1080,
	.hsync_start = 1080 + 40,
	.hsync_end = 1080 + 40 + 10,
	.htotal = 1080 + 40 + 10 + 20,
	.vdisplay = 2340,
	.vsync_start = 2340 + 20,
	.vsync_end = 2340 + 20 + 2,
	.vtotal = 2340 + 20 + 2 + 8,
	.vrefresh = 120,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x00,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2340,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 380,
};

static struct mtk_panel_params ext_params_120hz = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70200,
	.physical_height_um = 152100,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2340,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = 800,
};

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* display off */
		tianma_dcs_write_seq_static(ctx, 0x28);
		tianma_dcs_write_seq_static(ctx, 0x10);
		usleep_range(200 * 1000, 230 * 1000);
	} else if (stage == AFTER_DSI_POWERON) {
		/* set PLL to 380M */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB6, 0x6A, 0x00, 0x06, 0x23,
			0x8A, 0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

		/* switch to 120hz */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xF1, 0x2A);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x0C);
		tianma_dcs_write_seq_static(ctx, 0xC2, 0x09, 0x24, 0x0E,
			0x00, 0x00, 0x0E);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x94, 0x42, 0x00,
			0x16, 0x05);
		tianma_dcs_write_seq_static(ctx, 0xCF, 0x64, 0x0B, 0x00,
			0x5E, 0x00, 0xC3, 0x02, 0x4B, 0x0B, 0x77, 0x0B,
			0x8B, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
			0x05, 0x05, 0x06, 0x06, 0x06, 0x00, 0x9B, 0x00,
			0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00,
			0xB9, 0x00, 0xB9, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x00, 0x9B, 0x00,
			0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00,
			0xB9, 0x00, 0xB9, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x1C, 0x1C, 0x1C,
			0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
			0x1C, 0x00, 0x90, 0x01, 0x9A, 0x01, 0x9A, 0x03,
			0x33, 0x03, 0x33, 0x09, 0xA4, 0x09, 0xA4, 0x09,
			0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x0F,
			0x88, 0x19);
		tianma_dcs_write_seq_static(ctx, 0xC4, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x35, 0x00, 0x01);
		tianma_dcs_write_seq_static(ctx, 0xD7, 0x00, 0x69, 0x34,
			0x00, 0xa0, 0x0a, 0x00, 0x00, 0x39);
		tianma_dcs_write_seq_static(ctx, 0xD8, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00,
			0x3A, 0x00, 0x3A, 0x00, 0x3A, 0x00, 0x3A, 0x05,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x0F, 0x00, 0x32, 0x00, 0x00, 0x00, 0x17,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x42, 0x58,
			0x81, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xBC, 0x1D, 0xFF, 0xD4, 0x0E, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x0F, 0x00, 0x53, 0x18, 0xBB,
			0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xBB, 0x59, 0xC8, 0xC8,
			0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0x4A,
			0x48, 0x46, 0x44, 0x42, 0x40, 0x3E, 0x3C, 0x3A,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x04, 0x00, 0x02, 0x02, 0x00, 0x04,
			0x69, 0x5A, 0x00, 0x0B, 0x76, 0x0F, 0xFF, 0x0F,
			0xFF, 0x0F, 0xFF, 0x14, 0x81, 0xF4);

		tianma_dcs_write_seq_static(ctx, 0xE8, 0x00, 0x02);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x06);

		tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x33, 0xB4, 0x00,
			0x00, 0x00, 0x39, 0x04, 0x09, 0x34);

		tianma_dcs_write_seq_static(ctx, 0xD4, 0x93, 0x93, 0x60,
			0x1E, 0xE1, 0x02, 0x08, 0x00, 0x00, 0x02, 0x22,
			0x02, 0xEC, 0x03, 0x83, 0x04, 0x00, 0x04, 0x00,
			0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x00,
			0x01, 0x00);
		/* display on */
		tianma_dcs_write_seq_static(ctx, 0x29);
		usleep_range(120 * 1000, 150 * 1000);
		tianma_dcs_write_seq_static(ctx, 0x11);
		usleep_range(200 * 1000, 230 * 1000);
	}
}

static void mode_switch_120_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* display off */
		tianma_dcs_write_seq_static(ctx, 0x28);
		tianma_dcs_write_seq_static(ctx, 0x10);
		usleep_range(200 * 1000, 230 * 1000);
	} else if (stage == AFTER_DSI_POWERON) {
		/* set PLL to 190M */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB6, 0x51, 0x00, 0x06, 0x23,
			0x8A, 0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

		/* switch to 60hz */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xF1, 0x2A);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x0C);
		tianma_dcs_write_seq_static(ctx, 0xC2, 0x09, 0x24, 0x0E,
			0x00, 0x00, 0x0E);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x94, 0x42, 0x00,
			0x16, 0x05);
		tianma_dcs_write_seq_static(ctx, 0xCF, 0x64, 0x0B, 0x00,
			0x5E, 0x00, 0xC3, 0x02, 0x4B, 0x0B, 0x77, 0x0B,
			0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x9B, 0x00,
			0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00,
			0xB9, 0x00, 0xB9, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x00, 0x9B, 0x00,
			0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00, 0xB9, 0x00,
			0xB9, 0x00, 0xB9, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x1C, 0x1C, 0x1C,
			0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
			0x1C, 0x00, 0x90, 0x01, 0x9A, 0x01, 0x9A, 0x03,
			0x33, 0x03, 0x33, 0x09, 0xA4, 0x09, 0xA4, 0x09,
			0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x0F,
			0x88, 0x19);
		tianma_dcs_write_seq_static(ctx, 0xC4, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x50, 0x00, 0x01);
		tianma_dcs_write_seq_static(ctx, 0xD7, 0x00, 0x69, 0x34,
			0x00, 0xa0, 0x0a, 0x00, 0x00, 0x5a);
		tianma_dcs_write_seq_static(ctx, 0xD8, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5a, 0x00,
			0x5a, 0x00, 0x5a, 0x00, 0x5a, 0x00, 0x5a, 0x05,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x0A, 0x50, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x1A, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x24,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x42, 0x58,
			0x81, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x10, 0x00, 0xFF, 0xD4, 0x0E, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x0F, 0x00, 0x53, 0x18, 0x0F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xBB, 0x59, 0xC8, 0xC8,
			0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0x4A,
			0x48, 0x46, 0x44, 0x42, 0x40, 0x3E, 0x3C, 0x3A,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x04, 0x00, 0x01, 0x01, 0x00, 0x04,
			0x69, 0x5A, 0x00, 0x0B, 0x76, 0x0F, 0xFF, 0x0F,
			0xFF, 0x0F, 0xFF, 0x14, 0x81, 0xF4);

		tianma_dcs_write_seq_static(ctx, 0xE8, 0x00, 0x02);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x35);

		tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x33, 0xB4, 0x00,
			0x00, 0x00, 0x58, 0x04, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xD4, 0x93, 0x93, 0x60,
			0x1E, 0xE1, 0x02, 0x08, 0x00, 0x00, 0x02, 0x22,
			0x02, 0xEC, 0x03, 0x83, 0x04, 0x00, 0x04, 0x00,
			0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x00,
			0x01, 0x00);

		/* display on */
		tianma_dcs_write_seq_static(ctx, 0x29);
		usleep_range(120 * 1000, 150 * 1000);
		tianma_dcs_write_seq_static(ctx, 0x11);
		usleep_range(200 * 1000, 230 * 1000);
	}
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;

	if (cur_mode == 0 && dst_mode == 1) { /* 60 switch to 120 */
		mode_switch_60_to_120(panel, stage);
	} else if (cur_mode == 1 && dst_mode == 0) { /* 1200 switch to 60 */
		mode_switch_120_to_60(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct tianma *ctx = panel_to_tianma(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.reset = panel_ext_reset,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int tianma_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	panel->connector->display_info.width_mm = 70;
	panel->connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs tianma_drm_funcs = {
	.disable = tianma_disable,
	.unprepare = tianma_unprepare,
	.prepare = tianma_prepare,
	.enable = tianma_enable,
	.get_modes = tianma_get_modes,
};

static int tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct tianma *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &tianma_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

	return ret;
}

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id tianma_of_match[] = {
	{
		.compatible = "tianma,r66451,cmd,120hz",
	},
	{} };

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.driver = {

			.name = "panel-tianma-r66451-cmd-120hz",
			.owner = THIS_MODULE,
			.of_match_table = tianma_of_match,
		},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("tianma r66451 CMD 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
