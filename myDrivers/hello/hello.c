/*
 * linux-4.19.27
 * arm-linux-gcc-4.5.1
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#include <linux/device.h>    /* class_device ... */
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>



static int hello_probe(struct platform_device *platdata)
{
	printk(KERN_NOTICE "tiny4412 hello-driver probe .\n");

	return 0;
}


static int hello_remove(struct platform_device *platdata)
{
	printk(KERN_NOTICE "tiny4412 hello-driver remove .\n");

	return 0;
}


static const struct of_device_id of_match_hello[] = {
        { .compatible = "tiny4412_hello", .data = NULL },
        { /* sentinel */ }
};


static struct  platform_driver hello_plat_driver =
{
        .probe = hello_probe,
        .remove = hello_remove,
        .driver =
        {
                .name = "tiny4412_hello",
                .of_match_table = of_match_hello,
        },
};



static int hello_init(void)
{
	printk(KERN_NOTICE "tiny4412 hello-driver init .\n");

    platform_driver_register(&hello_plat_driver);
	
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_NOTICE "tiny4412 hello-driver exit .\n");

	platform_driver_unregister(&hello_plat_driver);
}


module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Flinn <Flinn682@foxmail.com>");
MODULE_DESCRIPTION("TINY4412 hello driver");
MODULE_LICENSE("GPL v2");

