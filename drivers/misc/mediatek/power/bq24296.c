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

#include <linux/i2c.h>
#include <linux/init.h>   /* For init/exit macros */
#include <linux/module.h> /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include "bq24296.h"
#include <mt-plat/charging.h>

/**********************************************************
 *
 *	[I2C Slave Setting]
 *
 *********************************************************/
#define bq24296_SLAVE_ADDR_WRITE 0xD6
#define bq24296_SLAVE_ADDR_READ 0xD7

#ifdef CONFIG_OF
static const struct of_device_id bq24296_id[] = {
	{.compatible = "ti,bq24296"}, {},
};
MODULE_DEVICE_TABLE(of, bq24296_id);
#endif

static struct i2c_client *new_client;
static const struct i2c_device_id bq24296_i2c_id[] = {{"bq24296", 0}, {} };

enum kal_bool chargin_hw_init_done = KAL_FALSE;
static int bq24296_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id);

static void bq24296_shutdown(struct i2c_client *client)
{
	battery_log(BAT_LOG_FULL, "[%s] driver shutdown\n", __func__);
	bq24296_set_chg_config(0x0);
}
static struct i2c_driver bq24296_driver = {
	.driver = {


			.name = "bq24296",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(bq24296_id),
#endif
		},
	.probe = bq24296_driver_probe,
	.id_table = bq24296_i2c_id,
	.shutdown = bq24296_shutdown,
};

/**********************************************************
 *
 *[Global Variable]
 *
 *********************************************************/
#define bq24296_REG_NUM 11
unsigned char bq24296_reg[bq24296_REG_NUM] = {0};

static DEFINE_MUTEX(bq24296_i2c_access);
/**********************************************************
 *
 *	[I2C Function For Read/Write bq24296]
 *
 *********************************************************/
int bq24296_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char readData = 0;
	int ret = 0;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap = new_client->adapter;

	mutex_lock(&bq24296_i2c_access);
	msg[0].addr = new_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &cmd;

	msg[1].addr = new_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &readData;

	ret = i2c_transfer(adap, msg, 2);
	if (ret < 0) {
		mutex_unlock(&bq24296_i2c_access);
		return 0;
	}
	*returnData = readData;

	mutex_unlock(&bq24296_i2c_access);
	return 1;
}

int bq24296_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = {0};
	int ret = 0;
	struct i2c_msg msg;
	struct i2c_adapter *adap = new_client->adapter;

	mutex_lock(&bq24296_i2c_access);
	write_data[0] = cmd;
	write_data[1] = writeData;
	msg.addr = new_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = (char *)write_data;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret < 0) {
		mutex_unlock(&bq24296_i2c_access);
		return 0;
	}

	mutex_unlock(&bq24296_i2c_access);
	return 1;
}

/**********************************************************
 *
 *   [Read / Write Function]
 *
 *********************************************************/
unsigned int bq24296_read_interface(unsigned char RegNum, unsigned char *val,
				    unsigned char MASK, unsigned char SHIFT)
{
	unsigned char bq24296_reg = 0;
	int ret = 0;

	battery_log(BAT_LOG_FULL,
		    "--------------------------------------------------\n");

	ret = bq24296_read_byte(RegNum, &bq24296_reg);

	battery_log(BAT_LOG_FULL, "[%s] Reg[%x]=0x%x\n", __func__, RegNum,
		    bq24296_reg);

	bq24296_reg &= (MASK << SHIFT);
	*val = (bq24296_reg >> SHIFT);

	battery_log(BAT_LOG_FULL, "[%s] val=0x%x\n", __func__, *val);

	return ret;
}

unsigned int bq24296_config_interface(unsigned char RegNum, unsigned char val,
				      unsigned char MASK, unsigned char SHIFT)
{
	unsigned char bq24296_reg = 0;
	int ret = 0;

	battery_log(BAT_LOG_FULL,
		    "--------------------------------------------------\n");

	ret = bq24296_read_byte(RegNum, &bq24296_reg);
	battery_log(BAT_LOG_FULL, "[%s] Reg[%x]=0x%x\n", __func__, RegNum,
		    bq24296_reg);

	bq24296_reg &= ~(MASK << SHIFT);
	bq24296_reg |= (val << SHIFT);

	ret = bq24296_write_byte(RegNum, bq24296_reg);
	battery_log(BAT_LOG_FULL, "[%s] write Reg[%x]=0x%x\n", __func__,
		    RegNum, bq24296_reg);

	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */

void bq24296_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON0),
				       (unsigned char)(val),
				       (unsigned char)(CON0_EN_HIZ_MASK),
				       (unsigned char)(CON0_EN_HIZ_SHIFT));
}

void bq24296_set_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON0),
				       (unsigned char)(val),
				       (unsigned char)(CON0_VINDPM_MASK),
				       (unsigned char)(CON0_VINDPM_SHIFT));
}

void bq24296_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON0),
				       (unsigned char)(val),
				       (unsigned char)(CON0_IINLIM_MASK),
				       (unsigned char)(CON0_IINLIM_SHIFT));
}

/* CON1---------------------------------------------------- */

void bq24296_set_reg_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_REG_RST_MASK),
				       (unsigned char)(CON1_REG_RST_SHIFT));
}

void bq24296_set_wdt_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_WDT_RST_MASK),
				       (unsigned char)(CON1_WDT_RST_SHIFT));
}

void bq24296_set_otg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_OTG_CONFIG_MASK),
				       (unsigned char)(CON1_OTG_CONFIG_SHIFT));
}

void bq24296_set_chg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_CHG_CONFIG_MASK),
				       (unsigned char)(CON1_CHG_CONFIG_SHIFT));
}

void bq24296_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_SYS_MIN_MASK),
				       (unsigned char)(CON1_SYS_MIN_SHIFT));
}

void bq24296_set_boost_lim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON1),
				       (unsigned char)(val),
				       (unsigned char)(CON1_BOOST_LIM_MASK),
				       (unsigned char)(CON1_BOOST_LIM_SHIFT));
}

/* CON2---------------------------------------------------- */

void bq24296_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON2),
				       (unsigned char)(val),
				       (unsigned char)(CON2_ICHG_MASK),
				       (unsigned char)(CON2_ICHG_SHIFT));
}

void bq24296_set_bcold(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON2),
				       (unsigned char)(val),
				       (unsigned char)(CON2_BCOLD_MASK),
				       (unsigned char)(CON2_BCOLD_SHIFT));
}

void bq24296_set_force_20pct(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON2),
				       (unsigned char)(val),
				       (unsigned char)(CON2_FORCE_20PCT_MASK),
				       (unsigned char)(CON2_FORCE_20PCT_SHIFT));
}

/* CON3---------------------------------------------------- */

void bq24296_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON3),
				       (unsigned char)(val),
				       (unsigned char)(CON3_IPRECHG_MASK),
				       (unsigned char)(CON3_IPRECHG_SHIFT));
}

void bq24296_set_iterm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON3),
				       (unsigned char)(val),
				       (unsigned char)(CON3_ITERM_MASK),
				       (unsigned char)(CON3_ITERM_SHIFT));
}

/* CON4---------------------------------------------------- */

void bq24296_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON4),
				       (unsigned char)(val),
				       (unsigned char)(CON4_VREG_MASK),
				       (unsigned char)(CON4_VREG_SHIFT));
}

void bq24296_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON4),
				       (unsigned char)(val),
				       (unsigned char)(CON4_BATLOWV_MASK),
				       (unsigned char)(CON4_BATLOWV_SHIFT));
}

void bq24296_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON4),
				       (unsigned char)(val),
				       (unsigned char)(CON4_VRECHG_MASK),
				       (unsigned char)(CON4_VRECHG_SHIFT));
}

/* CON5---------------------------------------------------- */

void bq24296_set_en_term(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON5),
				       (unsigned char)(val),
				       (unsigned char)(CON5_EN_TERM_MASK),
				       (unsigned char)(CON5_EN_TERM_SHIFT));
}

void bq24296_set_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON5),
				       (unsigned char)(val),
				       (unsigned char)(CON5_WATCHDOG_MASK),
				       (unsigned char)(CON5_WATCHDOG_SHIFT));
}

void bq24296_set_en_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON5),
				       (unsigned char)(val),
				       (unsigned char)(CON5_EN_TIMER_MASK),
				       (unsigned char)(CON5_EN_TIMER_SHIFT));
}

void bq24296_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON5),
				       (unsigned char)(val),
				       (unsigned char)(CON5_CHG_TIMER_MASK),
				       (unsigned char)(CON5_CHG_TIMER_SHIFT));
}

/* CON6---------------------------------------------------- */

void bq24296_set_treg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON6),
				       (unsigned char)(val),
				       (unsigned char)(CON6_TREG_MASK),
				       (unsigned char)(CON6_TREG_SHIFT));
}

void bq24296_set_boostv(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON6),
				       (unsigned char)(val),
				       (unsigned char)(CON6_BOOSTV_MASK),
				       (unsigned char)(CON6_BOOSTV_SHIFT));
}

void bq24296_set_bhot(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON6),
				       (unsigned char)(val),
				       (unsigned char)(CON6_BHOT_MASK),
				       (unsigned char)(CON6_BHOT_SHIFT));
}

/* CON7---------------------------------------------------- */

void bq24296_set_tmr2x_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON7),
				       (unsigned char)(val),
				       (unsigned char)(CON7_TMR2X_EN_MASK),
				       (unsigned char)(CON7_TMR2X_EN_SHIFT));
}

void bq24296_set_batfet_disable(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface(
		(unsigned char)(bq24296_CON7), (unsigned char)(val),
		(unsigned char)(CON7_BATFET_Disable_MASK),
		(unsigned char)(CON7_BATFET_Disable_SHIFT));
}

void bq24296_set_int_mask(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24296_config_interface((unsigned char)(bq24296_CON7),
				       (unsigned char)(val),
				       (unsigned char)(CON7_INT_MASK_MASK),
				       (unsigned char)(CON7_INT_MASK_SHIFT));
}

/* CON8---------------------------------------------------- */

unsigned int bq24296_get_system_status(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24296_read_interface((unsigned char)(bq24296_CON8), (&val),
				     (unsigned char)(0xFF),
				     (unsigned char)(0x0));
	return val;
}

unsigned int bq24296_get_vbus_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24296_read_interface((unsigned char)(bq24296_CON8), (&val),
				     (unsigned char)(CON8_VBUS_STAT_MASK),
				     (unsigned char)(CON8_VBUS_STAT_SHIFT));
	return val;
}

unsigned int bq24296_get_chrg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24296_read_interface((unsigned char)(bq24296_CON8), (&val),
				     (unsigned char)(CON8_CHRG_STAT_MASK),
				     (unsigned char)(CON8_CHRG_STAT_SHIFT));
	return val;
}

unsigned int bq24296_get_vsys_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24296_read_interface((unsigned char)(bq24296_CON8), (&val),
				     (unsigned char)(CON8_VSYS_STAT_MASK),
				     (unsigned char)(CON8_VSYS_STAT_SHIFT));
	return val;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
void bq24296_dump_register(void)
{
	int i = 0;

	battery_log(BAT_LOG_FULL, "[bq24296] ");
	for (i = 0; i < bq24296_REG_NUM; i++) {
		bq24296_read_byte(i, &bq24296_reg[i]);
		battery_log(BAT_LOG_FULL, "[0x%x]=0x%x ", i, bq24296_reg[i]);
	}
	battery_log(BAT_LOG_FULL, "\n");
}

static int bq24296_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{

	pr_notice("[%s]\n", __func__);

	new_client = client;

	/* --------------------- */
	/*bq24296_hw_component_detect();*/
	bq24296_dump_register();
	chargin_hw_init_done = KAL_TRUE;

	return 0;
}

/**********************************************************
 *
 *	[platform_driver API]
 *
 *********************************************************/
unsigned char g_reg_value_bq24296;

static ssize_t show_bq24296_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	battery_log(BAT_LOG_FULL, "[%s] 0x%x\n", __func__, g_reg_value_bq24296);
	return sprintf(buf, "%u\n", g_reg_value_bq24296);
}
static ssize_t store_bq24296_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	battery_log(BAT_LOG_FULL, "[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		battery_log(BAT_LOG_FULL, "[%s] buf is %s and size is %zu\n",
			    __func__, buf, size);
		/*reg_address = kstrtoul(buf, 16, &pvalue);*/

		pvalue = (char *)buf;
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16,
					(unsigned int *)&reg_address);

		if (size > 3) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			battery_log(
				BAT_LOG_FULL,
				"[%s] write bq24296 reg 0x%x with value 0x%x !\n",
				__func__, reg_address, reg_value);
			ret = bq24296_config_interface(reg_address, reg_value,
						       0xFF, 0x0);
		} else {
			ret = bq24296_read_interface(
				reg_address, &g_reg_value_bq24296, 0xFF, 0x0);
			battery_log(
				BAT_LOG_FULL,
				"[%s] read bq24296 reg 0x%x with value 0x%x !\n",
				__func__, reg_address, g_reg_value_bq24296);
			battery_log(
				BAT_LOG_FULL,
				"[%s] use \"cat bq24296_access\" to get value\r\n",
				__func__);
		}
	}
	return size;
}
static DEVICE_ATTR(bq24296_access, 0664, show_bq24296_access,
		   store_bq24296_access);

static int bq24296_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_notice("******** %s!! ********\n", __func__);

	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_bq24296_access);

	return 0;
}

struct platform_device bq24296_user_space_device = {
	.name = "bq24296-user", .id = -1,
};

static struct platform_driver bq24296_user_space_driver = {
	.probe = bq24296_user_space_probe,
	.driver = {


			.name = "bq24296-user",
		},
};

static int __init bq24296_init(void)
{
	int ret = 0;

	pr_notice("[%s] init start\n", __func__);

#ifndef CONFIG_OF
	i2c_register_board_info(BQ24296_BUSNUM, &i2c_bq24296, 1);
#endif

	if (i2c_add_driver(&bq24296_driver) != 0)
		pr_notice("[%s] failed to register bq24296 i2c driver.\n",
			  __func__);
	else
		pr_notice("[%s] Success to register bq24296 i2c driver.\n",
			  __func__);

	/*bq24296 user space access interface*/
	ret = platform_device_register(&bq24296_user_space_device);
	if (ret) {
		pr_notice("****[%s] Unable to device register(%d)\n", __func__,
			  ret);
		return ret;
	}
	ret = platform_driver_register(&bq24296_user_space_driver);
	if (ret) {
		pr_notice("****[%s] Unable to register driver (%d)\n", __func__,
			  ret);
		return ret;
	}
	return 0;
}

static void __exit bq24296_exit(void)
{
	i2c_del_driver(&bq24296_driver);
}
module_init(bq24296_init);
module_exit(bq24296_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24296 Driver");
MODULE_AUTHOR("YT Lee<yt.lee@mediatek.com>");
