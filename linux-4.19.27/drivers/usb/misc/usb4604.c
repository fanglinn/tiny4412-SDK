// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for SMSC USB4604 USB 2.0 hub controller driver
 *
 * Copyright (c) 2012-2013 Dongjin Kim (tobetter@gmail.com)
 */

#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb4604.h>
#include <linux/regmap.h>

#define USB4604_VIDL		0x00
#define USB4604_VIDM		0x01
#define USB4604_PIDL		0x02
#define USB4604_PIDM		0x03
#define USB4604_DIDL		0x04
#define USB4604_DIDM		0x05

#define USB4604_CFG1		0x06
#define USB4604_SELF_BUS_PWR	(1 << 7)

#define USB4604_CFG2		0x07
#define USB4604_CFG3		0x08
#define USB4604_NRD		0x09

#define USB4604_PDS		0x0a

#define USB4604_SP_ILOCK	0xe7
#define USB4604_SPILOCK_CONNECT	(1 << 1)
#define USB4604_SPILOCK_CONFIG	(1 << 0)

#define USB4604_CFGP		0xee
#define USB4604_CLKSUSP		(1 << 7)

#define USB4604_RESET		0xff

struct usb4604 {
	enum usb4604_mode	mode;
	struct regmap		*regmap;
	struct device		*dev;
	struct clk		*clk;
	u8	port_off_mask;
	int	gpio_intn;
	int	gpio_reset;
	int	gpio_connect;
	bool	secondary_ref_clk;
};

static int usb4604_reset(struct usb4604 *hub, int state)
{
	if (!state && gpio_is_valid(hub->gpio_connect))
		gpio_set_value_cansleep(hub->gpio_connect, 0);

	if (gpio_is_valid(hub->gpio_reset))
		gpio_set_value_cansleep(hub->gpio_reset, state);

	/* Wait T_HUBINIT == 4ms for hub logic to stabilize */
	if (state)
		usleep_range(4000, 10000);

	return 0;
}

static int usb4604_connect(struct usb4604 *hub)
{
	struct device *dev = hub->dev;
	int err;

	usb4604_reset(hub, 1);

	if (hub->regmap) {
		/* SP_ILOCK: set connect_n, config_n for config */
		err = regmap_write(hub->regmap, USB4604_SP_ILOCK,
			   (USB4604_SPILOCK_CONNECT
				 | USB4604_SPILOCK_CONFIG));
		if (err < 0) {
			dev_err(dev, "SP_ILOCK failed (%d)\n", err);
			return err;
		}

		/* PDS : Set the ports which are disabled in self-powered mode. */
		if (hub->port_off_mask) {
			err = regmap_update_bits(hub->regmap, USB4604_PDS,
					hub->port_off_mask,
					hub->port_off_mask);
			if (err < 0) {
				dev_err(dev, "PDS failed (%d)\n", err);
				return err;
			}
		}

		/* CFG1 : Set SELF_BUS_PWR, this enables self-powered operation. */
		err = regmap_update_bits(hub->regmap, USB4604_CFG1,
					 USB4604_SELF_BUS_PWR,
					 USB4604_SELF_BUS_PWR);
		if (err < 0) {
			dev_err(dev, "CFG1 failed (%d)\n", err);
			return err;
		}

		/* SP_LOCK: clear connect_n, config_n for hub connect */
		err = regmap_update_bits(hub->regmap, USB4604_SP_ILOCK,
					 (USB4604_SPILOCK_CONNECT
					  | USB4604_SPILOCK_CONFIG), 0);
		if (err < 0) {
			dev_err(dev, "SP_ILOCK failed (%d)\n", err);
			return err;
		}
	}

	if (gpio_is_valid(hub->gpio_connect))
		gpio_set_value_cansleep(hub->gpio_connect, 1);

	hub->mode = USB4604_MODE_HUB;
	dev_info(dev, "switched to HUB mode\n");

	return 0;
}

static int usb4604_switch_mode(struct usb4604 *hub, enum usb4604_mode mode)
{
	struct device *dev = hub->dev;
	int err = 0;

	switch (mode) {
	case USB4604_MODE_HUB:
		err = usb4604_connect(hub);
		break;

	case USB4604_MODE_STANDBY:
		usb4604_reset(hub, 0);
		dev_info(dev, "switched to STANDBY mode\n");
		break;

	default:
		dev_err(dev, "unknown mode is requested\n");
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct regmap_config usb4604_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = USB4604_RESET,
};

static int usb4604_probe(struct usb4604 *hub)
{
	struct device *dev = hub->dev;
	struct usb4604_platform_data *pdata = dev_get_platdata(dev);
	struct device_node *np = dev->of_node;
	int err;
	u32 mode = USB4604_MODE_HUB;
	const u32 *property;
	int len;

	if (pdata) {
		hub->port_off_mask	= pdata->port_off_mask;
		hub->gpio_intn		= pdata->gpio_intn;
		hub->gpio_connect	= pdata->gpio_connect;
		hub->gpio_reset		= pdata->gpio_reset;
		hub->mode		= pdata->initial_mode;
	} else if (np) {
		struct clk *clk;
		u32 rate = 0;
		hub->port_off_mask = 0;

		if (!of_property_read_u32(np, "refclk-frequency", &rate)) {
			switch (rate) {
			case 38400000:
			case 26000000:
			case 19200000:
			case 12000000:
				hub->secondary_ref_clk = 0;
				break;
			case 24000000:
			case 27000000:
			case 25000000:
			case 50000000:
				hub->secondary_ref_clk = 1;
				break;
			default:
				dev_err(dev,
					"unsupported reference clock rate (%d)\n",
					(int) rate);
				return -EINVAL;
			}
		}

		clk = devm_clk_get(dev, "refclk");
		if (IS_ERR(clk) && PTR_ERR(clk) != -ENOENT) {
			dev_err(dev, "unable to request refclk (%ld)\n",
					PTR_ERR(clk));
			return PTR_ERR(clk);
		}

		if (!IS_ERR(clk)) {
			hub->clk = clk;

			if (rate != 0) {
				err = clk_set_rate(hub->clk, rate);
				if (err) {
					dev_err(dev,
						"unable to set reference clock rate to %d\n",
						(int) rate);
					return err;
				}
			}

			err = clk_prepare_enable(hub->clk);
			if (err) {
				dev_err(dev,
					"unable to enable reference clock\n");
				return err;
			}
		}

		property = of_get_property(np, "disabled-ports", &len);
		if (property && (len / sizeof(u32)) > 0) {
			int i;
			for (i = 0; i < len / sizeof(u32); i++) {
				u32 port = be32_to_cpu(property[i]);
				if ((1 <= port) && (port <= 3))
					hub->port_off_mask |= (1 << port);
			}
		}

		hub->gpio_intn	= of_get_named_gpio(np, "intn-gpios", 0);
		if (hub->gpio_intn == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		hub->gpio_connect = of_get_named_gpio(np, "connect-gpios", 0);
		if (hub->gpio_connect == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		hub->gpio_reset = of_get_named_gpio(np, "reset-gpios", 0);
		if (hub->gpio_reset == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		of_property_read_u32(np, "initial-mode", &mode);
		hub->mode = mode;
	}

	if (hub->port_off_mask && !hub->regmap)
		dev_err(dev, "Ports disabled with no control interface\n");

	if (gpio_is_valid(hub->gpio_intn)) {
		int val = hub->secondary_ref_clk ? GPIOF_OUT_INIT_LOW :
						   GPIOF_OUT_INIT_HIGH;
		err = devm_gpio_request_one(dev, hub->gpio_intn, val,
					    "usb4604 intn");
		if (err) {
			dev_err(dev,
				"unable to request GPIO %d as interrupt pin (%d)\n",
				hub->gpio_intn, err);
			return err;
		}
	}

	if (gpio_is_valid(hub->gpio_connect)) {
		err = devm_gpio_request_one(dev, hub->gpio_connect,
				GPIOF_OUT_INIT_LOW, "usb4604 connect");
		if (err) {
			dev_err(dev,
				"unable to request GPIO %d as connect pin (%d)\n",
				hub->gpio_connect, err);
			return err;
		}
	}

	if (gpio_is_valid(hub->gpio_reset)) {
		err = devm_gpio_request_one(dev, hub->gpio_reset,
				GPIOF_OUT_INIT_LOW, "usb4604 reset");
		/* Datasheet defines a hardware reset to be at least 100us */
		usleep_range(100, 10000);
		if (err) {
			dev_err(dev,
				"unable to request GPIO %d as reset pin (%d)\n",
				hub->gpio_reset, err);
			return err;
		}
	}

	usb4604_switch_mode(hub, hub->mode);

	dev_info(dev, "%s: probed in %s mode\n", __func__,
			(hub->mode == USB4604_MODE_HUB) ? "hub" : "standby");

	return 0;
}

static int usb4604_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct usb4604 *hub;
	int err;

	hub = devm_kzalloc(&i2c->dev, sizeof(struct usb4604), GFP_KERNEL);
	if (!hub)
		return -ENOMEM;

	i2c_set_clientdata(i2c, hub);
	hub->regmap = devm_regmap_init_i2c(i2c, &usb4604_regmap_config);
	if (IS_ERR(hub->regmap)) {
		err = PTR_ERR(hub->regmap);
		dev_err(&i2c->dev, "Failed to initialise regmap: %d\n", err);
		return err;
	}
	hub->dev = &i2c->dev;

	return usb4604_probe(hub);
}

static int usb4604_i2c_remove(struct i2c_client *i2c)
{
	struct usb4604 *hub;

	hub = i2c_get_clientdata(i2c);
	if (hub->clk)
		clk_disable_unprepare(hub->clk);

	return 0;
}

static int usb4604_platform_probe(struct platform_device *pdev)
{
	struct usb4604 *hub;

	hub = devm_kzalloc(&pdev->dev, sizeof(struct usb4604), GFP_KERNEL);
	if (!hub)
		return -ENOMEM;
	hub->dev = &pdev->dev;
	platform_set_drvdata(pdev, hub);

	return usb4604_probe(hub);
}

static int usb4604_platform_remove(struct platform_device *pdev)
{
	struct usb4604 *hub;

	hub = platform_get_drvdata(pdev);
	if (hub->clk)
		clk_disable_unprepare(hub->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int usb4604_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct usb4604 *hub = i2c_get_clientdata(client);

	usb4604_switch_mode(hub, USB4604_MODE_STANDBY);

	if (hub->clk)
		clk_disable_unprepare(hub->clk);

	return 0;
}

static int usb4604_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct usb4604 *hub = i2c_get_clientdata(client);

	if (hub->clk)
		clk_prepare_enable(hub->clk);

	usb4604_switch_mode(hub, hub->mode);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(usb4604_i2c_pm_ops, usb4604_i2c_suspend,
		usb4604_i2c_resume);

static const struct i2c_device_id usb4604_id[] = {
	{ USB4604_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, usb4604_id);

#ifdef CONFIG_OF
static const struct of_device_id usb4604_of_match[] = {
	{ .compatible = "smsc,usb4604", },
	{ .compatible = "smsc,usb4604a", },
	{},
};
MODULE_DEVICE_TABLE(of, usb4604_of_match);
#endif

static struct i2c_driver usb4604_i2c_driver = {
	.driver = {
		.name = USB4604_I2C_NAME,
		.pm = &usb4604_i2c_pm_ops,
		.of_match_table = of_match_ptr(usb4604_of_match),
	},
	.probe		= usb4604_i2c_probe,
	.remove		= usb4604_i2c_remove,
	.id_table	= usb4604_id,
};

static struct platform_driver usb4604_platform_driver = {
	.driver = {
		.name = USB4604_I2C_NAME,
		.of_match_table = of_match_ptr(usb4604_of_match),
	},
	.probe		= usb4604_platform_probe,
	.remove		= usb4604_platform_remove,
};

static int __init usb4604_init(void)
{
	int err;

	err = i2c_add_driver(&usb4604_i2c_driver);
	if (err != 0)
		pr_err("usb4604: Failed to register I2C driver: %d\n", err);

	err = platform_driver_register(&usb4604_platform_driver);
	if (err != 0)
		pr_err("usb4604: Failed to register platform driver: %d\n",
		       err);

	return 0;
}
module_init(usb4604_init);

static void __exit usb4604_exit(void)
{
	platform_driver_unregister(&usb4604_platform_driver);
	i2c_del_driver(&usb4604_i2c_driver);
}
module_exit(usb4604_exit);

MODULE_AUTHOR("Dongjin Kim <tobetter@gmail.com>");
MODULE_DESCRIPTION("USB4604 USB HUB driver");
MODULE_LICENSE("GPL");
