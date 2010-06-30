/* arch/arm/mach-msm/board-incrediblec-microp.c
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#ifdef CONFIG_MICROP_COMMON
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <mach/atmega_microp.h>
#include <linux/capella_cm3602.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>

#include "board-incrediblec.h"


#define INT_PSENSOR	(1<<11)

static int misc_opened;
static struct i2c_client *incrediblec_microp_client;
/*
static struct led_trigger incrediblec_als_level_trigger = {
	.name     = "auto-backlight-trigger",
};
*/
/*
static void incrediblec_ap_key_led_delay(struct work_struct *work);
static DECLARE_DELAYED_WORK(incrediblec_notifier_delay_work,
		incrediblec_ap_key_led_delay);
*/
struct wake_lock proximity_wake_lock;

static struct capella_cm3602_data {
	struct input_dev *input_dev;
	struct capella_cm3602_platform_data *pdata;
	int enabled;
} the_data;
/*

static int incrediblec_als_intr_enable(struct i2c_client *client,
		uint32_t als_func, uint8_t enable)
{

	struct microp_i2c_client_data *cdata;

	cdata = i2c_get_clientdata(client);

	return microp_write_interrupt(client,
		cdata->int_pin.int_lsensor, enable);
*/
/*
	struct microp_i2c_client_data *cdata;
	uint8_t data[2];
	int ret = 0;

	cdata = i2c_get_clientdata(client);
	mutex_lock(&cdata->microp_i2c_mutex);
	cdata->als_func = enable ? (cdata->als_func |= als_func)
				: (cdata->als_func &= ~als_func);

	data[0] = 0;
	if (cdata->als_func)
		data[1] = 1;
	else
		data[1] = 0;

	ret = microp_i2c_write(MICROP_I2C_WCMD_AUTO_BL_CTL, data, 2);
	if (ret != 0)
		printk(KERN_ERR "%s: set auto light sensor fail\n", __func__);

	mutex_unlock(&cdata->microp_i2c_mutex);

	return ret;
*/

/*
}
*/

static int psensor_intr_enable(uint8_t enable)
{
	int ret;
	uint8_t addr, data[2];

	if (enable)
		addr = MICROP_I2C_WCMD_GPI_INT_CTL_EN;
	else
		addr = MICROP_I2C_WCMD_GPI_INT_CTL_DIS;

	data[0] = INT_PSENSOR >> 8;
	data[1] = INT_PSENSOR & 0xFF;
	ret = microp_i2c_write(addr, data, 2);
	if (ret < 0)
		pr_err("%s: %s p-sensor interrupt failed\n",
			__func__, (enable ? "enable" : "disable"));

	return ret;
}

/*
static int incrediblec_als_table_init(struct i2c_client *client,
			int i, uint32_t kadc, uint32_t gadc)
{
	struct microp_i2c_platform_data *pdata;
	uint8_t data[20];
	int j;

	pdata = client->dev.platform_data;

	for (j = 0; j < 10; j++) {
		data[j] = (uint8_t)(pdata->microp_function[i].levels[j]
				* kadc / gadc >> 8);
		data[j + 10] = (uint8_t)(pdata->microp_function[i].levels[j]
				* kadc / gadc);
	}

	return microp_i2c_write(MICROP_I2C_WCMD_ADC_TABLE, data, 20);
}
*/
/*
static void incrediblec_vkey_led_enable(int on)
{
	int ret;

	ret = gpio_direction_output(INCREDIBLEC_AP_KEY_LED_EN, on);
	if (ret < 0)
		pr_err("%s: failed on set AP Key LED=%d\n", __func__, on);
}

static void incrediblec_vkey_led_control(struct microp_led_data *ldata)
{
	incrediblec_vkey_led_enable((int)ldata->mode);
}
*/
/*
static void incrediblec_als_level_change(struct i2c_client *client,
		uint8_t *data)
{
	struct microp_i2c_client_data *cdata  = i2c_get_clientdata(client);
	int on = -1;

	if (cdata->als_func & ALS_VKEY_LED) {
		if (data[2] >= 4)
			on = 0;
		else if (data[2] <= 2)
			on = 1;

		if (on >= 0)
			incrediblec_vkey_led_enable(on);
	}
}

static void incrediblec_ap_key_led_delay(struct work_struct *work)
{
	struct i2c_client *client = incrediblec_microp_client;
	struct microp_led_data *ldata = incrediblec_vkey_backlight;
	uint8_t data[2];
	int ret = 0;

	data[0] = 0;
	data[1] = 0;
	ret = microp_i2c_write(MICROP_I2C_WCMD_AUTO_BL_CTL, data, 2);
	if (ret != 0)
		printk(KERN_ERR "%s: set auto light sensor disable fail\n",
			__func__);

	if (ldata->mode == 0)
		return;

	ret = incrediblec_als_intr_enable(client, ALS_VKEY_LED, 1);
	if (ret != 0)
		printk(KERN_ERR "%s: set auto light sensor enable fail\n",
			__func__);
}

static void incrediblec_notifier_func(struct i2c_client *client,
			struct microp_led_data *ldata)
{
	struct microp_i2c_client_data *cdata;
	unsigned long delay_time;
	int on;

	cdata = i2c_get_clientdata(client);
	incrediblec_vkey_backlight = ldata;
	cancel_delayed_work(&incrediblec_notifier_delay_work);

	on = ldata->ldev.brightness ? 1 : 0;
	incrediblec_vkey_led_enable(on);
	cdata->als_func &= ~ALS_VKEY_LED;

	if (ldata->mode)
		delay_time = HZ * 10;
	else
		delay_time = 0;
	queue_delayed_work(cdata->microp_queue,
			&incrediblec_notifier_delay_work, delay_time);
}
*/
/*
static int incrediblec_als_power_init(void)
{
	return incrediblec_als_power(LS_PWR_ON, 1);
}
*/
static int incrediblec_microp_function_init(struct i2c_client *client)
{
	struct microp_i2c_platform_data *pdata;
	struct microp_i2c_client_data *cdata;
	uint8_t data[20];
	int i, j;
	int ret;

	incrediblec_microp_client = client;
	pdata = client->dev.platform_data;
	cdata = i2c_get_clientdata(client);

	/* Light sensor */
/*
	ret = microp_function_check(client, MICROP_FUNCTION_LSENSOR);
	if (ret >= 0) {
		i = ret;
		pdata->function_node[MICROP_FUNCTION_LSENSOR] = i;
		cdata->int_pin.int_lsensor = pdata->microp_function[i].int_pin;
		microp_get_als_kvalue(i);

		ret = incrediblec_als_table_init(client, i,
				cdata->als_kadc, cdata->als_gadc);
		if (ret < 0)
			goto exit;

		incrediblec_als_power_init();
	}
*/
	/* Headset remote key */
	ret = microp_function_check(client, MICROP_FUNCTION_REMOTEKEY);
	if (ret >= 0) {
		i = ret;
		pdata->function_node[MICROP_FUNCTION_REMOTEKEY] = i;
		cdata->int_pin.int_remotekey =
				pdata->microp_function[i].int_pin;

		for (j = 0; j < 6; j++) {
			data[j] = (uint8_t)(pdata->microp_function[i].levels[j] >> 8);
			data[j + 6] = (uint8_t)(pdata->microp_function[i].levels[j]);
		}
		ret = microp_i2c_write(MICROP_I2C_WCMD_REMOTEKEY_TABLE,
				data, 12);
		if (ret)
			goto exit;
	}

	/* Reset button interrupt */
	data[0] = 0x08;
	ret = microp_i2c_write(MICROP_I2C_WCMD_MISC, data, 1);
	if (ret)
		goto exit;

	/* OJ interrupt */
	ret = microp_function_check(client, MICROP_FUNCTION_OJ);
	if (ret >= 0) {
		i = ret;
		cdata->int_pin.int_oj = pdata->microp_function[i].int_pin;

		ret = microp_write_interrupt(client, cdata->int_pin.int_oj, 1);
		if (ret)
			goto exit;
	}

	/* Proximity interrupt */
	ret = microp_function_check(client, MICROP_FUNCTION_P);
	if (ret >= 0) {
		i = ret;
		cdata->int_pin.int_psensor = pdata->microp_function[i].int_pin;
		cdata->gpio.psensor = pdata->microp_function[i].mask_r[0] << 16
				| pdata->microp_function[i].mask_r[1] << 8
				| pdata->microp_function[i].mask_r[2];
				cdata->fnode.psensor = i;
	}

	return 0;

exit:
	return ret;
}

static void psensor_init_callback(void);

void incrediblec_psensor_irq(int ps_data)
{
	if (ps_data < 0 || ps_data > 1) {
		pr_err("%s: interrupt data from microp error, ps_data = %d\n",
			__func__, ps_data);
		return;
	}

	pr_info("proximity %s\n", ps_data ? "FAR" : "NEAR");

	/* 0 is close, 1 is far */
	input_report_abs(the_data.input_dev, ABS_DISTANCE, ps_data);
	input_sync(the_data.input_dev);

	wake_lock_timeout(&proximity_wake_lock, 2*HZ);
}

static struct microp_psensor_callback ps_callback = {
	.ps_init = psensor_init_callback,
	.ps_intr = incrediblec_psensor_irq
};

static void psensor_init_callback(void)
{
	pr_info("%s\n", __func__);

	if (microp_register_ps_callback(&ps_callback))
		pr_err("%s: capella_cm3602 init before Microp!!\n",
			__func__);
}

static int report_psensor_data(void)
{
	int ret, ps_data = 0;
	uint8_t data[3] = {0, 0, 0};

	ret = microp_i2c_read(MICROP_I2C_RCMD_GPIO_STATUS, data, 3);
	if (ret < 0)
		pr_err("%s: read data failed\n", __func__);
	else {
		ps_data = (data[2] & 0x10) ? 1 : 0;
		incrediblec_psensor_irq(ps_data);
	}

	return ret;
}

static int capella_cm3602_enable(struct capella_cm3602_data *data)
{
	int rc;
	pr_info("%s\n", __func__);
	if (data->enabled) {
		pr_info("%s: already enabled\n", __func__);
		return 0;
	}

	/* dummy report */
	input_report_abs(data->input_dev, ABS_DISTANCE, -1);
	input_sync(data->input_dev);

	rc = data->pdata->power(PS_PWR_ON, 1);
	if (rc < 0)
		return -EIO;

	rc = gpio_direction_output(data->pdata->p_en, 0);
	if (rc < 0) {
		pr_err("%s: set psesnor enable failed!!",
			__func__);
		return -EIO;
	}
	msleep(220);
	rc = psensor_intr_enable(1);
	if (rc < 0)
		return -EIO;

	data->enabled = 1;
	report_psensor_data();

	return rc;
}

static int capella_cm3602_disable(struct capella_cm3602_data *data)
{
	int rc = -EIO;
	pr_info("%s\n", __func__);
	if (!data->enabled) {
		pr_info("%s: already disabled\n", __func__);
		return 0;
	}

	rc = psensor_intr_enable(0);
	if (rc < 0)
		return -EIO;

	rc = gpio_direction_output(data->pdata->p_en, 1);
	if (rc < 0) {
		pr_err("%s: set GPIO failed!!", __func__);
		return -EIO;
	}

	rc = data->pdata->power(PS_PWR_ON, 0);
	if (rc < 0)
		return -EIO;

	data->enabled = 0;
	return rc;
}

static ssize_t capella_cm3602_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "proximity enabled = %d\n", the_data.enabled);

	return ret;
}

static ssize_t capella_cm3602_store(struct device *dev,
			struct device_attribute *attr,
			const char              *buf,
			size_t                  count
			)
{
	ssize_t val;

	val = -1;
	sscanf(buf, "%u", &val);
	if (val < 0 || val > 1)
		return -EINVAL;

	/* Enable capella_cm3602*/
	if (val == 1)
		capella_cm3602_enable(&the_data);

	/* Disable capella_cm3602*/
	if (val == 0)
		capella_cm3602_disable(&the_data);

	return count;
}

static DEVICE_ATTR(proximity, 0644, capella_cm3602_show, capella_cm3602_store);

static int capella_cm3602_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	if (misc_opened)
		return -EBUSY;
	misc_opened = 1;
	return 0;
}

static int capella_cm3602_release(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);
	misc_opened = 0;
	return capella_cm3602_disable(&the_data);
}

static long capella_cm3602_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int val;
	pr_info("%s cmd %d\n", __func__, _IOC_NR(cmd));
	switch (cmd) {
	case CAPELLA_CM3602_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;
		if (val)
			return capella_cm3602_enable(&the_data);
		else
			return capella_cm3602_disable(&the_data);
		break;
	case CAPELLA_CM3602_IOCTL_GET_ENABLED:
		return put_user(the_data.enabled, (unsigned long __user *)arg);
		break;
	default:
		pr_err("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
		return -EINVAL;
	}
}

static struct file_operations capella_cm3602_fops = {
	.owner = THIS_MODULE,
	.open = capella_cm3602_open,
	.release = capella_cm3602_release,
	.unlocked_ioctl = capella_cm3602_ioctl
};

static struct miscdevice capella_cm3602_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cm3602",
	.fops = &capella_cm3602_fops
};

static int capella_cm3602_probe(struct platform_device *pdev)
{
	int rc = -1;
	struct input_dev *input_dev;
	struct capella_cm3602_data *ip;
	struct capella_cm3602_platform_data *pdata;

	struct class  *proximity_attr_class;
	struct device *proximity_attr_dev;

	pr_info("%s: probe\n", __func__);

	pdata = pdev->dev.platform_data;

	if (!microp_register_ps_callback(&ps_callback))
		printk(KERN_ERR "%s: Microp has been initialized!!\n",
				__func__);

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		rc = -ENODEV;
		goto done;
	}

	ip = &the_data;
	platform_set_drvdata(pdev, ip);

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		rc = -ENOMEM;
		goto done;
	}
	ip->input_dev = input_dev;
	ip->pdata = pdata;
	input_set_drvdata(input_dev, ip);

	input_dev->name = "proximity";

	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	rc = input_register_device(input_dev);
	if (rc < 0) {
		pr_err("%s: could not register input device\n", __func__);
		goto err_free_input_device;
	}

	rc = misc_register(&capella_cm3602_misc);
	if (rc < 0) {
		pr_err("%s: could not register misc device\n", __func__);
		goto err_unregister_input_device;
	}

	wake_lock_init(&proximity_wake_lock, WAKE_LOCK_SUSPEND, "proximity");

	proximity_attr_class = class_create(THIS_MODULE, "sensors");
	if (IS_ERR(proximity_attr_class)) {
		pr_err("%s: class_create failed\n", __func__);
		rc = PTR_ERR(proximity_attr_class);
		proximity_attr_class = NULL;
		goto err_create_class;
	}

	proximity_attr_dev = device_create(proximity_attr_class,
					NULL, 0, "%s", "proximity_sensor");
	if (unlikely(IS_ERR(proximity_attr_dev))) {
		pr_err("%s: device create failed\n", __func__);
		rc = PTR_ERR(proximity_attr_dev);
		proximity_attr_dev = NULL;
		goto err_create_proximity_attr_device;
	}

	rc = device_create_file(proximity_attr_dev, &dev_attr_proximity);
	if (rc) {
		pr_err("%s: device_create_file failed\n", __func__);
		goto err_create_proximity_device_file;
	}

	rc = gpio_request(pdata->p_en, "gpio_proximity_en");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->p_en, rc);
		goto err_request_proximity_en;
	}

	goto done;

err_request_proximity_en:
	device_remove_file(proximity_attr_dev, &dev_attr_proximity);
err_create_proximity_device_file:
	device_unregister(proximity_attr_dev);
err_create_proximity_attr_device:
	class_destroy(proximity_attr_class);
err_create_class:
	misc_deregister(&capella_cm3602_misc);
err_unregister_input_device:
	input_unregister_device(input_dev);
err_free_input_device:
	input_free_device(input_dev);
done:
	return rc;
}

static struct microp_ops ops = {
	.init_microp_func = incrediblec_microp_function_init,
	/*.als_pwr_enable = incrediblec_als_power,*/
	/*.als_level_change = incrediblec_als_level_change,*/
	/*.als_intr_enable = incrediblec_als_intr_enable,*/
	/*.notifier_func = incrediblec_notifier_func,*/
	/*.led_gpio_set = incrediblec_vkey_led_control,*/
};

void __init incrediblec_microp_init(void)
{
	/*led_trigger_register(&incrediblec_als_level_trigger);*/
	microp_register_ops(&ops);
}

static struct platform_driver capella_cm3602_driver = {
	.probe = capella_cm3602_probe,
	.driver = {
		.name = "incrediblec_proximity",
		.owner = THIS_MODULE
	},
};

static int __init incrediblec_capella_cm3602_init(void)
{
	if (!machine_is_incrediblec())
		return 0;

	return platform_driver_register(&capella_cm3602_driver);
}

device_initcall(incrediblec_capella_cm3602_init);
#endif
