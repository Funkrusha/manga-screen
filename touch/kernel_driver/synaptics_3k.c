/* drivers/input/touchscreen/synaptics_3k.c - Synaptics 3k serious touch panel driver
 *
 * Copyright (C) 2010 HTC Corporation.
 *
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/i2c/mms114.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
//#include <linux/irq.h>
#include <linux/interrupt.h>
//#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
//#include <linux/pm.h>
//#include <linux/earlysuspend.h>
//#include <linux/hrtimer.h>
//#include <linux/io.h>
//#include <linux/platform_device.h>
#include "synaptics_i2c_rmi.h"


#define DRIVER_NAME		"synaptics_ts"


struct synaptics_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *syn_wq;
	int use_irq;
	struct hrtimer timer;
	struct work_struct  work;
	uint16_t max[2];
	uint32_t flags;
	int8_t sensitivity_adjust;
	uint8_t finger_support;
	uint16_t finger_pressed;
	int (*power)(int on);
	struct page_description page_table[18];
	int pre_finger_data[11][2];
	uint8_t debug_log_level;
	uint32_t raw_base;
	uint32_t raw_ref;
	uint64_t timestamp;
	uint16_t *filter_level;
	uint8_t grip_suppression;
	uint8_t grip_b_suppression;
	uint8_t ambiguous_state;
};

static struct synaptics_ts_data *gl_ts;
static const char SYNAPTICSNAME[]	= "Synaptics_3K";
static uint32_t syn_panel_version;

static ssize_t touch_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s_%#x\n", SYNAPTICSNAME, syn_panel_version);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(vendor, 0444, touch_vendor_show, NULL);

static uint16_t syn_reg_addr;

static ssize_t register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	uint8_t data;
	struct synaptics_ts_data *ts;
	ts = gl_ts;

	data = i2c_smbus_read_byte_data(ts->client, syn_reg_addr);
	if (data < 0) {
		printk(KERN_WARNING "[TP] %s: read fail\n", __func__);
		return ret;
	}
	ret += sprintf(buf, "[TP] addr: 0x%X, data: 0x%X\n", syn_reg_addr, data);
	return ret;
}

static ssize_t register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct synaptics_ts_data *ts;
	char buf_tmp[4];
	uint8_t write_da;

	ts = gl_ts;
	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' &&
		(buf[5] == ':' || buf[5] == '\n')) {
		memcpy(buf_tmp, buf + 2, 3);
		syn_reg_addr = simple_strtol(buf_tmp, NULL, 16);
		printk(KERN_DEBUG "[TP] %s: set syn_reg_addr is: 0x%X\n",
						__func__, syn_reg_addr);
		if (buf[0] == 'w' && buf[5] == ':' && buf[9] == '\n') {
			memcpy(buf_tmp, buf + 6, 3);
			write_da = simple_strtol(buf_tmp, NULL, 10);
			printk(KERN_DEBUG "[TP] write addr: 0x%X, data: 0x%X\n",
						syn_reg_addr, write_da);
			ret = i2c_smbus_write_byte_data(ts->client,
					syn_reg_addr, write_da);
			if (ret < 0) {
				printk(KERN_ERR "[TP] %s: write fail(%d)\n",
								__func__, ret);
			}
		}
	}

	return count;
}

static DEVICE_ATTR(register, 0644, register_show, register_store);

static ssize_t debug_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts = gl_ts;

	return sprintf(buf, "%d\n", ts->debug_log_level);
}

static ssize_t debug_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_ts_data *ts = gl_ts;

	if (buf[0] >= '0' && buf[0] <= '9' && buf[1] == '\n')
		ts->debug_log_level = buf[0] - '0';

	return count;
}

static DEVICE_ATTR(debug_level, 0644, debug_level_show, debug_level_store);

static ssize_t syn_unlock_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_ts_data *ts;
	int unlock = -1;
	ts = gl_ts;

	if (buf[0] >= '0' && buf[0] <= '9' && buf[1] == '\n')
		unlock = buf[0] - '0';


	printk(KERN_INFO "[TP] unlock change to %d\n", unlock);

	if (unlock == 2) {
		ts->pre_finger_data[0][0] = 2;
#ifdef SYN_CALIBRATION_CONTROL
		i2c_smbus_write_byte_data(ts->client, ts->page_table[12].value - 1, 1);
#endif
		printk(KERN_INFO "[TP] %s: Touch Calibration Confirmed\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(unlock, (S_IWUSR|S_IRUGO),
	NULL, syn_unlock_store);

static struct kobject *android_touch_kobj;

static int synaptics_touch_sysfs_init(void)
{
	int ret;
	printk(KERN_ERR "synaptics touch fs init\n");

	android_touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR "touch_sysfs_init: sysfs_create_group failed\n");
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug_level.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	syn_reg_addr = 0;
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_register.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_unlock.attr);
	if (ret) {
		printk(KERN_ERR "%s: sysfs_create_file failed\n", __func__);
		return ret;
	}

	return 0;
}

static void synaptics_touch_sysfs_remove(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_debug_level.attr);
	kobject_del(android_touch_kobj);
}

static int synaptics_init_panel(struct synaptics_ts_data *ts)
{
	int ret = 0;

	if (ts->sensitivity_adjust) {
		ret = i2c_smbus_write_byte_data(ts->client,
			ts->page_table[2].value + 0x48, ts->sensitivity_adjust); /* Set Sensitivity */
		if (ret < 0)
			printk(KERN_ERR "[TP] TOUCH_ERR: i2c_smbus_write_byte_data failed for Sensitivity Set\n");
	}

	/* Position Threshold */
	i2c_smbus_write_byte_data(ts->client, ts->page_table[2].value+2, 3);
	i2c_smbus_write_byte_data(ts->client, ts->page_table[2].value+3, 3);

	/* 2D Gesture Enable */
	i2c_smbus_write_byte_data(ts->client, ts->page_table[2].value+10, 0);
	i2c_smbus_write_byte_data(ts->client, ts->page_table[2].value+11, 0);

	/* Configured */
	i2c_smbus_write_byte_data(ts->client, ts->page_table[8].value, 0x80);

	return ret;

}

static void synaptics_ts_work_func(struct work_struct *work)
{
	int i;
	int ret;
	struct i2c_msg msg[2];
	uint8_t start_reg;

	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);
	uint8_t buf[(ts->finger_support * 21 + 11) / 4];
	

	//ts->debug_log_level = 0x03;

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = ts->page_table[9].value;
	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;
	ret = i2c_transfer(ts->client->adapter, msg, 2);
	if (ret < 0 || ((buf[0] & 0x0F))) {
		if (ret < 0)
			printk(KERN_ERR "[TP] TOUCH_ERR: synaptics_ts_work_func: i2c_transfer failed\n");
		else
			printk(KERN_INFO "[TP] TOUCH_ERR: synaptics_ts_work_func: Status ERROR: %d\n", buf[0] & 0x0F);
		// reset touch control 
		if (ts->power) {
			ret = ts->power(0);
			if (ret < 0)
				printk(KERN_ERR "[TP] TOUCH_ERR: synaptics_ts_work_func power off failed\n");
			msleep(10);
			ret = ts->power(1);
			if (ret < 0)
				printk(KERN_ERR "[TP] synaptics_ts_work_func power on failed\n");
		} else {
			i2c_smbus_write_byte_data(ts->client, ts->page_table[7].value, 0x01);
			msleep(250);
		}
		synaptics_init_panel(ts);
		if (!ts->use_irq)
			hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		else
			i2c_smbus_write_byte_data(ts->client, ts->page_table[8].value + 1, 4);
	} else {
		
		int finger_data[ts->finger_support][4];
		int base = (ts->finger_support + 11) / 4;
		uint8_t loop_i, loop_j, finger_pressed = 0, finger_count = 0;
		uint8_t finger_press_changed = 0, finger_release_changed = 0;
		uint8_t pos_mask = 0x0f;

		if (ts->debug_log_level & 0x1) {
			printk("[TP] Touch:");
			for (i = 0; i < sizeof(buf); i++)
				printk(" %2x", buf[i]);
			printk("\n");
		}
		for (loop_i = 0; loop_i < ts->finger_support; loop_i++) {
			if (buf[1 + (ts->finger_support + 3) / 4] >> (loop_i * 2) & 0x03) {
				finger_pressed |= 1 << loop_i;
				finger_count++;
			} else if (ts->grip_suppression & BIT(loop_i)) {
				ts->grip_suppression &= ~BIT(loop_i);
				ts->grip_b_suppression &= ~BIT(loop_i);
			}
		}
		if (ts->finger_pressed != finger_pressed
			&& (ts->pre_finger_data[0][0] < 2 || ts->filter_level[0])) {
			finger_press_changed = ts->finger_pressed ^ finger_pressed;
			finger_release_changed = finger_press_changed & (~finger_pressed);
			finger_press_changed &= finger_pressed;
			ts->finger_pressed = finger_pressed;
		}
		if (finger_pressed == 0 ||
			((ts->grip_suppression | ts->grip_b_suppression) == finger_pressed && finger_release_changed)) {
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			ts->ambiguous_state = 0;
			ts->grip_b_suppression = 0;

			if (ts->debug_log_level & 0x2)
				printk(KERN_INFO "[TP] Finger leave\n");
		}
		if (ts->pre_finger_data[0][0] < 2 || finger_pressed) {
		for (loop_i = 0; loop_i < ts->finger_support; loop_i++) {
			uint32_t flip_flag = SYNAPTICS_FLIP_X;
			if ((((finger_pressed | finger_release_changed) >> loop_i) & 1) == 1) {
				pos_mask = 0x0f;
				for (loop_j = 0; loop_j < 2; loop_j++) {
					finger_data[loop_i][loop_j]
						= (buf[base+2] & pos_mask) >> (loop_j * 4) |
						(uint16_t)buf[base + loop_j] << 4;
					if (ts->flags & flip_flag)
						finger_data[loop_i][loop_j]
							= ts->max[loop_j] - finger_data[loop_i][loop_j];
					flip_flag <<= 1;
					pos_mask <<= 4;
				}
				finger_data[loop_i][2] = (buf[base+3] >> 4 & 0x0F) + (buf[base+3] & 0x0F);
				if (((buf[base+3] >> 4 & 0x0F) - (buf[base+3] & 0x0F)) >= 10)
					ts->grip_b_suppression |= BIT(loop_i);
				finger_data[loop_i][3] = buf[base+4];
				if (ts->flags & SYNAPTICS_SWAP_XY)
					swap(finger_data[loop_i][0], finger_data[loop_i][1]);
				if (((finger_release_changed >> loop_i) & 0x1) && ts->pre_finger_data[0][0] < 2) {
					printk(KERN_INFO "[TP] E%d@%d, %d\n", loop_i + 1,
					finger_data[loop_i][0], finger_data[loop_i][1]);
				}

				if (ts->filter_level[0] &&
					((finger_press_changed | ts->grip_suppression) & BIT(loop_i))) {
					if ((finger_data[loop_i][0] < (ts->filter_level[0] + ts->ambiguous_state * 20) ||
						finger_data[loop_i][0] > (ts->filter_level[3] - ts->ambiguous_state * 20)) &&
						!(ts->grip_suppression & BIT(loop_i))) {
						ts->grip_suppression |= BIT(loop_i);
					} else if ((finger_data[loop_i][0] < (ts->filter_level[1] + ts->ambiguous_state * 20) ||
						finger_data[loop_i][0] > (ts->filter_level[2] - ts->ambiguous_state * 20)) &&
						(ts->grip_suppression & BIT(loop_i)))
						ts->grip_suppression |= BIT(loop_i);
					else if (finger_data[loop_i][0] > (ts->filter_level[1] + ts->ambiguous_state * 20) &&
						finger_data[loop_i][0] < (ts->filter_level[2] - ts->ambiguous_state * 20)) {
						ts->grip_suppression &= ~BIT(loop_i);
					}
				}
				if ((ts->grip_suppression | ts->grip_b_suppression) & BIT(loop_i)) {
					finger_pressed &= ~(1 << loop_i);

				} else if (((finger_pressed >> loop_i) & 1) == 1) {
					finger_pressed &= ~(1 << loop_i);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
						finger_data[loop_i][3]);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
						finger_data[loop_i][2]);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						finger_data[loop_i][0]);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						finger_data[loop_i][1]);
					input_mt_sync(ts->input_dev);
					if (ts->pre_finger_data[0][0] < 2) {
						if ((finger_press_changed >> loop_i) & 0x1) {
							ts->pre_finger_data[loop_i + 1][0] = finger_data[loop_i][0];
							ts->pre_finger_data[loop_i + 1][1] = finger_data[loop_i][1];
							printk(KERN_INFO "[TP] S%d@%d, %d\n", loop_i + 1,
								ts->pre_finger_data[loop_i + 1][0], ts->pre_finger_data[loop_i + 1][1]);
							if (finger_count == ts->finger_support)
								i2c_smbus_write_byte_data(ts->client, ts->page_table[12].value - 1, 1);
							else if (!ts->pre_finger_data[0][0] && finger_count > 1)
								ts->pre_finger_data[0][0] = 1;
						}
					}
					if (ts->debug_log_level & 0x2)
						printk(KERN_INFO "[TP] Finger %d=> X:%d, Y:%d w:%d, z:%d\n",
							loop_i + 1, finger_data[loop_i][0],
							finger_data[loop_i][1], finger_data[loop_i][2],
							finger_data[loop_i][3]);
				}
				if (((finger_release_changed >> loop_i) & 0x1) && ts->pre_finger_data[0][0] < 2)
					i2c_smbus_write_byte_data(ts->client, ts->page_table[12].value - 1, 1);
				if (!finger_count)
					ts->pre_finger_data[0][0] = 0;
			}
			base += 5;
		}
		ts->ambiguous_state = 0;
		for (loop_i = 0; loop_i < ts->finger_support; loop_i++)
			if (((ts->grip_suppression >> loop_i) & 1) == 1)
				ts->ambiguous_state++;
		}	
	}
	input_sync(ts->input_dev);

	if (ts->debug_log_level & 0x4)
		printk(KERN_INFO "[TP] ts->grip_suppression: %x, ts->ambiguous_state: %x\n",
			ts->grip_suppression, ts->ambiguous_state);

	if (ts->use_irq)
		enable_irq(ts->client->irq);

}

static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);
	queue_work(ts->syn_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{
	struct synaptics_ts_data *ts = dev_id;
	disable_irq_nosync(ts->client->irq);
	queue_work(ts->syn_wq, &ts->work);
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static struct synaptics_i2c_rmi_platform_data *synaptics_ts_parse_dt(struct device *dev){
	struct synaptics_i2c_rmi_platform_data *pdata;
	struct device_node *np = dev->of_node;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return NULL;
	}

	if (of_property_read_u8(np, "version", &pdata->finger_support)) {		
		dev_warn(dev, "[TP] failed to get version property, defaulting to 0x0103\n");
		pdata->version = 0x0103;
	}

	if (of_property_read_u32(np, "display-height", &pdata->display_height)) {
		dev_warn(dev, "failed to get display-height property, defaulting to 800\n");
		pdata->display_height = 800;
	}

	if (of_property_read_u32(np, "abs-x-min", &pdata->abs_x_min)) {
		dev_warn(dev, "failed to get abs-x-min property, defaulting to 0\n");
		pdata->abs_x_min = 0;
	}
	if (of_property_read_u32(np, "abs-x-max", &pdata->abs_x_max)) {
		dev_warn(dev, "failed to get abs-x-max property, defaulting to 1172\n");
		pdata->abs_x_max = 1172;
	}

	if (of_property_read_u32(np, "abs-y-min", &pdata->abs_y_min)) {
		dev_warn(dev, "failed to get abs-y-min property, defaulting to 0\n");
		pdata->abs_y_min = 0;
	}

	if (of_property_read_u32(np, "abs-y-max", &pdata->abs_y_max)) {
		dev_warn(dev, "failed to get abs-y-max property, defaulting to 1900\n");
		pdata->abs_y_max = 1900;
	}

	if (of_property_read_u8(np, "sensitivity-adjust", &pdata->sensitivity_adjust)) {		
		dev_info(dev, "failed to get sensititivy-adjust property, defaulting to 0\n");
		pdata->sensitivity_adjust = 0;
	}

	if (of_property_read_u8	(np, "finger-support", &pdata->finger_support)) {		
		dev_info(dev, "failed to get finger-support property, defaulting to 4\n");
		pdata->finger_support = 4;
	}

	pdata->power = NULL;
	pdata->filter_level[0] = 20; 
	pdata->filter_level[1] = 60; 
	pdata->filter_level[2] = 1112; 
	pdata->filter_level[3] = 1152;
	
	return pdata;
}
#else
static struct synaptics_i2c_rmi_platform_data *synaptics_ts_parse_dt(struct device *dev){{
	return NULL;
}
#endif
static int synaptics_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	uint8_t loop_i, loop_j;
	int ret = 0;
	uint16_t max_x, max_y;
	struct synaptics_i2c_rmi_platform_data *pdata;
	uint32_t panel_version;
	
	printk("[TP] Synaptics probe\n");

	pdata = dev_get_platdata(&client->dev);
	if (!pdata)		
		pdata = synaptics_ts_parse_dt(&client->dev);
	if (!pdata) {
		dev_err(&client->dev, "Need platform data\n");
		ret = -EINVAL;
		goto err_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "[TP] synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	
	ts->client = client;
	i2c_set_clientdata(client, ts);
	//pdata = client->dev.platform_data;

	if (pdata)
		ts->power = pdata->power;
	if (ts->power)
		ret = ts->power(1);

	for (loop_i = 0; loop_i < 10; loop_i++) {
		if (i2c_smbus_read_byte_data(ts->client, 0x00DD) >= 0)
			break;
		msleep(10);
	}

	if (loop_i == 10) {
		printk(KERN_ERR "[TP] No Synaptics chip\n");
		goto err_detect_failed;
	}

	for (loop_i = 0x00DD, loop_j = 0; loop_i <= 0x00EE; loop_i++) {
		ts->page_table[loop_j].addr = loop_i;
		ts->page_table[loop_j].value = i2c_smbus_read_byte_data(ts->client, loop_i);
/*		printk("[0x%2.2X]%2.2X\n", ts->page_table[loop_j].addr, ts->page_table[loop_j].value); */
		while (ts->page_table[loop_j].value != i2c_smbus_read_byte_data(ts->client, loop_i))
			ts->page_table[loop_j].value = i2c_smbus_read_byte_data(ts->client, loop_i);
		loop_j++;
	}

	panel_version =
		i2c_smbus_read_byte_data(ts->client, ts->page_table[6].value + 3) |
		i2c_smbus_read_byte_data(ts->client, ts->page_table[6].value + 2) << 8;
	printk(KERN_INFO "[TP] %s: panel_version: %x\n", __func__, panel_version);
	syn_panel_version = panel_version;

	if (pdata) {
		while (pdata->version > panel_version) {
			printk(KERN_INFO "[TP] synaptics_ts_probe: old tp detected, "
					"panel version = %x\n", panel_version);
			pdata++;
		}
		ts->flags = pdata->flags;
		ts->sensitivity_adjust = pdata->sensitivity_adjust;
		ts->finger_support = pdata->finger_support;
		ts->filter_level = pdata->filter_level;
	}

	ts->max[0] = max_x =
		i2c_smbus_read_byte_data(ts->client, ts->page_table[2].value+6) |
		i2c_smbus_read_byte_data(ts->client, ts->page_table[2].value+7) << 8;

	ts->max[1] = max_y =
		i2c_smbus_read_byte_data(ts->client, ts->page_table[2].value+8) |
		i2c_smbus_read_byte_data(ts->client, ts->page_table[2].value+9) << 8;
	printk(KERN_INFO"[TP] max_x: %X, max_y: %X\n", max_x, max_y);

	if (pdata->abs_x_min == pdata->abs_x_max && pdata->abs_y_min == pdata->abs_y_max) {
		pdata->abs_x_min = 0;
		pdata->abs_x_max = max_x;
		pdata->abs_y_min = 0;
		pdata->abs_y_max = max_y;
	}
	if (pdata->display_height) {
		ts->raw_ref = 115 * ((pdata->abs_y_max - pdata->abs_y_min) +
			pdata->abs_y_min) / pdata->display_height;
		ts->raw_base = 650 * ((pdata->abs_y_max - pdata->abs_y_min) +
			pdata->abs_y_min) / pdata->display_height;
		printk(KERN_INFO "[TP] ts->raw_ref: %d, ts->raw_base: %d\n", ts->raw_ref, ts->raw_base);
	} else {
		ts->raw_ref = 0;
		ts->raw_base = 0;
	}
	ret = synaptics_init_panel(ts);
	ts->timestamp = jiffies + 60 * HZ;
	if (ret < 0) {
		printk(KERN_ERR "[TP] TOUCH_ERR: synaptics_init_panel failed\n");
		goto err_detect_failed;
	}
	printk(KERN_INFO "[TP] synaptics_init_panel OK\n");
	ts->syn_wq = create_singlethread_workqueue("synaptics_wq");
	if (!ts->syn_wq)
		goto err_create_wq_failed;

	INIT_WORK(&ts->work, synaptics_ts_work_func);
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_ERR "[TP] TOUCH_ERR: synaptics_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "synaptics-rmi-touchscreen";
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_ABS_X, ts->input_dev->evbit);
	set_bit(EV_ABS_Y, ts->input_dev->evbit);

	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);

	printk(KERN_INFO "[TP] synaptics_ts_probe: max_x %d, max_y %d\n", max_x, max_y);
	printk(KERN_INFO "[TP] input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
		pdata->abs_x_min, pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, pdata->abs_x_min, pdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, pdata->abs_y_min, pdata->abs_y_max, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 30, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		printk(KERN_ERR "[TP] TOUCH_ERR: synaptics_ts_probe: "
				"Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	gl_ts = ts;

	if (client->irq) {
		ret = request_irq(client->irq, synaptics_ts_irq_handler, IRQF_TRIGGER_LOW,
				client->name, ts);
		if (ret == 0) {
			/* enable abs int */
			ret = i2c_smbus_write_byte_data(ts->client, ts->page_table[8].value + 1, 4);
			if (ret)
				free_irq(client->irq, ts);
		}
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "[TP] TOUCH_ERR: request_irq failed\n");
	}
	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	synaptics_touch_sysfs_init();

	printk(KERN_INFO "[TP] synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	destroy_workqueue(ts->syn_wq);

err_create_wq_failed:
err_detect_failed:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);

	synaptics_touch_sysfs_remove();

	kfree(ts);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int synaptic_ts_suspend(struct device *dev){	
	return 0;
}
static int synaptic_ts_resume(struct device *dev){
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(synaptic_ts_pm_ops, synaptic_ts_suspend, synaptic_ts_resume);

static const struct i2c_device_id synaptic_ts_id[] = {
	{ "3k", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, synaptic_ts_id);

#ifdef CONFIG_OF
static struct of_device_id synaptic_ts_dt_match[] = {
	{ .compatible = "synaptic,3k" },
	{ }
};
#endif

static struct i2c_driver synaptics_ts_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm		= &synaptic_ts_pm_ops,
		.of_match_table = of_match_ptr(synaptic_ts_dt_match),
	},
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
	.id_table	= synaptic_ts_id,
};

module_i2c_driver(synaptics_ts_driver);

/* Module information */
MODULE_DESCRIPTION("Synaptics 3K touchscreen driver");
MODULE_AUTHOR("Mike Rapoport, Igor Grinberg, Compulab, Elias Bakken");
MODULE_LICENSE("GPL");
