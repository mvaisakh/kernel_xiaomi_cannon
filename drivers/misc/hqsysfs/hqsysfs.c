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

#include <linux/hqsysfs.h>

#include "hqsys_misc.h"
#include "hqsys_pcba.h"

#define HQ_SYS_FS_VER "2016-03-11 V0.2"

static HW_INFO(HWID_VER, ver);
static HW_INFO(HWID_SUMMARY, hw_summary);
static HW_INFO(HWID_DDR, ram);
static HW_INFO(HWID_EMMC, emmc);
static HW_INFO(HWID_LCM, lcm);
//static HW_INFO(HWID_BIAS_IC,lcm_bias_ic);
static HW_INFO(HWID_CTP, ctp);
static HW_INFO(HWID_SUB_CAM, sub_cam);      //sub
static HW_INFO(HWID_SUB_CAM_2, main0_cam);  //main0
static HW_INFO(HWID_MAIN_CAM, main1_cam);  //main1
static HW_INFO(HWID_MAIN_CAM_2, main2_cam);  //main2
static HW_INFO(HWID_MAIN_CAM_3, main3_cam);  //main3

static HW_INFO(HWID_MAIN_LENS, main_cam_len);
static HW_INFO(HWID_FLASHLIGHT, flashlight);
static HW_INFO(HWID_GSENSOR, gsensor);
static HW_INFO(HWID_ALSPS, alsps);
static HW_INFO(HWID_MSENSOR, msensor);
static HW_INFO(HWID_GYRO, gyro);
static HW_INFO(HWID_IRDA, irda);
static HW_INFO(HWID_FUEL_GAUGE_IC, fuel_gauge_ic);
static HW_INFO(HWID_NFC, nfc);
static HW_INFO(HWID_FP, fingerprint);
//static HW_INFO(HWID_TEE,tee);
static HW_INFO(HWID_PCBA, pcba_config);
/* BSP.Charge - 2020.11.26 - Add node to show pmic_version */
static HW_INFO(HWID_PMIC, pmic_version);

/* BSP.Audio - 2020.11.11 - modify to bring up second PA */
static HW_INFO(HWID_AUDIO, audio_PA);
/* end modify*/

struct pcba_info K19_PCBA[] = {
	{PCBA_K19_P0_1_CN, "PCBA_K19_P0-1_CN"},
	{PCBA_K19_P0_1_GLOBAL, "PCBA_K19_P0-1_GLOBAL"},
	{PCBA_K19P_P1_INDIA, "PCBA_K19P_P1_INDIA"},
	{PCBA_K19_P1_CN_NEW, "PCBA_K19_P1_CN_NEW"},
	{PCBA_K19_P1_GLOBAL_NEW, "PCBA_K19_P1_GLOBAL_NEW"},
	{PCBA_K19P_P1_1_INDIA, "PCBA_K19P_P1-1_INDIA"},
	{PCBA_K19_P1_1_CN_NEW, "PCBA_K19_P1-1_CN_NEW"},
	{PCBA_K19_P1_1_GLOBAL_NEW, "PCBA_K19_P1-1_GLOBAL_NEW"},
	{PCBA_K19P_P1_1_GLOBAL, "PCBA_K19P_P1-1_GLOBAL"},
	{PCBA_K19P_P2_INDIA, "PCBA_K19P_P2_INDIA"},
	{PCBA_K19_P2_INDIA, "PCBA_K19_P2_INDIA"},
	{PCBA_K19_P2_CN_FINAL, "PCBA_K19_P2_CN_FINAL"},
	{PCBA_K19_P2_CN_NEW, "PCBA_K19_P2_CN_NEW"},
	{PCBA_K19_P2_GLOBAL_NEW, "PCBA_K19_P2_GLOBAL_NEW"},
	{PCBA_K19P_P2_GLOBAL, "PCBA_K19P_P2_GLOBAL"},
	{PCBA_K19P_MP_INDIA, "PCBA_K19P_MP_INDIA"},
	{PCBA_K19_MP_INDIA, "PCBA_K19_MP_INDIA"},
	{PCBA_K19_MP_CN_FINAL, "PCBA_K19_MP_CN_FINAL"},
	{PCBA_K19_MP_CN_NEW, "PCBA_K19_MP_CN_NEW"},
	{PCBA_K19_MP_GLOBAL_NEW, "PCBA_K19_MP_GLOBAL_NEW"},
	{PCBA_K19P_MP_GLOBAL, "PCBA_K19P_MP_GLOBAL"},
};

static struct attribute *huaqin_attrs[] = {
	&hw_info_ver.attr,
	&hw_info_hw_summary.attr,
	&hw_info_ram.attr,
	&hw_info_emmc.attr,
	&hw_info_lcm.attr,
//	&hw_info_lcm_bias_ic.attr,
	&hw_info_ctp.attr,
	&hw_info_sub_cam.attr,
	&hw_info_main0_cam.attr,
	&hw_info_main1_cam.attr,
	&hw_info_main2_cam.attr,
	&hw_info_main3_cam.attr,

	&hw_info_main_cam_len.attr,
	&hw_info_flashlight.attr,
	&hw_info_gsensor.attr,
	&hw_info_alsps.attr,
	&hw_info_msensor.attr,
	&hw_info_gyro.attr,
	&hw_info_irda.attr,
	&hw_info_fuel_gauge_ic.attr,
	&hw_info_nfc.attr,
	&hw_info_fingerprint.attr,
	&hw_info_pcba_config.attr,
	/* BSP.Charge - 2020.11.26 - Add node to show pmic_version */
	&hw_info_pmic_version.attr,
//	&hw_info_tee.attr,
/* BSP.Audio - 2020.11.11 - modify to bring up second PA */
	&hw_info_audio_PA.attr,
/* end modify*/
	NULL
};

static ssize_t huaqin_show(struct kobject *kobj, struct attribute *a, char *buf)
{
	ssize_t count = 0;
	int i = 0;
	struct hw_info *hw = container_of(a, struct hw_info, attr);

	if (NULL == hw) {
		return sprintf(buf, "Data error\n");
	}

	if (HWID_VER == hw->hw_id) {
		count = sprintf(buf, "%s\n", HQ_SYS_FS_VER);
	} else if (HWID_SUMMARY == hw->hw_id) {
		//iterate all device and output the detail
		int iterator = 0;
		struct hw_info *curent_hw = NULL;
		struct attribute *attr = huaqin_attrs[iterator];

		while (attr) {
			curent_hw = container_of(attr, struct hw_info, attr);
			iterator += 1;
			attr = huaqin_attrs[iterator];

			if (curent_hw->hw_exist && (NULL != curent_hw->hw_device_name)) {
				count += sprintf(buf+count, "%s: %s\n", curent_hw->attr.name, curent_hw->hw_device_name);
			}
		}

	} else if (HWID_PCBA == hw->hw_id) {

		if (get_huaqin_pcba_config() >= PCBA_UNKNOW && get_huaqin_pcba_config() < PCBA_END) {
			huaqin_pcba_config = get_huaqin_pcba_config();
		} else {
			huaqin_pcba_config = PCBA_UNKNOW;
		}

		for (i = 0; i < sizeof(K19_PCBA)/sizeof(struct pcba_info); i++) {
			if (huaqin_pcba_config == K19_PCBA[i].pcba_config) {
				count = sprintf(buf, "%s\n", K19_PCBA[i].pcba_name);
				return count;
			}
		}
		count = sprintf(buf, "%s\n", "PCBA_UNKONW");
	/* BSP.Charge - 2020.11.26 - Add node to show pmic_version start */
	} else if (HWID_PMIC == hw->hw_id) {
		count = sprintf(buf, "%s\n", "PMIC: MT6360 V1.0");
	/* BSP.Charge - 2020.11.26 - Add node to show pmic_version end */
	/* BSP.Charge - 2020.11.26 - Show fuel_gauge_ic start */
	} else if (HWID_FUEL_GAUGE_IC == hw->hw_id) {
		count = sprintf(buf, "%s\n", "MT6365");
	/* BSP.Charge - 2020.11.26 - Show fuel_gauge_ic end */
	/* BSP.Lcm - 2020.12.04 - Show hw_info LCM - Start */
	} else if (HWID_LCM == hw->hw_id) {
		if (strstr(saved_command_line, "dsi_panel_K19_36_02_0a_vdo")) {
			count = sprintf(buf, "%s\n", "incell,vendor:36,IC:0a");
		} else if (strstr(saved_command_line, "dsi_panel_K19_36_02_0c_vdo")) {
			count = sprintf(buf, "%s\n", "incell,vendor:36,IC:0c");
		} else if (strstr(saved_command_line, "dsi_panel_K19_43_02_0b_vdo")) {
			count = sprintf(buf, "%s\n", "incell,vendor:43,IC:0b");
		} else {
			count = sprintf(buf, "LCM error\n");
		}
	/* BSP.Lcm - 2020.12.04 - Show hw_info LCM - End */
	} else {
		if (0 == hw->hw_exist) {
			count = sprintf(buf, "Not support\n");
		} else if (NULL == hw->hw_device_name) {
			count = sprintf(buf, "Installed with no device Name\n");
		} else {
			count = sprintf(buf, "%s\n", hw->hw_device_name);
		}
	}

	return count;
}

static ssize_t huaqin_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
	return count;
}

/* huaqin object */
static struct kobject huaqin_kobj;
static const struct sysfs_ops huaqin_sysfs_ops = {
	.show = huaqin_show,
	.store = huaqin_store,
};

/* huaqin type */
static struct kobj_type huaqin_ktype = {
	.sysfs_ops = &huaqin_sysfs_ops,
	.default_attrs = huaqin_attrs
};

/* huaqin device class */
static struct class  *huaqin_class;
static struct device *huaqin_hw_device;


int register_kboj_under_hqsysfs (struct kobject *kobj, struct kobj_type *ktype, const char *fmt, ...) {
	return kobject_init_and_add(kobj, ktype, &(huaqin_hw_device->kobj), fmt);
}

static int __init create_sysfs(void)
{
	int ret;

	/* create class (device model) */
	huaqin_class = class_create(THIS_MODULE, HUAQIN_CLASS_NAME);
	if (IS_ERR(huaqin_class)) {
		pr_err("%s fail to create class\n", __func__);
		return -1;
	}

	huaqin_hw_device = device_create(huaqin_class, NULL, MKDEV(0, 0), NULL, HUAIN_INTERFACE_NAME);
	if (IS_ERR(huaqin_hw_device)) {
		pr_warn("fail to create device\n");
		return -1;
	}

	/* add kobject */
	ret = kobject_init_and_add(&huaqin_kobj, &huaqin_ktype, &(huaqin_hw_device->kobj), HUAQIN_HWID_NAME);
	if (ret < 0) {
		pr_err("%s fail to add kobject\n", __func__);
		return ret;
	}

	return 0;
}

int hq_deregister_hw_info (enum hardware_id id, char *device_name) {
	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;

	struct hw_info *hw = NULL;
	struct attribute *attr = huaqin_attrs[iterator];

	if (NULL == device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}

	while (attr) {
		hw = container_of(attr, struct hw_info, attr);

		iterator += 1;
		attr = huaqin_attrs[iterator];

		if (NULL == hw) {
			continue;
		}

		if (id == hw->hw_id) {
			find_hw_id = 1;

			if (0 == hw->hw_exist) {
				pr_err("[%s]: device has not registed hw->id:0x%x . Cant be deregistered\n"
					, __func__
					, hw->hw_id);

				ret = -4;
				goto err;
			} else if (NULL == hw->hw_device_name) {

				pr_err("[%s]:hw_id is 0x%x Device name cant be NULL\n"
					, __func__
					, hw->hw_id);
				ret = -5;
				goto err;
			} else {
				if (0 == strncmp(hw->hw_device_name, device_name, strlen(hw->hw_device_name))) {
					hw->hw_device_name = NULL;
					hw->hw_exist = 0;
				} else {
					pr_err("[%s]: hw_id is 0x%x Registered device name %s , want to deregister: %s\n"
						, __func__
						, hw->hw_id
						, hw->hw_device_name
						, device_name);
					ret = -6;
					goto err;
				}
			}

			goto err;

		} else
			continue;

	}

	if (0 == find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}

err:
	return ret;

}

/* BSP.Audio - 2020.11.11 - modify to bring up second PA */
static char *audio_pa;
/* end modify*/
int hq_regiser_hw_info (enum hardware_id id, char *device_name) {

	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;

	struct hw_info *hw = NULL;
	struct attribute *attr = huaqin_attrs[iterator];

	if (NULL == device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}

	while (attr) {
		hw = container_of(attr, struct hw_info, attr);

		iterator += 1;
		attr = huaqin_attrs[iterator];

		if (NULL == hw) {
			continue;
		}

		if (id == hw->hw_id) {
			find_hw_id = 1;

			if (hw->hw_exist) {
				pr_err("[%s]: device has already registed hw->id:0x%x hw_device_name:%s\n"
					, __func__
					, hw->hw_id
					, hw->hw_device_name);
				ret = -4;
				goto err;
			}
/* BSP.Audio - 2020.11.11 - modify to bring up second PA */
			switch (id) {
			case HWID_AUDIO:
				audio_pa = device_name;
				break;
			}
/* end modify*/

//			switch (hw->hw_id) {
				/*
				if(map_cam_drv_to_vendor(device_name))
				    hw->hw_device_name = map_cam_drv_to_vendor(device_name);
				else
				    hw->hw_device_name = "Can't find Camera Vendor";
				break;
				*/
//			    default:
				hw->hw_device_name = device_name;
//				break;
//			}


			hw->hw_exist = 1;
			goto err;

		} else
			continue;

	}

	if (0 == find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}

err:
	return ret;
}

#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#define PROC_BOOT_REASON_FILE "boot_status"
#define SDC_DETECT_STATUS "sdc_det_gpio_status"

#define SDC_DETECT_GPIO  343

static struct proc_dir_entry *boot_reason_proc;
static struct proc_dir_entry *sdc_detect_status;
static unsigned int boot_into_factory;
static int boot_reason_proc_show(struct seq_file *file, void *data)
{
	char temp[40] = {0};

	sprintf(temp, "%d\n", boot_into_factory);
	seq_printf(file, "%s\n", temp);
	return 0;
}

static int boot_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_reason_proc_show, inode->i_private);
}

static const struct file_operations boot_reason_proc_fops = {
	.open = boot_reason_proc_open,
	.read = seq_read,
};

static int __init get_boot_rease(char *str)
{
	if (strcmp("boot_with_factory", str) == 0) {
		boot_into_factory = 1;
	}

	return 0;
}
__setup("androidboot.boot_reason=", get_boot_rease);


static int sdc_detect_proc_show(struct seq_file *file, void *data)
{
	int gpio_value = -1;

	gpio_value = gpio_get_value(SDC_DETECT_GPIO);
	seq_printf(file, "%d\n", gpio_value ? 1 : 0);
	return 0;
}

static int sdc_detect_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, sdc_detect_proc_show, inode->i_private);
}

static const struct file_operations sdc_detect_proc_fops = {
	.open = sdc_detect_proc_open,
	.read = seq_read,
};


static int sdc_detect_init(void)
{
	sdc_detect_status = proc_create(SDC_DETECT_STATUS, 0644, NULL, &sdc_detect_proc_fops);
	if (sdc_detect_status == NULL) {
		pr_err("[%s]: create_proc_entry sdc_detect_status failed\n", __func__);
		return -1;
	}
	return 0;
}

static int __init hq_harware_init(void)
{
	/* create sysfs entry at /sys/class/huaqin/interface/hw_info */
	create_sysfs();

	boot_reason_proc = proc_create(PROC_BOOT_REASON_FILE, 0644, NULL, &boot_reason_proc_fops);

	if (boot_reason_proc == NULL) {
		pr_err("[%s]: create_proc_entry boot_reason_proc failed\n", __func__);
	}
	if (sdc_detect_init() < 0)
		pr_err("[%s]: create_proc_entry sdc_detect_proc failed\n", __func__);

	return 0;
}

/* BSP.Audio - 2020.11.11 - modify to bring up second PA */
char *get_audio_pa_vendor(void)
{
	return audio_pa;
}
EXPORT_SYMBOL(get_audio_pa_vendor);
/* end modify*/
core_initcall(hq_harware_init);
MODULE_AUTHOR("KaKa Ni <nigang@huaqin.com>");
MODULE_DESCRIPTION("Huaqin Hardware Info Driver");
MODULE_LICENSE("GPL");
