/*
 * hello driver for tiny4412 on linux-4.19.27
*/
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/platform_device.h>



static int gpio_demo_probe(struct platform_device *pdev)
{
	printk("%s enter.\n", __func__);
	
	return 0;
}


static int gpio_demo_remove(struct platform_device *pdev)
{
	printk("%s enter.\n", __func__);

	//device_remove_file(&pdev->dev, &dev_attr_gpio_demo);
	return 0;
}


static const struct of_device_id gpio_demo_dt_ids[] = {
	{ .compatible = "tiny4412,gpio_demo", },
	{},
};


MODULE_DEVICE_TABLE(of, gpio_demo_dt_ids);


static struct platform_driver gpio_demo_driver = {
	.driver        = {
		.name    = "gpio_demo",
		.of_match_table    = of_match_ptr(gpio_demo_dt_ids),
	},
	.probe        = gpio_demo_probe,
	.remove        = gpio_demo_remove,
};


static int __init gpio_demo_init(void)
{
	int ret;
	ret = platform_driver_register(&gpio_demo_driver);
	if (ret)
		printk(KERN_ERR "int demo: probe failed: %d\n", ret);

	return ret;
}


static void __exit gpio_demo_exit(void)
{
	platform_driver_unregister(&gpio_demo_driver);
}


module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");

