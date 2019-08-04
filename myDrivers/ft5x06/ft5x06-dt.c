/*
 * ft5x06-dt.c driver for tiny4412 on linux-4.19.27
*/
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <linux/platform_device.h>
#include <linux/slab.h>       //
#include <linux/io.h>

#include <linux/gfp.h>
#include <linux/cdev.h>

#include <linux/uaccess.h> /* copy_from_usr, */
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/wait.h>

#include <linux/i2c.h>
#include <linux/input.h>


static int ft5x06_probe(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);

	return 0;
}

static int ft5x06_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);

	return 0;
}


const struct of_device_id tiny4412_ft5x06_match_tbl[] = 
{
	{.compatible = "tiny4412,ft5x06"},
	{},
};


static struct platform_driver tiny4412_ft5x06_dt_drv = 
{
	.probe = ft5x06_probe,
	.remove = ft5x06_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "ts",
		.of_match_table = tiny4412_ft5x06_match_tbl,
	}
};


static int __init ft5x06_ts_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tiny4412_ft5x06_dt_drv);
	if(ret)
		printk(KERN_ERR "probe failed: %d\n", ret);

	return ret;
}

static void __exit ft5x06_ts_exit(void)
{
	platform_driver_unregister(&tiny4412_ft5x06_dt_drv);
}

module_init(ft5x06_ts_init);
module_exit(ft5x06_ts_exit);
MODULE_LICENSE("GPL");
