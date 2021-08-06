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

/* ps_strm sensor driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <sensors_io.h>
#include "ps_factory_stream.h"
#include "situation.h"

#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "scp_helper.h"


static int ps_strm_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_PS_FACTORY_STRM, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*probability = data.gesture_data_t.probability;
	pr_err("recv ipi: timestamp: %lld, probability: %d!\n", time_stamp,
		*probability);
	return 0;
}

static int ps_strm_open_report_data(int open)
{
	int ret = 0;

	pr_info("%s : enable=%d\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_PS_FACTORY_STRM, 120);
#elif defined CONFIG_NANOHUB

#else

#endif

	ret = sensor_enable_to_hub(ID_PS_FACTORY_STRM, open);
	return ret;
}

static int ps_strm_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	pr_info("%s : flag=%d\n", __func__, flag);

	return sensor_batch_to_hub(ID_PS_FACTORY_STRM, flag, samplingPeriodNs,
			maxBatchReportLatencyNs);
}

static int ps_strm_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		pr_err("ps_strm do not support flush\n");
	else if (event->flush_action == DATA_ACTION)
		err = situation_data_report_t(ID_PS_FACTORY_STRM, (uint32_t)event->data[0], (int64_t)event->time_stamp);
	return err;
}

static int ps_strm_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = ps_strm_open_report_data;
	ctl.batch = ps_strm_batch;
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_PS_FACTORY_STRM);
	if (err) {
		pr_err("register ps_strm control path err\n");
		goto exit;
	}

	data.get_data = ps_strm_get_data;
	err = situation_register_data_path(&data, ID_PS_FACTORY_STRM);
	if (err) {
		pr_err("register ps_strm data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_PS_FACTORY_STRM, ps_strm_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int ps_strm_local_uninit(void)
{
	return 0;
}

static struct situation_init_info ps_strm_init_info = {
	.name = "ps_strm_hub",
	.init = ps_strm_local_init,
	.uninit = ps_strm_local_uninit,
};

static int __init ps_strm_init(void)
{
	situation_driver_add(&ps_strm_init_info, ID_PS_FACTORY_STRM);
	return 0;
}

static void __exit ps_strm_exit(void)
{
	pr_info("ps_strm exit\n");
}

module_init(ps_strm_init);
module_exit(ps_strm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("xiongyucong@xiaomi.com");

