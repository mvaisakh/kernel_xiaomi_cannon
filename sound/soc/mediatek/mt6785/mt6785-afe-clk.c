// SPDX-License-Identifier: GPL-2.0
//
// mt6785-afe-clk.c  --  Mediatek 6785 afe clock ctrl
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Eason Yen <eason.yen@mediatek.com>

#include <linux/clk.h>
#include <linux/regmap.h>

#include "mt6785-afe-common.h"
#include "mt6785-afe-clk.h"

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#include <mtk_idle.h>
#include <mtk_spm_resource_req.h>
#endif

/* for apmixed / cksys access */
#include <mt-plat/sync_write.h>

void *APMIXEDSYS_ADDRESS;
void *CKSYS_ADDRESS;

#define APMIXEDSYS_BASE (0x1000C000L)

#define CKSYS_BASE (0x10000000L)

static DEFINE_MUTEX(mutex_request_dram);

enum {
	CLK_AFE = 0,
	/*CLK_DAC,*/
	/*CLK_DAC_PREDIS,*/
	/*CLK_ADC,*/
	CLK_TML,
	CLK_APLL22M,
	CLK_APLL24M,
	CLK_APLL1_TUNER,
	CLK_APLL2_TUNER,
	CLK_NLE,
	CLK_SCP_SYS_AUD,
	CLK_INFRA_SYS_AUDIO,
	/*CLK_MTKAIF_26M,*/
	CLK_MUX_AUDIO,
	CLK_MUX_AUDIOINTBUS,
	CLK_TOP_MAINPLL_D2_D4,
	/* apll related mux */
	CLK_TOP_MUX_AUD_1,
	CLK_TOP_APLL1_CK,
	CLK_TOP_MUX_AUD_2,
	CLK_TOP_APLL2_CK,
	CLK_TOP_MUX_AUD_ENG1,
	CLK_TOP_APLL1_D8,
	CLK_TOP_MUX_AUD_ENG2,
	CLK_TOP_APLL2_D8,
	CLK_TOP_MUX_AUDIO_H,
	CLK_TOP_I2S0_M_SEL,
	CLK_TOP_I2S1_M_SEL,
	CLK_TOP_I2S2_M_SEL,
	CLK_TOP_I2S3_M_SEL,
	CLK_TOP_I2S4_M_SEL,
	CLK_TOP_I2S5_M_SEL,
	CLK_TOP_APLL12_DIV0,
	CLK_TOP_APLL12_DIV1,
	CLK_TOP_APLL12_DIV2,
	CLK_TOP_APLL12_DIV3,
	CLK_TOP_APLL12_DIV4,
	CLK_TOP_APLL12_DIVB,
	CLK_TOP_APLL12_DIV5,
	CLK_CLK26M,
	CLK_NUM
};

static const char *aud_clks[CLK_NUM] = {
	[CLK_AFE] = "aud_afe_clk",
	/*[CLK_DAC] = "aud_dac_clk",*/
	/*[CLK_DAC_PREDIS] = "aud_dac_predis_clk",*/
	/*[CLK_ADC] = "aud_adc_clk",*/
	[CLK_TML] = "aud_tml_clk",
	[CLK_APLL22M] = "aud_apll22m_clk",
	[CLK_APLL24M] = "aud_apll24m_clk",
	[CLK_APLL1_TUNER] = "aud_apll1_tuner_clk",
	[CLK_APLL2_TUNER] = "aud_apll2_tuner_clk",
	[CLK_NLE] = "aud_nle",
	[CLK_SCP_SYS_AUD] = "scp_sys_audio",
	[CLK_INFRA_SYS_AUDIO] = "aud_infra_clk",
	/*[CLK_MTKAIF_26M] = "mtkaif_26m_clk",*/
	[CLK_MUX_AUDIO] = "top_mux_audio",
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_MAINPLL_D2_D4] = "top_mainpll_d2_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_2] = "top_mux_aud_2",
	[CLK_TOP_APLL2_CK] = "top_apll2_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D8] = "top_apll1_d8",
	[CLK_TOP_MUX_AUD_ENG2] = "top_mux_aud_eng2",
	[CLK_TOP_APLL2_D8] = "top_apll2_d8",
	[CLK_TOP_MUX_AUDIO_H] = "top_mux_audio_h",
	[CLK_TOP_I2S0_M_SEL] = "top_i2s0_m_sel",
	[CLK_TOP_I2S1_M_SEL] = "top_i2s1_m_sel",
	[CLK_TOP_I2S2_M_SEL] = "top_i2s2_m_sel",
	[CLK_TOP_I2S3_M_SEL] = "top_i2s3_m_sel",
	[CLK_TOP_I2S4_M_SEL] = "top_i2s4_m_sel",
	[CLK_TOP_I2S5_M_SEL] = "top_i2s5_m_sel",
	[CLK_TOP_APLL12_DIV0] = "top_apll12_div0",
	[CLK_TOP_APLL12_DIV1] = "top_apll12_div1",
	[CLK_TOP_APLL12_DIV2] = "top_apll12_div2",
	[CLK_TOP_APLL12_DIV3] = "top_apll12_div3",
	[CLK_TOP_APLL12_DIV4] = "top_apll12_div4",
	[CLK_TOP_APLL12_DIVB] = "top_apll12_divb",
	[CLK_TOP_APLL12_DIV5] = "top_apll12_div5",
	[CLK_CLK26M] = "top_clk26m_clk",
};

static int mt6785_set_audio_int_bus_parent(struct mtk_base_afe *afe,
					   int clk_id)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLK_MUX_AUDIOINTBUS],
		       aud_clks[clk_id], ret);
	}

	return ret;
}

static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLK_TOP_MUX_AUD_1],
			       aud_clks[CLK_TOP_APLL1_CK], ret);
			goto EXIT;
		}

		/* 180.6336 / 8 = 22.5792MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
			       aud_clks[CLK_TOP_APLL1_D8], ret);
			goto EXIT;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
			       aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLK_TOP_MUX_AUD_1],
			       aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	}


EXIT:
	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_TOP_APLL2_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_TOP_APLL2_CK], ret);
			goto EXIT;
		}

		/* 196.608 / 8 = 24.576MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_TOP_APLL2_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_TOP_APLL2_D8], ret);
			goto EXIT;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	}

EXIT:
	return 0;
}


int mt6785_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_info(afe->dev, "%s()\n", __func__);

	ret = clk_prepare_enable(afe_priv->clk[CLK_SCP_SYS_AUD]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_SCP_SYS_AUD], ret);
		goto CLK_SCP_SYS_AUD_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_INFRA_SYS_AUDIO]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_INFRA_SYS_AUDIO], ret);
		goto CLK_INFRA_SYS_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIO]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO], ret);
		goto CLK_MUX_AUDIO_ERR;
	}
	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIO],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO],
			aud_clks[CLK_CLK26M], ret);
		goto CLK_MUX_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6785_set_audio_int_bus_parent(afe,
					      CLK_TOP_MAINPLL_D2_D4);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUDIO_H],
			     afe_priv->clk[CLK_TOP_APLL2_CK]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLK_TOP_MUX_AUDIO_H],
		       aud_clks[CLK_TOP_APLL2_CK], ret);
		goto CLK_MUX_AUDIO_H_PARENT_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE], ret);
		goto CLK_AFE_ERR;
	}

	return 0;

CLK_AFE_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);
CLK_MUX_AUDIO_H_PARENT_ERR:
CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6785_set_audio_int_bus_parent(afe, CLK_CLK26M);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
CLK_MUX_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);
CLK_INFRA_SYS_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_SYS_AUDIO]);
CLK_SCP_SYS_AUD_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_SCP_SYS_AUD]);

	return ret;

}

void mt6785_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s()\n", __func__);

	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);

	mt6785_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);

	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_SYS_AUDIO]);
	clk_disable_unprepare(afe_priv->clk[CLK_SCP_SYS_AUD]);
}

int mt6785_afe_suspend_clock(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* set audio int bus to 26M */
	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6785_set_audio_int_bus_parent(afe, CLK_CLK26M);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return 0;

CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6785_set_audio_int_bus_parent(afe, CLK_TOP_MAINPLL_D2_D4);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	return ret;
}

int mt6785_afe_resume_clock(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* set audio int bus to normal working clock */
	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6785_set_audio_int_bus_parent(afe,
					      CLK_TOP_MAINPLL_D2_D4);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return 0;

CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6785_set_audio_int_bus_parent(afe, CLK_CLK26M);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	return ret;
}

int mt6785_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6785_afe_private *afe_priv = afe->platform_priv;

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (afe_priv->dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_ALL);
#endif
	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6785_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6785_afe_private *afe_priv = afe->platform_priv;

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (afe_priv->dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_RELEASE);
#endif

	if (afe_priv->dram_resource_counter < 0) {
		dev_warn(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

static struct mtk_base_afe *local_afe;
static bool check_apmixed_offset(unsigned int offset)
{
	return offset <= APMIXEDSYS_MAX_LENGTH;
}

unsigned int get_apmixed_reg(unsigned int offset)
{
	long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	unsigned int *value = (unsigned int *)(address);

	if (!check_apmixed_offset(offset)) {
		dev_warn(local_afe->dev, "%s(), offset 0x%x invalid\n",
			 __func__, offset);
		return 0xffffffff;
	}

	return *value;
}

void set_apmixed_reg(unsigned int offset, unsigned int mask, unsigned int value)
{
	long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	unsigned int *AFE_Register = (unsigned int *)address;
	unsigned int val_tmp;

	if (check_apmixed_offset(offset)) {
		val_tmp = get_apmixed_reg(offset);
		val_tmp &= (~mask);
		val_tmp |= (value & mask);
		mt_reg_sync_writel(val_tmp, AFE_Register);
	} else {
		dev_warn(local_afe->dev, "%s(), offset 0x%x invalid, value 0x%x\n",
			 __func__, offset, value);
	}
}

static bool check_cksys_offset(unsigned int offset)
{
	return offset <= CLK_MAX_LENGTH;
}

unsigned int get_cksys_reg(unsigned int offset)
{
	long address = (long)((char *)CKSYS_ADDRESS + offset);
	unsigned int *value = (unsigned int *)(address);

	if (!check_cksys_offset(offset)) {
		dev_warn(local_afe->dev, "%s(), offset 0x%x invalid\n",
			 __func__, offset);
		return 0xffffffff;
	}

	return *value;
}

void set_cksys_reg(unsigned int offset, unsigned int mask, unsigned int value)
{
	long address = (long)((char *)CKSYS_ADDRESS + offset);
	unsigned int *AFE_Register = (unsigned int *)address;
	unsigned int val_tmp;

	if (check_cksys_offset(offset)) {
		val_tmp = get_cksys_reg(offset);
		val_tmp &= (~mask);
		val_tmp |= (value & mask);
		mt_reg_sync_writel(val_tmp, AFE_Register);
	}
}

int mt6785_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL22M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL22M], ret);
		goto ERR_CLK_APLL22M;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   0x0000FFF7, 0x00000832);
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x1 << AFE_22M_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
ERR_CLK_APLL22M:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	return ret;
}

void mt6785_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x0 << AFE_22M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	apll1_mux_setting(afe, false);
}

int mt6785_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL24M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL24M], ret);
		goto ERR_CLK_APLL24M;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL2_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   0x0000FFF7, 0x00000634);
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x1 << AFE_24M_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
ERR_CLK_APLL24M:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	return ret;

	return 0;
}

void mt6785_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x0 << AFE_24M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	apll2_mux_setting(afe, false);
}

int mt6785_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return (apll == MT6785_APLL1) ? 180633600 : 196608000;
}

int mt6785_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT6785_APLL2 : MT6785_APLL1;
}

int mt6785_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT6785_APLL1;
	else
		return MT6785_APLL2;
}

/* mck */
struct mt6785_mck_div {
	int m_sel_id;
	int div_clk_id;
	/* below will be deprecated */
	int div_pdn_reg;
	int div_pdn_mask_sft;
	int div_reg;
	int div_mask_sft;
	int div_mask;
	int div_sft;
	int div_msb_reg;
	int div_msb_mask_sft;
	int div_msb_mask;
	int div_msb_sft;
	int div_apll_sel_reg;
	int div_apll_sel_mask_sft;
	int div_apll_sel_sft;
	int div_inv_reg;
	int div_inv_mask_sft;
};

static const struct mt6785_mck_div mck_div[MT6785_MCK_NUM] = {
	[MT6785_I2S0_MCK] = {
		.m_sel_id = CLK_TOP_I2S0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV0,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV0_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_1,
		.div_mask_sft = APLL12_CK_DIV0_MASK_SFT,
		.div_mask = APLL12_CK_DIV0_MASK,
		.div_sft = APLL12_CK_DIV0_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S0_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S0_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIV0_INV_MASK_SFT,
	},
	[MT6785_I2S1_MCK] = {
		.m_sel_id = CLK_TOP_I2S1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV1,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV1_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_1,
		.div_mask_sft = APLL12_CK_DIV1_MASK_SFT,
		.div_mask = APLL12_CK_DIV1_MASK,
		.div_sft = APLL12_CK_DIV1_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S1_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S1_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIV1_INV_MASK_SFT,
	},
	[MT6785_I2S2_MCK] = {
		.m_sel_id = CLK_TOP_I2S2_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV2,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV2_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_1,
		.div_mask_sft = APLL12_CK_DIV2_MASK_SFT,
		.div_mask = APLL12_CK_DIV2_MASK,
		.div_sft = APLL12_CK_DIV2_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S2_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S2_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIV2_INV_MASK_SFT,
	},
	[MT6785_I2S3_MCK] = {
		.m_sel_id = CLK_TOP_I2S3_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV3,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV3_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_1,
		.div_mask_sft = APLL12_CK_DIV3_MASK_SFT,
		.div_mask = APLL12_CK_DIV3_MASK,
		.div_sft = APLL12_CK_DIV3_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S3_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S3_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIV3_INV_MASK_SFT,
	},
	[MT6785_I2S4_MCK] = {
		.m_sel_id = CLK_TOP_I2S4_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV4,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV4_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV4_MASK_SFT,
		.div_mask = APLL12_CK_DIV4_MASK,
		.div_sft = APLL12_CK_DIV4_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S4_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S4_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIV4_INV_MASK_SFT,
	},
	[MT6785_I2S4_BCK] = {
		.m_sel_id = -1,
		.div_clk_id = CLK_TOP_APLL12_DIVB,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIVB_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIVB_MASK_SFT,
		.div_mask = APLL12_CK_DIVB_MASK,
		.div_sft = APLL12_CK_DIVB_SFT,
		.div_inv_reg = CLK_AUDDIV_0,
		.div_inv_mask_sft = APLL12_DIVB_INV_MASK_SFT,
	},
	[MT6785_I2S5_MCK] = {
		.m_sel_id = CLK_TOP_I2S5_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV5,
		.div_pdn_reg = CLK_AUDDIV_2,
		.div_pdn_mask_sft = APLL12_DIV5_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV5_LSB_MASK_SFT,
		.div_mask = APLL12_CK_DIV5_LSB_MASK,
		.div_sft = APLL12_CK_DIV5_LSB_SFT,
		.div_msb_reg = CLK_AUDDIV_3,
		.div_msb_mask_sft = APLL12_CK_DIV5_MSB_MASK_SFT,
		.div_msb_mask = APLL12_CK_DIV5_MSB_MASK,
		.div_msb_sft = APLL12_CK_DIV5_MSB_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_2,
		.div_apll_sel_mask_sft = APLL_I2S5_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S5_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_2,
		.div_inv_mask_sft = APLL12_DIV5_INV_MASK_SFT,
	},
};

int mt6785_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int div_mask;
	int msb_sft = 0;
	int apll = mt6785_get_apll_by_rate(afe, rate);
	int apll_rate = mt6785_get_apll_rate(afe, apll);
	int apll_clk_id = apll == MT6785_APLL1 ?
			  CLK_TOP_MUX_AUD_1 : CLK_TOP_MUX_AUD_2;
	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;
	int div;
	int ret;

	/* select apll */
	if (m_sel_id >= 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	/* enable div, set rate */
	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	/* below will be deprecated, i2s5 not full support by ccf now */
	if (mck_id != MT6785_I2S5_MCK)
		return 0;

	afe_priv->mck_rate[mck_id] = rate;

	div = apll_rate / rate - 1;

	/* set ck div */
	div_mask = mck_div[mck_id].div_mask;

	if (mck_div[mck_id].div_msb_mask) {
		msb_sft = fls(mck_div[mck_id].div_mask);
		div_mask |= mck_div[mck_id].div_msb_mask << msb_sft;
	}

	if (div > div_mask) {
		AUDIO_AEE("mclk_div not valid");
		return -EINVAL;
	}
	set_cksys_reg(mck_div[mck_id].div_reg,
		      mck_div[mck_id].div_mask_sft,
		      div << mck_div[mck_id].div_sft);

	if (mck_div[mck_id].div_msb_mask)
		set_cksys_reg(mck_div[mck_id].div_msb_reg,
			      mck_div[mck_id].div_msb_mask_sft,
			      (div >> msb_sft) <<
			      mck_div[mck_id].div_msb_sft);
	/* select apll */
	if (mck_div[mck_id].div_apll_sel_mask_sft)
		set_cksys_reg(mck_div[mck_id].div_apll_sel_reg,
			      mck_div[mck_id].div_apll_sel_mask_sft,
			      apll <<
			      mck_div[mck_id].div_apll_sel_sft);
	/* reset inverse */
	set_cksys_reg(mck_div[mck_id].div_inv_reg,
		      mck_div[mck_id].div_inv_mask_sft, 0);
	/* enable div */
	set_cksys_reg(mck_div[mck_id].div_pdn_reg,
		      mck_div[mck_id].div_pdn_mask_sft, 0);

	return 0;
}

void mt6785_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;

	clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
enum {
	aud_intbus_sel_26m = 0,
	aud_intbus_sel_mainpll_d2_d4,
	aud_intbus_sel_mainpll_d7_d2,
};

static int mt6785_afe_idle_notify_call(struct notifier_block *nfb,
				       unsigned long id,
				       void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
		aud_intbus_mux_sel(aud_intbus_sel_26m);
		break;
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
		aud_intbus_mux_sel(aud_intbus_sel_mainpll_d2_d4);
		break;
	case NOTIFY_SOIDLE3_ENTER:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mt6785_afe_idle_nfb = {
	.notifier_call = mt6785_afe_idle_notify_call,
};
#endif

int mt6785_init_clock(struct mtk_base_afe *afe)
{
	struct mt6785_afe_private *afe_priv = afe->platform_priv;
	int i = 0;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_warn(afe->dev, "%s devm_clk_get %s fail, ret %ld\n",
				 __func__,
				 aud_clks[i], PTR_ERR(afe_priv->clk[i]));
			/*return PTR_ERR(clks[i]);*/
			afe_priv->clk[i] = NULL;
		}
	}

	/* TODO: change to use syscon */
	APMIXEDSYS_ADDRESS = ioremap_nocache(APMIXEDSYS_BASE, 0x1000);
	CKSYS_ADDRESS = ioremap_nocache(CKSYS_BASE, 0x1000);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	local_afe = afe;
	mtk_idle_notifier_register(&mt6785_afe_idle_nfb);
#endif

	return 0;
}
